
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q105: How to Debug Scripts?

## Problem Analysis

This question tests your understanding of script debugging:
- Log debugging
- Breakpoint debugging
- Remote debugging
- KBEngine debugging

---

## 1. Overview of Debugging Methods

### 1.1 Debugging Techniques

```
┌─────────────────────────────────────────────────────────────┐
│                   Script Debugging Methods                   │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Log Debugging:                                             │
│  ├── print output                                           │
│  ├── structured logging                                     │
│  ├── log levels                                             │
│  └── Example: print(f"x={x}")                               │
│                                                             │
│  Breakpoint Debugging:                                      │
│  ├── IDE breakpoints                                        │
│  ├── conditional breakpoints                                │
│  ├── step execution                                         │
│  └── Example: pdb, VSCode debugger                          │
│                                                             │
│  Remote Debugging:                                          │
│  ├── remote breakpoints                                     │
│  ├── remote console                                         │
│  ├── profiling                                              │
│  └── Example: rpdb, pydevd                                  │
│                                                             │
│  Profiling:                                                 │
│  ├── cProfile                                               │
│  ├── memory profiling                                       │
│  ├── call tracing                                           │
│  └── Example: python -m cProfile                            │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. Python Debugging

### 2.1 pdb Basics

```python
# Python pdb 调试 (Python pdb debugging)

import pdb

def complex_calculation(a, b, c):
    """复杂计算 (complex calculation)"""

    # 设置断点 (set breakpoint)
    pdb.set_trace()

    result = a * b + c

    for i in range(10):
        result += i

    return result

# pdb 常用命令 (common pdb commands)
"""
pdb 命令 (pdb commands):

n (next)        # 执行下一行 (execute next line)
s (step)        # 进入函数 (step into function)
c (continue)    # 继续执行 (continue execution)
l (list)        # 显示代码 (show code)
p variable      # 打印变量 (print variable)
pp variable     # 美化打印 (pretty-print)
w (where)       # 显示堆栈 (show stack)
b 10            # 在第10行设置断点 (set breakpoint at line 10)
b func_name     # 在函数设置断点 (set breakpoint in function)
cl 1            # 清除断点1 (clear breakpoint 1)
                # 清除所有断点 (clear all breakpoints)
!expression     # 执行表达式 (execute expression)
q (quit)        # 退出 (quit)
"""

# 使用示例 (usage example)
if __name__ == "__main__":
    x = 5
    y = 10
    z = 3

    result = complex_calculation(x, y, z)
    print(f"Result: {result}")
```

### 2.2 Advanced pdb Usage

```python
# 高级 pdb 技巧 (advanced pdb techniques)

import pdb
import sys

class CustomPdb(pdb.Pdb):
    """自定义 pdb (custom pdb)"""

    def __init__(self):
        super().__init__()
        self.prompt = "(DEBUG) "

    def do_stack(self, arg):
        """自定义命令: 显示完整堆栈 (custom command: show full stack)"""
        import traceback
        traceback.print_stack()

    def do_vars(self, arg):
        """自定义命令: 显示所有局部变量 (custom command: show all local variables)"""
        frame = self.curframe
        if frame:
            for name, value in frame.f_locals.items():
                print(f"{name} = {value}")

def debug_on_error(func):
    """错误时自动进入调试 (automatically enter debugging on error)"""
    def wrapper(*args, **kwargs):
        try:
            return func(*args, **kwargs)
        except Exception as e:
            print(f"Error occurred: {e}")
            print("Entering debugger...")
            debugger = CustomPdb()
            debugger.set_trace(sys._getframe().f_back)
            raise
    return wrapper

@debug_on_error
def buggy_function(data):
    """有问题的函数 (buggy function)"""
    result = []
    for item in data:
        # 这里可能会有 bug (there may be a bug here)
        value = int(item) / len(item)
        result.append(value)
    return result

# 条件断点 (conditional breakpoint)
def conditional_break():
    """条件断点示例 (conditional breakpoint example)"""
    items = [1, 2, 3, 4, 5]

    for i, item in enumerate(items):
        # 只在特定条件下断点 (break only under specific conditions)
        if item == 3:
            pdb.set_trace()

        print(f"Processing: {item}")
```

### 2.3 Remote Debugging

```python
# 远程调试 rpdb (remote debugging with rpdb)

import rpdb
import socket
import time

class RemoteDebugger:
    """远程调试器 (remote debugger)"""

    def __init__(self, port=4444):
        self.port = port
        self.debugger = None

    def start(self):
        """启动远程调试服务器 (start the remote debugging server)"""
        try:
            self.debugger = rpdb.Rpdb(port=self.port)
            print(f"Remote debugger started on port {self.port}")
            print(f"Connect with: gdb -ex 'target remote localhost:{self.port}'")
            return True
        except Exception as e:
            print(f"Failed to start remote debugger: {e}")
            return False

    def set_trace(self):
        """设置断点 (set breakpoint)"""
        if self.debugger:
            self.debugger.set_trace()

