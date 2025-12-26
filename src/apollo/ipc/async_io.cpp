/**
 * @file async_io.cpp
 * @brief 跨平台异步 I/O 实现 (IOCP / io_uring / epoll / kqueue)
 */

#include "apollo/ipc/async_io.h"
#include <algorithm>
#include <iostream>

#ifdef _WIN32
    #pragma comment(lib, "ws2_32.lib")
#endif

namespace apollo {
namespace ipc {

//==============================================================================
// 平台检测与后端名称
//==============================================================================

const char* IoMultiplexer::getBackendName() {
#if defined(APOLLO_IO_IOCP)
    return "IOCP (Windows I/O Completion Ports)";
#elif defined(APOLLO_IO_URING)
    return "io_uring (Linux async I/O)";
#elif defined(APOLLO_IO_EPOLL)
    return "epoll (Linux compatible)";
#elif defined(APOLLO_IO_KQUEUE)
    return "kqueue (BSD/macOS)";
#else
    return "Unknown";
#endif
}

//==============================================================================
// 工厂方法
//==============================================================================

std::unique_ptr<IoMultiplexer> IoMultiplexer::create(size_t maxEvents) {
#if defined(APOLLO_IO_IOCP)
    return std::make_unique<IocpMultiplexer>(maxEvents);
#elif defined(APOLLO_IO_URING)
    return std::make_unique<IoUringMultiplexer>(maxEvents, maxEvents / 4);
#elif defined(APOLLO_IO_EPOLL)
    return std::make_unique<EpollMultiplexer>(maxEvents);
#elif defined(APOLLO_IO_KQUEUE)
    return std::make_unique<KqueueMultiplexer>(maxEvents);
#else
    #error "No supported IO backend for this platform"
#endif
}

//==============================================================================
// IOCP 实现 (Windows)
//==============================================================================

#ifdef APOLLO_IO_IOCP

IocpMultiplexer::IocpMultiplexer(size_t maxEvents)
    : maxEvents_(maxEvents) {
}

IocpMultiplexer::~IocpMultiplexer() {
    stop();
}

bool IocpMultiplexer::start() {
    if (running_) {
        return true;
    }

    // 创建完成端口
    iocpHandle_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    if (!iocpHandle_) {
        return false;
    }

    // 创建唤醒事件
    wakeupEvent_ = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!wakeupEvent_) {
        CloseHandle(iocpHandle_);
        iocpHandle_ = nullptr;
        return false;
    }

    // 注册唤醒事件到完成端口
    HANDLE result = CreateIoCompletionPort(wakeupEvent_, iocpHandle_, 1, 0);
    if (!result) {
        CloseHandle(wakeupEvent_);
        CloseHandle(iocpHandle_);
        return false;
    }

    running_ = true;
    return true;
}

void IocpMultiplexer::stop() {
    if (!running_) {
        return;
    }

    running_ = false;
    SetEvent(wakeupEvent_);

    if (iocpHandle_) {
        // 等待一段时间让操作完成
        Sleep(100);
        CloseHandle(iocpHandle_);
        iocpHandle_ = nullptr;
    }

    if (wakeupEvent_) {
        CloseHandle(wakeupEvent_);
        wakeupEvent_ = nullptr;
    }
}

bool IocpMultiplexer::registerFd(int fd, uint32_t events) {
    // 在 Windows 上，fd 实际上是 SOCKET
    HANDLE result = CreateIoCompletionPort(
        reinterpret_cast<HANDLE>(fd),
        iocpHandle_,
        reinterpret_cast<ULONG_PTR>(fd),
        0
    );
    return result != nullptr;
}

bool IocpMultiplexer::unregisterFd(int fd) {
    // IOCP 不需要显式注销，关闭 fd 即可
    (void)fd;
    return true;
}

bool IocpMultiplexer::modifyFd(int fd, uint32_t events) {
    // IOCP 通过 post 操作自动处理
    (void)fd;
    (void)events;
    return true;
}

bool IocpMultiplexer::postRead(int fd, void* buffer, size_t size,
                                IoCallback callback, void* userData) {
    AsyncOp op;
    op.callback = std::move(callback);
    op.fd = fd;
    op.userData = userData;
    op.buffer.resize(size);

    IocpOperation* iocpOp = createOp(fd, op);
    if (!iocpOp) {
        return false;
    }

    DWORD flags = 0;
    WSABUF buf;
    buf.buf = reinterpret_cast<char*>(iocpOp->buffer.data());
    buf.len = static_cast<ULONG>(size);

    int result = WSARecv(
        reinterpret_cast<SOCKET>(fd),
        &buf, 1,
        nullptr,
        &iocpOp->overlapped,
        nullptr
    );

    if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != ERROR_IO_PENDING) {
            delete iocpOp;
            return false;
        }
    }

    pendingOps_++;
    return true;
}

