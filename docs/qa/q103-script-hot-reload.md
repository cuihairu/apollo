# Q103: 如何实现脚本热更新？

## 问题分析

本题考察对脚本热更新的理解：
- 热更新原理
- 状态保持
- 模块重载
- KBEngine 热更

---

## 一、热更新原理

### 1.1 热更类型

```
┌─────────────────────────────────────────────────────────────┐
│                    脚本热更新类型                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  配置热更新:                                                 │
│  ├── 配置文件变化                                            │
│  ├── 重新加载配置                                            │
│  ├── 无需重启                                              │
│  └── 示例: 伤害系数、掉落表                                   │
│                                                             │
│  代码热更新:                                                 │
│  ├── 函数实现变化                                            │
│  ├── 新增函数                                               │
│  ├── 模块重载                                               │
│  └── 示例: Bug 修复、功能增强                                 │
│                                                             │
│  结构热更新 (复杂):                                          │
│  ├── 类定义变化                                              │
│  ├── 继承关系变化                                            │
│  ├── 需要重启或特殊处理                                       │
│  └── 示例: 新增实体属性                                       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、Python 热更新

### 2.1 importlib.reload

```python
# Python 热更新实现

import sys
import importlib
import os
import time
from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler

class ScriptReloader(FileSystemEventHandler):
    """脚本文件变化处理器"""

    def __init__(self, script_dir="scripts"):
        self.script_dir = script_dir
        self.loaded_modules = {}
        self.reload_callbacks = []

    def on_modified(self, event):
        """文件修改时调用"""
        if event.is_directory:
            return

        file_path = event.src_path
        if not file_path.endswith('.py'):
            return

        # 转换为模块名
        module_name = self.path_to_module(file_path)

        if module_name in sys.modules:
            print(f"[HotReload] Reloading: {module_name}")

            # 保存旧模块状态
            old_module = sys.modules[module_name]

            # 调用清理函数
            if hasattr(old_module, 'on_unload'):
                old_module.on_unload()

            # 重新加载
            try:
                new_module = importlib.reload(old_module)

                # 调用加载函数
                if hasattr(new_module, 'on_load'):
                    new_module.on_load()

                # 通知回调
                for callback in self.reload_callbacks:
                    callback(module_name)

                print(f"[HotReload] Success: {module_name}")
            except Exception as e:
                print(f"[HotReload] Error reloading {module_name}: {e}")

    def path_to_module(self, file_path):
        """文件路径转模块名"""
        # 移除 .py 后缀
        module_path = file_path.replace('.py', '')

        # 转换路径分隔符
        module_path = module_path.replace('/', '.').replace('\\', '.')

        # 移除 scripts 前缀
        if module_path.startswith('scripts.'):
            module_path = module_path[8:]

        return module_path


class HotReloadManager:
    """热更新管理器"""

    def __init__(self, script_dir="scripts"):
        self.script_dir = script_dir
        self.observer = None
        self.handlers = {}

    def start(self):
        """启动热更新监控"""
        event_handler = ScriptReloader(self.script_dir)
        self.observer = Observer()
        self.observer.schedule(event_handler, self.script_dir, recursive=True)
        self.observer.start()

        print(f"[HotReload] Watching: {self.script_dir}")

    def stop(self):
        """停止监控"""
        if self.observer:
            self.observer.stop()
            self.observer.join()


# 使用示例
if __name__ == "__main__":
    reloader = HotReloadManager("scripts")
    reloader.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        reloader.stop()
```

### 2.2 带状态保持的热更新

```python
# 状态保持的热更新

import types
import copy

class StatefulReloader:
    """带状态保持的重新加载器"""

    def __init__(self):
        self.module_states = {}

    def reload_with_state(self, module_name):
        """重新加载模块并保持状态"""
        if module_name not in sys.modules:
            return None

        old_module = sys.modules[module_name]

        # 保存状态
        state = self.save_state(old_module)
        self.module_states[module_name] = state

        # 重新加载
        new_module = importlib.reload(old_module)

        # 恢复状态
        self.restore_state(new_module, state)

        return new_module

    def save_state(self, module):
        """保存模块状态"""
        state = {}

        for name in dir(module):
            if name.startswith('_'):
                continue

            value = getattr(module, name)

            # 保存数据属性，跳过函数/类
            if not isinstance(value, (types.FunctionType, types.ModuleType, type)):
                try:
                    # 尝试深拷贝
                    state[name] = copy.deepcopy(value)
                except:
                    # 无法拷贝的跳过
                    pass

        return state

    def restore_state(self, module, state):
        """恢复模块状态"""
        for name, value in state.items():
            if hasattr(module, name):
                try:
                    setattr(module, name, value)
                except:
                    pass


