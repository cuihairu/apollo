[English](q103-script-hot-reload.en.md) | [中文](q103-script-hot-reload.md)

# Q103: How do you implement script hot updates?

## Problem Analysis

This question tests your understanding of script hot updates:
- Hot update principles
- State preservation
- Module reloading
- KBEngine hot updates

---

## One: Hot Update Principles

### 1.1 Hot Update Types

```
┌─────────────────────────────────────────────────────────────┐
│                    脚本热更新类型                              │ (Script Hot Update Types                   │)
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  配置热更新:                                                 │ (Configuration hot update:                                  │)
│  ├── 配置文件变化                                            │ (Configuration file changes                             │)
│  ├── 重新加载配置                                            │ (Reload configuration                                   │)
│  ├── 无需重启                                              │ (No restart needed                                      │)
│  └── 示例: 伤害系数、掉落表                                   │ (Example: damage coefficients, drop tables              │)
│                                                             │
│  代码热更新:                                                 │ (Code hot update:                                           │)
│  ├── 函数实现变化                                            │ (Function implementation changes                        │)
│  ├── 新增函数                                               │ (New functions                                          │)
│  ├── 模块重载                                               │ (Module reload                                          │)
│  └── 示例: Bug 修复、功能增强                                 │ (Example: bug fixes, feature enhancements               │)
│                                                             │
│  结构热更新 (复杂):                                          │ (Structural hot update (complex):                          │)
│  ├── 类定义变化                                              │ (Class definition changes                               │)
│  ├── 继承关系变化                                            │ (Inheritance relationship changes                       │)
│  ├── 需要重启或特殊处理                                       │ (Requires restart or special handling                   │)
│  └── 示例: 新增实体属性                                       │ (Example: adding entity attributes                      │)
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## Two: Python Hot Updates

### 2.1 importlib.reload

```python
# Python 热更新实现 (Python hot update implementation)

import sys
import importlib
import os
import time
from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler

class ScriptReloader(FileSystemEventHandler):
    """脚本文件变化处理器 (Script file change handler)"""

    def __init__(self, script_dir="scripts"):
        self.script_dir = script_dir
        self.loaded_modules = {}
        self.reload_callbacks = []

    def on_modified(self, event):
        """文件修改时调用 (Called when a file is modified)"""
        if event.is_directory:
            return

        file_path = event.src_path
        if not file_path.endswith('.py'):
            return

        # 转换为模块名 (Convert to a module name)
        module_name = self.path_to_module(file_path)

        if module_name in sys.modules:
            print(f"[HotReload] Reloading: {module_name}")

            # 保存旧模块状态 (Save the old module state)
            old_module = sys.modules[module_name]

            # 调用清理函数 (Call the cleanup function)
            if hasattr(old_module, 'on_unload'):
                old_module.on_unload()

            # 重新加载 (Reload)
            try:
                new_module = importlib.reload(old_module)

                # 调用加载函数 (Call the load function)
                if hasattr(new_module, 'on_load'):
                    new_module.on_load()

                # 通知回调 (Notify callbacks)
                for callback in self.reload_callbacks:
                    callback(module_name)

                print(f"[HotReload] Success: {module_name}")
            except Exception as e:
                print(f"[HotReload] Error reloading {module_name}: {e}")

    def path_to_module(self, file_path):
        """文件路径转模块名 (File path to module name)"""
        # 移除 .py 后缀 (Remove the .py suffix)
        module_path = file_path.replace('.py', '')

        # 转换路径分隔符 (Convert path separators)
        module_path = module_path.replace('/', '.').replace('\\', '.')

        # 移除 scripts 前缀 (Remove the scripts prefix)
        if module_path.startswith('scripts.'):
            module_path = module_path[8:]

        return module_path


class HotReloadManager:
    """热更新管理器 (Hot update manager)"""

    def __init__(self, script_dir="scripts"):
        self.script_dir = script_dir
        self.observer = None
        self.handlers = {}

    def start(self):
        """启动热更新监控 (Start hot update monitoring)"""
        event_handler = ScriptReloader(self.script_dir)
        self.observer = Observer()
        self.observer.schedule(event_handler, self.script_dir, recursive=True)
        self.observer.start()

        print(f"[HotReload] Watching: {self.script_dir}")

    def stop(self):
        """停止监控 (Stop monitoring)"""
        if self.observer:
            self.observer.stop()
            self.observer.join()


# 使用示例 (Usage example)
if __name__ == "__main__":
    reloader = HotReloadManager("scripts")
    reloader.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        reloader.stop()
```

### 2.2 Hot Updates with State Preservation

```python
# 状态保持的热更新 (Hot updates with state preservation)

import types
import copy

