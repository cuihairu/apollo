/**
 * @file event_loop.cpp
 * @brief 增强版事件循环实现
 */

#include "apollo/net/event_loop.h"
#include <algorithm>
#include <iostream>

#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
    typedef SOCKET socket_t;
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
    #define SOCKET_ERROR_VALUE SOCKET_ERROR
#else
    #include <sys/poll.h>
    #include <unistd.h>
    #include <fcntl.h>
    typedef int socket_t;
    #define INVALID_SOCKET_VALUE -1
    #define SOCKET_ERROR_VALUE -1
#endif

namespace apollo {
namespace net {

//==============================================================================
// 线程本地事件循环
//==============================================================================

static thread_local EventLoop* g_currentEventLoop = nullptr;

EventLoop* getCurrentEventLoop() {
    return g_currentEventLoop;
}

void setCurrentEventLoop(EventLoop* loop) {
    g_currentEventLoop = loop;
}

//==============================================================================
// Socket 信息
//==============================================================================

struct SocketInfo {
    socket_t sockfd;
    EventType events;
    SocketCallback callback;
};

//==============================================================================
// 事件循环实现
//==============================================================================

class EventLoop::Impl {
public:
    Impl() : running_(false), wakeupFd_(-1), nextTimerId_(1) {
#ifdef _WIN32
        // Windows 使用管道进行唤醒
        SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        CreatePipe(&wakeupRead_, &wakeupWrite_, &sa, 0);
#else
        // Unix 使用 pipe
        int pipefds[2];
        pipe(pipefds);
        wakeupRead_ = pipefds[0];
        wakeupWrite_ = pipefds[1];
#endif
    }

    ~Impl() {
#ifdef _WIN32
        if (wakeupRead_) CloseHandle(wakeupRead_);
        if (wakeupWrite_) CloseHandle(wakeupWrite_);
#else
        if (wakeupRead_ >= 0) ::close(wakeupRead_);
        if (wakeupWrite_ >= 0) ::close(wakeupWrite_);
#endif
    }

    bool init(const EventLoopConfig& config) {
        config_ = config;
        return true;
    }

    bool run() {
        running_ = true;
        setCurrentEventLoop(&outer_);

#ifdef _WIN32
        constexpr short kReadEvent = POLLRDNORM;
        constexpr short kWriteEvent = POLLWRNORM;
        using PollFd = WSAPOLLFD;
#else
        constexpr short kReadEvent = POLLIN;
        constexpr short kWriteEvent = POLLOUT;
        using PollFd = pollfd;
#endif

        while (running_) {
            // 准备 pollfd
            std::vector<PollFd> fds;
            fds.reserve(sockets_.size() + 1);  // +1 for wakeup fd

            // 添加唤醒 fd
            PollFd wakeupPfd{};
            wakeupPfd.fd = wakeupFd();
            wakeupPfd.events = kReadEvent;
            fds.push_back(wakeupPfd);

            // 添加所有 socket
            {
                std::lock_guard<std::mutex> lock(mutex_);
                for (const auto& [fd, info] : sockets_) {
                    PollFd pfd{};
                    pfd.fd = fd;
                    pfd.events = 0;

                    if (HasEvent(info.events, EventType::READ)) {
                        pfd.events |= kReadEvent;
                    }
                    if (HasEvent(info.events, EventType::WRITE)) {
                        pfd.events |= kWriteEvent;
                    }

                    fds.push_back(pfd);
                }
            }

            // 计算超时
            int timeout = config_.timeoutMs;
            if (config_.enableTimer) {
                int timerTimeout = calculateNextTimeout();
                if (timerTimeout >= 0 && timerTimeout < timeout) {
                    timeout = timerTimeout;
                }
            }

            // 等待事件
#ifdef _WIN32
            int ready = WSAPoll(fds.data(), static_cast<ULONG>(fds.size()), timeout);
#else
            int ready = poll(fds.data(), fds.size(), timeout);
#endif

            stats_.loopCount++;

            // 处理定时器
            if (config_.enableTimer) {
                processTimers();
            }

            // 处理任务
            processPendingTasks();

            if (ready <= 0) {
                continue;
            }

            // 处理事件
            for (size_t i = 0; i < fds.size(); ++i) {
                const auto& pfd = fds[i];
                if (pfd.revents == 0) continue;

                // 处理唤醒事件
                if (i == 0) {
                    char buf[128];
#ifdef _WIN32
                    DWORD bytesRead;
                    ReadFile(wakeupRead_, buf, sizeof(buf), &bytesRead, nullptr);
#else
                    ::read(wakeupRead_, buf, sizeof(buf));
#endif
                    stats_.wakeups++;
                    continue;
                }

                // 处理 socket 事件
                EventType fired = EventType::NONE;
                if (pfd.revents & kReadEvent) {
                    fired |= EventType::READ;
                }
                if (pfd.revents & kWriteEvent) {
                    fired |= EventType::WRITE;
                }
                if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                    fired |= EventType::ERROR;
                }

                SocketCallback cb;
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    auto it = sockets_.find(pfd.fd);
                    if (it == sockets_.end()) continue;
                    cb = it->second.callback;
                }

                if (cb) {
                    stats_.eventsProcessed++;
                    cb(pfd.fd, fired);
                }
            }
        }