bool IocpMultiplexer::postWrite(int fd, const void* data, size_t size,
                                 IoCallback callback, void* userData) {
    AsyncOp op;
    op.callback = std::move(callback);
    op.fd = fd;
    op.userData = userData;
    op.buffer.assign(static_cast<const uint8_t*>(data),
                     static_cast<const uint8_t*>(data) + size);

    IocpOperation* iocpOp = createOp(fd, op);
    if (!iocpOp) {
        return false;
    }

    WSABUF buf;
    buf.buf = reinterpret_cast<char*>(iocpOp->buffer.data());
    buf.len = static_cast<ULONG>(size);

    int result = WSASend(
        reinterpret_cast<SOCKET>(fd),
        &buf, 1,
        nullptr,
        &iocpOp->overlapped,
        nullptr
    );

    if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != ERROR_IO_PENDING) {
            delete iocpOp;
            return false;
        }
    }

    pendingOps_++;
    return true;
}

bool IocpMultiplexer::postAccept(int listenFd,
                                 IoCallback callback, void* userData) {
    AsyncOp op;
    op.callback = std::move(callback);
    op.fd = listenFd;
    op.userData = userData;

    IocpOperation* iocpOp = createOp(listenFd, op);
    if (!iocpOp) {
        return false;
    }

    iocpOp->accept = true;
    iocpOp->acceptSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);

    DWORD bytesReceived = 0;
    BOOL result = AcceptEx(
        reinterpret_cast<SOCKET>(listenFd),
        iocpOp->acceptSocket,
        iocpOp->acceptAddrBuf,
        sizeof(iocpOp->acceptAddrBuf) - 16,
        0,
        sizeof(iocpOp->acceptAddrBuf) / 2,
        &bytesReceived,
        &iocpOp->overlapped
    );

    if (!result) {
        int error = WSAGetLastError();
        if (error != ERROR_IO_PENDING) {
            closesocket(iocpOp->acceptSocket);
            delete iocpOp;
            return false;
        }
    }

    pendingOps_++;
    return true;
}

bool IocpMultiplexer::postConnect(int fd, const std::string& address, uint16_t port,
                                  IoCallback callback, void* userData) {
    AsyncOp op;
    op.callback = std::move(callback);
    op.fd = fd;
    op.userData = userData;

    IocpOperation* iocpOp = createOp(fd, op);
    if (!iocpOp) {
        return false;
    }

    // ConnectEx 需要函数指针动态加载
    // 简化实现：这里使用同步方式，实际项目中应使用 ConnectEx
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, address.c_str(), &addr.sin_addr);

    int result = connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK) {
            delete iocpOp;
            IoEvent event{};
            event.userData = userData;
            event.result = -1;
            event.error = error;
            callback(event);
            return false;
        }
    }

    // 异步连接成功
    pendingOps_++;
    return true;
}

