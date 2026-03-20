# Q77: IOCP vs Epoll 有什么区别？

## 问题分析

本题考察对跨平台异步 IO 的理解：
- IOCP (Windows)
- Epoll (Linux)
- kqueue (BSD/macOS)
- 性能对比
- 跨平台方案

---

## 一、基本概念

### 1.1 功能对比

```
┌─────────────────────────────────────────────────────────────┐
│                    IOCP vs Epoll                             │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  IOCP (I/O Completion Ports) - Windows:                     │
│  ├── 真正的异步 IO                                          │
│  ├── 内核完成 IO 后通知                                      │
│  ├── 支持文件 IO                                           │
│  ├── 基于 IO 对象                                           │
│  └── GetQueuedCompletionStatus                              │
│                                                             │
│  Epoll - Linux:                                             │
│  ├── 同步多路复用                                           │
│  ├── 检测 IO 就绪状态                                        │
│  ├── 主要支持网络 IO                                        │
│  ├── 基于文件描述符                                         │
│  └── epoll_wait                                           │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 详细对比

| 特性 | IOCP (Windows) | Epoll (Linux) |
|------|----------------|---------------|
| **IO 模型** | 异步 | 多路复用 |
| **通知方式** | 完成后通知 | 就绪后通知 |
| **文件支持** | 原生支持 | 5.1+ 后支持 (io_uring) |
| **API 风格** | OOP | 面向过程 |
| **线程模型** | 多线程友好 | 单线程高效 |
| **边缘触发** | 不适用 | 支持 (EPOLLET) |
| **跨平台** | 仅 Windows | 仅 Linux |

---

## 二、IOCP 实现

### 2.1 IOCP 基础

```cpp
// Windows IOCP 实现

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

class IOCPServer {
public:
    IOCPServer() {
        // 创建 IOCP
        iocp_ = CreateIoCompletionPort(
            INVALID_HANDLE_VALUE,  // 文件句柄
            nullptr,                // 现有 IOCP
            0,                      // 完成键
            0                       // 并发线程数 (0 = CPU 数)
        );

        if (iocp_ == nullptr) {
            throw std::runtime_error("CreateIoCompletionPort failed");
        }

        // 启动工作线程
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        for (DWORD i = 0; i < sysInfo.dwNumberOfProcessors; ++i) {
            workers_.emplace_back(&IOCPServer::workerLoop, this);
        }
    }

    ~IOCPServer() {
        CloseHandle(iocp_);
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    // 关联套接字到 IOCP
    void associateSocket(SOCKET sock) {
        CreateIoCompletionPort(
            reinterpret_cast<HANDLE>(sock),
            iocp_,
            0,  // 完成键
            0
        );
    }

    // 异步接收
    void postRecv(SOCKET sock, PerIoData* ioData) {
        DWORD flags = 0;
        DWORD bytesReceived = 0;

        int result = WSARecv(
            sock,
            &ioData->wsabuf,
            1,
            &bytesReceived,
            &flags,
            &ioData->overlapped,
            nullptr
        );

        if (result == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error != WSA_IO_PENDING) {
                // 错误处理
            }
        }
    }

    // 异步发送
    void postSend(SOCKET sock, PerIoData* ioData) {
        DWORD bytesSent = 0;

        int result = WSASend(
            sock,
            &ioData->wsabuf,
            1,
            &bytesSent,
            0,
            &ioData->overlapped,
            nullptr
        );

        if (result == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error != WSA_IO_PENDING) {
                // 错误处理
            }
        }
    }

private:
    void workerLoop() {
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

            if (overlapped == nullptr) {
                break;  // 退出信号
            }

            auto* ioData = CONTAINING_RECORD(overlapped, PerIoData, overlapped);

            if (success && bytesTransferred > 0) {
                // 处理完成的数据
                handleCompletion(ioData, bytesTransferred);
            } else {
                // 连接关闭或错误
                handleClose(ioData);
            }
        }
    }

    void handleCompletion(PerIoData* ioData, DWORD bytes) {
        switch (ioData->operation) {
        case OP_RECV:
            onRecv(ioData, bytes);
            break;
        case OP_SEND:
            onSend(ioData, bytes);
            break;
        }
    }

    void onRecv(PerIoData* ioData, DWORD bytes) {
        // 处理接收的数据
        // 继续发起下一次接收
        postRecv(ioData->socket, ioData);
    }

    void onSend(PerIoData* ioData, DWORD bytes) {
        // 发送完成
    }

    void handleClose(PerIoData* ioData) {
        closesocket(ioData->socket);
        delete ioData;
    }

    HANDLE iocp_;
    std::vector<std::thread> workers_;

    enum Operation { OP_RECV, OP_SEND };

    struct PerIoData {
        WSAOVERLAPPED overlapped = {};
        WSABUF wsabuf;
        SOCKET socket;
        Operation operation;
        char buffer[4096];
    };
};
```

---

## 三、Epoll 实现

### 3.1 Epoll 基础

```cpp
// Linux Epoll 实现