        setCurrentEventLoop(nullptr);
        return true;
    }

    void stop() {
        running_ = false;
        wakeup();
    }

    void wakeup() {
        char c = 1;
#ifdef _WIN32
        DWORD bytesWritten;
        WriteFile(wakeupWrite_, &c, 1, &bytesWritten, nullptr);
#else
        ::write(wakeupWrite_, &c, 1);
#endif
    }

    bool addSocket(socket_t sockfd, EventType events, SocketCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        sockets_[sockfd] = {sockfd, events, std::move(callback)};
        stats_.currentConnections = sockets_.size();
        wakeup();
        return true;
    }

    bool modifySocket(socket_t sockfd, EventType events) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sockets_.find(sockfd);
        if (it == sockets_.end()) return false;
        it->second.events = events;
        wakeup();
        return true;
    }

    bool removeSocket(socket_t sockfd) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto erased = sockets_.erase(sockfd) > 0;
        stats_.currentConnections = sockets_.size();
        wakeup();
        return erased;
    }

    TimerId addTimer(uint64_t delayMs, TimerCallback callback, bool periodic) {
        std::lock_guard<std::mutex> lock(timerMutex_);

        TimerId id = nextTimerId_++;
        auto timer = std::make_unique<Timer>(id, std::move(callback), delayMs, periodic);
        timers_.push_back(std::move(timer));

        wakeup();
        return id;
    }

    bool removeTimer(TimerId timerId) {
        std::lock_guard<std::mutex> lock(timerMutex_);
        auto it = std::remove_if(timers_.begin(), timers_.end(),
            [timerId](const auto& t) { return t->id() == timerId; });
        bool removed = it != timers_.end();
        timers_.erase(it, timers_.end());
        wakeup();
        return removed;
    }

    bool hasTimer(TimerId timerId) const {
        std::lock_guard<std::mutex> lock(timerMutex_);
        return std::any_of(timers_.begin(), timers_.end(),
            [timerId](const auto& t) { return t->id() == timerId; });
    }

    void executeInLoop(Task task) {
        if (running_ && std::this_thread::get_id() == loopThreadId_) {
            task();
        } else {
            {
                std::lock_guard<std::mutex> lock(taskMutex_);
                pendingTasks_.push(std::move(task));
            }
            wakeup();
        }
    }

    EventLoopStats getStats() const { return stats_; }
    void resetStats() { stats_ = {}; }