# 使用示例 (usage example)
def remote_debug_example():
    """远程调试示例 (remote debugging example)"""

    debugger = RemoteDebugger(port=4444)
    debugger.start()

    # 代码执行 (code execution)
    data = list(range(100))

    for i, value in enumerate(data):
        # 在这里设置断点 (set a breakpoint here)
        if i == 50:
            debugger.set_trace()

        result = value * 2

    return data

# 使用 pydevd 调试 (debug with pydevd, PyCharm compatible)
def pydevd_debug():
    """使用 pydevd 调试 (debug with pydevd)"""

    try:
        import pydevd
        # 启用远程调试 (enable remote debugging)
        pydevd.settrace('localhost', port=5678, stdoutToServer=True,
                       stderrToServer=True, trace_only_current_thread=False)
    except:
        pass

    # 代码在这里可以调试 (code here can be debugged)
    x = 10
    y = 20
    z = x + y

    return z
```

---

## 3. Lua Debugging

### 3.1 Lua Debug Library

```lua
-- Lua 调试 (Lua debugging)

-- 调试辅助函数 (debug helper functions)
local debug = require "debug"

local function print_traceback(level)
    level = level or 1
    print("=== Traceback ===")
    for i = level, math.huge do
        local info = debug.getinfo(i, "Slfn")
        if not info then break end

        print(string.format("[%d] %s:%d in %s",
            i - level,
            info.short_src,
            info.currentline,
            info.name or "(anonymous)"))
    end
end

local function print_locals(level)
    level = level or 2
    print("=== Locals ===")
    local i = 1
    while true do
        local name, value = debug.getlocal(level, i)
        if not name then break end

        print(string.format("%s = %s", name, tostring(value)))
        i = i + 1
    end
end

local function debug_hook(event, line)
    if event == "line" then
        -- 可以在这里设置断点条件 (breakpoint conditions can be set here)
        local info = debug.getinfo(2, "Sn")
        if info.currentline == 10 then  -- 在第10行断点 (breakpoint at line 10)
            print("Breakpoint at line 10")
            print_locals()
            debug.sethook()  -- 清除钩子 (clear hook)
        end
    end
end

-- 使用示例 (usage example)
local function complex_function(a, b)
    local result = a + b

    -- 设置断点 (set breakpoint)
    debug.sethook(debug_hook, "l")

    for i = 1, 10 do
        result = result + i
    end

    debug.sethook()  -- 清除钩子 (clear hook)

    return result
end

-- 错误处理 (error handling)
local function safe_call(func, ...)
    local ok, result = pcall(func, ...)

    if not ok then
        print("Error occurred:")
        print(result)
        print_traceback(2)
    end

    return ok, result
end

-- 测试 (test)
safe_call(complex_function, 10, 20)
```

---

## 4. KBEngine Debugging

### 4.1 Built-in KBEngine Debugging

```python
# KBEngine 调试工具 (KBEngine debugging tools)

import KBEngine

class KBEngineDebugger:
    """KBEngine 调试器 (KBEngine debugger)"""

    @staticmethod
    def list_all_entities():
        """列出所有实体 (list all entities)"""
        print("=== All Entities ===")
        for entity_id, entity in KBEngine.entities.items():
            print(f"ID: {entity_id}, Type: {type(entity).__name__}")
            if hasattr(entity, 'position'):
                print(f"  Position: {entity.position}")
            if hasattr(entity, 'playerName'):
                print(f"  Name: {entity.playerName}")

    @staticmethod
    def find_entity_by_name(name):
        """按名称查找实体 (find entity by name)"""
        for entity_id, entity in KBEngine.entities.items():
            if hasattr(entity, 'playerName') and entity.playerName == name:
                return entity
        return None

    @staticmethod
    def dump_entity(entity):
        """转储实体详情 (dump entity details)"""
        print(f"=== Entity {entity.id} ===")
        print(f"Type: {type(entity).__name__}")

        # 打印所有属性 (print all attributes)
        for attr in dir(entity):
            if not attr.startswith('_'):
                try:
                    value = getattr(entity, attr)
                    if not callable(value):
                        print(f"  {attr}: {value}")
                except:
                    pass

    @staticmethod
    def watch_variable(entity_id, attr_name):
        """监控实体属性 (watch entity attribute)"""
        entity = KBEngine.getEntity(entity_id)
        if not entity:
            print(f"Entity {entity_id} not found")
            return

        if not hasattr(entity, attr_name):
            print(f"Entity has no attribute '{attr_name}'")
            return

        value = getattr(entity, attr_name)
        print(f"Entity {entity_id}.{attr_name} = {value}")

    @staticmethod
    def profile_entity_update(duration=10):
        """分析实体更新性能 (profile entity update performance)"""
        import time
        import sys

        entity_times = {}

        def trace_calls(frame, event, arg):
            if event == 'call':
                func_name = frame.f_code.co_name
                if 'update' in func_name.lower():
                    entity_times[func_name] = entity_times.get(func_name, 0) + 1
            return trace_calls

        old_trace = sys.gettrace()
        sys.settrace(trace_calls)

        start_time = time.time()
        while time.time() - start_time < duration:
            time.sleep(0.1)

        sys.settrace(old_trace)

        print("=== Update Profile ===")
        for func, count in sorted(entity_times.items(),
                                   key=lambda x: x[1],
                                   reverse=True):
            print(f"{func}: {count} calls")