# 脚本中定义保存/恢复函数
"""
# game_logic.py

class GameState:
    def __init__(self):
        self.players = {}
        self.counter = 0

# 全局实例
game_state = GameState()

def on_load():
    print("Module loaded")

def on_unload():
    print("Module unloading")
    # 保存必要数据
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

## 三、Lua 热更新

### 3.1 Lua 模块重载

```lua
-- Lua 热更新实现

-- 模块缓存
local module_cache = {}
local module_states = {}

-- 保存模块状态
local function save_module_state(module_name)
    local module = package.loaded[module_name]
    if not module then return nil end

    local state = {}
    for k, v in pairs(module) do
        -- 只保存数据，不保存函数
        if type(v) ~= "function" and type(k) ~= "string" or
           not string.match(k, "^_") then
            state[k] = v
        end
    end

    return state
end

-- 恢复模块状态
local function restore_module_state(module_name, state)
    local module = package.loaded[module_name]
    if not module then return end

    if state then
        for k, v in pairs(state) do
            module[k] = v
        end
    end
end

-- 重新加载模块
function reload_module(module_name)
    print("Reloading module:", module_name)

    -- 保存状态
    local state = save_module_state(module_name)
    module_states[module_name] = state

    -- 清除模块缓存
    package.loaded[module_name] = nil

    -- 重新加载
    local ok, new_module = pcall(require, module_name)

    if not ok then
        print("Error reloading:", new_module)
        -- 恢复旧模块
        if state then
            package.loaded[module_name] = state
        end
        return false, new_module
    end

    -- 恢复状态
    restore_module_state(module_name, state)

    -- 调用重载回调
    if new_module.on_reload then
        new_module.on_reload()
    end

    print("Module reloaded:", module_name)
    return true, new_module
end

-- 示例模块
--[[
-- player.lua
local player = {}

player.data = {
    count = 0,
    items = {}
}

function player.on_reload()
    print("Player module reloaded!")
    -- 保持 data 不变
end

return player
--]]

-- 使用
local player = require("player")
-- ... 修改文件 ...
reload_module("player")
```

---

## 四、KBEngine 脚本热更新

### 4.1 KBEngine 热更机制

```python
# KBEngine 脚本热更新

"""
KBEngine 脚本热更新配置:

在 kbengine_defaults.xml 中配置:

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
    """KBEngine 热更新处理器"""

    def __init__(self):
        self.entity_types = {}
        self.callbacks = []

    def onInit(self, isReload):
        """初始化回调"""
        if isReload:
            self.handle_reload()
        else:
            print("Scripts initializing...")
            self.register_entities()

    def handle_reload(self):
        """处理热更新"""
        print("Scripts reloading...")

        # 通知所有实体
        for entity_id, entity in KBEngine.entities.items():
            if hasattr(entity, 'onScriptReload'):
                try:
                    entity.onScriptReload()
                except Exception as e:
                    print(f"Error in entity {entity_id} onScriptReload: {e}")

        # 执行注册回调
        for callback in self.callbacks:
            try:
                callback()
            except Exception as e:
                print(f"Error in reload callback: {e}")

        print("Scripts reloaded successfully")

    def register_callback(self, callback):
        """注册重载回调"""
        self.callbacks.append(callback)

    def register_entities(self):
        """注册实体类型"""
        # 注册所有实体类
        KBEngine.registerEntityType("Account", Account)
        KBEngine.registerEntityType("Avatar", Avatar)
        # ...


# 实体中处理热更新
class Account(KBEngine.Account):
    def __init__(self):
        KBEngine.Account.__init__(self)
        self._state_version = 0

    def onScriptReload(self):
        """脚本热更新时调用"""
        print(f"Account {self.id}: Script reloaded")

        # 保存需要保持的状态
        self._save_reload_state()

        self._state_version += 1

    def _save_reload_state(self):
        """保存重载状态"""
        self._reload_state = {
            'playerName': self.playerName,
            'level': getattr(self, 'level', 1),
            'gold': getattr(self, 'gold', 0)
        }

    def _restore_reload_state(self):
        """恢复重载状态"""
        if hasattr(self, '_reload_state'):
            state = self._reload_state
            self.playerName = state.get('playerName', '')
            self.level = state.get('level', 1)
            self.gold = state.get('gold', 0)
            delattr(self, '_reload_state')
```

---

## 五、热更新策略

### 5.1 渐进式热更新

```python
# 渐进式热更新策略

class ProgressiveReloader:
    """渐进式热更新器"""

    def __init__(self):
        self.phases = []
        self.current_phase = 0

    def add_phase(self, description, action):
        """添加热更新阶段"""
        self.phases.append({
            'description': description,
            'action': action
        })

    def execute(self):
        """执行渐进式热更新"""
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


# 使用示例
def hot_update_game_logic():
    reloader = ProgressiveReloader()

    # Phase 1: 保存关键状态
    reloader.add_phase("Saving critical state", lambda: save_critical_state())

    # Phase 2: 重新加载核心模块
    reloader.add_phase("Reloading core modules", lambda: reload_core_modules())

    # Phase 3: 更新配置
    reloader.add_phase("Updating configuration", lambda: update_configuration())

    # Phase 4: 通知玩家
    reloader.add_phase("Notifying players", lambda: notify_players_reload())

    # Phase 5: 验证状态
    reloader.add_phase("Validating state", lambda: validate_reload_state())

    return reloader.execute()
```

---

## 六、最佳实践

### 6.1 热更新建议

| 实践 | 说明 |
|------|------|
| **状态保持** | 热更前保存关键状态 |
| **版本兼容** | 新版本兼容旧状态 |
| **回滚机制** | 热更失败可回滚 |
| **灰度发布** | 部分服务器先热更 |
| **日志记录** | 记录所有热更操作 |
| **测试验证** | 热更前充分测试 |

---

## 七、总结

### 脚本热更新核心

```
脚本热更新 = 状态保存 + 模块重载 + 状态恢复 + 验证
- Python: importlib.reload
- Lua: package.loaded 清除
- KBEngine 内置支持
- 保持兼容性是关键
```

---

## 参考资料

- [Python Module Reloading](https://docs.python.org/3/library/importlib.html)
- [Lua Package Loading](https://www.lua.org/manual/5.4/manual.html#pdf-require)
- [KBEngine Script Reload](https://kbengine.github.io/docs/en/configuration.html#reloadscripts)
