#pragma once

#include "apollo/actor/actor_ref.h"
#include "apollo/actor/actor_cell.h"
#include "apollo/actor/actor_system.h"
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>

namespace apollo {
namespace actor {

//==============================================================================
// 调度策略
//==============================================================================

enum class DispatchStrategy : uint8_t {
    RoundRobin,     // 轮询（默认）
    LeastLoaded,    // 最少负载
    Pinned,         // 固定线程
    Random          // 随机
};

//==============================================================================
// 调度任务
//==============================================================================

struct DispatchTask {
    std::shared_ptr<ActorCell> cell;
    uint32_t priority = 0;      // 优先级（数字越小优先级越高）
    uint64_t sequence = 0;      // 序列号

    DispatchTask() = default;

    DispatchTask(std::shared_ptr<ActorCell> c, uint32_t p = 0, uint64_t seq = 0)
        : cell(std::move(c)), priority(p), sequence(seq) {}

    // 优先级比较（用于优先队列）
    bool operator>(const DispatchTask& other) const {
        if (priority != other.priority) {
            return priority > other.priority;
        }
        return sequence > other.sequence;
    }
};

//==============================================================================
// 工作线程状态
//==============================================================================

enum class WorkerState : uint8_t {
    Idle,       // 空闲
    Running,    // 运行中
    Stopping    // 停止中
};

//==============================================================================
// 调度器配置
//==============================================================================

struct DispatcherConfig {
    size_t numThreads = 0;              // 线程数（0 = CPU 核心数）
    DispatchStrategy strategy = DispatchStrategy::RoundRobin;
    size_t maxQueueSize = 10000;        // 最大队列大小
    int64_t idleTimeoutMs = 100;        // 空闲超时
    bool enableWorkStealing = true;     // 启用工作窃取
    bool enablePriority = true;         // 启用优先级调度
};

//==============================================================================
// 消息调度器
//==============================================================================

class MessageDispatcher {
public:
    explicit MessageDispatcher(const DispatcherConfig& config = DispatcherConfig{});
    ~MessageDispatcher();

    //==========================================================================
    // 生命周期
    //==========================================================================

    bool start(ActorSystem& system);
    void stop();
    bool isRunning() const { return running_.load(); }

    //==========================================================================
    // 调度接口
    //==========================================================================

    // 调度 Actor（有待处理消息时调用）
    void schedule(const std::shared_ptr<ActorCell>& cell, uint32_t priority = 0);

    // 立即调度（高优先级）
    void scheduleImmediate(const std::shared_ptr<ActorCell>& cell);

    // 取消调度
    void unschedule(const std::string& actorName);

    //==========================================================================
    // 统计信息
    //==========================================================================

    struct Stats {
        size_t totalTasks = 0;
        size_t pendingTasks = 0;
        size_t processedTasks = 0;
        size_t idleThreads = 0;
        uint64_t totalProcessingTimeUs = 0;
    };

    Stats getStats() const;

private:
    //==========================================================================
    // 内部类型
    //==========================================================================

    class Worker {
    public:
        Worker(MessageDispatcher& dispatcher, size_t id);
        ~Worker();

        void start();
        void stop();
        void join();

        bool isIdle() const { return state_ == WorkerState::Idle; }
        size_t processedCount() const { return processedCount_; }

    private:
        void run();

        MessageDispatcher& dispatcher_;
        size_t id_;
        std::thread thread_;
        std::atomic<WorkerState> state_{WorkerState::Idle};
        std::atomic<size_t> processedCount_{0};
    };

    //==========================================================================
    // 内部方法
    //==========================================================================

    // 工作线程主循环
    void workerLoop(Worker& worker);

    // 处理单个任务
    bool processTask(DispatchTask& task);

    // 获取下一个任务（支持工作窃取）
    bool getNextTask(DispatchTask& task, size_t workerId);

    // 选择工作线程
    size_t selectWorker(const std::string& actorName);

    //==========================================================================
    // 成员变量
    //==========================================================================

    DispatcherConfig config_;
    ActorSystem* system_ = nullptr;
    std::atomic<bool> running_{false};

    // 任务队列（优先级队列）
    std::priority_queue<DispatchTask, std::vector<DispatchTask>,
                       std::greater<DispatchTask>> taskQueue_;
    mutable std::mutex queueMutex_;
    std::condition_variable queueCond_;

    // 任务序列号
    std::atomic<uint64_t> nextSequence_{0};

    // Actor -> Worker 映射（用于 Pinned 策略）
    std::unordered_map<std::string, size_t> actorWorkerMap_;
    mutable std::shared_mutex actorWorkerMutex_;

    // 工作线程
    std::vector<std::unique_ptr<Worker>> workers_;

    // 统计信息
    mutable std::atomic<size_t> totalTasks_{0};
    mutable std::atomic<size_t> processedTasks_{0};

    // 轮询索引
    std::atomic<size_t> roundRobinIndex_{0};
};

//==============================================================================
// 调度器辅助函数
//==============================================================================

namespace detail {

// 获取当前线程的 Worker ID
thread_local size_t t_currentWorkerId = SIZE_MAX;

inline size_t getCurrentWorkerId() {
    return t_currentWorkerId;
}

inline void setCurrentWorkerId(size_t id) {
    t_currentWorkerId = id;
}

} // namespace detail

//==============================================================================
// 调度器工厂
//==============================================================================

class DispatcherFactory {
public:
    static std::unique_ptr<MessageDispatcher> createDefault();
    static std::unique_ptr<MessageDispatcher> create(const DispatcherConfig& config);
};

} // namespace actor
} // namespace apollo