class StatefulReloader:
    """带状态保持的重新加载器 (Reloader with state preservation)"""

    def __init__(self):
        self.module_states = {}

    def reload_with_state(self, module_name):
        """重新加载模块并保持状态 (Reload a module while preserving state)"""
        if module_name not in sys.modules:
            return None

        old_module = sys.modules[module_name]

        # 保存状态 (Save state)
        state = self.save_state(old_module)
        self.module_states[module_name] = state

        # 重新加载 (Reload)
        new_module = importlib.reload(old_module)

        # 恢复状态 (Restore state)
        self.restore_state(new_module, state)

        return new_module

    def save_state(self, module):
        """保存模块状态 (Save module state)"""
        state = {}

        for name in dir(module):
            if name.startswith('_'):
                continue

            value = getattr(module, name)

            # 保存数据属性，跳过函数/类 (Save data attributes, skip functions/classes)
            if not isinstance(value, (types.FunctionType, types.ModuleType, type)):
                try:
                    # 尝试深拷贝 (Attempt a deep copy)
                    state[name] = copy.deepcopy(value)
                except:
                    # 无法拷贝的跳过 (Skip anything that cannot be copied)
                    pass

        return state

    def restore_state(self, module, state):
        """恢复模块状态 (Restore module state)"""
        for name, value in state.items():
            if hasattr(module, name):
                try:
                    setattr(module, name, value)
                except:
                    pass


# 脚本中定义保存/恢复函数 (Define save/restore functions in the script)
"""
# game_logic.py

class GameState:
    def __init__(self):
        self.players = {}
        self.counter = 0

# 全局实例 (Global instance)
game_state = GameState()

def on_load():
    print("Module loaded")

def on_unload():
    print("Module unloading")
    # 保存必要数据 (Save necessary data)
    return save_game_state()

def save_game_state():
    return {
        'players': game_state.players.copy(),
        'counter': game_state.counter
    }

def load_game_state(state):
    game_state.players.update(state.get('players', {}))
    game_state.counter = state.get('counter', 0)
"""
```

---

## Three: Lua Hot Updates

### 3.1 Lua Module Reloading

```lua
-- Lua 热更新实现 (Lua hot update implementation)

-- 模块缓存 (Module cache)
local module_cache = {}
local module_states = {}

-- 保存模块状态 (Save module state)
local function save_module_state(module_name)
    local module = package.loaded[module_name]
    if not module then return nil end

    local state = {}
    for k, v in pairs(module) do
        -- 只保存数据，不保存函数 (Save data only, not functions)
        if type(v) ~= "function" and type(k) ~= "string" or
           not string.match(k, "^_") then
            state[k] = v
        end
    end

    return state
end

-- 恢复模块状态 (Restore module state)
local function restore_module_state(module_name, state)
    local module = package.loaded[module_name]
    if not module then return end

    if state then
        for k, v in pairs(state) do
            module[k] = v
        end
    end
end

-- 重新加载模块 (Reload a module)
function reload_module(module_name)
    print("Reloading module:", module_name)

    -- 保存状态 (Save state)
    local state = save_module_state(module_name)
    module_states[module_name] = state

    -- 清除模块缓存 (Clear the module cache)
    package.loaded[module_name] = nil

    -- 重新加载 (Reload)
    local ok, new_module = pcall(require, module_name)

    if not ok then
        print("Error reloading:", new_module)
        -- 恢复旧模块 (Restore the old module)
        if state then
            package.loaded[module_name] = state
        end
        return false, new_module
    end

    -- 恢复状态 (Restore state)
    restore_module_state(module_name, state)

    -- 调用重载回调 (Call the reload callback)
    if new_module.on_reload then
        new_module.on_reload()
    end

    print("Module reloaded:", module_name)
    return true, new_module
end

-- 示例模块 (Example module)
--[[
-- player.lua
local player = {}

player.data = {
    count = 0,
    items = {}
}

function player.on_reload()
    print("Player module reloaded!")
    -- 保持 data 不变 (Keep data unchanged)
end

return player
--]]

-- 使用 (Usage)
local player = require("player")
-- ... 修改文件 ... (... modify the file ...)
reload_module("player")
```

---

## Four: KBEngine Script Hot Updates

### 4.1 The KBEngine Hot Update Mechanism

```python
# KBEngine 脚本热更新 (KBEngine script hot updates)

"""
KBEngine 脚本热更新配置: (script hot update configuration:)

在 kbengine_defaults.xml 中配置: (Configure in kbengine_defaults.xml:)

<root>
    <reloadScripts>
        <enable>true</enable>
        <scanInterval>1.0</scanInterval>
        <scriptsPath>scripts/</scriptsPath>
    </reloadScripts>
</root>
"""

import KBEngine
import sys