# 实体中添加调试方法 (add debugging methods to entities)
class DebuggableEntity(KBEngine.Entity):
    """可调试的实体 (debuggable entity)"""

    def __init__(self):
        KBEngine.Entity.__init__(self)
        self._debug_mode = False
        self._debug_log = []

    def enable_debug(self):
        """启用调试模式 (enable debug mode)"""
        self._debug_mode = True
        INFO_MSG(f"Entity {self.id} debug mode enabled")

    def disable_debug(self):
        """禁用调试模式 (disable debug mode)"""
        self._debug_mode = False
        INFO_MSG(f"Entity {self.id} debug mode disabled")

    def debug_log(self, message):
        """调试日志 (debug log)"""
        if self._debug_mode:
            log_entry = {
                'time': time.time(),
                'message': message
            }
            self._debug_log.append(log_entry)

            # 限制日志长度 (limit log length)
            if len(self._debug_log) > 1000:
                self._debug_log = self._debug_log[-1000:]

            print(f"[DEBUG] Entity {self.id}: {message}")

    def get_debug_log(self):
        """获取调试日志 (get debug log)"""
        return self._debug_log

    def onDebugCommand(self, command, *args):
        """调试命令处理 (debug command handling)"""
        if command == "dump":
            KBEngineDebugger.dump_entity(self)
        elif command == "watch":
            if args:
                KBEngineDebugger.watch_variable(self.id, args[0])
        elif command == "profile":
            KBEngineDebugger.profile_entity_update()
        elif command == "help":
            print("Debug commands: dump, watch <attr>, profile")
```

---

## 5. Profiling

### 5.1 cProfile

```python
# 性能分析 (profiling)

import cProfile
import pstats
import io

def profile_function(func, *args, **kwargs):
    """分析函数性能 (profile function performance)"""

    # 创建分析器 (create profiler)
    profiler = cProfile.Profile()
    profiler.enable()

    # 执行函数 (execute function)
    result = func(*args, **kwargs)

    profiler.disable()

    # 输出统计 (output statistics)
    s = io.StringIO()
    stats = pstats.Stats(profiler, stream=s)
    stats.strip_dirs()
    stats.sort_stats('cumulative')
    stats.print_stats(20)  # 打印前 20 个 (print top 20)

    print(s.getvalue())

    return result

# 使用示例 (usage example)
def game_logic_update():
    """游戏逻辑更新 (game logic update)"""
    players = list(range(1000))

    for player_id in players:
        # 模拟玩家更新 (simulate player update)
        x = player_id * 2
        y = player_id * 3
        z = x + y

    return len(players)

# 分析 (profile it)
profile_function(game_logic_update)
```

---

## 6. Recommended Debugging Tools

### 6.1 Tool Comparison

| Tool | Language | Type | Features |
|------|----------|------|----------|
| pdb | Python | Command line | Built-in, no installation needed |
| ipdb | Python | Command line | Enhanced pdb with tab completion |
| pudb | Python | TUI | IDE-like interface |
| VSCode | Python | GUI | Integrated debugging |
| PyCharm | Python | GUI | Powerful Python IDE |
| lua-debugger | Lua | Command line | Built-in debug library |
| ZeroBrane | Lua | GUI | Lua IDE |
| Decoda | Lua | GUI | Commercial Lua debugger |

---

## 7. Best Practices

### 7.1 Debugging Advice

| Practice | Description |
|----------|-------------|
| **Log first** | Locate the problem with logs first |
| **Unit tests** | Test functions in isolation |
| **Minimal reproduction** | Simplify the problem scenario |
| **Bisection** | Locate the problem code by bisection |
| **Version control** | Compare normal/abnormal versions |
| **Remote debugging** | Use with caution in production |

---

## 8. Summary

### Core of Script Debugging

```
Script debugging = logs + breakpoints + remote + profiling
- pdb/Lua debugger built-in support
- VSCode/PyCharm powerful and easy to use
- Remote debugging for production environments
- Profiling to find bottlenecks
```

---

## References

- [Python pdb Documentation](https://docs.python.org/3/library/pdb.html)
- [Lua Debug Library](https://www.lua.org/manual/5.4/manual.html#pdf-debug)
- [VSCode Python Debugging](https://code.visualstudio.com/docs/python/debugging)
