[English](q94-hot-reload.en.md) | [中文](q94-hot-reload.md)

# Q94: How Do You Implement Hot Updates?

## Problem Analysis

This question examines understanding of hot updates:
- Principles of hot updates
- Dynamic loading
- State preservation
- KBEngine script hot updates

---

## 1. Principles of Hot Updates

### 1.1 Types of Hot Updates

```
┌─────────────────────────────────────────────────────────────┐
│                    Types of Hot Updates                      │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Configuration hot updates:                                 │
│  ├── Watch for configuration file changes                   │
│  ├── Seamlessly load new configuration                      │
│  ├── Does not affect running                                │
│  └── Example: log level adjustment                          │
│                                                             │
│  Code hot updates:                                          │
│  ├── Python script reload                                   │
│  ├── Lua script hot updates                                │
│  ├── C++ module loading                                    │
│  └── Example: bug fixes, feature additions                  │
│                                                             │
│  Resource hot updates:                                      │
│  ├── Images/models                                          │
│  ├── Configuration data                                     │
│  ├── Client resources                                       │
│  └── Example: UI asset updates                              │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. Python Script Hot Updates

### 2.1 KBEngine Script Hot Updates

```python
# KBEngine Python script hot updates

"""
KBEngine script hot update mechanism:

1. KBEngine supports reloading scripts
2. reloadScripts needs to be configured
3. State is preserved during hot updates
"""

