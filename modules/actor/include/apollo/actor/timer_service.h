#pragma once

#include "apollo/actor/actor_ref.h"
#include "apollo/actor/actor.h"
#include <chrono>
#include <functional>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>
#include <cstdint>

namespace apollo {
namespace actor {

//==============================================================================
// 定时器任务
//==============================================================================

class TimerTask {
public:
    uint64_t id;                          // 定时器 ID
    int64_t executeTime;                  // 执行时间戳（毫秒）
    int64_t interval;                     // 重复间隔（毫秒，0 表示一次性）
    std::string targetActor;              // 目标 Actor
    std::function<void()> callback;       // 回调函数
    bool cancelled = false;               // 是否已取消

    TimerTask() : id(0), executeTime(0), interval(0) {}

    TimerTask(uint64_t i, int64_t exec, int64_t iv,
              const std::string& target, std::function<void()> cb)
        : id(i), executeTime(exec), interval(iv),
          targetActor(target), callback(std::move(cb)) {}

    // 优先级比较（用于优先队列，执行时间越小优先级越高）
    bool operator>(const TimerTask& other) const {
        return executeTime > other.executeTime;
    }
};

//==============================================================================
// 定时器配置
//==============================================================================

struct TimerServiceConfig {
    size_t workerThreads = 1;             // 定时器线程数
    int64_t precisionMs = 10;             // 定时精度（毫秒）
    size_t maxTimers = 10000;             // 最大定时器数量
    bool enableMetrics = true;            // 启用指标收集
};

//==============================================================================
// 定时器统计
//==============================================================================

struct TimerStats {
    uint64_t activeTimers = 0;            // 活跃定时器
    uint64_t totalTimersCreated = 0;      // 总创建数
    uint64_t totalTimersFired = 0;        // 总触发数
    uint64_t totalTimersCancelled = 0;    // 总取消数
    uint64_t totalExecutedCallbacks = 0;  // 总执行回调数
    uint64_t totalCallbackErrors = 0;     // 回调错误数
};

//==============================================================================
// 定时器服务
//==============================================================================

class TimerService {
public:
    explicit TimerService(const TimerServiceConfig& config = TimerServiceConfig{});
    ~TimerService();

    //==========================================================================
    // 生命周期
    //==========================================================================

    bool start();
    void stop();
    bool isRunning() const { return running_.load(); }

    //==========================================================================
    // 定时器操作
    //==========================================================================

    // 单次定时器
    uint64_t scheduleOnce(int64_t delayMs, std::function<void()> callback);

    // 单次定时器（发送消息到 Actor）
    uint64_t scheduleOnce(int64_t delayMs,
                         const std::string& targetActor,
                         const Message& message);

    // 重复定时器
    uint64_t scheduleRepeated(int64_t intervalMs, std::function<void()> callback);

    // 重复定时器（发送消息到 Actor）
    uint64_t scheduleRepeated(int64_t intervalMs,
                              const std::string& targetActor,
                              const Message& message);

    // 固定速率定时器（不管执行时间，固定间隔触发）
    uint64_t scheduleAtFixedRate(int64_t intervalMs, std::function<void()> callback);

    // 取消定时器
    bool cancel(uint64_t timerId);

    // 取消 Actor 的所有定时器
    size_t cancelAllForActor(const std::string& actorName);

    //==========================================================================
    // 查询
    //==========================================================================

    // 获取定时器统计
    TimerStats getStats() const;

    // 获取活跃定时器数量
    size_t getActiveCount() const;

    //==========================================================================
    // 高级功能
    //==========================================================================

    // 延迟执行（使用 std::duration）
    template<typename Rep, typename Period>
    uint64_t scheduleAfter(std::chrono::duration<Rep, Period> delay,
                          std::function<void()> callback) {
        auto delayMs = std::chrono::duration_cast<std::chrono::milliseconds>(delay).count();
        return scheduleOnce(delayMs, std::move(callback));
    }

    // 在指定时间点执行
    uint64_t scheduleAt(int64_t timestampMs, std::function<void()> callback);

