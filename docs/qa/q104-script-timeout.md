# Q104: 如何限制脚本的执行时间？

## 问题分析

本题考察对脚本执行控制的理解：
- 执行超时检测
- 中断执行
- 沙箱隔离
- KBEngine 机制

---

## 一、执行超时问题

### 1.1 为什么需要限制

```
┌─────────────────────────────────────────────────────────────┐
│                    脚本执行风险                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  无限循环:                                                   │
│  ├── while True: pass                                       │
│  ├── 占用 CPU                                              │
│  ├── 阻塞其他请求                                           │
│  └── 示例: 死循环搜索                                         │
│                                                             │
│  长时间运行:                                                 │
│  ├── 复杂计算                                               │
│  ├── 网络请求                                               │
│  ├── 超时响应                                               │
│  └── 示例: 大量数据处理                                       │
│                                                             │
│  恶意代码:                                                   │
│  ├── 故意延迟                                               │
│  ├── 资源耗尽                                               │
│  └── 示例: 外挂脚本                                           │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、Python 超时控制

### 2.1 信号超时

```python
# Python 执行超时控制

import signal
import time
from contextlib import contextmanager

class TimeoutError(Exception):
    """超时异常"""
    pass

@contextmanager
def time_limit(seconds):
    """时间限制上下文管理器"""
    def signal_handler(signum, frame):
        raise TimeoutError(f"Timed out after {seconds} seconds")

    # 注册信号处理
    old_handler = signal.signal(signal.SIGALRM, signal_handler)
    signal.alarm(seconds)

    try:
        yield
    finally:
        # 恢复旧处理
        signal.alarm(0)
        signal.signal(signal.SIGALRM, old_handler)


# 使用示例
def execute_script_with_timeout(script_func, timeout=5):
    """执行脚本并限制时间"""
    try:
        with time_limit(timeout):
            return script_func()
    except TimeoutError as e:
        print(f"Script execution timeout: {e}")
        return None
    except Exception as e:
        print(f"Script execution error: {e}")
        return None


# 测试
def long_running_task():
    """长时间运行的任务"""
    print("Task started...")
    time.sleep(10)  # 模拟长时间运行
    print("Task completed")
    return "Done"


# 正常情况
result = execute_script_with_timeout(lambda: time.sleep(1), timeout=5)
print(f"Result: {result}")  # Result: None (但执行完成)

# 超时情况
result = execute_script_with_timeout(long_running_task, timeout=3)
print(f"Result: {result}")  # Result: None (超时)
```

### 2.2 线程超时

```python
# 使用线程控制超时

import threading
import queue

def execute_with_timeout(func, args=(), kwargs={}, timeout=5):
    """使用线程执行并超时控制"""
    result_queue = queue.Queue()

    def worker():
        try:
            result = func(*args, **kwargs)
            result_queue.put((True, result))
        except Exception as e:
            result_queue.put((False, e))

    thread = threading.Thread(target=worker)
    thread.daemon = True
    thread.start()

    thread.join(timeout)

    if thread.is_alive():
        # 线程仍在运行，超时
        print(f"Function execution timeout after {timeout}s")
        return None

    try:
        success, result = result_queue.get_nowait()
        if success:
            return result
        else:
            print(f"Function execution error: {result}")
            return None
    except queue.Empty:
        return None


# 使用示例
def complex_calculation(n):
    """复杂计算"""
    total = 0
    for i in range(n):
        total += i ** 2
        time.sleep(0.001)  # 模拟耗时
    return total


# 超时控制
result = execute_with_timeout(complex_calculation, args=(1000000,), timeout=2)
print(f"Result: {result}")
```

### 2.3 多进程隔离

```python
# 使用多进程完全隔离

import multiprocessing
import time

def execute_in_process(func, args=(), kwargs={}, timeout=5):
    """在独立进程中执行函数"""
    # 创建进程
    process = multiprocessing.Process(
        target=process_wrapper,
        args=(func, args, kwargs)
    )

    process.start()
    process.join(timeout)

    if process.is_alive():
        # 超时，终止进程
        process.terminate()
        process.join(timeout=1)  # 等待 1 秒清理

        if process.is_alive():
            process.kill()  # 强制杀死

        print(f"Process execution timeout after {timeout}s")
        return None

    return process.exitcode


