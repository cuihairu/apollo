# Q94: 如何实现热更新？

## 问题分析

本题考察对热更新的理解：
- 热更新原理
- 动态加载
- 状态保持
- KBEngine 脚本热更

---

## 一、热更新原理

### 1.1 热更新类型

```
┌─────────────────────────────────────────────────────────────┐
│                    热更新类型                                  │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  配置热更新:                                                │
│  ├── 配置文件变化监听                                      │
│  ├── 无缝加载新配置                                        │
│  ├── 不影响运行                                              │
│  └── 示例: 日志级别调整                                      │
│                                                             │
│  代码热更新:                                                │
│  ├── Python 脚本重载                                          │
│  ├── Lua 脚本热更                                            │
│  ├── C++ 模块加载                                           │
│  └── 示例: Bug 修复、功能增加                                 │
│                                                             │
│  资源热更新:                                                │
│  ├── 图片/模型                                              │
│  ├── 配置数据                                              │
│  ├── 客户端资源                                              │
│  └── 示例: UI 素材更新                                        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、Python 脚本热更新

### 2.1 KBEngine 脚本热更新

```python
# KBEngine Python 脚本热更新

"""
KBEngine 脚本热更新机制:

1. KBEngine 支持脚本的重新加载
2. 需要配置 reloadScripts
3. 热更新时状态会保持
"""