private:
    int calculateNextTimeout() {
        std::lock_guard<std::mutex> lock(timerMutex_);
        if (timers_.empty()) return -1;

        auto now = std::chrono::steady_clock::now();
        int minTimeout = -1;

        for (const auto& timer : timers_) {
            auto next = timer->nextTrigger();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(next - now);
            int timeout = static_cast<int>(duration.count());
            if (timeout < 0) timeout = 0;
            if (minTimeout < 0 || timeout < minTimeout) {
                minTimeout = timeout;
            }
        }

        return minTimeout;
    }

    void processTimers() {
        std::lock_guard<std::mutex> lock(timerMutex_);

        auto now = std::chrono::steady_clock::now();
        std::vector<Timer*> fired;

        for (auto& timer : timers_) {
            if (now >= timer->nextTrigger()) {
                fired.push_back(timer.get());
            }
        }

        for (auto* timer : fired) {
            timer->fire();
            stats_.timersFired++;

            // 移除一次性定时器
            if (!timer->periodic()) {
                auto it = std::remove_if(timers_.begin(), timers_.end(),
                    [timer](const auto& t) { return t.get() == timer; });
                timers_.erase(it, timers_.end());
            }
        }
    }

    void processPendingTasks() {
        std::queue<Task> tasks;
        {
            std::lock_guard<std::mutex> lock(taskMutex_);
            tasks.swap(pendingTasks_);
        }

        while (!tasks.empty()) {
            tasks.front()();
            tasks.pop();
        }
    }

    socket_t wakeupFd() const {
#ifdef _WIN32
        return reinterpret_cast<socket_t>(wakeupRead_);
#else
        return wakeupRead_;
#endif
    }

    EventLoop& outer_;
    EventLoopConfig config_;
    std::atomic<bool> running_;
    std::thread::id loopThreadId_;

#ifdef _WIN32
    HANDLE wakeupRead_ = nullptr;
    HANDLE wakeupWrite_ = nullptr;
#else
    int wakeupRead_ = -1;
    int wakeupWrite_ = -1;
#endif

    mutable std::mutex mutex_;
    std::unordered_map<socket_t, SocketInfo> sockets_;

    mutable std::mutex timerMutex_;
    std::vector<std::unique_ptr<Timer>> timers_;
    TimerId nextTimerId_;

    std::mutex taskMutex_;
    std::queue<Task> pendingTasks_;

    EventLoopStats stats_;

    friend class EventLoop;
};

//==============================================================================
// EventLoop 实现
//==============================================================================

EventLoop::EventLoop() : impl_(new Impl()) {
    impl_->outer_ = *this;
}

EventLoop::~EventLoop() {
    stop();
    delete impl_;
}

bool EventLoop::init(const EventLoopConfig& config) {
    return impl_->init(config);
}

bool EventLoop::run() {
    impl_->loopThreadId_ = std::this_thread::get_id();
    return impl_->run();
}

bool EventLoop::runInThread() {
    if (impl_->running_) return false;

    eventThread_ = std::thread([this]() {
        impl_->loopThreadId_ = std::this_thread::get_id();
        impl_->run();
    });

    return true;
}

void EventLoop::stop() {
    impl_->stop();
    if (eventThread_.joinable()) {
        eventThread_.join();
    }
}

void EventLoop::wakeup() {
    impl_->wakeup();
}

bool EventLoop::addSocket(socket_t sockfd, EventType events, SocketCallback callback) {
    return impl_->addSocket(sockfd, events, std::move(callback));
}

bool EventLoop::modifySocket(socket_t sockfd, EventType events) {
    return impl_->modifySocket(sockfd, events);
}

bool EventLoop::removeSocket(socket_t sockfd) {
    return impl_->removeSocket(sockfd);
}

TimerId EventLoop::addTimer(uint64_t delayMs, TimerCallback callback) {
    return impl_->addTimer(delayMs, std::move(callback), false);
}

TimerId EventLoop::addPeriodicTimer(uint64_t intervalMs, TimerCallback callback) {
    return impl_->addTimer(intervalMs, std::move(callback), true);
}

bool EventLoop::removeTimer(TimerId timerId) {
    return impl_->removeTimer(timerId);
}

bool EventLoop::hasTimer(TimerId timerId) const {
    return impl_->hasTimer(timerId);
}

void EventLoop::executeInLoop(Task task) {
    impl_->executeInLoop(std::move(task));
}

void EventLoop::executeInLoopAsync(Task task) {
    {
        std::lock_guard<std::mutex> lock(impl_->taskMutex_);
        impl_->pendingTasks_.push(std::move(task));
    }
    impl_->wakeup();
}

EventLoopStats EventLoop::getStats() const {
    return impl_->getStats();
}

void EventLoop::resetStats() {
    impl_->resetStats();
}

//==============================================================================
// HeartbeatManager 实现
//==============================================================================