def process_wrapper(func, args, kwargs):
    """进程包装器"""
    try:
        result = func(*args, **kwargs)
        # 通过队列返回结果
    except Exception as e:
        print(f"Process error: {e}")


# 安全的脚本执行
class SafeScriptExecutor:
    """安全的脚本执行器"""

    def __init__(self, timeout=10):
        self.timeout = timeout

    def execute(self, script_code, context=None):
        """执行脚本代码"""
        if context is None:
            context = {}

        def script_runner():
            # 在独立进程中执行
            exec(script_code, context)

        # 使用多进程执行
        result = execute_in_process(script_runner, timeout=self.timeout)

        return result


# 使用示例
executor = SafeScriptExecutor(timeout=3)

script = """
import time
def slow_function():
    time.sleep(10)
    return "Done"
slow_function()
"""

executor.execute(script)
```

---

## 三、Lua 超时控制

### 3.1 钩子超时

```c
// Lua 执行超时控制

#include <lua.hpp>
#include <chrono>
#include <thread>

// 超时检测结构
struct TimeoutState {
    bool timeout;
    time_t deadline;
};

// 钩子函数
static void timeout_hook(lua_State *L, lua_Debug *ar) {
    TimeoutState *state = (TimeoutState *)lua_getextraspace(L);

    if (time(NULL) >= state->deadline) {
        state->timeout = true;
        luaL_error(L, "script execution timeout");
    }
}

// 带超时的执行
bool execute_with_timeout(lua_State *L, const char *script, int timeout_ms) {
    // 设置超时
    TimeoutState *state = (TimeoutState *)lua_getextraspace(L);
    state->timeout = false;
    state->deadline = time(NULL) + (timeout_ms / 1000) + 1;

    // 设置钩子 (每条指令检查)
    lua_sethook(L, timeout_hook, LUA_MASKCOUNT, 100);

    // 加载并执行
    bool success = (luaL_dostring(L, script) == LUA_OK);

    // 清除钩子
    lua_sethook(L, NULL, 0, 0);

    return success && !state->timeout;
}

// 使用示例
void example() {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    // 正常执行
    const char *normal_script = "print('Hello, World!')";
    bool ok = execute_with_timeout(L, normal_script, 5000);
    printf("Normal script: %s\n", ok ? "OK" : "FAILED");

    // 超时执行
    const char *timeout_script = "while true do end";
    ok = execute_with_timeout(L, timeout_script, 1000);
    printf("Timeout script: %s\n", ok ? "OK" : "TIMEOUT");

    lua_close(L);
}
```

### 3.2 Lua 线程隔离

```c
// 使用独立线程执行 Lua

#include <lua.hpp>
#include <thread>
#include <atomic>

struct LuaExecution {
    lua_State *L;
    const char *script;
    std::atomic<bool> completed;
    std::atomic<bool> timeout;
};

void lua_thread_main(LuaExecution *exec) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    if (luaL_dostring(L, exec->script) == LUA_OK) {
        exec->completed = true;
    } else {
        // 错误
        const char *error = lua_tostring(L, -1);
        fprintf(stderr, "Lua error: %s\n", error);
    }

    lua_close(L);
}

bool execute_in_thread(const char *script, int timeout_ms) {
    LuaExecution exec = {nullptr, script, false, false};

    // 启动线程
    std::thread thread(lua_thread_main, &thread);

    // 等待完成或超时
    auto start = std::chrono::steady_clock::now();
    while (!exec.completed) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start
        ).count();

        if (elapsed >= timeout_ms) {
            exec.timeout = true;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    thread.detach();  // 分离线程 (让 Lua 自己清理)

    return exec.completed && !exec.timeout;
}
```

---

## 四、沙箱隔离

### 4.1 Python 沙箱

```python
# Python 沙箱执行

import sys
import types
import time