#include <sys/epoll.h>

class EpollServer {
public:
    EpollServer() {
        epollFd_ = epoll_create1(0);
        if (epollFd_ < 0) {
            throw std::runtime_error("epoll_create1 failed");
        }
    }

    ~EpollServer() {
        close(epollFd_);
    }

    // 添加文件描述符
    void addFd(int fd, uint32_t events) {
        struct epoll_event ev;
        ev.events = events;
        ev.data.fd = fd;

        epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);
    }

    // 修改事件
    void modifyFd(int fd, uint32_t events) {
        struct epoll_event ev;
        ev.events = events;
        ev.data.fd = fd;

        epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
    }

    // 移除文件描述符
    void removeFd(int fd) {
        epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, nullptr);
    }

    // 事件循环
    void run() {
        struct epoll_event events[MAX_EVENTS];

        while (running_) {
            int nfds = epoll_wait(epollFd_, events, MAX_EVENTS, timeout_);

            for (int i = 0; i < nfds; ++i) {
                int fd = events[i].data.fd;
                uint32_t revents = events[i].events;

                if (revents & EPOLLIN) {
                    handleRead(fd);
                }
                if (revents & EPOLLOUT) {
                    handleWrite(fd);
                }
                if (revents & (EPOLLERR | EPOLLHUP)) {
                    handleError(fd);
                }
            }
        }
    }

private:
    void handleRead(int fd) {
        char buffer[4096];
        ssize_t n = read(fd, buffer, sizeof(buffer));

        if (n > 0) {
            // 处理数据
            onDataReceived(fd, buffer, n);
        } else if (n == 0) {
            // 连接关闭
            handleClose(fd);
        } else if (errno != EAGAIN) {
            handleError(fd);
        }
    }

    void handleWrite(int fd) {
        // 处理可写
    }

    void handleError(int fd) {
        close(fd);
    }

    void handleClose(int fd) {
        close(fd);
    }

    void onDataReceived(int fd, const char* data, size_t len) {
        // 处理接收的数据
    }

    static constexpr int MAX_EVENTS = 1024;
    int epollFd_;
    int timeout_ = 100;  // ms
    bool running_ = true;
};
```

### 3.2 Epoll ET 模式

```cpp
// Epoll 边缘触发 (Edge-Triggered) 模式

class EpollETServer {
public:
    void addFd(int fd) {
        // 设置为非阻塞
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        // 注册 EPOLLET (边缘触发)
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP;
        ev.data.fd = fd;

        epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);
    }

    void handleRead(int fd) {
        // ET 模式需要一次性读完所有数据
        while (true) {
            char buffer[4096];
            ssize_t n = read(fd, buffer, sizeof(buffer));

            if (n > 0) {
                onDataReceived(fd, buffer, n);
            } else if (n == 0) {
                handleClose(fd);
                break;
            } else if (errno == EAGAIN) {
                // 数据读完
                break;
            } else {
                handleError(fd);
                break;
            }
        }
    }

private:
    int epollFd_;
};
```

---

## 四、跨平台方案

### 4.1 抽象层设计

```cpp
// 跨平台抽象层

#if defined(_WIN32)
    #define PLATFORM_WINDOWS
    #include <windows.h>
    #include <winsock2.h>
#elif defined(__linux__)
    #define PLATFORM_LINUX
    #include <sys/epoll.h>
    #include <unistd.h>
    #include <fcntl.h>
#elif defined(__APPLE__)
    #define PLATFORM_MACOS
    #include <sys/types.h>
    #include <sys/event.h>
#endif

class Poller {
public:
    virtual ~Poller() = default;

    virtual bool addFd(int fd, uint32_t events) = 0;
    virtual bool modifyFd(int fd, uint32_t events) = 0;
    virtual bool removeFd(int fd) = 0;
    virtual int wait(int timeout_ms) = 0;

    static std::unique_ptr<Poller> create();
};

// Linux 实现
class EpollPoller : public Poller {
public:
    EpollPoller() : epollFd_(epoll_create1(0)) {}

    bool addFd(int fd, uint32_t events) override {
        struct epoll_event ev;
        ev.events = events;
        ev.data.fd = fd;
        return epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev) == 0;
    }

    int wait(int timeout_ms) override {
        return epoll_wait(epollFd_, events_, MAX_EVENTS, timeout_ms);
    }

private:
    int epollFd_;
    struct epoll_event events_[1024];
    static constexpr int MAX_EVENTS = 1024;
};

// Windows 实现
class IOCPPoller : public Poller {
    // IOCP 实现
};

// macOS 实现
class KqueuePoller : public Poller {
public:
    KqueuePoller() : kqueueFd_(kqueue()) {}

