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

#ifdef _WIN32
// EventLoop 依赖 Winsock（WSAPoll/socket/recv/send）。测试目标仅引用
// event_loop.o，http.o 的静态初始化不保证被链接，因此在本翻译单元内
// 显式初始化/清理 Winsock，保证任何 EventLoop 使用者都能安全建 socket。
namespace {
struct WinSockGuard {
    WinSockGuard() {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    }
    ~WinSockGuard() {
        WSACleanup();
    }
} g_winSockGuard;
}
#endif

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

class net::EventLoop::Impl {
public:
    explicit Impl(EventLoop& outer) : outer_(outer), running_(false), nextTimerId_(1) {
#ifdef _WIN32
        // Windows 唤醒必须用真实 Winsock socket：WSAPoll 只接受 SOCKET，
        // 若把 CreatePipe 的 HANDLE 强转后传入，是非法 socket → UB/SEGFAULT
        // （CI 41907458774 EventLoopReentrantTests 实证）。回环 TCP pair 是
        // 最小可轮询对：读端 = accept，写端 = connect。
        SOCKET ls = INVALID_SOCKET, conn = INVALID_SOCKET, acc = INVALID_SOCKET;
        SOCKADDR_IN addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
        int alen = static_cast<int>(sizeof addr);
        ls = ::socket(AF_INET, SOCK_STREAM, 0);
        if (ls != INVALID_SOCKET &&
            ::bind(ls, reinterpret_cast<SOCKADDR*>(&addr), sizeof addr) == 0 &&
            ::getsockname(ls, reinterpret_cast<SOCKADDR*>(&addr), &alen) == 0 &&
            ::listen(ls, 1) == 0) {
            conn = ::socket(AF_INET, SOCK_STREAM, 0);
            if (conn != INVALID_SOCKET &&
                ::connect(conn, reinterpret_cast<SOCKADDR*>(&addr), sizeof addr) == 0) {
                acc = ::accept(ls, nullptr, nullptr);
            }
        }
        if (ls != INVALID_SOCKET) ::closesocket(ls);
        if (conn != INVALID_SOCKET && acc == INVALID_SOCKET) ::closesocket(conn);
        wakeupRead_ = acc;
        wakeupWrite_ = conn;
#else
        // Unix 使用 pipe
        int pipefds[2];
        if (pipe(pipefds) != 0) {
            // Handle error
            return;
        }
        wakeupRead_ = pipefds[0];
        wakeupWrite_ = pipefds[1];
#endif
    }

    ~Impl() {
#ifdef _WIN32
        if (wakeupRead_ != INVALID_SOCKET) ::closesocket(wakeupRead_);
        if (wakeupWrite_ != INVALID_SOCKET) ::closesocket(wakeupWrite_);
#else
        if (wakeupRead_ >= 0) ::close(wakeupRead_);
        if (wakeupWrite_ >= 0) ::close(wakeupWrite_);
#endif
    }

    bool init(const EventLoopConfig& config) {
        config_ = config;
        return true;
    }

