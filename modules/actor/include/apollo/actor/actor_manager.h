#pragma once

#include "apollo/actor/actor_ref.h"
#include "apollo/actor/actor.h"
#include "apollo/actor/dispatcher.h"
#include "apollo/actor/actor_cell.h"
#include <unordered_map>
#include <memory>
#include <mutex>
#include <functional>
#include <atomic>

namespace apollo {
namespace actor {

//==============================================================================
// Actor 状态信息（用于监控）
//==============================================================================

struct ActorInfo {
    ActorPath path;
    ActorState state;
    std::string type;           // Actor 类型名
    size_t mailboxSize = 0;     // 邮箱中消息数
    uint64_t processedMessages = 0;  // 已处理消息数
    uint64_t droppedMessages = 0;    // 丢弃消息数
    int64_t cpuTimeUs = 0;      // CPU 时间（微秒）
    int64_t lastActivityTime = 0;    // 最后活动时间

    // 子 Actor
    std::vector<std::string> children;

    // 父 Actor
    std::string parent;

    // 自定义标签
    std::unordered_map<std::string, std::string> tags;
};

//==============================================================================
// 线程池配置
//==============================================================================

struct ThreadPoolConfig {
    // 调度线程（处理 Actor 消息）
    size_t dispatcherThreads = 0;  // 0 = CPU 核心数
    DispatchStrategy dispatchStrategy = DispatchStrategy::RoundRobin;

    // 阻塞 IO 线程（用于网络、数据库等）
    size_t ioThreads = 4;

    // 后台任务线程
    size_t backgroundThreads = 2;

    // 线程优先级（Windows/Linux）
    int threadPriority = 0;  // 0 = 普通, >0 = 高, <0 = 低

    // 线程名称前缀
    std::string dispatcherThreadPrefix = "actor-dispatch";
    std::string ioThreadPrefix = "actor-io";
    std::string backgroundThreadPrefix = "actor-bg";
};

//==============================================================================
// 观测事件
//==============================================================================

enum class ObservationEvent : uint8_t {
    ActorCreated,
    ActorStarted,
    ActorStopped,
    ActorTerminated,
    ActorRestarted,
    MessageReceived,
    MessageProcessed,
    MessageDropped,
    ActorException,
    MailboxOverflow,
    DeadlockDetected
};

struct Observation {
    ObservationEvent event;
    ActorPath actorPath;
    int64_t timestamp;
    std::string details;        // JSON 格式的详细信息

    // 扩展字段
    std::unordered_map<std::string, std::string> metadata;
};

// 观察者回调
using ObserverCallback = std::function<void(const Observation&)>;

//==============================================================================
// Actor 管理器（核心管理组件）
//==============================================================================

class ActorManager {
public:
    explicit ActorManager(const ThreadPoolConfig& config = ThreadPoolConfig{});
    ~ActorManager();

    //==========================================================================
    // 生命周期
    //==========================================================================

    bool start(ActorSystem& system);
    void stop();
    bool isRunning() const { return running_.load(); }

    //==========================================================================
    // Actor 注册管理
    //==========================================================================

    // 注册 Actor
    bool registerActor(const std::shared_ptr<ActorCell>& cell);

    // 注销 Actor
    bool unregisterActor(const std::string& name);

    // 获取 Actor
    std::shared_ptr<ActorCell> getActor(const std::string& name) const;

    // 获取所有 Actor 名称
    std::vector<std::string> getAllActorNames() const;

    // 按前缀查找 Actor
    std::vector<std::string> findActorsByPrefix(const std::string& prefix) const;

    // 按标签查找 Actor
    std::vector<std::string> findActorsByTag(const std::string& key,
                                            const std::string& value) const;

    //==========================================================================
    // Actor 控制
    //==========================================================================

    // 启动 Actor
    bool startActor(const std::string& name);

    // 停止 Actor
    bool stopActor(const std::string& name);

    // 重启 Actor
    bool restartActor(const std::string& name);

    // 暂停 Actor
    bool suspendActor(const std::string& name);

    // 恢复 Actor
    bool resumeActor(const std::string& name);

    //==========================================================================
    // 线程池管理
    //==========================================================================

    // 获取调度器
    MessageDispatcher& dispatcher() { return *dispatcher_; }

    // 提交 IO 任务
    template<typename F>
    auto submitIoTask(F&& task) -> std::future<decltype(task())> {
        using R = decltype(task());
        std::promise<R> promise;

        auto future = promise.get_future();
        ioQueue_.push([p = std::move(promise), f = std::forward<F>(task)]() mutable {
            try {
                if constexpr (std::is_void_v<R>) {
                    f();
                    p.set_value();
                } else {
                    p.set_value(f());
                }
            } catch (...) {
                p.set_exception(std::current_exception());
            }
        });

        return future;
    }

    // 提交后台任务
    template<typename F>
    auto submitBackgroundTask(F&& task) -> std::future<decltype(task())> {
        using R = decltype(task());
        std::promise<R> promise;

        auto future = promise.get_future();
        bgQueue_.push([p = std::move(promise), f = std::forward<F>(task)]() mutable {
            try {
                if constexpr (std::is_void_v<R>) {
                    f();
                    p.set_value();
                } else {
                    p.set_value(f());
                }
            } catch (...) {
                p.set_exception(std::current_exception());
            }
        });

        return future;
    }

