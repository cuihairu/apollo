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

} // namespace ipc
} // namespace apollo