    bool isRunning() const {
        return running_.load();
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
                    (void)::recv(wakeupRead_, buf, sizeof(buf), 0);
#else
                    (void)::read(wakeupRead_, buf, sizeof(buf));  // Return value intentionally ignored for wakeup
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
                    fired |= EventType::ERR;
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
        (void)::send(wakeupWrite_, &c, 1, 0);
#else
        (void)::write(wakeupWrite_, &c, 1);  // Return value intentionally ignored for wakeup
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
        auto now = std::chrono::steady_clock::now();
        std::vector<std::pair<bool, TimerCallback>> fired;  // {periodic, callback}
        std::vector<TimerId> oneShotIds;
        {
            std::lock_guard<std::mutex> lock(timerMutex_);
            for (auto& timer : timers_) {
                if (now >= timer->nextTrigger()) {
                    fired.emplace_back(timer->periodic(), timer->callback());
                    if (!timer->periodic()) {
                        oneShotIds.push_back(timer->id());
                    } else {
                        timer->reset();
                    }
                }
            }
            // 一次性定时器出列（出锁后触发）
            if (!oneShotIds.empty()) {
                auto it = std::remove_if(timers_.begin(), timers_.end(),
                    [&](const auto& t) {
                        return std::find(oneShotIds.begin(), oneShotIds.end(),
                                         t->id()) != oneShotIds.end();
                    });
                timers_.erase(it, timers_.end());
            }
        }

        for (auto& [periodic, cb] : fired) {
            if (cb) {
                cb();
            }
            stats_.timersFired++;
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
        return wakeupRead_;
    }

    EventLoop& outer_;
    EventLoopConfig config_;
    std::atomic<bool> running_;
    std::thread::id loopThreadId_;

    socket_t wakeupRead_ = static_cast<socket_t>(-1);
    socket_t wakeupWrite_ = static_cast<socket_t>(-1);

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

net::EventLoop::EventLoop() : impl_(new Impl(*this)) {}

net::EventLoop::~EventLoop() {
    stop();
    delete impl_;
}

bool net::EventLoop::init(const EventLoopConfig& config) {
    return impl_->init(config);
}

bool net::EventLoop::run() {
    impl_->loopThreadId_ = std::this_thread::get_id();
    return impl_->run();
}

bool net::EventLoop::runInThread() {
    if (impl_->isRunning()) return false;

    eventThread_ = std::thread([this]() {
        impl_->loopThreadId_ = std::this_thread::get_id();
        impl_->run();
    });

    return true;
}

void net::EventLoop::stop() {
    impl_->stop();
    if (eventThread_.joinable()) {
        eventThread_.join();
    }
}

void net::EventLoop::wakeup() {
    impl_->wakeup();
}

bool net::EventLoop::isRunning() const {
    return impl_ && impl_->isRunning();
}

bool net::EventLoop::addSocket(socket_t sockfd, EventType events, SocketCallback callback) {
    return impl_->addSocket(sockfd, events, std::move(callback));
}

bool net::EventLoop::modifySocket(socket_t sockfd, EventType events) {
    return impl_->modifySocket(sockfd, events);
}

bool net::EventLoop::removeSocket(socket_t sockfd) {
    return impl_->removeSocket(sockfd);
}

TimerId net::EventLoop::addTimer(uint64_t delayMs, TimerCallback callback) {
    return impl_->addTimer(delayMs, std::move(callback), false);
}

TimerId net::EventLoop::addPeriodicTimer(uint64_t intervalMs, TimerCallback callback) {
    return impl_->addTimer(intervalMs, std::move(callback), true);
}

bool net::EventLoop::removeTimer(TimerId timerId) {
    return impl_->removeTimer(timerId);
}

bool net::EventLoop::hasTimer(TimerId timerId) const {
    return impl_->hasTimer(timerId);
}

void net::EventLoop::executeInLoop(Task task) {
    impl_->executeInLoop(std::move(task));
}

void net::EventLoop::executeInLoopAsync(Task task) {
    {
        std::lock_guard<std::mutex> lock(impl_->taskMutex_);
        impl_->pendingTasks_.push(std::move(task));
    }
    impl_->wakeup();
}

EventLoopStats net::EventLoop::getStats() const {
    return impl_->getStats();
}

void net::EventLoop::resetStats() {
    impl_->resetStats();
}

//==============================================================================
// HeartbeatManager 实现
//==============================================================================

net::HeartbeatManager::HeartbeatManager(EventLoop* loop)
    : loop_(loop) {

    // 启动定时检查
    checkTimerId_ = loop_->addPeriodicTimer(1000, [this]() {
        onCheckTimer();
    });
}

net::HeartbeatManager::~HeartbeatManager() {
    // 移除周期检查定时器：回调捕获 this，manager 先死则回调打悬空 this
    // （audit P0#3）
    if (loop_) {
        loop_->removeTimer(checkTimerId_);
    }
}

void net::HeartbeatManager::addHeartbeat(socket_t sockfd, uint64_t intervalMs,
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

void net::HeartbeatManager::removeHeartbeat(socket_t sockfd) {
    std::lock_guard<std::mutex> lock(mutex_);
    heartbeats_.erase(sockfd);
}

void net::HeartbeatManager::updateHeartbeat(socket_t sockfd) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = heartbeats_.find(sockfd);
    if (it != heartbeats_.end()) {
        it->second.lastBeat = std::chrono::steady_clock::now();
    }
}

void net::HeartbeatManager::checkTimeouts() {
    onCheckTimer();
}

void net::HeartbeatManager::onCheckTimer() {
    auto now = std::chrono::steady_clock::now();
    std::vector<std::pair<socket_t, HeartbeatCallback>> beatCallbacks;
    std::vector<std::pair<socket_t, TimeoutCallback>> timeoutCallbacks;
    std::vector<socket_t> timedOut;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [sockfd, info] : heartbeats_) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - info.lastBeat).count();
            const uint64_t elapsedMs = static_cast<uint64_t>(elapsed);