    //==========================================================================
    // 观测性
    //==========================================================================

    // 添加观察者
    uint64_t addObserver(const std::string& name, ObserverCallback callback,
                        const std::unordered_set<ObservationEvent>& events = {});

    // 移除观察者
    void removeObserver(uint64_t id);

    // 移除观察者（按名称）
    void removeObserver(const std::string& name);

    // 发布事件
    void publish(const Observation& event);

    // 获取 Actor 信息
    ActorInfo getActorInfo(const std::string& name) const;

    // 获取所有 Actor 信息
    std::vector<ActorInfo> getAllActorInfo() const;

    // 获取统计信息
    struct ManagerStats {
        size_t totalActors = 0;
        size_t runningActors = 0;
        size_t suspendedActors = 0;
        size_t stoppedActors = 0;
        uint64_t totalMessages = 0;
        uint64_t processedMessages = 0;
        uint64_t droppedMessages = 0;

        // 线程池状态
        size_t activeDispatcherThreads = 0;
        size_t activeIoThreads = 0;
        size_t activeBackgroundThreads = 0;

        // 队列状态
        size_t dispatcherQueueSize = 0;
        size_t ioQueueSize = 0;
        size_t backgroundQueueSize = 0;
    };

    ManagerStats getStats() const;

    //==========================================================================
    // 配置
    //==========================================================================

    const ThreadPoolConfig& getConfig() const { return config_; }
    void setConfig(const ThreadPoolConfig& config);

    //==========================================================================
    // 健康检查
    //==========================================================================

    struct HealthStatus {
        bool healthy = true;
        std::vector<std::string> unhealthyActors;
        std::vector<std::string> warnings;
    };

    HealthStatus checkHealth() const;

    // 检测死锁
    std::vector<std::string> detectDeadlock() const;

private:
    //==========================================================================
    // 内部方法
    //==========================================================================

    // 初始化线程池
    bool initDispatcher(ActorSystem& system);
    bool initIoPool();
    bool initBackgroundPool();

    // 启动工作线程
    void startIoWorkers();
    void startBackgroundWorkers();

    // 通知观察者
    void notifyObservers(const Observation& event);

    //==========================================================================
    // 成员变量
    //==========================================================================

    ThreadPoolConfig config_;
    std::atomic<bool> running_{false};

    // Actor 注册表
    std::unordered_map<std::string, std::shared_ptr<ActorCell>> actors_;
    mutable std::shared_mutex actorsMutex_;

    // Actor 标签索引
    std::unordered_map<std::string,           // tag key
        std::unordered_map<std::string,       // tag value
            std::unordered_set<std::string>>> // actor names
        tagIndex_;
    mutable std::shared_mutex tagIndexMutex_;

    // 调度器
    std::unique_ptr<MessageDispatcher> dispatcher_;

    // IO 线程池
    struct TaskQueue {
        std::queue<std::function<void()>> queue;
        std::mutex mutex;
        std::condition_variable cond;
        std::atomic<bool> running{true};
    };
    TaskQueue ioQueue_;
    std::vector<std::thread> ioWorkers_;

    // 后台线程池
    TaskQueue bgQueue_;
    std::vector<std::thread> bgWorkers_;

    // 观察者
    struct Observer {
        uint64_t id;
        std::string name;
        ObserverCallback callback;
        std::unordered_set<ObservationEvent> events;
    };
    std::unordered_map<uint64_t, Observer> observers_;
    std::unordered_map<std::string, uint64_t> observerNameMap_;
    mutable std::shared_mutex observersMutex_;
    std::atomic<uint64_t> nextObserverId_{1};

    // 统计信息
    mutable std::atomic<uint64_t> totalMessages_{0};
    mutable std::atomic<uint64_t> processedMessages_{0};
    mutable std::atomic<uint64_t> droppedMessages_{0};
};

//==============================================================================
// Actor 监控 API（外部访问）
//==============================================================================

class ActorMonitor {
public:
    explicit ActorMonitor(ActorManager& manager) : manager_(manager) {}

    // 获取 Actor 信息
    ActorInfo getActor(const std::string& name) const {
        return manager_.getActorInfo(name);
    }

    // 获取所有 Actor
    std::vector<ActorInfo> listActors() const {
        return manager_.getAllActorInfo();
    }

    // 获取统计信息
    ActorManager::ManagerStats stats() const {
        return manager_.getStats();
    }

    // 健康检查
    ActorManager::HealthStatus health() const {
        return manager_.checkHealth();
    }

    // 查找 Actor
    std::vector<std::string> findByTag(const std::string& key,
                                       const std::string& value) const {
        return manager_.findActorsByTag(key, value);
    }

    // 订阅事件
    uint64_t subscribe(const std::string& name, ObserverCallback callback) {
        return manager_.addObserver(name, std::move(callback));
    }

    // 取消订阅
    void unsubscribe(uint64_t id) {
        manager_.removeObserver(id);
    }

    // 导出为 JSON（用于 HTTP API）
    std::string exportStatsAsJson() const;
    std::string exportActorInfoAsJson(const std::string& name) const;
    std::string exportAllActorsAsJson() const;

private:
    ActorManager& manager_;
};

} // namespace actor
} // namespace apollo
