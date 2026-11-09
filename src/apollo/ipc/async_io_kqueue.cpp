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
