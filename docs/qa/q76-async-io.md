# Q76: 如何实现异步 IO？

## 问题分析

本题考察对异步 IO 的理解：
- 同步 vs 异步 IO
- Reactor 模式
- Proactor 模式
- 协程实现

---

## 一、IO 模型对比

### 1.1 五种 IO 模型

```
┌─────────────────────────────────────────────────────────────┐
│                    IO 模型对比                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 阻塞 IO (Blocking IO):                                 │
│     应用程序调用 recv，阻塞等待数据到达                       │
│     ┌────────┐         ┌──────────┐                        │
│     │ App    │────────>│ Kernel   │                        │
│     │        │< blocked│          │                        │
│     └────────┘         └──────────┘                        │
│                                                             │
│  2. 非阻塞 IO (Non-blocking IO):                           │
│     应用程序不断调用 recv，立即返回 EAGAIN                   │
│     ┌────────┐  poll   ┌──────────┐                        │
│     │ App    │<───────││ Kernel   │                        │
│     │        │────────>│          │                        │
│     └────────┘  no data└──────────┘                        │
│                                                             │
│  3. IO 多路复用 (IO Multiplexing):                         │
│     select/poll/epoll 等待多个描述字                         │
│     ┌────────┐         ┌──────────┐                        │
│     │ App    │<───────││ Kernel   │                        │
│     │        │  ready  │          │                        │
│     └────────┘         └──────────┘                        │
│                                                             │
│  4. 信号驱动 IO (Signal-driven IO):                        │
│     数据就绪时内核发送 SIGIO 信号                            │
│     ┌────────┤ signal  ┌──────────┐                        │
│     │ App    │<────────│ Kernel   │                        │
│     └────────┘         └──────────┘                        │
│                                                             │
│  5. 异步 IO (Asynchronous IO):                             │
│     应用程序发起 IO，内核完成后通知                           │
│     ┌────────┐         ┌──────────┐                        │
│     │ App    │────────>│ Kernel   │                        │
│     │        │<────────│ callback │                        │
│     └────────┘         └──────────┘                        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 同步 vs 异步

| 特性 | 同步 IO | 异步 IO |
|------|---------|---------|
| **调用行为** | 阻塞等待 | 立即返回 |
| **完成通知** | 返回值 | 回调/事件 |
| **编程复杂度** | 简单 | 复杂 |
| **并发能力** | 依赖线程/多路复用 | 高 |
| **适用场景** | 简单应用 | 高性能服务器 |

---

## 二、Reactor 模式

### 2.1 Reactor 架构

```cpp
// Reactor 模式实现

#include <sys/epoll.h>

class Reactor {
public:
    Reactor() : epollFd_(epoll_create1(0)) {
        if (epollFd_ < 0) {
            throw std::runtime_error("epoll_create1 failed");
        }
    }

    ~Reactor() {
        close(epollFd_);
    }

    // 注册事件处理器
    void registerHandler(int fd, uint32_t events, EventHandler* handler) {
        struct epoll_event ev;
        ev.events = events;
        ev.data.ptr = handler;

        epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);
        handlers_[fd] = handler;
    }

    // 移除事件处理器
    void removeHandler(int fd) {
        epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, nullptr);
        handlers_.erase(fd);
    }

    // 事件循环
    void run() {
        struct epoll_event events[MAX_EVENTS];

        while (running_) {
            int nfds = epoll_wait(epollFd_, events, MAX_EVENTS, timeout_);

            for (int i = 0; i < nfds; ++i) {
                auto* handler = static_cast<EventHandler*>(events[i].data.ptr);

                if (events[i].events & EPOLLIN) {
                    handler->handleInput();
                }
                if (events[i].events & EPOLLOUT) {
                    handler->handleOutput();
                }
                if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                    handler->handleError();
                }
            }
        }
    }

private:
    static constexpr int MAX_EVENTS = 1024;

    int epollFd_;
    int timeout_ = 100;  // ms
    bool running_ = true;
    std::unordered_map<int, EventHandler*> handlers_;
};

// 事件处理器接口
class EventHandler {
public:
    virtual ~EventHandler() = default;
    virtual void handleInput() = 0;
    virtual void handleOutput() = 0;
    virtual void handleError() = 0;
    virtual int getFd() const = 0;
};

// 示例: TCP 连接处理器
class TcpConnection : public EventHandler {
public:
    TcpConnection(int fd, Reactor* reactor)
        : fd_(fd), reactor_(reactor) {

        // 注册可读事件
        reactor_->registerHandler(fd_, EPOLLIN | EPOLLET, this);
    }

    ~TcpConnection() {
        reactor_->removeHandler(fd_);
        close(fd_);
    }

