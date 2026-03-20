# Q105: 如何调试脚本？

## 问题分析

本题考察对脚本调试的理解：
- 日志调试
- 断点调试
- 远程调试
- KBEngine 调试

---

## 一、调试方法概述

### 1.1 调试技术

```
┌─────────────────────────────────────────────────────────────┐
│                    脚本调试方法                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  日志调试:                                                   │
│  ├── print 输出                                             │
│  ├── 结构化日志                                             │
│  ├── 日志级别                                               │
│  └── 示例: print(f"x={x}")                                   │
│                                                             │
│  断点调试:                                                   │
│  ├── IDE 断点                                               │
│  ├── 条件断点                                               │
│  ├── 单步执行                                               │
│  └── 示例: pdb, VSCode debugger                             │
│                                                             │
│  远程调试:                                                   │
│  ├── 远程断点                                               │
│  ├── 远程控制台                                             │
│  ├── 性能分析                                               │
│  └── 示例: rpdb, pydevd                                     │
│                                                             │
│  性能分析:                                                   │
│  ├── cProfile                                               │
│  ├── 内存分析                                               │
│  ├── 调用追踪                                               │
│  └── 示例: python -m cProfile                               │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、Python 调试

### 2.1 pdb 基础

```python
# Python pdb 调试

import pdb

def complex_calculation(a, b, c):
    """复杂计算"""

    # 设置断点
    pdb.set_trace()

    result = a * b + c

    for i in range(10):
        result += i

    return result


# pdb 常用命令
"""
pdb 命令:

n (next)        # 执行下一行
s (step)        # 进入函数
c (continue)    # 继续执行
l (list)        # 显示代码
p variable      # 打印变量
pp variable     # 美化打印
w (where)       # 显示堆栈
b 10            # 在第10行设置断点
b func_name     # 在函数设置断点
cl 1            # 清除断点1
                # 清除所有断点
!expression     # 执行表达式
q (quit)        # 退出
"""


# 使用示例
if __name__ == "__main__":
    x = 5
    y = 10
    z = 3

    result = complex_calculation(x, y, z)
    print(f"Result: {result}")
```

### 2.2 pdb 高级用法

```python
# 高级 pdb 技巧

import pdb
import sys

class CustomPdb(pdb.Pdb):
    """自定义 pdb"""

    def __init__(self):
        super().__init__()
        self.prompt = "(DEBUG) "

    def do_stack(self, arg):
        """自定义命令: 显示完整堆栈"""
        import traceback
        traceback.print_stack()

    def do_vars(self, arg):
        """自定义命令: 显示所有局部变量"""
        frame = self.curframe
        if frame:
            for name, value in frame.f_locals.items():
                print(f"{name} = {value}")


def debug_on_error(func):
    """错误时自动进入调试"""
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
    """有问题的函数"""
    result = []
    for item in data:
        # 这里可能会有 bug
        value = int(item) / len(item)
        result.append(value)
    return result


# 条件断点
def conditional_break():
    """条件断点示例"""
    items = [1, 2, 3, 4, 5]

    for i, item in enumerate(items):
        # 只在特定条件下断点
        if item == 3:
            pdb.set_trace()

        print(f"Processing: {item}")
```

### 2.3 远程调试

```python
# 远程调试 (rpdb)

import rpdb
import socket
import time

class RemoteDebugger:
    """远程调试器"""

    def __init__(self, port=4444):
        self.port = port
        self.debugger = None

    def start(self):
        """启动远程调试服务器"""
        try:
            self.debugger = rpdb.Rpdb(port=self.port)
            print(f"Remote debugger started on port {self.port}")
            print(f"Connect with: gdb -ex 'target remote localhost:{self.port}'")
            return True
        except Exception as e:
            print(f"Failed to start remote debugger: {e}")
            return False

    def set_trace(self):
        """设置断点"""
        if self.debugger:
            self.debugger.set_trace()


# 使用示例
def remote_debug_example():
    """远程调试示例"""

    debugger = RemoteDebugger(port=4444)
    debugger.start()

    # 代码执行
    data = list(range(100))

    for i, value in enumerate(data):
        # 在这里设置断点
        if i == 50:
            debugger.set_trace()

        result = value * 2

    return data


# 使用 pydevd (PyCharm 兼容)
def pydevd_debug():
    """使用 pydevd 调试"""

    try:
        import pydevd
        # 启用远程调试
        pydevd.settrace('localhost', port=5678, stdoutToServer=True,
                       stderrToServer=True, trace_only_current_thread=False)
    except:
        pass

    # 代码在这里可以调试
    x = 10
    y = 20
    z = x + y

    return z
```

---

## 三、Lua 调试

### 3.1 Lua 调试库

```lua
-- Lua 调试

-- 调试辅助函数
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
        -- 可以在这里设置断点条件
        local info = debug.getinfo(2, "Sn")
        if info.currentline == 10 then  -- 在第10行断点
            print("Breakpoint at line 10")
            print_locals()
            debug.sethook()  -- 清除钩子
        end
    end
end