HeartbeatManager::HeartbeatManager(EventLoop* loop)
    : loop_(loop), nextTimerId_(1) {

    // 启动定时检查
    checkTimerId_ = loop_->addPeriodicTimer(1000, [this]() {
        onCheckTimer();
    });
}

void HeartbeatManager::addHeartbeat(socket_t sockfd, uint64_t intervalMs,
                                   HeartbeatCallback heartbeatCb,
                                   TimeoutCallback timeoutCb) {
    std::lock_guard<std::mutex> lock(mutex_);

    HeartbeatInfo info;
    info.sockfd = sockfd;
    info.intervalMs = intervalMs;
    info.lastBeat = std::chrono::steady_clock::now();
    info.heartbeatCallback = std::move(heartbeatCb);
    info.timeoutCallback = std::move(timeoutCb);

    heartbeats_[sockfd] = std::move(info);
}

void HeartbeatManager::removeHeartbeat(socket_t sockfd) {
    std::lock_guard<std::mutex> lock(mutex_);
    heartbeats_.erase(sockfd);
}

void HeartbeatManager::updateHeartbeat(socket_t sockfd) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = heartbeats_.find(sockfd);
    if (it != heartbeats_.end()) {
        it->second.lastBeat = std::chrono::steady_clock::now();
    }
}

void HeartbeatManager::checkTimeouts() {
    onCheckTimer();
}

void HeartbeatManager::onCheckTimer() {
    auto now = std::chrono::steady_clock::now();
    std::vector<socket_t> timedOut;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [sockfd, info] : heartbeats_) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - info.lastBeat).count();

            // 发送心跳
            if (elapsed >= info.intervalMs && info.heartbeatCallback) {
                info.heartbeatCallback(sockfd);
            }

            // 检查超时 (3倍心跳间隔)
            if (elapsed >= info.intervalMs * 3) {
                timedOut.push_back(sockfd);
            }
        }
    }

    // 处理超时
    for (auto sockfd : timedOut) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = heartbeats_.find(sockfd);
        if (it != heartbeats_.end()) {
            if (it->second.timeoutCallback) {
                it->second.timeoutCallback(sockfd);
            }
            heartbeats_.erase(it);
        }
    }
}

//==============================================================================
// ReconnectManager 实现
//==============================================================================

ReconnectManager::ReconnectManager(EventLoop* loop)
    : loop_(loop), nextReconnectId_(1) {
}

uint64_t ReconnectManager::addReconnect(const std::string& address, uint16_t port,
                                       uint64_t intervalMs, uint32_t maxRetries,
                                       ReconnectCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);

    uint64_t id = nextReconnectId_++;

    ReconnectInfo info;
    info.id = id;
    info.address = address;
    info.port = port;
    info.intervalMs = intervalMs;
    info.maxRetries = maxRetries;
    info.currentRetries = 0;
    info.callback = std::move(callback);
    info.active = true;

    // 设置定时器
    info.timerId = loop_->addPeriodicTimer(intervalMs, [this, id]() {
        onReconnectTimer(id);
    });

    reconnects_[id] = std::move(info);
    return id;
}

bool ReconnectManager::removeReconnect(uint64_t reconnectId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = reconnects_.find(reconnectId);
    if (it == reconnects_.end()) return false;

    loop_->removeTimer(it->second.timerId);
    reconnects_.erase(it);
    return true;
}

bool ReconnectManager::triggerReconnect(uint64_t reconnectId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = reconnects_.find(reconnectId);
    if (it == reconnects_.end() || !it->second.active) return false;

    return it->second.callback ? it->second.callback() : false;
}

void ReconnectManager::onReconnectTimer(uint64_t reconnectId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = reconnects_.find(reconnectId);
    if (it == reconnects_.end() || !it->second.active) return;

    auto& info = it->second;

    if (info.currentRetries >= info.maxRetries) {
        // 达到最大重试次数
        loop_->removeTimer(info.timerId);
        info.active = false;
        return;
    }

    info.currentRetries++;

    if (info.callback) {
        bool success = info.callback();
        if (success) {
            // 重连成功，移除定时器
            loop_->removeTimer(info.timerId);
            info.active = false;
        }
    }
}

} // namespace net
} // namespace apollo