uint64_t IocpMultiplexer::scheduleTimer(uint32_t delayMs, std::function<void()> callback) {
    // 创建定时器队列
    uint64_t timerId = nextTimerId_++;

    // 使用 Windows Timer Queue
    HANDLE timerHandle = CreateWaitableTimer(nullptr, FALSE, nullptr);
    if (!timerHandle) {
        return 0;
    }

    LARGE_INTEGER dueTime;
    dueTime.QuadPart = -static_cast<int64_t>(delayMs * 10000);  // 100ns 单位

    if (!SetWaitableTimer(timerHandle, &dueTime, 0, nullptr, nullptr, FALSE)) {
        CloseHandle(timerHandle);
        return 0;
    }

    // 在独立线程中处理定时器（简化实现）
    std::thread([this, timerId, timerHandle, callback]() {
        WaitForSingleObject(timerHandle, INFINITE);
        CloseHandle(timerHandle);
        if (running_) {
            callback();
        }
    }).detach();

    return timerId;
}

bool IocpMultiplexer::cancelTimer(uint64_t timerId) {
    // 简化实现：不支持取消
    (void)timerId;
    return false;
}

void IocpMultiplexer::runOnce(uint32_t timeoutMs) {
    DWORD bytesTransferred = 0;
    ULONG_PTR completionKey = 0;
    LPOVERLAPPED overlapped = nullptr;

    BOOL result = GetQueuedCompletionStatus(
        iocpHandle_,
        &bytesTransferred,
        &completionKey,
        &overlapped,
        static_cast<DWORD>(timeoutMs)
    );

    if (!result) {
        DWORD error = GetLastError();
        if (error == WAIT_TIMEOUT) {
            return;  // 超时
        }
    }

    if (overlapped) {
        processCompletion(bytesTransferred, completionKey, overlapped);
    }
}

void IocpMultiplexer::run() {
    while (running_) {
        runOnce(100);  // 100ms 超时
    }
}

bool IocpMultiplexer::processCompletion(DWORD bytesTransferred, ULONG_PTR completionKey,
                                       LPOVERLAPPED overlapped) {
    IocpOperation* iocpOp = CONTAINING_RECORD(overlapped, IocpOperation, overlapped);

    IoEvent event;
    event.userData = iocpOp->asyncOp.userData;
    event.result = bytesTransferred;
    event.error = 0;

    if (iocpOp->accept) {
        event.operation = IoOp::Accept;
    } else {
        event.operation = (bytesTransferred == 0) ? IoOp::Close : IoOp::Read;
    }

    if (iocpOp->asyncOp.callback) {
        iocpOp->asyncOp.callback(event);
    }

    delete iocpOp;
    processedOps_++;
    pendingOps_--;

    return true;
}

IocpMultiplexer::IocpOperation* IocpMultiplexer::createOp(int fd, const AsyncOp& asyncOp) {
    auto* op = new IocpOperation();
    op->asyncOp = asyncOp;
    op->fd = asyncOp.fd;
    op->buffer = asyncOp.buffer;
    ZeroMemory(&op->overlapped, sizeof(op->overlapped));
    op->accept = false;

    return op;
}

#endif // APOLLO_IO_IOCP


//==============================================================================
// io_uring 实现 (Linux 5.1+)
//==============================================================================

#ifdef APOLLO_IO_URING

IoUringMultiplexer::RingDeleter::void operator()(struct io_uring* ring) const {
    if (ring) {
        io_uring_queue_exit(ring);
    }
}

IoUringMultiplexer::IoUringMultiplexer(size_t queueDepth, size_t entries)
    : queueDepth_(queueDepth) {

    struct io_uring_params params{};
    params.flags = 0;

    // 尝试使用 IORING_SETUP_SQPOLL (内核轮询模式)
    if (io_uring_queue_init_params(queueDepth, &params, ring_.get()) < 0) {
        // 失败则尝试普通模式
        if (io_uring_queue_init(queueDepth, ring_.get()) < 0) {
            throw std::runtime_error("Failed to initialize io_uring");
        }
    }
}

IoUringMultiplexer::~IoUringMultiplexer() {
    stop();
}

bool IoUringMultiplexer::start() {
    running_ = true;
    return true;
}

