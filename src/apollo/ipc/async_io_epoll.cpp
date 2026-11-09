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

} // namespace ipc
} // namespace apollo