class Sandbox:
    """Python 沙箱"""

    # 允许的内置函数
    ALLOWED_BUILTINS = {
        'print', 'len', 'range', 'str', 'int', 'float',
        'list', 'dict', 'tuple', 'set', 'bool',
        'min', 'max', 'sum', 'abs', 'round',
        'enumerate', 'zip', 'map', 'filter'
    }

    def __init__(self, timeout=10):
        self.timeout = timeout
        self.globals = self._create_globals()

    def _create_globals(self):
        """创建安全的全局命名空间"""
        safe_globals = {
            '__builtins__': {
                name: __builtins__[name]
                for name in self.ALLOWED_BUILTINS
                if name in __builtins__
            }
        }

        # 添加允许的模块
        safe_globals['math'] = __import__('math')
        safe_globals['time'] = time

        return safe_globals

    def execute(self, code, context=None):
        """执行沙箱代码"""
        if context is None:
            context = {}

        # 合并上下文
        exec_globals = self.globals.copy()
        exec_globals.update(context)

        # 使用线程执行
        def run():
            exec(code, exec_globals)

        thread = threading.Thread(target=run, daemon=True)
        thread.start()
        thread.join(self.timeout)

        if thread.is_alive():
            raise TimeoutError(f"Script execution timeout ({self.timeout}s)")

        return exec_globals


# 使用示例
sandbox = Sandbox(timeout=3)

# 正常代码
code1 = """
result = 0
for i in range(100):
    result += i
print("Sum:", result)
"""
sandbox.execute(code1)

# 超时代码
code2 = """
import time
time.sleep(10)
print("Done")
"""
try:
    sandbox.execute(code2)
except TimeoutError as e:
    print(f"Error: {e}")
```

---

## 五、KBEngine 执行控制

### 5.1 KBEngine 机制

```python
# KBEngine 脚本执行控制

"""
KBEngine 内置机制:

1. 单线程执行: Python 脚本在单线程中执行
2. 消息分片: 大任务分片处理
3. 定时器限制: 单次处理不超过一定时间
"""

import KBEngine

class TaskExecutor:
    """任务执行器"""

    MAX_EXECUTION_TIME = 0.1  # 单次最多执行 100ms

    def __init__(self):
        self.task_queue = []
        self.current_task = None
        self.task_progress = {}

    def submit_task(self, entity_id, task_func, total_steps):
        """提交任务"""
        self.task_queue.append({
            'entity_id': entity_id,
            'func': task_func,
            'total_steps': total_steps,
            'current_step': 0
        })

    def process_tasks(self):
        """处理任务队列"""
        start_time = time.time()

        while self.task_queue:
            # 检查执行时间
            if time.time() - start_time > self.MAX_EXECUTION_TIME:
                break

            task = self.task_queue.pop(0)

            # 执行一步
            self.execute_step(task)

            # 如果未完成，放回队列
            if task['current_step'] < task['total_steps']:
                self.task_queue.append(task)

    def execute_step(self, task):
        """执行任务的一步"""
        try:
            # 调用任务函数，传入进度
            task['func'](
                task['current_step'],
                task['total_steps']
            )

            task['current_step'] += 1

        except Exception as e:
            ERROR_MSG(f"Task execution error: {e}")


# 玩家任务示例
class Avatar(KBEngine.Entity):
    """角色实体"""

    def __init__(self):
        KBEngine.Entity.__init__(self)

    def start_long_task(self, total_steps):
        """启动长时间任务"""
        # 使用任务执行器
        KBEngine.task_executor.submit_task(
            self.id,
            self.process_task_step,
            total_steps
        )

    def process_task_step(self, current, total):
        """处理任务的一步"""
        INFO_MSG(f"Processing {current}/{total}")

        # 执行一小部分工作
        # 这里保证每次执行很快

        if current >= total:
            INFO_MSG("Task completed!")
            self.on_task_complete()

    def on_task_complete(self):
        """任务完成"""
        # 发送结果给客户端
        if self.client:
            self.client.onTaskComplete()
```

---

## 六、最佳实践

### 6.1 超时控制建议

| 实践 | 说明 |
|------|------|
| **进程隔离** | 最安全的方式 |
| **信号控制** | Unix 系统推荐 |
| **线程超时** | 跨平台但资源开销大 |
| **分片处理** | 大任务分成小步骤 |
| **超时检测** | 定期检查执行时间 |
| **资源限制** | 限制内存/CPU |

---

## 七、总结

### 脚本超时控制核心

```
超时控制 = 进程隔离 + 信号中断 + 定期检测 + 分片处理
- 多进程最安全
- 信号轻量但 Unix only
- 线程通用但开销大
- 分片处理是游戏服务器推荐方案
```

---

## 参考资料

- [Python Signal Module](https://docs.python.org/3/library/signal.html)
- [Lua Hooks](https://www.lua.org/manual/5.4/manual.html#lua_sethook)
- [Process Isolation](https://docs.python.org/3/library/multiprocessing.html)