    void handleInput() override {
        char buffer[4096];
        ssize_t n = read(fd_, buffer, sizeof(buffer));

        if (n > 0) {
            // 处理接收的数据
            onDataReceived(buffer, n);
        } else if (n == 0) {
            // 连接关闭
            onClose();
        } else if (errno != EAGAIN) {
            handleError();
        }
    }

    void handleOutput() override {
        // 发送缓冲区数据
        flushSendBuffer();
    }

    void handleError() override {
        close(fd_);
    }

    int getFd() const override { return fd_; }

    void send(const char* data, size_t len) {
        sendBuffer_.append(data, len);
        // 注册可写事件
        reactor_->registerHandler(fd_, EPOLLIN | EPOLLOUT | EPOLLET, this);
    }

private:
    void flushSendBuffer() {
        while (!sendBuffer_.empty()) {
            ssize_t n = write(fd_, sendBuffer_.data(), sendBuffer_.size());

            if (n > 0) {
                sendBuffer_.erase(0, n);
            } else if (n < 0 && errno != EAGAIN) {
                handleError();
                return;
            } else {
                // EAGAIN，等待下次可写
                return;
            }
        }

        // 发送完成，取消可写事件
        reactor_->registerHandler(fd_, EPOLLIN | EPOLLET, this);
    }

    void onDataReceived(const char* data, size_t len) {
        // 处理接收到的数据
    }

    void onClose() {
        delete this;  // 自删除
    }

    int fd_;
    Reactor* reactor_;
    std::string sendBuffer_;
};
```

### 2.2 事件分发器

```cpp
// 事件分发器

class EventDemultiplexer {
public:
    struct Event {
        int fd;
        uint32_t events;  // EPOLLIN, EPOLLOUT, etc.
    };

    // 等待事件
    std::vector<Event> waitForEvents(int timeout_ms) {
        struct epoll_event events[MAX_EVENTS];

        int nfds = epoll_wait(epollFd_, events, MAX_EVENTS, timeout_ms);

        std::vector<Event> result;
        result.reserve(nfds);

        for (int i = 0; i < nfds; ++i) {
            result.push_back({
                events[i].data.fd,
                events[i].events
            });
        }

        return result;
    }

private:
    static constexpr int MAX_EVENTS = 1024;
    int epollFd_;
};
```

---

## 三、Proactor 模式

### 3.1 Proactor 架构

```cpp
// Proactor 模式 (使用 IOCP 或 libaio)

#ifdef _WIN32
class Proactor {
public:
    Proactor() {
        iocp_ = CreateIoCompletionPort(
            INVALID_HANDLE_VALUE,
            nullptr,
            0,
            0
        );
    }

    void run() {
        DWORD bytesTransferred;
        ULONG_PTR completionKey;
        LPOVERLAPPED overlapped;

        while (true) {
            BOOL success = GetQueuedCompletionStatus(
                &iocp_,
                &bytesTransferred,
                &completionKey,
                &overlapped,
                INFINITE
            );

            if (overlapped) {
                auto* handler = reinterpret_cast<AsyncHandler*>(completionKey);
                handler->onComplete(bytesTransferred, overlapped);
            }
        }
    }

private:
    HANDLE iocp_;
};
#endif

// 跨平台异步 IO (使用 io_uring 或 libuv)
class AsyncIOOperation {
public:
    using Callback = std::function<void(size_t)>;

    void asyncRead(int fd, void* buffer, size_t size, Callback cb) {
        // 使用 io_uring 或异步 API
        // 完成后调用 cb
    }

    void asyncWrite(int fd, const void* buffer, size_t size, Callback cb) {
        // 异步写入
    }
};
```

---

## 四、协程实现

### 4.1 用户态协程

```cpp
// 协程实现异步 IO

#include <ucontext.h>
#include <functional>

class Coroutine {
public:
    using Func = std::function<void()>;

    Coroutine(Func func)
        : func_(std::move(func)),
          stack_(new char[STACK_SIZE]) {

        getcontext(&context_);
        context_.uc_stack.ss_sp = stack_;
        context_.uc_stack.ss_size = STACK_SIZE;
        context_.uc_link = &mainContext_;

        makecontext(&context_, &Coroutine::trampoline, 1, this);
    }

    ~Coroutine() {
        delete[] stack_;
    }

    void resume() {
        if (!finished_) {
            swapcontext(&mainContext_, &context_);
        }
    }

    void yield() {
        swapcontext(&context_, &mainContext_);
    }

    bool isFinished() const {
        return finished_;
    }

private:
    static void trampoline(Coroutine* coro) {
        coro->func_();
        coro->finished_ = true;
        coro->yield();  // 返回主协程
    }

    Func func_;
    ucontext_t context_;
    ucontext_t mainContext_;
    char* stack_;
    bool finished_ = false;

    static constexpr size_t STACK_SIZE = 1024 * 1024;
};