# KBEngine configuration
"""
<root>
    <reloadScripts>
        <!-- 是否启用脚本热更新 (whether to enable script hot updates) -->
        <enable>true</enable>

        <!-- 扫描间隔 (秒) (scan interval (seconds)) -->
        <scanInterval>1.0</scanInterval>

        <!-- 需要热更的脚本目录 (script directory to hot update) -->
        <scriptsPath>scripts/</scriptsPath>

        <!-- 排除的脚本 (excluded scripts) -->
        <exclude>
            <script>scripts/common/__init__.py</script>
        </exclude>
    </reloadScripts>
</root>
```

```python
# scripts/__init__.py
# KBEngine script entry point

import KBEngine
import importlib

# 模块缓存 (module cache)
_module_cache = {}

def onInit(isReload):
    """
    初始化函数 (initialization function)
    isReload: 是否是热更新调用 (whether this is a hot update call)
    """
    if isReload:
        INFO_MSG("Scripts reloading...")
    else:
        INFO_MSG("Scripts initializing...")

    # 注册热更新回调 (register hot update callback)
    KBEngine.setCallback("onReload", onReloadCallback)

def onReloadCallback():
    """热更新回调 (hot update callback)"""
    INFO_MSG("Script files changed, reloading...")

    # 清理旧的模块缓存 (clean up old module cache)
    _cleanup_module_cache()

    # 重新加载模块 (reload modules)
    _reload_modules()

    INFO_MSG("Scripts reloaded successfully")

def _cleanup_module_cache():
    """清理模块缓存 (clean up module cache)"""
    global _module_cache

    for name in list(_module_cache.keys()):
        try:
            module = _module_cache[name]
            # 调用清理函数 (如果存在) (call cleanup function (if present))
            if hasattr(module, 'onUnload'):
                module.onUnload()
            del sys.modules[name]
            del _module_cache[name]
        except:
            pass

def _reload_modules():
    """重新加载所有模块 (reload all modules)"""
    # 获取需要重新加载的模块列表 (get list of modules to reload)
    modules_to_reload = _get_modified_modules()

    for module_name in modules_to_reload:
        try:
            if module_name in sys.modules:
                # 重新加载模块 (reload module)
                importlib.reload(sys.modules[module_name])

                INFO_MSG(f"Reloaded module: {module_name}")
        except Exception as e:
            ERROR_MSG(f"Failed to reload {module_name}: {e}")

def _get_modified_modules():
    """获取修改过的模块 (get modified modules)"""
    modified = set()

    # 检查 scripts 目录下的所有 .py 文件 (check all .py files under the scripts directory)
    import os
    scripts_path = "scripts/"

    for root, dirs, files in os.walk(scripts_path):
        # 排除 __pycache__ (exclude __pycache__)
        dirs[:] = [d for d in dirs if d not in ['__pycache__']]

        for file in files:
            if file.endswith('.py'):
                module_path = os.path.join(root, file)
                module_name = _path_to_module(module_path)

                # 检查文件修改时间 (check file modification time)
                mtime = os.path.getmtime(module_path)

                if module_name in _module_cache:
                    cached_mtime = _module_cache[module_name].get('mtime', 0)

                    if mtime > cached_mtime:
                        modified.add(module_name)

                # 缓存模块信息 (cache module info)
                _module_cache[module_name] = {
                    'path': module_path,
                    'mtime': mtime
                }

    return modified

def _path_to_module(file_path):
    """将文件路径转换为模块名 (convert file path to module name)"""
    path = file_path.replace('/', '.')
    if path.startswith('scripts.'):
        path = path[8:]  # 移除 'scripts.' (remove 'scripts.')

    if path.endswith('.py'):
        path = path[:-3]  # 移除 '.py' (remove '.py')

    return path
```

---

## 3. State Preservation

### 3.1 State Serialization

```python
# 状态序列化保持 (state serialization preservation)

class StatefulEntity(KBEngine.Entity):
    """有状态的实体 (stateful entity)"""

    def __init__(self):
        KBEngine.Entity.__init__(self)

        # 需要保持的状态 (state to preserve)
        self.state = "idle"
        self.target = None
        self.path = []
        self.inventory_items = {}

        # 状态版本 (state version)
        self.state_version = 0

    def onReload(self):
        """热更新时调用 (called during hot update)"""
        # 保存当前状态 (save current state)
        old_state = self.serializeState()

        # 执行热更新后 (after performing the hot update)
        new_state = self.loadState(old_state)

        INFO_MSG(f"Entity {self.id} state reloaded: {old_state} -> {new_state}")

    def serializeState(self):
        """序列化状态 (serialize state)"""
        return {
            "state": self.state,
            "target": self.target,
            "path": self.path,
            "inventory": self.inventory_items,
            "version": self.state_version
        }

    def loadState(self, state_data):
        """加载状态 (load state)"""
        self.state = state_data.get("state", "idle")
        self.target = state_data.get("target")
        self.path = state_data.get("path", [])
        self.inventory_items = state_data.get("inventory", {})
        self.state_version = state_data.get("version", 0)

        return f"{self.state}|{self.target}"
```

---

## 4. C++ Module Hot Loading

### 4.1 Dynamic Library Loading

```cpp
// 动态库热加载 (dynamic library hot loading)

class HotReloadManager {
public:
    // 加载模块 (load module)
    bool loadModule(const std::string& moduleName) {
        // 加载动态库 (load dynamic library)
        HMODULE handle = LoadLibrary((moduleName + ".dll").c_str());

        if (!handle) {
            ERROR("Failed to load module: {}", moduleName);
            return false;
        }

        // 获取初始化函数 (get initialization function)
        using InitFunc = void(*)();
        InitFunc init = (InitFunc)GetProcAddress(handle, "initModule");

        if (!init) {
            FreeLibrary(handle);
            ERROR("Module missing initModule function: {}", moduleName);
            return false;
        }

        // 初始化模块 (initialize module)
        init();

        modules_[moduleName] = handle;

        INFO("Module {} loaded successfully", moduleName);
        return true;
    }

    // 热重载模块 (hot reload module)
    bool reloadModule(const std::string& moduleName) {
        auto it = modules_.find(moduleName);
        if (it == modules_.end()) {
            ERROR("Module not loaded: {}", moduleName);
            return false;
        }

        HMODULE handle = it->second;

        // 获取清理函数 (get cleanup function)
        using CleanupFunc = void(*)();
        CleanupFunc cleanup = (CleanupFunc)GetProcAddress(handle, "cleanupModule");

        if (cleanup) {
            cleanup();
        }

        FreeLibrary(handle);

        // 重新加载 (reload)
        return loadModule(moduleName);
    }

private:
    std::unordered_map<std::string, HMODULE> modules_;
};
```

---

## 5. Best Practices

### 5.1 Hot Update Recommendations

| Practice | Description |
|------|------|
| **状态保持 (State preservation)** | 热更后保持状态 (preserve state after hot update) |
| **版本兼容 (Version compatibility)** | 新版本兼容旧状态 (new versions compatible with old state) |
| **灰度发布 (Canary release)** | 先部分服务器热更 (hot update some servers first) |
| **回滚机制 (Rollback mechanism)** | 热更失败可回滚 (hot update can be rolled back on failure) |
| **日志记录 (Logging)** | 记录热更操作 (record hot update operations) |

---

## 6. Summary

### Hot Update Essentials

```
热更新 = 状态序列化 + 动态加载 + 版本管理 + 灰度发布
(Hot update = state serialization + dynamic loading + version management + canary release)
- KBEngine 支持脚本热更 (KBEngine supports script hot updates)
- 保持兼容状态 (preserve compatible state)
- 失败可回滚 (rollback on failure)
- 充分测试 (full testing)
```

---

## References

- [Python Module Reloading](https://docs.python.org/3/library/importlib.html)
- [Hot Reload Best Practices](https://www.kernel.org/doc/Documentation/process/coding-style.rst)