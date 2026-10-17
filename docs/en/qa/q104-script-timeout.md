
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q104: How Do You Limit Script Execution Time?

## Core Conclusion

This question tests understanding of script execution control:

- Execution timeout detection
- Interrupting execution
- Sandbox isolation
- KBEngine mechanisms

---

## I. The Execution Timeout Problem

### 1.1 Why Limits Are Needed

```
┌─────────────────────────────────────────────────────────────┐
│                    Script Execution Risks                    │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Infinite loops:                                            │
│  ├── while True: pass                                       │
│  ├── Consumes CPU                                           │
│  ├── Blocks other requests                                  │
│  └── Example: dead-loop search                              │
│                                                             │
│  Long-running execution:                                    │
│  ├── Complex computation                                    │
│  ├── Network requests                                       │
│  ├── Timeout responses                                      │
│  └── Example: processing large amounts of data              │
│                                                             │
│  Malicious code:                                            │
│  ├── Intentional delays                                     │
│  ├── Resource exhaustion                                    │
│  └── Example: cheat scripts                                 │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## II. Python Timeout Control

### 2.1 Signal Timeout

```python
# Python 执行超时控制 — Python execution timeout control

import signal
import time
from contextlib import contextmanager

class TimeoutError(Exception):
    """超时异常 — timeout exception"""
    pass

@contextmanager
def time_limit(seconds):
    """时间限制上下文管理器 — time-limit context manager"""
    def signal_handler(signum, frame):
        raise TimeoutError(f"Timed out after {seconds} seconds")

    # 注册信号处理 — register the signal handler
    old_handler = signal.signal(signal.SIGALRM, signal_handler)
    signal.alarm(seconds)

    try:
        yield
    finally:
        # 恢复旧处理 — restore the old handler
        signal.alarm(0)
        signal.signal(signal.SIGALRM, old_handler)

# 使用示例 — usage example
def execute_script_with_timeout(script_func, timeout=5):
    """执行脚本并限制时间 — execute a script with a time limit"""
    try:
        with time_limit(timeout):
            return script_func()
    except TimeoutError as e:
        print(f"Script execution timeout: {e}")
        return None
    except Exception as e:
        print(f"Script execution error: {e}")
        return None

# 测试 — test
def long_running_task():
    """长时间运行的任务 — a long-running task"""
    print("Task started...")
    time.sleep(10)  # 模拟长时间运行 — simulate a long run
    print("Task completed")
    return "Done"

# 正常情况 — normal case
result = execute_script_with_timeout(lambda: time.sleep(1), timeout=5)
print(f"Result: {result}")  # Result: None (但执行完成 — but execution completed)

# 超时情况 — timeout case
result = execute_script_with_timeout(long_running_task, timeout=3)
print(f"Result: {result}")  # Result: None (超时 — timed out)
```

### 2.2 Thread Timeout

```python
# 使用线程控制超时 — use a thread to control the timeout

import threading
import queue

def execute_with_timeout(func, args=(), kwargs={}, timeout=5):
    """使用线程执行并超时控制 — execute in a thread with timeout control"""
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
        # 线程仍在运行，超时 — the thread is still running; timed out
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

# 使用示例 — usage example
def complex_calculation(n):
    """复杂计算 — complex calculation"""
    total = 0
    for i in range(n):
        total += i ** 2
        time.sleep(0.001)  # 模拟耗时 — simulate work
    return total

# 超时控制 — timeout control
result = execute_with_timeout(complex_calculation, args=(1000000,), timeout=2)
print(f"Result: {result}")
```

### 2.3 Multiprocess Isolation

```python
# 使用多进程完全隔离 — use a separate process for full isolation

import multiprocessing
import time

def execute_in_process(func, args=(), kwargs={}, timeout=5):
    """在独立进程中执行函数 — execute a function in a separate process"""
    # 创建进程 — create the process
    process = multiprocessing.Process(
        target=process_wrapper,
        args=(func, args, kwargs)
    )

    process.start()
    process.join(timeout)

    if process.is_alive():
        # 超时，终止进程 — timed out; terminate the process
        process.terminate()
        process.join(timeout=1)  # 等待 1 秒清理 — wait 1 second for cleanup

        if process.is_alive():
            process.kill()  # 强制杀死 — force kill

        print(f"Process execution timeout after {timeout}s")
        return None

    return process.exitcode