void IoUringMultiplexer::stop() {
    running_ = false;
}

bool IoUringMultiplexer::registerFd(int fd, uint32_t events) {
    // io_uring 不需要预先注册 fd
    (void)fd;
    (void)events;
    return true;
}

bool IoUringMultiplexer::unregisterFd(int fd) {
    (void)fd;
    return true;
}

bool IoUringMultiplexer::modifyFd(int fd, uint32_t events) {
    (void)fd;
    (void)events;
    return true;
}

bool IoUringMultiplexer::postRead(int fd, void* buffer, size_t size,
                                  IoCallback callback, void* userData) {
    if (!running_) return false;

    struct io_uring_sqe* sqe = io_uring_get_sqe(ring_.get());
    if (!sqe) {
        return false;
    }

    io_uring_prep_recv(sqe, fd, buffer, size, 0);
    io_uring_sqe_set_data(sqe, userData);

    // 存储 callback (简化：使用全局映射)
    // 实际实现中应该使用更复杂的内存管理

    pendingOps_++;
    return io_uring_submit(ring_.get()) >= 0;
}

bool IoUringMultiplexer::postWrite(int fd, const void* data, size_t size,
                                   IoCallback callback, void* userData) {
    if (!running_) return false;

    struct io_uring_sqe* sqe = io_uring_get_sqe(ring_.get());
    if (!sqe) {
        return false;
    }

    io_uring_prep_send(sqe, fd, data, size, 0);
    io_uring_sqe_set_data(sqe, userData);

    pendingOps_++;
    return io_uring_submit(ring_.get()) >= 0;
}

bool IoUringMultiplexer::postAccept(int listenFd,
                                   IoCallback callback, void* userData) {
    if (!running_) return false;

    struct io_uring_sqe* sqe = io_uring_get_sqe(ring_.get());
    if (!sqe) {
        return false;
    }

    io_uring_prep_accept(sqe, listenFd, nullptr, nullptr, 0);
    io_uring_sqe_set_data(sqe, userData);

    pendingOps_++;
    return io_uring_submit(ring_.get()) >= 0;
}

bool IoUringMultiplexer::postConnect(int fd, const std::string& address, uint16_t port,
                                     IoCallback callback, void* userData) {
    if (!running_) return false;

    struct io_uring_sqe* sqe = io_uring_get_sqe(ring_.get());
    if (!sqe) {
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, address.c_str(), &addr.sin_addr);

    io_uring_prep_connect(sqe, fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    io_uring_sqe_set_data(sqe, userData);

    pendingOps_++;
    return io_uring_submit(ring_.get()) >= 0;
}

uint64_t IoUringMultiplexer::scheduleTimer(uint32_t delayMs, std::function<void()> callback) {
    struct io_uring_sqe* sqe = io_uring_get_sqe(ring_.get());
    if (!sqe) {
        return 0;
    }

    __kernel_timespec ts{};
    ts.tv_sec = delayMs / 1000;
    ts.tv_nsec = (delayMs % 1000) * 1000000ULL;

    io_uring_prep_timeout(sqe, &ts, 0, 0);
    uint64_t timerId = nextTimerId_++;
    io_uring_sqe_set_data(sqe, reinterpret_cast<void*>(timerId));

    // 存储回调
    {
        std::lock_guard<std::mutex> lock(timerMutex_);
        timers_[timerId] = std::move(callback);
    }

    return io_uring_submit(ring_.get()) >= 0 ? timerId : 0;
}

bool IoUringMultiplexer::cancelTimer(uint64_t timerId) {
    std::lock_guard<std::mutex> lock(timerMutex_);
    return timers_.erase(timerId) > 0;
}

void IoUringMultiplexer::runOnce(uint32_t timeoutMs) {
    struct io_uring_cqe* cqe;
    unsigned int submitted;
    unsigned int wait_nr = 1;

    int ret = io_uring_wait_cqe_nr(ring_.get(), &cqe, wait_nr);
    if (ret < 0) {
        return;  // 错误
    }

    if (cqe) {
        IoEvent event;
        event.userData = io_uring_cqe_get_data(cqe);
        event.result = cqe->res;
        event.error = cqe->res < 0 ? -cqe->res : 0;

        // 处理完成队列
        io_uring_cqe_seen(ring_.get(), cqe);
        processedOp_++;
        pendingOp_--;
    }
}

void IoUringMultiplexer::run() {
    while (running_) {
        runOnce(100);
    }
}

size_t IoUringMultiplexer::getPendingOps() const {
    return pendingOp_;
}

size_t IoUringMultiplexer::getProcessedOps() const {
    return processedOp_;
}

#endif // APOLLO_IO_URING


//==============================================================================
// epoll 实现 (Linux fallback)
//==============================================================================

#ifdef APOLLO_IO_EPOLL

EpollMultiplexer::EpollMultiplexer(size_t maxEvents)
    : events_(maxEvents), maxEvents_(maxEvents) {

    // 创建 eventfd 用于唤醒
    #ifdef __linux__
        wakeupFd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    #endif

    epollFd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epollFd_ < 0) {
        throw std::runtime_error("Failed to create epoll");
    }

    // 注册 wakeup fd
    if (wakeupFd_ >= 0) {
        struct epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = wakeupFd_;
        epoll_ctl(epollFd_, EPOLL_CTL_ADD, wakeupFd_, &ev);
    }
}