    // 倒计时
    uint64_t countdown(int64_t durationMs,
                      std::function<void(int64_t remaining)> tickCallback,
                      std::function<void()> completeCallback);

private:
    //==========================================================================
    // 内部方法
    //==========================================================================

    // 工作线程主循环
    void workerLoop();

    // 处理到期的定时器
    void processExpiredTimers();

    // 执行定时器任务
    void executeTask(TimerTask& task);

    // 生成定时器 ID
    uint64_t generateId();

    // 添加定时器任务
    uint64_t addTask(TimerTask task);

    //==========================================================================
    // 成员变量
    //==========================================================================

    TimerServiceConfig config_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> nextId_{1};

    // 定时器任务队列（优先队列）
    std::priority_queue<TimerTask, std::vector<TimerTask>, std::greater<TimerTask>> taskQueue_;
    mutable std::mutex queueMutex_;
    std::condition_variable queueCond_;

    // 定时器索引（用于快速查找和取消）
    std::unordered_map<uint64_t, TimerTask> taskIndex_;
    mutable std::shared_mutex indexMutex_;

    // Actor -> 定时器 ID 映射（用于批量取消）
    std::unordered_map<std::string, std::unordered_set<uint64_t>> actorTimers_;
    mutable std::shared_mutex actorTimersMutex_;

    // 工作线程
    std::vector<std::thread> workerThreads_;

    // 统计信息
    mutable std::atomic<uint64_t> activeTimers_{0};
    mutable std::atomic<uint64_t> totalCreated_{0};
    mutable std::atomic<uint64_t> totalFired_{0};
    mutable std::atomic<uint64_t> totalCancelled_{0};
    mutable std::atomic<uint64_t> totalExecuted_{0};
    mutable std::atomic<uint64_t> totalErrors_{0};
};

//==============================================================================
// 定时器消息（内建）
//==============================================================================

namespace Messages {

// 定时器触发消息
struct TimerTick {
    uint64_t timerId;
    int64_t scheduledTime;     // 预定执行时间
    int64_t actualTime;        // 实际执行时间

    static const char* typeName() { return "TimerTick"; }
};

// 定时器完成消息（倒计时结束）
struct TimerComplete {
    uint64_t timerId;

    static const char* typeName() { return "TimerComplete"; }
};

} // namespace Messages

//==============================================================================
// Actor 定时器辅助类（简化 Actor 中的定时器使用）
//==============================================================================

class ActorScheduler {
public:
    explicit ActorScheduler(const ActorPath& actorPath,
                           TimerService& timerService,
                           ActorSystem& system);

    // 单次定时器
    uint64_t scheduleOnce(int64_t delayMs, std::function<void()> callback);

    // 重复定时器
    uint64_t scheduleRepeated(int64_t intervalMs, std::function<void()> callback);

    // 延迟发送消息给自己
    uint64_t tellLater(int64_t delayMs, const Message& msg);

    // 重复发送消息给自己
    uint64_t tellRepeated(int64_t intervalMs, const Message& msg);

    // 取消定时器
    bool cancel(uint64_t timerId);

    // 取消所有定时器
    void cancelAll();

private:
    ActorPath actorPath_;
    TimerService& timerService_;
    ActorSystem& system_;
    std::unordered_set<uint64_t> timerIds_;
    std::mutex mutex_;
};

//==============================================================================
// 全局定时器服务（单例）
//==============================================================================

namespace timer {
    // 获取全局定时器服务
    TimerService& instance();

    // 初始化全局定时器服务
    bool init(const TimerServiceConfig& config = TimerServiceConfig{});

    // 关闭全局定时器服务
    void shutdown();

    // 便捷函数
    inline uint64_t scheduleOnce(int64_t delayMs, std::function<void()> callback) {
        return instance().scheduleOnce(delayMs, std::move(callback));
    }

    inline uint64_t scheduleRepeated(int64_t intervalMs, std::function<void()> callback) {
        return instance().scheduleRepeated(intervalMs, std::move(callback));
    }

    inline bool cancel(uint64_t timerId) {
        return instance().cancel(timerId);
    }
}

} // namespace actor
} // namespace apollo