def process_wrapper(func, args, kwargs):
    """进程包装器 — process wrapper"""
    try:
        result = func(*args, **kwargs)
        # 通过队列返回结果 — return the result via a queue
    except Exception as e:
        print(f"Process error: {e}")

# 安全的脚本执行 — safe script execution
class SafeScriptExecutor:
    """安全的脚本执行器 — safe script executor"""

    def __init__(self, timeout=10):
        self.timeout = timeout

    def execute(self, script_code, context=None):
        """执行脚本代码 — execute script code"""
        if context is None:
            context = {}

        def script_runner():
            # 在独立进程中执行 — execute in a separate process
            exec(script_code, context)

        # 使用多进程执行 — execute in a separate process
        result = execute_in_process(script_runner, timeout=self.timeout)

        return result

# 使用示例 — usage example
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

## III. Lua Timeout Control

### 3.1 Hook Timeout

```c
// Lua 执行超时控制 — Lua execution timeout control

#include <lua.hpp>
#include <chrono>
#include <thread>

// 超时检测结构 — timeout detection state
struct TimeoutState {
    bool timeout;
    time_t deadline;
};

// 钩子函数 — hook function
static void timeout_hook(lua_State *L, lua_Debug *ar) {
    TimeoutState *state = (TimeoutState *)lua_getextraspace(L);

    if (time(NULL) >= state->deadline) {
        state->timeout = true;
        luaL_error(L, "script execution timeout");
    }
}

// 带超时的执行 — execute with a timeout
bool execute_with_timeout(lua_State *L, const char *script, int timeout_ms) {
    // 设置超时 — set the timeout
    TimeoutState *state = (TimeoutState *)lua_getextraspace(L);
    state->timeout = false;
    state->deadline = time(NULL) + (timeout_ms / 1000) + 1;

    // 设置钩子 (每条指令检查) — set the hook (checked every instruction)
    lua_sethook(L, timeout_hook, LUA_MASKCOUNT, 100);

    // 加载并执行 — load and execute
    bool success = (luaL_dostring(L, script) == LUA_OK);

    // 清除钩子 — clear the hook
    lua_sethook(L, NULL, 0, 0);

    return success && !state->timeout;
}

// 使用示例 — usage example
void example() {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    // 正常执行 — normal execution
    const char *normal_script = "print('Hello, World!')";
    bool ok = execute_with_timeout(L, normal_script, 5000);
    printf("Normal script: %s\n", ok ? "OK" : "FAILED");

    // 超时执行 — timeout execution
    const char *timeout_script = "while true do end";
    ok = execute_with_timeout(L, timeout_script, 1000);
    printf("Timeout script: %s\n", ok ? "OK" : "TIMEOUT");

    lua_close(L);
}
```

### 3.2 Lua Thread Isolation

```c
// 使用独立线程执行 Lua — execute Lua in a separate thread

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
        // 错误 — error
        const char *error = lua_tostring(L, -1);
        fprintf(stderr, "Lua error: %s\n", error);
    }

    lua_close(L);
}

bool execute_in_thread(const char *script, int timeout_ms) {
    LuaExecution exec = {nullptr, script, false, false};

    // 启动线程 — start the thread
    std::thread thread(lua_thread_main, &thread);

    // 等待完成或超时 — wait for completion or timeout
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

    thread.detach();  // 分离线程 (让 Lua 自己清理) — detach the thread (let Lua clean up itself)

    return exec.completed && !exec.timeout;
}
```

---

## IV. Sandbox Isolation

### 4.1 Python Sandbox

```python
# Python 沙箱执行 — Python sandbox execution

import sys
import types
import time

class Sandbox:
    """Python 沙箱 — Python sandbox"""

    # 允许的内置函数 — allowed builtins
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
        """创建安全的全局命名空间 — create a safe global namespace"""
        safe_globals = {
            '__builtins__': {
                name: __builtins__[name]
                for name in self.ALLOWED_BUILTINS
                if name in __builtins__
            }
        }

        # 添加允许的模块 — add the allowed modules
        safe_globals['math'] = __import__('math')
        safe_globals['time'] = time

        return safe_globals

    def execute(self, code, context=None):
        """执行沙箱代码 — execute sandbox code"""
        if context is None:
            context = {}

        # 合并上下文 — merge the context
        exec_globals = self.globals.copy()
        exec_globals.update(context)

        # 使用线程执行 — execute in a thread
        def run():
            exec(code, exec_globals)

        thread = threading.Thread(target=run, daemon=True)
        thread.start()
        thread.join(self.timeout)

        if thread.is_alive():
            raise TimeoutError(f"Script execution timeout ({self.timeout}s)")

        return exec_globals

# 使用示例 — usage example
sandbox = Sandbox(timeout=3)

# 正常代码 — normal code
code1 = """
result = 0
for i in range(100):
    result += i
print("Sum:", result)
"""
sandbox.execute(code1)

# 超时代码 — timeout code
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

## V. KBEngine Execution Control

### 5.1 KBEngine Mechanisms

```python
# KBEngine 脚本执行控制 — KBEngine script execution control