// 协程调度器
class CoroutineScheduler {
public:
    Coroutine& createCoroutine(std::function<void()> func) {
        auto coro = std::make_unique<Coroutine>(std::move(func));
        coroutines_.push_back(std::move(coro));
        return *coroutines_.back();
    }

    void run() {
        while (!coroutines_.empty()) {
            for (auto it = coroutines_.begin(); it != coroutines_.end(); ) {
                (*it)->resume();

                if ((*it)->isFinished()) {
                    it = coroutines_.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

private:
    std::vector<std::unique_ptr<Coroutine>> coroutines_;
};

// 异步操作封装
class AsyncFileReader {
public:
    void readAsync(const std::string& path,
                   std::function<void(const std::string&)> callback) {

        // 在协程中执行
        scheduler_.createCoroutine([this, path, callback]() {
            std::string content = readFile(path);
            callback(content);
        });

        scheduler_.run();
    }

private:
    CoroutineScheduler scheduler_;

    std::string readFile(const std::string& path) {
        // 同步读取，但在协程中执行不阻塞其他协程
        // 实际应该使用真正的异步 IO
        std::ifstream file(path);
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        return content;
    }
};
```

### 4.2 C++20 协程

```cpp
// C++20 协程实现异步 IO

#include <coroutine>

template<typename T>
class Task {
public:
    struct promise_type {
        T value_;
        std::exception_ptr exception_;

        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        void return_value(T value) {
            value_ = std::move(value);
        }

        void unhandled_exception() {
            exception_ = std::current_exception();
        }
    };

    explicit Task(std::coroutine_handle<promise_type> handle)
        : handle_(handle) {}

    ~Task() {
        if (handle_) {
            handle_.destroy();
        }
    }

    T get() {
        if (!handle_.done()) {
            handle_.resume();
        }

        if (handle_.promise().exception_) {
            std::rethrow_exception(handle_.promise().exception_);
        }

        return std::move(handle_.promise().value_);
    }

private:
    std::coroutine_handle<promise_type> handle_;
};

// 异步文件读取
Task<std::string> readFileAsync(const std::string& path) {
    // 使用异步 IO 或在线程池中执行
    co_return readFileContent(path);
}
```

---

## 五、KBEngine 异步 IO

### 5.1 KBEngine 网络模型

```python
# KBEngine 异步网络模型

"""
KBEngine 网络模型:

1. 使用 epoll 实现异步网络
2. 单线程事件循环
3. 消息队列缓冲
4. 邮箱模式处理

核心类:
- EventPoller: 事件轮询器
- PacketReceiver: 数据包接收器
- Channel: 网络通道
"""

# KBEngine 风格的异步网络
class AsyncNetwork:
    def __init__(self):
        self.epoll_fd = select.epoll()
        self.channels = {}  # fd -> Channel
        self.read_fds = set()
        self.write_fds = set()

    def register_channel(self, channel):
        """注册网络通道"""
        fd = channel.fileno()
        self.channels[fd] = channel
        self.read_fds.add(fd)

        # 注册到 epoll
        self.epoll_fd.register(fd, select.EPOLLIN | select.EPOLLERR)

    def process(self, timeout_ms=100):
        """处理网络事件"""
        events = self.epoll_fd.poll(timeout_ms)

        for fd, event in events:
            channel = self.channels.get(fd)
            if not channel:
                continue

            if event & (select.EPOLLIN | select.EPOLLHUP | select.EPOLLERR):
                self.handle_read(channel)

            if event & select.EPOLLOUT:
                self.handle_write(channel)

    def handle_read(self, channel):
        """处理可读事件"""
        try:
            data = channel.recv()
            if data:
                channel.process_data(data)
            else:
                self.close_channel(channel)
        except Exception as e:
            KBEngine.error(f"Read error: {e}")
            self.close_channel(channel)

    def handle_write(self, channel):
        """处理可写事件"""
        try:
            channel.flush_send_buffer()
        except Exception as e:
            KBEngine.error(f"Write error: {e}")
            self.close_channel(channel)

    def close_channel(self, channel):
        """关闭通道"""
        fd = channel.fileno()
        self.epoll_fd.unregister(fd)
        del self.channels[fd]
        self.read_fds.discard(fd)
        self.write_fds.discard(fd)
        channel.on_close()
```

---

## 六、总结

### 异步 IO 选择

```
异步 IO 实现选择:

- Linux: epoll (Reactor) 或 io_uring
- Windows: IOCP (Proactor)
- 跨平台: libuv / asio
- 简化开发: 协程 (C++20, Python asyncio)

KBEngine 选择: epoll + 单线程事件循环
```

---

## 参考资料

- [libuv Documentation](https://docs.libuv.org/)
- [io_uring by Example](https://unixism.net/loti/tutorial/)
- [C++20 Coroutines](https://en.cppreference.com/w/cpp/language/coroutines)
