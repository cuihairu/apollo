# Q61: 如何优化日志系统？

## 问题分析

本题考察对日志系统优化的理解：
- 异步日志写入
- 日志分级与过滤
- 日志格式优化
- 日志轮转与归档
- 性能影响最小化

---

## 一、日志系统架构

### 1.1 系统组成

```
┌─────────────────────────────────────────────────────────────┐
│                    日志系统架构                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  应用层 (Application Layer):                                │
│  ├── DEBUG() - 调试信息                                     │
│  ├── INFO()  - 一般信息                                     │
│  ├── WARN()  - 警告信息                                     │
│  ├── ERROR() - 错误信息                                     │
│  └── FATAL() - 致命错误                                     │
│                          │                                  │
│  ▼                                                          │
│  前端层 (Frontend Layer):                                   │
│  ├── 日志格式化 (Format)                                    │
│  ├── 日志过滤 (Filter)                                      │
│  ├── 条件编译 (Conditional)                                 │
│  └── 零拷贝优化 (Zero-copy)                                 │
│                          │                                  │
│  ▼                                                          │
│  缓冲层 (Buffer Layer):                                     │
│  ├── 线程本地缓冲 (Thread-local Buffer)                     │
│  ├── 环形缓冲区 (Ring Buffer)                               │
│  └── 批量写入 (Batch Write)                                 │
│                          │                                  │
│  ▼                                                          │
│  后端层 (Backend Layer):                                    │
│  ├── 异步写入线程 (Async Writer)                            │
│  ├── 文件轮转 (File Rotation)                               │
│  ├── 压缩归档 (Compression)                                 │
│  └── 网络传输 (Network Transport)                           │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 日志级别

```
┌─────────────────────────────────────────────────────────────┐
│                    日志级别定义                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  TRACE (追踪):                                              │
│  ┌─────────────────────────────────────────────────┐       │
│  │  - 最详细的调试信息                                 │       │
│  │  - 生产环境通常关闭                                 │       │
│  │  - 示例: 变量值、函数进出                             │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  DEBUG (调试):                                              │
│  ┌─────────────────────────────────────────────────┐       │
│  │  - 调试信息                                         │       │
│  │  - 开发环境使用                                     │       │
│  │  - 示例: 状态变化、中间结果                           │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  INFO (信息):                                               │
│  ┌─────────────────────────────────────────────────┐       │
│  │  - 一般运行信息                                     │       │
│  │  - 生产环境默认级别                                 │       │
│  │  - 示例: 玩家登录、系统启动                           │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  WARN (警告):                                               │
│  ┌─────────────────────────────────────────────────┐       │
│  │  - 潜在问题                                         │       │
│  │  - 需要关注但不影响运行                               │       │
│  │  - 示例: 连接慢、重试                                 │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  ERROR (错误):                                              │
│  ┌─────────────────────────────────────────────────┐       │
│  │  - 错误事件                                         │       │
│  │  - 影响部分功能                                     │       │
│  │  - 示例: 请求失败、异常捕获                           │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  FATAL (致命):                                              │
│  ┌─────────────────────────────────────────────────┐       │
│  │  - 严重错误                                         │       │
│  │  - 可能导致程序退出                                   │       │
│  │  - 示例: 数据库连接失败、OOM                          │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、异步日志实现

### 2.1 环形缓冲区