"""
KBEngine 内置机制:

1. 单线程执行: Python 脚本在单线程中执行 — single-threaded execution: Python scripts run in a single thread
2. 消息分片: 大任务分片处理 — message sharding: large tasks are processed in slices
3. 定时器限制: 单次处理不超过一定时间 — timer limit: a single processing pass does not exceed a certain time
"""

import KBEngine

class TaskExecutor:
    """任务执行器 — task executor"""

    MAX_EXECUTION_TIME = 0.1  # 单次最多执行 100ms — at most 100ms per pass

    def __init__(self):
        self.task_queue = []
        self.current_task = None
        self.task_progress = {}

    def submit_task(self, entity_id, task_func, total_steps):
        """提交任务 — submit a task"""
        self.task_queue.append({
            'entity_id': entity_id,
            'func': task_func,
            'total_steps': total_steps,
            'current_step': 0
        })

    def process_tasks(self):
        """处理任务队列 — process the task queue"""
        start_time = time.time()

        while self.task_queue:
            # 检查执行时间 — check the execution time
            if time.time() - start_time > self.MAX_EXECUTION_TIME:
                break

            task = self.task_queue.pop(0)

            # 执行一步 — execute one step
            self.execute_step(task)

            # 如果未完成，放回队列 — if not finished, put it back in the queue
            if task['current_step'] < task['total_steps']:
                self.task_queue.append(task)

    def execute_step(self, task):
        """执行任务的一步 — execute one step of the task"""
        try:
            # 调用任务函数，传入进度 — call the task function with the progress
            task['func'](
                task['current_step'],
                task['total_steps']
            )

            task['current_step'] += 1

        except Exception as e:
            ERROR_MSG(f"Task execution error: {e}")

# 玩家任务示例 — player task example
class Avatar(KBEngine.Entity):
    """角色实体 — avatar entity"""

    def __init__(self):
        KBEngine.Entity.__init__(self)

    def start_long_task(self, total_steps):
        """启动长时间任务 — start a long-running task"""
        # 使用任务执行器 — use the task executor
        KBEngine.task_executor.submit_task(
            self.id,
            self.process_task_step,
            total_steps
        )

    def process_task_step(self, current, total):
        """处理任务的一步 — process one step of the task"""
        INFO_MSG(f"Processing {current}/{total}")

        # 执行一小部分工作 — do a small amount of work
        # 这里保证每次执行很快 — this guarantees each pass is fast

        if current >= total:
            INFO_MSG("Task completed!")
            self.on_task_complete()

    def on_task_complete(self):
        """任务完成 — task complete"""
        # 发送结果给客户端 — send the result to the client
        if self.client:
            self.client.onTaskComplete()
```

---

## VI. Best Practices

### 6.1 Timeout Control Suggestions

| Practice | Description |
|------|------|
| **Process isolation** | The safest approach |
| **Signal control** | Recommended on Unix systems |
| **Thread timeout** | Cross-platform but high resource overhead |
| **Sharded processing** | Split large tasks into small steps |
| **Timeout detection** | Check execution time periodically |
| **Resource limits** | Limit memory/CPU |

---

## VII. Summary

### The Core of Script Timeout Control

```
Timeout control = process isolation + signal interruption + periodic detection + sharded processing
- Multiprocess is safest
- Signals are lightweight but Unix only
- Threads are universal but costly
- Sharded processing is the recommended approach for game servers
```

---

## References

- [Python Signal Module](https://docs.python.org/3/library/signal.html)
- [Lua Hooks](https://www.lua.org/manual/5.4/manual.html#lua_sethook)
- [Process Isolation](https://docs.python.org/3/library/multiprocessing.html)