-- 使用示例
local function complex_function(a, b)
    local result = a + b

    -- 设置断点
    debug.sethook(debug_hook, "l")

    for i = 1, 10 do
        result = result + i
    end

    debug.sethook()  -- 清除钩子

    return result
end

-- 错误处理
local function safe_call(func, ...)
    local ok, result = pcall(func, ...)

    if not ok then
        print("Error occurred:")
        print(result)
        print_traceback(2)
    end

    return ok, result
end

-- 测试
safe_call(complex_function, 10, 20)
```

---

## 四、KBEngine 调试

### 4.1 KBEngine 内置调试

```python
# KBEngine 调试工具

import KBEngine

class KBEngineDebugger:
    """KBEngine 调试器"""

    @staticmethod
    def list_all_entities():
        """列出所有实体"""
        print("=== All Entities ===")
        for entity_id, entity in KBEngine.entities.items():
            print(f"ID: {entity_id}, Type: {type(entity).__name__}")
            if hasattr(entity, 'position'):
                print(f"  Position: {entity.position}")
            if hasattr(entity, 'playerName'):
                print(f"  Name: {entity.playerName}")

    @staticmethod
    def find_entity_by_name(name):
        """按名称查找实体"""
        for entity_id, entity in KBEngine.entities.items():
            if hasattr(entity, 'playerName') and entity.playerName == name:
                return entity
        return None

    @staticmethod
    def dump_entity(entity):
        """转储实体详情"""
        print(f"=== Entity {entity.id} ===")
        print(f"Type: {type(entity).__name__}")

        # 打印所有属性
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
        """监控实体属性"""
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
        """分析实体更新性能"""
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


# 实体中添加调试方法
class DebuggableEntity(KBEngine.Entity):
    """可调试的实体"""

    def __init__(self):
        KBEngine.Entity.__init__(self)
        self._debug_mode = False
        self._debug_log = []

    def enable_debug(self):
        """启用调试模式"""
        self._debug_mode = True
        INFO_MSG(f"Entity {self.id} debug mode enabled")

    def disable_debug(self):
        """禁用调试模式"""
        self._debug_mode = False
        INFO_MSG(f"Entity {self.id} debug mode disabled")

    def debug_log(self, message):
        """调试日志"""
        if self._debug_mode:
            log_entry = {
                'time': time.time(),
                'message': message
            }
            self._debug_log.append(log_entry)

            # 限制日志长度
            if len(self._debug_log) > 1000:
                self._debug_log = self._debug_log[-1000:]

            print(f"[DEBUG] Entity {self.id}: {message}")

    def get_debug_log(self):
        """获取调试日志"""
        return self._debug_log

    def onDebugCommand(self, command, *args):
        """调试命令处理"""
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

## 五、性能分析

### 5.1 cProfile

```python
# 性能分析

import cProfile
import pstats
import io

def profile_function(func, *args, **kwargs):
    """分析函数性能"""

    # 创建分析器
    profiler = cProfile.Profile()
    profiler.enable()

    # 执行函数
    result = func(*args, **kwargs)

    profiler.disable()

    # 输出统计
    s = io.StringIO()
    stats = pstats.Stats(profiler, stream=s)
    stats.strip_dirs()
    stats.sort_stats('cumulative')
    stats.print_stats(20)  # 打印前 20 个

    print(s.getvalue())

    return result


# 使用示例
def game_logic_update():
    """游戏逻辑更新"""
    players = list(range(1000))

    for player_id in players:
        # 模拟玩家更新
        x = player_id * 2
        y = player_id * 3
        z = x + y

    return len(players)


# 分析
profile_function(game_logic_update)
```

---

## 六、调试工具推荐

### 6.1 工具对比

| 工具 | 语言 | 类型 | 特点 |
|------|------|------|------|
| pdb | Python | 命令行 | 内置，无需安装 |
| ipdb | Python | 命令行 | 增强 pdb，支持 tab 补全 |
| pudb | Python | TUI | 类似 IDE 的界面 |
| VSCode | Python | GUI | 集成调试 |
| PyCharm | Python | GUI | 强大的 Python IDE |
| lua-debugger | Lua | 命令行 | 内置调试库 |
| ZeroBrane | Lua | GUI | Lua IDE |
| Decoda | Lua | GUI | 商业 Lua 调试器 |

---

## 七、最佳实践

### 7.1 调试建议

| 实践 | 说明 |
|------|------|
| **日志优先** | 先用日志定位问题 |
| **单元测试** | 隔离测试函数 |
| **最小复现** | 简化问题场景 |
| **二分法** | 通过二分定位问题代码 |
| **版本控制** | 对比正常/异常版本 |
| **远程调试** | 生产环境谨慎使用 |

---

## 八、总结

### 脚本调试核心

```
脚本调试 = 日志 + 断点 + 远程 + 性能分析
- pdb/Lua debugger 内置支持
- VSCode/PyCharm 强大易用
- 远程调试用于生产环境
- 性能分析找瓶颈
```

---

## 参考资料

- [Python pdb Documentation](https://docs.python.org/3/library/pdb.html)
- [Lua Debug Library](https://www.lua.org/manual/5.4/manual.html#pdf-debug)
- [VSCode Python Debugging](https://code.visualstudio.com/docs/python/debugging)