```cpp
// 无锁环形缓冲区用于日志系统

template<typename T, size_t Size>
class RingBuffer {
public:
    static_assert(Size && !(Size & (Size - 1)), "Size must be power of 2");

    bool push(T&& item) {
        size_t writePos = writePos_.load(std::memory_order_relaxed);
        size_t nextPos = (writePos + 1) & (Size - 1);

        // 检查缓冲区是否已满
        if (nextPos == readPos_.load(std::memory_order_acquire)) {
            return false;  // 满了
        }

        buffer_[writePos] = std::move(item);

        // 确保数据写入后再更新写指针
        writePos_.store(nextPos, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        size_t readPos = readPos_.load(std::memory_order_relaxed);

        if (readPos == writePos_.load(std::memory_order_acquire)) {
            return false;  // 空了
        }

        item = std::move(buffer_[readPos]);

        // 更新读指针
        readPos_.store((readPos + 1) & (Size - 1), std::memory_order_release);
        return true;
    }

    bool empty() const {
        return readPos_.load(std::memory_order_acquire) ==
               writePos_.load(std::memory_order_acquire);
    }

private:
    std::array<T, Size> buffer_;
    std::atomic<size_t> readPos_{0};
    std::atomic<size_t> writePos_{0};
};
```

### 2.2 异步日志器

```cpp
// 异步日志系统实现

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <fstream>
#include <chrono>
#include <sstream>
#include <iomanip>

class AsyncLogger {
public:
    static AsyncLogger& instance() {
        static AsyncLogger logger;
        return logger;
    }

    void log(const std::string& message) {
        // 线程本地缓冲减少锁竞争
        thread_local std::vector<std::string> localBuffer;
        localBuffer.push_back(message);

        // 批量提交
        if (localBuffer.size() >= 32) {
            flushLocalBuffer(localBuffer);
        }
    }

    void flush() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return queue_.empty(); });
        writer_.flush();
    }

    void setLevel(LogLevel level) {
        logLevel_.store(level, std::memory_order_release);
    }

private:
    enum class LogLevel {
        TRACE = 0,
        DEBUG = 1,
        INFO = 2,
        WARN = 3,
        ERROR = 4,
        FATAL = 5
    };

    AsyncLogger() : running_(true), logLevel_(LogLevel::INFO) {
        // 启动写入线程
        writerThread_ = std::thread(&AsyncLogger::writerLoop, this);

        // 打开日志文件
        openNewFile();
    }

    ~AsyncLogger() {
        // 停止写入线程
        running_ = false;
        cv_.notify_all();
        if (writerThread_.joinable()) {
            writerThread_.join();
        }

        writer_.flush();
        writer_.close();
    }

    void flushLocalBuffer(std::vector<std::string>& buffer) {
        if (buffer.empty()) return;

        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& msg : buffer) {
            queue_.push(std::move(msg));
        }
        buffer.clear();
        cv_.notify_one();
    }

    void writerLoop() {
        std::vector<std::string> batch;
        batch.reserve(128);

        while (running_) {
            std::unique_lock<std::mutex> lock(mutex_);

            // 等待数据或超时
            cv_.wait_for(lock, std::chrono::milliseconds(100), [this] {
                return !queue_.empty() || !running_;
            });

            // 批量取出
            while (!queue_.empty() && batch.size() < 128) {
                batch.push_back(std::move(queue_.front()));
                queue_.pop();
            }

            lock.unlock();

            // 写入文件
            for (const auto& msg : batch) {
                writer_ << msg << std::endl;
            }

            // 检查文件轮转
            checkRotation();

            batch.clear();
        }
    }

    void openNewFile() {
        if (writer_.is_open()) {
            writer_.close();
        }

        // 生成文件名: app_YYYYMMDD_HHMMSS.log
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);

        std::stringstream ss;
        ss << "logs/app_"
           << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S")
           << ".log";

        writer_.open(ss.str(), std::ios::app);
        currentFileSize_ = 0;
    }

    void checkRotation() {
        // 文件大小超过限制则轮转
        if (currentFileSize_ > maxFileSize_) {
            openNewFile();
        }
    }

    std::thread writerThread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<std::string> queue_;

    std::ofstream writer_;
    size_t currentFileSize_ = 0;
    static constexpr size_t maxFileSize_ = 100 * 1024 * 1024;  // 100MB

    std::atomic<bool> running_;
    std::atomic<LogLevel> logLevel_;
};
```

---