            // 发送心跳
            if (elapsedMs >= info.intervalMs && info.heartbeatCallback) {
                info.lastBeat = now;
                beatCallbacks.emplace_back(sockfd, info.heartbeatCallback);
            }

            // 检查超时 (3倍心跳间隔)
            if (elapsedMs >= info.intervalMs * 3) {
                timeoutCallbacks.emplace_back(sockfd, info.timeoutCallback);
                timedOut.push_back(sockfd);
            }
        }
        // 超时 fd 出锁回调前先删除——原实现在遍历体内 erase(sockfd) 致迭代器
        // 失效 UB（audit P0#3）
        for (const auto fd : timedOut) {
            heartbeats_.erase(fd);
        }
    }

    // 心跳回调出锁触发（回调可能重入 addHeartbeat/updateHeartbeat）
    for (auto& [sockfd, cb] : beatCallbacks) {
        if (cb) {
            cb(sockfd);
        }
    }
    // 超时回调出锁触发
    for (auto& [sockfd, cb] : timeoutCallbacks) {
        if (cb) {
            cb(sockfd);
        }
    }
}

//==============================================================================
// ReconnectManager 实现
//==============================================================================

net::ReconnectManager::ReconnectManager(EventLoop* loop)
    : loop_(loop), nextReconnectId_(1) {
}

net::ReconnectManager::~ReconnectManager() {
    // 移除全部活跃重连定时器：每个定时器回调捕获 (this, id)，manager 先死
    // 则回调打悬空 this（audit P0#3）
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [id, info] : reconnects_) {
        (void)id;
        loop_->removeTimer(info.timerId);
    }
    reconnects_.clear();
}

uint64_t net::ReconnectManager::addReconnect(const std::string& address, uint16_t port,
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

bool net::ReconnectManager::removeReconnect(uint64_t reconnectId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = reconnects_.find(reconnectId);
    if (it == reconnects_.end()) return false;

    loop_->removeTimer(it->second.timerId);
    reconnects_.erase(it);
    return true;
}

bool net::ReconnectManager::triggerReconnect(uint64_t reconnectId) {
    ReconnectCallback callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = reconnects_.find(reconnectId);
        if (it == reconnects_.end() || !it->second.active) return false;
        callback = it->second.callback;
    }

    // 回调在出锁后触发（同 CR-06 onReconnectTimer）：回调可重入
    // removeReconnect/addReconnect，持锁触发即自锁死锁。
    return callback ? callback() : false;
}

void net::ReconnectManager::onReconnectTimer(uint64_t reconnectId) {
    ReconnectCallback callback;
    bool expired = false;
    TimerId timerId = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = reconnects_.find(reconnectId);
        if (it == reconnects_.end() || !it->second.active) return;

        auto& info = it->second;
        if (info.currentRetries >= info.maxRetries) {
            expired = true;
            timerId = info.timerId;
            info.active = false;
        } else {
            info.currentRetries++;
            callback = info.callback;
            timerId = info.timerId;
        }
    }

    if (expired) {
        loop_->removeTimer(timerId);
        return;
    }

    if (callback) {
        bool success = callback();
        if (success) {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = reconnects_.find(reconnectId);
            if (it != reconnects_.end() && it->second.active) {
                loop_->removeTimer(it->second.timerId);
                it->second.active = false;
            }
        }
    }
}

} // namespace net
} // namespace apollo