# KBEngine 配置
"""
<root>
    <reloadScripts>
        <!-- 是否启用脚本热更新 -->
        <enable>true</enable>

        <!-- 扫描间隔 (秒) -->
        <scanInterval>1.0</scanInterval>

        <!-- 需要热更的脚本目录 -->
        <scriptsPath>scripts/</scriptsPath>

        <!-- 排除的脚本 -->
        <exclude>
            <script>scripts/common/__init__.py</script>
        </exclude>
    </reloadScripts>
</root>
```

```python
# scripts/__init__.py
# KBEngine 脚本入口

import KBEngine
import importlib

# 模块缓存
_module_cache = {}

def onInit(isReload):
    """
    初始化函数
    isReload: 是否是热更新调用
    """
    if isReload:
        INFO_MSG("Scripts reloading...")
    else:
        INFO_MSG("Scripts initializing...")

    # 注册热更新回调
    KBEngine.setCallback("onReload", onReloadCallback)

def onReloadCallback():
    """热更新回调"""
    INFO_MSG("Script files changed, reloading...")

    # 清理旧的模块缓存
    _cleanup_module_cache()

    # 重新加载模块
    _reload_modules()

    INFO_MSG("Scripts reloaded successfully")

def _cleanup_module_cache():
    """清理模块缓存"""
    global _module_cache

    for name in list(_module_cache.keys()):
        try:
            module = _module_cache[name]
            # 调用清理函数 (如果存在)
            if hasattr(module, 'onUnload'):
                module.onUnload()
            del sys.modules[name]
            del _module_cache[name]
        except:
            pass

def _reload_modules():
    """重新加载所有模块"""
    # 获取需要重新加载的模块列表
    modules_to_reload = _get_modified_modules()

    for module_name in modules_to_reload:
        try:
            if module_name in sys.modules:
                # 重新加载模块
                importlib.reload(sys.modules[module_name])

                INFO_MSG(f"Reloaded module: {module_name}")
        except Exception as e:
            ERROR_MSG(f"Failed to reload {module_name}: {e}")

def _get_modified_modules():
    """获取修改过的模块"""
    modified = set()

    # 检查 scripts 目录下的所有 .py 文件
    import os
    scripts_path = "scripts/"

    for root, dirs, files in os.walk(scripts_path):
        # 排除 __pycache__
        dirs[:] = [d for d in dirs if d not in ['__pycache__']]

        for file in files:
            if file.endswith('.py'):
                module_path = os.path.join(root, file)
                module_name = _path_to_module(module_path)

                # 检查文件修改时间
                mtime = os.path.getmtime(module_path)

                if module_name in _module_cache:
                    cached_mtime = _module_cache[module_name].get('mtime', 0)

                    if mtime > cached_mtime:
                        modified.add(module_name)

                # 缓存模块信息
                _module_cache[module_name] = {
                    'path': module_path,
                    'mtime': mtime
                }

    return modified

def _path_to_module(file_path):
    """将文件路径转换为模块名"""
    path = file_path.replace('/', '.')
    if path.startswith('scripts.'):
        path = path[8:]  # 移除 'scripts.'

    if path.endswith('.py'):
        path = path[:-3]  # 移除 '.py'

    return path
```

---

## 三、状态保持

### 3.1 状态序列化

```python
# 状态序列化保持

class StatefulEntity(KBEngine.Entity):
    """有状态的实体"""

    def __init__(self):
        KBEngine.Entity.__init__(self)

        # 需要保持的状态
        self.state = "idle"
        self.target = None
        self.path = []
        self.inventory_items = {}

        # 状态版本
        self.state_version = 0

    def onReload(self):
        """热更新时调用"""
        # 保存当前状态
        old_state = self.serializeState()

        # 执行热更新后
        new_state = self.loadState(old_state)

        INFO_MSG(f"Entity {self.id} state reloaded: {old_state} -> {new_state}")

    def serializeState(self):
        """序列化状态"""
        return {
            "state": self.state,
            "target": self.target,
            "path": self.path,
            "inventory": self.inventory_items,
            "version": self.state_version
        }

    def loadState(self, state_data):
        """加载状态"""
        self.state = state_data.get("state", "idle")
        self.target = state_data.get("target")
        self.path = state_data.get("path", [])
        self.inventory_items = state_data.get("inventory", {})
        self.state_version = state_data.get("version", 0)

        return f"{self.state}|{self.target}"
```

---

## 四、C++ 模块热加载

### 4.1 动态库加载

```cpp
// 动态库热加载

class HotReloadManager {
public:
    // 加载模块
    bool loadModule(const std::string& moduleName) {
        // 加载动态库
        HMODULE handle = LoadLibrary((moduleName + ".dll").c_str());

        if (!handle) {
            ERROR("Failed to load module: {}", moduleName);
            return false;
        }

        // 获取初始化函数
        using InitFunc = void(*)();
        InitFunc init = (InitFunc)GetProcAddress(handle, "initModule");

        if (!init) {
            FreeLibrary(handle);
            ERROR("Module missing initModule function: {}", moduleName);
            return false;
        }

        // 初始化模块
        init();

        modules_[moduleName] = handle;

        INFO("Module {} loaded successfully", moduleName);
        return true;
    }

    // 热重载模块
    bool reloadModule(const std::string& moduleName) {
        auto it = modules_.find(moduleName);
        if (it == modules_.end()) {
            ERROR("Module not loaded: {}", moduleName);
            return false;
        }

        HMODULE handle = it->second;

        // 获取清理函数
        using CleanupFunc = void(*)();
        CleanupFunc cleanup = (CleanupFunc)GetProcAddress(handle, "cleanupModule");

        if (cleanup) {
            cleanup();
        }

        FreeLibrary(handle);

        // 重新加载
        return loadModule(moduleName);
    }

private:
    std::unordered_map<std::string, HMODULE> modules_;
};
```

---

## 五、最佳实践

### 5.1 热更新建议

| 实践 | 说明 |
|------|------|
| **状态保持** | 热更后保持状态 |
| **版本兼容** | 新版本兼容旧状态 |
| **灰度发布** | 先部分服务器热更 |
| **回滚机制** | 热更失败可回滚 |
| **日志记录** | 记录热更操作 |

---

## 六、总结

### 热更新核心

```
热更新 = 状态序列化 + 动态加载 + 版本管理 + 灰度发布
- KBEngine 支持脚本热更
- 保持兼容状态
- 失败可回滚
- 充分测试
```

---

## 参考资料

- [Python Module Reloading](https://docs.python.org/3/library/importlib.html)
- [Hot Reload Best Practices](https://www.kernel.org/doc/Documentation/process/coding-style.rst)