## 三、日志格式优化

### 3.1 高效格式化

```cpp
// 高效的日志格式化器

class LogFormatter {
public:
    // 格式化日志条目
    static std::string format(
        LogLevel level,
        const char* file,
        int line,
        const char* function,
        const std::string& message
    ) {
        // 使用栈上的缓冲区避免堆分配
        char buffer[4096];
        int offset = 0;

        // 时间戳 (缓存以减少系统调用)
        auto now = getCurrentTime();
        offset += formatTime(buffer + offset, sizeof(buffer) - offset, now);

        // 级别
        offset += formatLevel(buffer + offset, sizeof(buffer) - offset, level);

        // 线程 ID
        offset += formatThread(buffer + offset, sizeof(buffer) - offset);

        // 文件:行号 (仅 DEBUG 级别)
        if (level <= LogLevel::DEBUG) {
            offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                              " [%s:%d]", file, line);
        }

        // 消息
        offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                          " %s", message.c_str());

        return std::string(buffer, offset);
    }

    // 零拷贝拼接 (使用引用)
    template<typename... Args>
    static std::string concat(Args&&... args) {
        std::size_t totalSize = (sizeof(args) + ...);
        std::string result;
        result.reserve(totalSize);

        (result.append(std::forward<Args>(args)), ...);
        return result;
    }

private:
    static std::string getCurrentTime() {
        // 使用 thread-local 缓存时间戳
        thread_local std::string lastTimeString;
        thread_local std::chrono::system_clock::time_point lastTimePoint;

        auto now = std::chrono::system_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastTimePoint
        );

        // 10ms 内使用缓存
        if (diff.count() < 10) {
            return lastTimeString;
        }

        // 更新缓存
        lastTimePoint = now;
        lastTimeString = formatTime(now);
        return lastTimeString;
    }

    static std::string formatTime(std::chrono::system_clock::time_point time) {
        auto t = std::chrono::system_clock::to_time_t(time);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            time.time_since_epoch()
        ) % 1000;

        char buffer[64];
        snprintf(buffer, sizeof(buffer), "[%02d:%02d:%02d.%03d]",
                 localtime(&t)->tm_hour,
                 localtime(&t)->tm_min,
                 localtime(&t)->tm_sec,
                 ms.count());

        return buffer;
    }
};
```

### 3.2 类型安全格式化

```cpp
// 类型安全的格式化 (类似 fmt library)

template<typename... Args>
std::string format(const char* fmt, Args&&... args) {
    // 编译时格式化检查
    // 使用 C++20 std::format 或自定义实现
    return std::vformat(fmt,
        std::make_format_args(std::forward<Args>(args)...));
}

// 使用示例
LOG_INFO("Player {} logged in from {}", playerId, ip);
// 而不是
LOG_INFO("Player " + std::to_string(playerId) + " logged in from " + ip);
```

---

## 四、KBEngine 日志系统

### 4.1 KBEngine 日志宏

```cpp
// KBEngine 日志系统
// src/lib/logger.h

#define DEBUG_MSG(fmt, ...) \
    KBEngine::debugLog(fmt, ##__VA_ARGS__)

#define INFO_MSG(fmt, ...) \
    KBEngine::infoLog(fmt, ##__VA_ARGS__)

#define WARNING_MSG(fmt, ...) \
    KBEngine::warningLog(fmt, ##__VA_ARGS__)

#define ERROR_MSG(fmt, ...) \
    KBEngine::errorLog(fmt, ##__VA_ARGS__)

#define CRITICAL_MSG(fmt, ...) \
    KBEngine::criticalLog(fmt, ##__VA_ARGS__)

// 使用示例
void onPlayerLogin(Entity* entity) {
    INFO_MSG("Player %d logged in", entity->id());

    if (entity->level() < 10) {
        WARNING_MSG("Low level player %d", entity->id());
    }
}
```

### 4.2 KBEngine Python 日志

