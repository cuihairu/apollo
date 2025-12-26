#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <queue>
#include <chrono>

#ifdef _WIN32
    #include <winsock2.h>
    typedef SOCKET socket_t;
#else
    #include <sys/types.h>
    typedef int socket_t;
#endif

namespace apollo {
namespace net {

//==============================================================================
// 事件类型
//==============================================================================

enum class EventType : uint32_t {
    NONE = 0,
    READ = 1 << 0,      // 可读事件
    WRITE = 1 << 1,     // 可写事件
    ERROR = 1 << 2,     // 错误事件
    EDGE_TRIGGER = 1 << 3  // 边缘触发 (仅支持 EPOLL)
};

inline EventType operator|(EventType a, EventType b) {
    return static_cast<EventType>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline EventType operator&(EventType a, EventType b) {
    return static_cast<EventType>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline bool HasEvent(EventType events, EventType flag) {
    return (static_cast<uint32_t>(events) & static_cast<uint32_t>(flag)) != 0;
}

//==============================================================================
// Socket 事件回调
//==============================================================================

using SocketCallback = std::function<void(socket_t sockfd, EventType events)>;

//==============================================================================
// 定时器 ID
//==============================================================================

using TimerId = uint64_t;

//==============================================================================
// 定时器回调
//==============================================================================

using TimerCallback = std::function<void()>;

//==============================================================================
// 事件循环统计
//==============================================================================

struct EventLoopStats {
    uint64_t loopCount = 0;        // 循环次数
    uint64_t eventsProcessed = 0;  // 处理的事件数
    uint64_t timersFired = 0;      // 触发的定时器数
    uint64_t wakeups = 0;          // 唤醒次数
    uint64_t currentConnections = 0; // 当前连接数
};

//==============================================================================
// 定时器
//==============================================================================

class Timer {
public:
    Timer(TimerId id, TimerCallback cb, uint64_t intervalMs, bool periodic)
        : id_(id), callback_(std::move(cb)), intervalMs_(intervalMs), periodic_(periodic) {
    }

    TimerId id() const { return id_; }
    uint64_t intervalMs() const { return intervalMs_; }
    bool periodic() const { return periodic_; }

    // 计算下一次触发时间
    std::chrono::steady_clock::time_point nextTrigger() const {
        return lastTrigger_ + std::chrono::milliseconds(intervalMs_);
    }

    // 触发定时器
    void fire() {
        lastTrigger_ = std::chrono::steady_clock::now();
        if (callback_) {
            callback_();
        }
    }

    // 重置定时器
    void reset() {
        lastTrigger_ = std::chrono::steady_clock::now();
    }

private:
    TimerId id_;
    TimerCallback callback_;
    uint64_t intervalMs_;
    bool periodic_;
    std::chrono::steady_clock::time_point lastTrigger_;
};

//==============================================================================
// 事件循环配置
//==============================================================================

struct EventLoopConfig {
    uint32_t maxEvents = 1024;      // 每次处理的最大事件数
    int timeoutMs = 1000;           // poll/epoll_wait 超时时间
    bool enableTimer = true;        // 启用定时器
    bool enableStats = true;        // 启用统计
};

//==============================================================================
// 事件循环 - 增强版 Reactor
//==============================================================================

class EventLoop {
public:
    EventLoop();
    virtual ~EventLoop();

    // 禁止拷贝和移动
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;
    EventLoop(EventLoop&&) = delete;
    EventLoop& operator=(EventLoop&&) = delete;

    // 初始化
    bool init(const EventLoopConfig& config = {});

    // 启动事件循环
    bool run();

    // 在独立线程中运行
    bool runInThread();

    // 停止事件循环
    void stop();

    // 唤醒事件循环
    void wakeup();

    // 是否运行中
    bool isRunning() const { return running_; }

    // 在事件循环线程中执行任务
    using Task = std::function<void()>;
    void executeInLoop(Task task);
    void executeInLoopAsync(Task task);

    //==========================================================================
    // Socket 管理
    //==========================================================================

    // 添加 Socket 监听
    bool addSocket(socket_t sockfd, EventType events, SocketCallback callback);

    // 修改 Socket 事件
    bool modifySocket(socket_t sockfd, EventType events);

    // 移除 Socket 监听
    bool removeSocket(socket_t sockfd);

    //==========================================================================
    // 定时器管理
    //==========================================================================

    // 添加一次性定时器
    TimerId addTimer(uint64_t delayMs, TimerCallback callback);

    // 添加周期性定时器
    TimerId addPeriodicTimer(uint64_t intervalMs, TimerCallback callback);

    // 移除定时器
    bool removeTimer(TimerId timerId);

    // 检查定时器是否存在
    bool hasTimer(TimerId timerId) const;

    //==========================================================================
    // 统计信息
    //==========================================================================

    EventLoopStats getStats() const;
    void resetStats();

private:
    // 处理定时器
    void processTimers();

    // 计算下一个定时器的超时时间
    int calculateNextTimeout();

    // 执行待处理任务
    void processPendingTasks();

    // 实现
    class Impl;
    Impl* impl_;
};

//==============================================================================
// 心跳管理器
//==============================================================================

class HeartbeatManager {
public:
    using HeartbeatCallback = std::function<void(socket_t sockfd)>;
    using TimeoutCallback = std::function<void(socket_t sockfd)>;

    HeartbeatManager(EventLoop* loop);

    // 添加心跳检测
    void addHeartbeat(socket_t sockfd, uint64_t intervalMs,
                     HeartbeatCallback heartbeatCb,
                     TimeoutCallback timeoutCb);

    // 移除心跳检测
    void removeHeartbeat(socket_t sockfd);

    // 更新心跳 (收到数据时调用)
    void updateHeartbeat(socket_t sockfd);

    // 检查超时
    void checkTimeouts();

private:
    struct HeartbeatInfo {
        socket_t sockfd;
        uint64_t intervalMs;
        std::chrono::steady_clock::time_point lastBeat;
        HeartbeatCallback heartbeatCallback;
        TimeoutCallback timeoutCallback;
        TimerId timerId;
    };

    EventLoop* loop_;
    std::unordered_map<socket_t, HeartbeatInfo> heartbeats_;
    std::mutex mutex_;
    TimerId checkTimerId_;
    uint64_t nextTimerId_;

    void onCheckTimer();
};

//==============================================================================
// 重连管理器
//==============================================================================

class ReconnectManager {
public:
    using ReconnectCallback = std::function<bool()>;

    ReconnectManager(EventLoop* loop);

    // 添加重连任务
    uint64_t addReconnect(const std::string& address, uint16_t port,
                         uint64_t intervalMs, uint32_t maxRetries,
                         ReconnectCallback callback);

    // 移除重连任务
    bool removeReconnect(uint64_t reconnectId);

    // 手动触发重连
    bool triggerReconnect(uint64_t reconnectId);

private:
    struct ReconnectInfo {
        uint64_t id;
        std::string address;
        uint16_t port;
        uint64_t intervalMs;
        uint32_t maxRetries;
        uint32_t currentRetries;
        ReconnectCallback callback;
        TimerId timerId;
        bool active;
    };

    EventLoop* loop_;
    std::unordered_map<uint64_t, ReconnectInfo> reconnects_;
    std::mutex mutex_;
    uint64_t nextReconnectId_;

    void onReconnectTimer(uint64_t reconnectId);
};

//==============================================================================
// 便捷全局函数
//==============================================================================

// 获取当前线程的事件循环
EventLoop* getCurrentEventLoop();

// 设置当前线程的事件循环
void setCurrentEventLoop(EventLoop* loop);

} // namespace net
} // namespace apollo