class HotReloadHandler:
    """KBEngine 热更新处理器 (KBEngine hot update handler)"""

    def __init__(self):
        self.entity_types = {}
        self.callbacks = []

    def onInit(self, isReload):
        """初始化回调 (Initialization callback)"""
        if isReload:
            self.handle_reload()
        else:
            print("Scripts initializing...")
            self.register_entities()

    def handle_reload(self):
        """处理热更新 (Handle the hot update)"""
        print("Scripts reloading...")

        # 通知所有实体 (Notify all entities)
        for entity_id, entity in KBEngine.entities.items():
            if hasattr(entity, 'onScriptReload'):
                try:
                    entity.onScriptReload()
                except Exception as e:
                    print(f"Error in entity {entity_id} onScriptReload: {e}")

        # 执行注册回调 (Execute registration callbacks)
        for callback in self.callbacks:
            try:
                callback()
            except Exception as e:
                print(f"Error in reload callback: {e}")

        print("Scripts reloaded successfully")

    def register_callback(self, callback):
        """注册重载回调 (Register a reload callback)"""
        self.callbacks.append(callback)

    def register_entities(self):
        """注册实体类型 (Register entity types)"""
        # 注册所有实体类 (Register all entity classes)
        KBEngine.registerEntityType("Account", Account)
        KBEngine.registerEntityType("Avatar", Avatar)
        # ...


# 实体中处理热更新 (Handle hot updates inside entities)
class Account(KBEngine.Account):
    def __init__(self):
        KBEngine.Account.__init__(self)
        self._state_version = 0

    def onScriptReload(self):
        """脚本热更新时调用 (Called during a script hot update)"""
        print(f"Account {self.id}: Script reloaded")

        # 保存需要保持的状态 (Save state that needs to be preserved)
        self._save_reload_state()

        self._state_version += 1

    def _save_reload_state(self):
        """保存重载状态 (Save reload state)"""
        self._reload_state = {
            'playerName': self.playerName,
            'level': getattr(self, 'level', 1),
            'gold': getattr(self, 'gold', 0)
        }

    def _restore_reload_state(self):
        """恢复重载状态 (Restore reload state)"""
        if hasattr(self, '_reload_state'):
            state = self._reload_state
            self.playerName = state.get('playerName', '')
            self.level = state.get('level', 1)
            self.gold = state.get('gold', 0)
            delattr(self, '_reload_state')
```

---

## Five: Hot Update Strategies

### 5.1 Progressive Hot Updates

```python
# 渐进式热更新策略 (Progressive hot update strategy)

class ProgressiveReloader:
    """渐进式热更新器 (Progressive hot reloader)"""

    def __init__(self):
        self.phases = []
        self.current_phase = 0

    def add_phase(self, description, action):
        """添加热更新阶段 (Add a hot update phase)"""
        self.phases.append({
            'description': description,
            'action': action
        })

    def execute(self):
        """执行渐进式热更新 (Execute a progressive hot update)"""
        for i, phase in enumerate(self.phases):
            print(f"[Phase {i+1}/{len(self.phases)}] {phase['description']}")

            try:
                result = phase['action']()

                if result is False:
                    print(f"Phase {i+1} failed, aborting")
                    return False

                print(f"Phase {i+1} completed")

            except Exception as e:
                print(f"Phase {i+1} error: {e}")
                return False

        print("All phases completed successfully")
        return True


# 使用示例 (Usage example)
def hot_update_game_logic():
    reloader = ProgressiveReloader()

    # Phase 1: 保存关键状态 (Phase 1: Save critical state)
    reloader.add_phase("Saving critical state", lambda: save_critical_state())

    # Phase 2: 重新加载核心模块 (Phase 2: Reload core modules)
    reloader.add_phase("Reloading core modules", lambda: reload_core_modules())

    # Phase 3: 更新配置 (Phase 3: Update configuration)
    reloader.add_phase("Updating configuration", lambda: update_configuration())

    # Phase 4: 通知玩家 (Phase 4: Notify players)
    reloader.add_phase("Notifying players", lambda: notify_players_reload())

    # Phase 5: 验证状态 (Phase 5: Validate state)
    reloader.add_phase("Validating state", lambda: validate_reload_state())

    return reloader.execute()
```

---

## Six: Best Practices

### 6.1 Hot Update Recommendations

| Practice | Description |
|------|------|
| **State preservation** | Save critical state before a hot update |
| **Version compatibility** | New versions compatible with old state |
| **Rollback mechanism** | Roll back when a hot update fails |
| **Canary release** | Hot update some servers first |
| **Logging** | Record all hot update operations |
| **Test validation** | Thoroughly test before a hot update |

---

## Seven: Summary

### Core of Script Hot Updates

```
脚本热更新 = 状态保存 + 模块重载 + 状态恢复 + 验证 (Script hot update = state saving + module reload + state restoration + validation)
- Python: importlib.reload
- Lua: package.loaded 清除 (clear package.loaded)
- KBEngine 内置支持 (built-in support)
- 保持兼容性是关键 (Maintaining compatibility is the key)
```

---

## References

- [Python Module Reloading](https://docs.python.org/3/library/importlib.html)
- [Lua Package Loading](https://www.lua.org/manual/5.4/manual.html#pdf-require)
- [KBEngine Script Reload](https://kbengine.github.io/docs/en/configuration.html#reloadscripts)