    bool addFd(int fd, uint32_t events) override {
        struct kevent ke;

        if (events & POLLIN) {
            EV_SET(&ke, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
            kevent(kqueueFd_, &ke, 1, nullptr, 0, nullptr);
        }

        if (events & POLLOUT) {
            EV_SET(&ke, fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, nullptr);
            kevent(kqueueFd_, &ke, 1, nullptr, 0, nullptr);
        }

        return true;
    }

    int wait(int timeout_ms) override {
        struct timespec timeout = {
            .tv_sec = timeout_ms / 1000,
            .tv_nsec = (timeout_ms % 1000) * 1000000
        };

        return kevent(kqueueFd_, nullptr, 0, events_, MAX_EVENTS, &timeout);
    }

private:
    int kqueueFd_;
    struct kevent events_[1024];
    static constexpr int MAX_EVENTS = 1024;
};

// 工厂函数
std::unique_ptr<Poller> Poller::create() {
#if defined(PLATFORM_LINUX)
    return std::make_unique<EpollPoller>();
#elif defined(PLATFORM_WINDOWS)
    return std::make_unique<IOCPPoller>();
#elif defined(PLATFORM_MACOS)
    return std::make_unique<KqueuePoller>();
#else
    #error "Unsupported platform"
#endif
}
```

### 4.2 使用 libuv

```cpp
// 使用 libuv 跨平台

#include <uv.h>

class UVServer {
public:
    void start() {
        uv_loop_init(&loop_);

        // 初始化 TCP
        uv_tcp_init(&loop_, &server_);
        uv_tcp_bind(&server_, addr, 0);

        // 监听
        uv_listen(reinterpret_cast<uv_stream_t*>(&server_), 128,
                 onNewConnection);

        // 事件循环
        uv_run(&loop_, UV_RUN_DEFAULT);
    }

private:
    static void onNewConnection(uv_stream_t* server, int status) {
        if (status < 0) return;

        uv_tcp_t* client = new uv_tcp_t;
        uv_tcp_init(uv_default_loop(), client);

        uv_accept(server, reinterpret_cast<uv_stream_t*>(client));

        // 开始读取
        uv_read_start(reinterpret_cast<uv_stream_t*>(client),
                      allocBuffer, onRead);
    }

    static void onRead(uv_stream_t* stream, ssize_t nread,
                       const uv_buf_t* buf) {
        if (nread > 0) {
            // 处理数据
        }

        if (nread < 0) {
            uv_close(reinterpret_cast<uv_handle_t*>(stream), nullptr);
        }

        delete[] buf->base;
    }

    static uv_buf_t allocBuffer(uv_handle_t* handle, size_t suggested) {
        char* buffer = new char[65536];
        return uv_buf_init(buffer, 65536);
    }

    uv_loop_t loop_;
    uv_tcp_t server_;
};
```

---

## 五、性能对比

### 5.1 性能特点

| 场景 | IOCP | Epoll |
|------|------|-------|
| **连接数 < 1000** | 相当 | 相当 |
| **连接数 1000-10000** | 优秀 | 优秀 |
| **连接数 > 10000** | 优秀 | 优秀 |
| **单线程** | 一般 | 优秀 |
| **多线程** | 优秀 | 一般 |
| **文件 IO** | 原生支持 | 需 io_uring |

### 5.2 吞吐量对比

```
┌─────────────────────────────────────────────────────────────┐
│                    性能测试参考                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  简单回显服务器 (10000 并发连接):                            │
│                                                             │
│  Windows IOCP:                                              │
│  ├── 单线程: ~80K req/s                                     │
│  ├── 多线程: ~200K req/s                                    │
│  └── CPU 使用: 较高                                         │
│                                                             │
│  Linux Epoll:                                              │
│  ├── 单线程: ~150K req/s                                    │
│  ├── 多线程: ~180K req/s                                    │
│  └── CPU 使用: 较低                                         │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 六、最佳实践

### 6.1 选择建议

```
选择建议:

Windows:
- 使用 IOCP
- 多线程处理完成事件

Linux:
- 使用 Epoll (简单场景)
- 使用 io_uring (高性能场景)

macOS/BSD:
- 使用 kqueue

跨平台:
- libuv
- libevent
- asio
```

---

## 七、总结

```
IOCP vs Epoll:

- IOCP: 真正异步，多线程友好
- Epoll: 多路复用，单线程高效
- 跨平台: 使用 libuv 抽象

KBEngine: Epoll + 单线程事件循环
```

---

## 参考资料

- [Windows IOCP Documentation](https://docs.microsoft.com/en-us/windows/win32/fileio/i-o-completion-ports)
- [Epoll Man Page](https://man7.org/linux/man-pages/man7/epoll.7.html)
- [libuv Documentation](https://docs.libuv.org/)