EpollMultiplexer::~EpollMultiplexer() {
    stop();

    if (epollFd_ >= 0) {
        close(epollFd_);
    }
    if (wakeupFd_ >= 0) {
        close(wakeupFd_);
    }
}

bool EpollMultiplexer::start() {
    running_ = true;

    // 启动定时器线程
    timerRunning_ = true;
    timerThread_ = std::thread([this]() {
        while (timerRunning_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            // 检查并触发定时器...
        }
    });

    return true;
}

void EpollMultiplexer::stop() {
    running_ = false;
    timerRunning_ = false;

    // 唤醒 epoll_wait
    if (wakeupFd_ >= 0) {
        uint64_t value = 1;
        write(wakeupFd_, &value, sizeof(value));
    }

    if (timerThread_.joinable()) {
        timerThread_.join();
    }
}

bool EpollMultiplexer::registerFd(int fd, uint32_t events) {
    struct epoll_event ev{};
    ev.events = EPOLLET;  // 边缘触发
    if (events & 0x01) ev.events |= EPOLLIN;
    if (events & 0x02) ev.events |= EPOLLOUT;
    ev.data.fd = fd;

    return epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev) == 0;
}

bool EpollMultiplexer::unregisterFd(int fd) {
    std::lock_guard<std::mutex> lock(fdMutex_);
    fdContexts_.erase(fd);
    return epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, nullptr) == 0;
}

bool EpollMultiplexer::modifyFd(int fd, uint32_t events) {
    struct epoll_event ev{};
    ev.events = EPOLLET;
    if (events & 0x01) ev.events |= EPOLLIN;
    if (events & 0x02) ev.events |= EPOLLOUT;
    ev.data.fd = fd;

    return epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev) == 0;
}

bool EpollMultiplexer::postRead(int fd, void* buffer, size_t size,
                                  IoCallback callback, void* userData) {
    std::lock_guard<std::mutex> lock(fdMutex_);
    auto& ctx = fdContexts_[fd];
    ctx.readBuffer.resize(size);
    ctx.callback = [callback, userData](int fd, uint32_t events) {
        IoEvent event;
        event.userData = userData;
        event.result = events;
        event.error = 0;
        callback(event);
    };

    // 修改为监听可读
    return modifyFd(fd, EPOLLIN);
}