```python
# KBEngine Python 日志
# scripts/kbe_scripts/debug_common.py

import KBEngine
import logging

# 配置日志
logging.basicConfig(
    level=logging.INFO,
    format='[%(asctime)s] %(levelname)s %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)

# KBEngine 日志封装
def DEBUG_MSG(fmt, *args):
    """调试日志"""
    logging.debug(fmt, *args)
    KBEngine.debugMsg(fmt % args)

def INFO_MSG(fmt, *args):
    """信息日志"""
    logging.info(fmt, *args)
    KBEngine.infoMsg(fmt % args)

def WARNING_MSG(fmt, *args):
    """警告日志"""
    logging.warning(fmt, *args)
    KBEngine.warningMsg(fmt % args)

def ERROR_MSG(fmt, *args):
    """错误日志"""
    logging.error(fmt, *args)
    KBEngine.errorMsg(fmt % args)

# 使用示例
class Account(KBEngine.Entity):
    def onLogin(self, entityType):
        INFO_MSG("Account %s login as %s", self.id, entityType)

        if self.isNewPlayer():
            WARNING_MSG("New player detected: %s", self.id)
```

---

## 五、性能优化

### 5.1 优化策略对比

| 策略 | 优化效果 | 实现难度 | 适用场景 |
|------|----------|----------|----------|
| **异步写入** | 极高 | 中 | 所有场景 |
| **线程本地缓冲** | 高 | 低 | 多线程 |
| **零拷贝** | 中 | 中 | 高频日志 |
| **条件编译** | 高 | 低 | 调试日志 |
| **延迟格式化** | 中 | 中 | 稀有日志 |
| **批量写入** | 高 | 低 | 所有场景 |

### 5.2 性能对比

```
┌─────────────────────────────────────────────────────────────┐
│                    日志性能对比                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  同步日志写入:                                               │
│  ├── 每次调用 ~1000ns                                       │
│  ├── 包含磁盘 I/O                                           │
│  └── 影响主线程性能                                         │
│                                                             │
│  异步日志写入:                                               │
│  ├── 每次调用 ~50ns                                         │
│  ├── 仅内存操作                                             │
│  └── 几乎不影响主线程                                        │
│                                                             │
│  延迟格式化:                                                 │
│  ├── DEBUG 级别关闭时 ~0ns                                  │
│  ├── 字符串拼接仅在需要时执行                                │
│  └── 节省 CPU 和内存                                        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 六、最佳实践

### 6.1 日志优化建议

| 实践 | 说明 |
|------|------|
| **异步写入** | 避免阻塞主线程 |
| **分级输出** | 生产环境关闭 DEBUG |
| **批量写入** | 减少 I/O 次数 |
| **结构化日志** | 便于解析分析 |
| **避免循环日志** | 减少重复信息 |
| **上下文信息** | 包含请求 ID 等标识 |

### 6.2 KBEngine 日志技巧

```python
# 1. 使用日志掩码
KBEngine.setLogLevel(KBEngine.LOG_LEVEL_INFO)

# 2. 分类日志到不同文件
# KBEngine 支持按组件输出日志

# 3. 性能关键代码避免日志
# 战斗循环内尽量少打日志

# 4. 使用延迟格式化
# 不要: DEBUG_MSG("Value: " + str(value))
# 推荐: DEBUG_MSG("Value: %s", value)
```

---

## 七、总结

### 日志优化核心

```
日志优化 = 异步写入 + 批量处理 + 分级过滤 + 格式优化
- 不阻塞主线程
- 最小化性能影响
- 保留关键信息
- 便于问题排查
```

---

## 参考资料

- [KBEngine Logging](https://github.com/kbengine/kbengine/tree/master/src/lib)
- [spdlog - Fast C++ Logging](https://github.com/gabime/spdlog)
- [Java Log4j Performance](https://logging.apache.org/log4j/2.x/manual/performance.html)