bool EpollMultiplexer::postWrite(int fd, const void* data, size_t size,
                                   IoCallback callback, void* userData) {
    std::lock_guard<std::mutex> lock(fdMutex_);
    auto& ctx = fdContexts_[fd];
    while (!ctx.writeQueue.empty()) {
        ctx.writeQueue.pop();
    }
    ctx.writeQueue.emplace(static_cast<const uint8_t*>(data),
                          static_cast<const uint8_t*>(data) + size);
    ctx.callback = [callback, userData](int fd, uint32_t events) {
        IoEvent event;
        event.userData = userData;
        event.result = events;
        event.error = 0;
        callback(event);
    };

    return modifyFd(fd, EPOLLOUT);
}

bool EpollMultiplexer::postAccept(int listenFd,
                                  IoCallback callback, void* userData) {
    std::lock_guard<std::mutex> lock(fdMutex_);
    auto& ctx = fdContexts_[listenFd];
    ctx.callback = [callback, userData](int fd, uint32_t events) {
        IoEvent event;
        event.userData = userData;
        event.operation = IoOp::Accept;
        event.result = events;
        callback(event);
    };

    return modifyFd(listenFd, EPOLLIN);
}

bool EpollMultiplexer::postConnect(int fd, const std::string& address, uint16_t port,
                                    IoCallback callback, void* userData) {
    // 设置为非阻塞
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, address.c_str(), &addr.sin_addr);

    int result = connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (result < 0 && errno != EINPROGRESS) {
        return false;
    }

    std::lock_guard<std::mutex> lock(fdMutex_);
    auto& ctx = fdContexts_[fd];
    ctx.callback = [callback, userData](int fd, uint32_t events) {
        IoEvent event;
        event.userData = userData;
        event.operation = IoOp::Connect;
        event.result = (events & EPOLLOUT) ? 0 : -1;
        event.error = (events & EPOLLERR) ? errno : 0;
        callback(event);
    };

    return modifyFd(fd, EPOLLOUT | EPOLLERR);
}

uint64_t EpollMultiplexer::scheduleTimer(uint32_t delayMs, std::function<void()> callback) {
    uint64_t timerId = nextTimerId_++;
    {
        std::lock_guard<std::mutex> lock(timerMutex_);
        timers_[timerId] = std::move(callback);
    }

    // 使用 timerfd 创建定时器
    int timerFd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timerFd < 0) {
        return 0;
    }

    struct itimerspec its{};
    its.it_value.tv_sec = delayMs / 1000;
    its.it_value.tv_nsec = (delayMs % 1000) * 1000000ULL;

    timerfd_settime(timerFd, 0, &its, nullptr);

    // 注册到 epoll
    registerFd(timerFd, EPOLLIN);

    return timerId;
}

bool EpollMultiplexer::cancelTimer(uint64_t timerId) {
    std::lock_guard<std::mutex> lock(timerMutex_);
    return timers_.erase(timerId) > 0;
}

void EpollMultiplexer::runOnce(uint32_t timeoutMs) {
    int count = epoll_wait(epollFd_, events_.data(), maxEvents_,
                           static_cast<int>(timeoutMs));
    if (count < 0) {
        return;
    }

    processEvents(count);
}

void EpollMultiplexer::run() {
    while (running_) {
        runOnce(100);
    }
}

bool EpollMultiplexer::processEvents(int count) {
    for (int i = 0; i < count; ++i) {
        int fd = events_[i].data.fd;
        uint32_t events = events_[i].events;

        if (fd == wakeupFd_) {
            // 清空 wakeup
            uint64_t value;
            read(wakeupFd_, &value, sizeof(value));
            continue;
        }

        std::lock_guard<std::mutex> lock(fdMutex_);
        auto it = fdContexts_.find(fd);
        if (it == fdContexts_.end()) {
            continue;
        }

        if (events & EPOLLIN) {
            handleRead(&it->second);
        }
        if (events & EPOLLOUT) {
            handleWrite(&it->second);
        }
        if (events & EPOLLERR) {
            if (it->second.callback) {
                it->second.callback(fd, EPOLLERR);
            }
        }
    }

    return true;
}

void EpollMultiplexer::handleRead(FdContext* ctx) {
    // TODO: 实现读取逻辑
}

void EpollMultiplexer::handleWrite(FdContext* ctx) {
    if (!ctx->writeQueue.empty()) {
        const auto& data = ctx->writeQueue.front();
        ssize_t sent = send(ctx->fd, data.data(), data.size(), 0);
        if (sent > 0) {
            ctx->writeQueue.pop();
        }
    }
}

void EpollMultiplexer::handleAccept(FdContext* ctx) {
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    int clientFd = accept(ctx->fd, reinterpret_cast<sockaddr*>(&addr), &len);
    if (clientFd >= 0 && ctx->callback) {
        ctx->callback(clientFd, EPOLLIN);
    }
}

size_t EpollMultiplexer::getPendingOps() const {
    return pendingOp_;
}

size_t EpollMultiplexer::getProcessedOps() const {
    return processedOp_;
}

#endif // APOLLO_IO_EPOLL


//==============================================================================
// kqueue 实现 (macOS/BSD)
//==============================================================================

#ifdef APOLLO_IO_KQUEUE

KqueueMultiplexer::KqueueMultiplexer(size_t maxEvents)
    : events_(maxEvents), maxEvents_(maxEvents) {

    kqFd_ = kqueue();
    if (kqFd_ < 0) {
        throw std::runtime_error("Failed to create kqueue");
    }
}

KqueueMultiplexer::~KqueueMultiplexer() {
    stop();

    if (kqFd_ >= 0) {
        close(kqFd_);
    }
}

bool KqueueMultiplexer::start() {
    running_ = true;
    return true;
}

void KqueueMultiplexer::stop() {
    running_ = false;
}

bool KqueueMultiplexer::registerFd(int fd, uint32_t events) {
    std::lock_guard<std::mutex> lock(fdMutex_);

    struct kevent change[2];
    int n = 0;

    if (events & 0x01) {  // 读
        EV_SET(&change[n++], fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, 0);
    }
    if (events & 0x02) {  // 写
        EV_SET(&change[n++], fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, 0);
    }

    return kevent(kqFd_, change, n, nullptr, 0, nullptr) >= 0;
}

bool KqueueMultiplexer::unregisterFd(int fd) {
    std::lock_guard<std::mutex> lock(fdMutex_);
    fdContexts_.erase(fd);

    struct kevent change[2];
    EV_SET(&change[0], fd, EVFILT_READ, EV_DELETE, 0, 0, 0);
    EV_SET(&change[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, 0);

    return kevent(kqFd_, change, 2, nullptr, 0, nullptr) >= 0;
}

bool KqueueMultiplexer::modifyFd(int fd, uint32_t events) {
    return registerFd(fd, events);  // kqueue 通过 ADD/DELETE 实现修改
}

bool KqueueMultiplexer::postRead(int fd, void* buffer, size_t size,
                                  IoCallback callback, void* userData) {
    std::lock_guard<std::mutex> lock(fdMutex_);
    auto& ctx = fdContexts_[fd];
    ctx.readBuffer.resize(size);
    ctx.callback = [callback, userData](int fd, uint16_t filter) {
        IoEvent event;
        event.userData = userData;
        event.result = 0;
        event.error = 0;
        callback(event);
    };

    return registerFd(fd, 0x01);  // 读事件
}

bool KqueueMultiplexer::postWrite(int fd, const void* data, size_t size,
                                   IoCallback callback, void* userData) {
    std::lock_guard<std::mutex> lock(fdMutex_);
    auto& ctx = fdContexts_[fd];
    while (!ctx.writeQueue.empty()) {
        ctx.writeQueue.pop();
    }
    ctx.writeQueue.emplace(static_cast<const uint8_t*>(data),
                          static_cast<const uint8_t*>(data) + size);
    ctx.callback = [callback, userData](int fd, uint16_t filter) {
        IoEvent event;
        event.userData = userData;
        event.result = 0;
        event.error = 0;
        callback(event);
    };

    return registerFd(fd, 0x02);  // 写事件
}

bool KqueueMultiplexer::postAccept(int listenFd,
                                  IoCallback callback, void* userData) {
    std::lock_guard<std::mutex> lock(fdMutex_);
    auto& ctx = fdContexts_[listenFd];
    ctx.callback = [callback, userData](int fd, uint16_t filter) {
        if (filter == EVFILT_READ) {
            IoEvent event;
            event.userData = userData;
            event.operation = IoOp::Accept;
            event.result = 0;
            callback(event);
        }
    };

    return registerFd(listenFd, 0x01);
}

bool KqueueMultiplexer::postConnect(int fd, const std::string& address, uint16_t port,
                                    IoCallback callback, void* userData) {
    // 设置非阻塞
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, address.c_str(), &addr.sin_addr);

    int result = connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (result < 0 && errno != EINPROGRESS) {
        return false;
    }

    std::lock_guard<std::mutex> lock(fdMutex_);
    auto& ctx = fdContexts_[fd];
    ctx.callback = [callback, userData](int fd, uint16_t filter) {
        IoEvent event;
        event.userData = userData;
        event.operation = IoOp::Connect;
        event.result = 0;
        event.error = 0;
        callback(event);
    };

    return registerFd(fd, 0x02);  // 监听可写表示连接完成
}

uint64_t KqueueMultiplexer::scheduleTimer(uint32_t delayMs, std::function<void()> callback) {
    uint64_t timerId = nextTimerId_++;
    {
        std::lock_guard<std::mutex> lock(timerMutex_);
        timers_[timerId] = std::move(callback);
    }

    struct kevent change;
    EV_SET(&change, timerId, EVFILT_TIMER, EV_ADD | EV_ONESHOT, 0,
            delayMs, nullptr);

    return kevent(kqFd_, &change, 1, nullptr, 0, nullptr) >= 0 ? timerId : 0;
}

bool KqueueMultiplexer::cancelTimer(uint64_t timerId) {
    struct kevent change;
    EV_SET(&change, timerId, EVFILT_TIMER, EV_DELETE, 0, 0, nullptr);

    std::lock_guard<std::mutex> lock(timerMutex_);
    timers_.erase(timerId);

    return kevent(kqFd_, &change, 1, nullptr, 0, nullptr) >= 0;
}

void KqueueMultiplexer::runOnce(uint32_t timeoutMs) {
    struct timespec ts{};
    ts.tv_sec = timeoutMs / 1000;
    ts.tv_nsec = (timeoutMs % 1000) * 1000000ULL;

    int count = kevent(kqFd_, nullptr, 0, events_.data(),
                       maxEvents_, &ts);

    if (count < 0) {
        return;
    }

    for (int i = 0; i < count; ++i) {
        int fd = static_cast<int>(events_[i].ident);
        uint16_t filter = events_[i].filter;
        uint16_t flags = events_[i].flags;
        void* data = events_[i].udata;

        if (filter == EVFILT_TIMER) {
            uint64_t timerId = reinterpret_cast<uint64_t>(data);
            std::lock_guard<std::mutex> lock(timerMutex_);
            auto it = timers_.find(timerId);
            if (it != timers_.end()) {
                it->second();
                timers_.erase(it);
            }
        } else if (filter == EVFILT_READ || filter == EVFILT_WRITE) {
            std::lock_guard<std::mutex> lock(fdMutex_);
            auto it = fdContexts_.find(fd);
            if (it != fdContexts_.end() && it->second.callback) {
                it->second.callback(fd, filter);
            }
        }
    }

    processedOps_ += count;
}

void KqueueMultiplexer::run() {
    while (running_) {
        runOnce(100);
    }
}

size_t KqueueMultiplexer::getPendingOps() const {
    return pendingOp_;
}

size_t KqueueMultiplexer::getProcessedOps() const {
    return processedOp_;
}

#endif // APOLLO_IO_KQUEUE

} // namespace ipc
} // namespace apollo
