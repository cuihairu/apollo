/**
 * @file timing_wheel.h
 * @brief 分层时间轮实现 (Timing Wheel)
 *
 * 时间轮是一种高效的定时器数据结构，特别适合管理大量超时事件。
 * 相比传统优先队列，时间轮的插入和删除操作都是 O(1) 时间复杂度。
 *
 * 适用场景：
 * - 技能冷却管理
 * - Buff/Debuff 效果管理
 * - 网络连接超时
 * - 大量短时间定时器
 *
 * 参考：
 * - "Hashed and Hierarchical Timing Wheels" by Varghese and Lauck
 * - Linux Kernel 的定时器实现
 * - Kubernetes 的 heap 实现借鉴
 */

#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include <list>
#include <array>
#include <memory>
#include <atomic>
#include <mutex>
#include <chrono>

namespace apollo {
namespace utils {

//==============================================================================
// 时间轮配置
//==============================================================================

/**
 * @brief 时间轮配置
 */
struct TimingWheelConfig {
    int64_t tickMs = 10;           // 时间轮滴答间隔（毫秒）
    size_t wheelSize = 512;         // 时间轮槽数量
    size_t maxWheels = 5;           // 最大分层层数
    bool enableStats = true;        // 启用统计
};

//==============================================================================
// 定时任务接口
//==============================================================================

/**
 * @brief 定时任务接口
 */
class ITimerTask {
public:
    virtual ~ITimerTask() = default;

    /**
     * @brief 执行任务
     */
    virtual void run() = 0;

    /**
     * @brief 获取过期时间（毫秒）
     */
    virtual int64_t getExpiryMs() const = 0;

    /**
     * @brief 任务是否已取消
     */
    virtual bool isCancelled() const = 0;

    /**
     * @brief 取消任务
     */
    virtual void cancel() = 0;
};

//==============================================================================
// 通用定时任务实现
//==============================================================================

/**
 * @brief 通用定时任务
 */
class TimerTask : public ITimerTask {
public:
    using Callback = std::function<void()>;

    TimerTask(int64_t expiryMs, Callback callback)
        : expiryMs_(expiryMs)
        , callback_(std::move(callback))
        , cancelled_(false) {
    }

    void run() override {
        if (!cancelled_ && callback_) {
            callback_();
        }
    }

    int64_t getExpiryMs() const override {
        return expiryMs_;
    }

    bool isCancelled() const override {
        return cancelled_.load(std::memory_order_acquire);
    }

    void cancel() override {
        cancelled_.store(true, std::memory_order_release);
    }

    int64_t expiryMs_;
    Callback callback_;
    std::atomic<bool> cancelled_;
};

//==============================================================================
// 时间槽
//==============================================================================

/**
 * @brief 时间槽（存储同一时间点到期的任务）
 */
using TimerSlot = std::list<std::shared_ptr<ITimerTask>>;

//==============================================================================
// 分层时间轮实现
//==============================================================================

/**
 * @brief 分层时间轮
 *
 * 时间轮结构：
 * - 每层有固定数量的槽
 * - 低层的槽满了会溢出到高层
 * - 指针移动时将高层任务降级到低层
 *
 * 时间复杂度：
 * - 插入：O(1)
 * - 删除：O(1)（需要持有任务引用）
 * - 推进：O(1) 平均
 */
class TimingWheel {
public:
    /**
     * @brief 构造函数
     * @param config 时间轮配置
     */
    explicit TimingWheel(const TimingWheelConfig& config = TimingWheelConfig{});

    ~TimingWheel() = default;

    /**
     * @brief 添加定时任务
     * @param delayMs 延迟时间（毫秒）
     * @param task 任务
     * @return 任务 ID（用于取消）
     */
    uint64_t add(int64_t delayMs, std::shared_ptr<ITimerTask> task);

    /**
     * @brief 添加回调任务
     * @param delayMs 延迟时间（毫秒）
     * @param callback 回调函数
     * @return 任务 ID
     */
    uint64_t add(int64_t delayMs, std::function<void()> callback);

    /**
     * @brief 取消任务
     * @param taskId 任务 ID
     * @return 是否成功
     */
    bool cancel(uint64_t taskId);

    /**
     * @brief 推进时间（通常由定时器线程调用）
     * @param currentMs 当前时间（毫秒）
     * @return 到期任务列表
     */
    std::vector<std::shared_ptr<ITimerTask>> tick(int64_t currentMs);

    /**
     * @brief 获取下一个到期时间
     * @return 毫秒时间戳，0 表示没有任务
     */
    int64_t getNextExpiry() const;

    /**
     * @brief 获取任务数量
     */
    size_t size() const { return taskCount_.load(std::memory_order_relaxed); }

    /**
     * @brief 是否为空
     */
    bool empty() const { return size() == 0; }

    /**
     * @brief 统计信息
     */
    struct Stats {
        size_t totalTasks = 0;
        size_t activeTasks = 0;
        size_t expiredTasks = 0;
        size_t cancelledTasks = 0;
        size_t cascadeCount = 0;       // 层级下降次数
        size_t currentWheelSize = 0;   // 当前使用的最大层级
    };

    Stats getStats() const;

private:
    /**
     * @brief 单层时间轮
     */
    class Wheel {
    public:
        explicit Wheel(size_t size, int64_t intervalMs)
            : slots_(size)
            , size_(size)
            , intervalMs_(intervalMs)
            , currentSlot_(0) {
        }

        size_t size() const { return size_; }
        int64_t intervalMs() const { return intervalMs_; }
        size_t currentSlot() const { return currentSlot_; }

        TimerSlot& getSlot(size_t index) {
            return slots_[index % size_];
        }

        const TimerSlot& getSlot(size_t index) const {
            return slots_[index % size_];
        }

        void advance() {
            currentSlot_ = (currentSlot_ + 1) % size_;
        }

        void clearSlot(size_t index) {
            slots_[index % size_].clear();
        }

    private:
        std::vector<TimerSlot> slots_;
        size_t size_;
        int64_t intervalMs_;  // 每个槽代表的时间跨度
        size_t currentSlot_;
    };

    /**
     * @brief 任务元数据（用于跟踪和取消）
     */
    struct TaskMeta {
        uint64_t id;
        std::shared_ptr<ITimerTask> task;
        size_t wheelIndex;
        size_t slotIndex;
    };

    void cascadeTasks(size_t wheelIndex);
    size_t calculateWheel(int64_t delayMs) const;
    size_t calculateSlot(size_t wheelIndex, int64_t delayMs) const;
    uint64_t generateTaskId();

    TimingWheelConfig config_;
    std::vector<std::unique_ptr<Wheel>> wheels_;

    std::atomic<uint64_t> nextTaskId_{1};
    std::atomic<size_t> taskCount_{0};

    mutable std::mutex tasksMutex_;
    std::unordered_map<uint64_t, TaskMeta> taskMetas_;

    mutable std::mutex statsMutex_;
    Stats stats_;

    int64_t lastTickMs_ = 0;
};

//==============================================================================
// 内联实现
//==============================================================================

inline TimingWheel::TimingWheel(const TimingWheelConfig& config)
    : config_(config) {

    // 创建分层时间轮
    // 第 i 层的时间跨度 = tickMs * wheelSize ^ i
    int64_t intervalMs = config_.tickMs;
    for (size_t i = 0; i < config_.maxWheels; ++i) {
        wheels_.push_back(std::make_unique<Wheel>(config_.wheelSize, intervalMs));
        intervalMs *= config_.wheelSize;

        // 防止溢出
        if (intervalMs < 0) {
            break;
        }
    }

    lastTickMs_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

inline uint64_t TimingWheel::add(int64_t delayMs, std::shared_ptr<ITimerTask> task) {
    if (delayMs <= 0 || !task) {
        return 0;
    }

    uint64_t taskId = generateTaskId();

    std::lock_guard lock(tasksMutex_);

    size_t wheelIndex = calculateWheel(delayMs);
    if (wheelIndex >= wheels_.size()) {
        // 延迟时间太长，放到最大的轮中
        wheelIndex = wheels_.size() - 1;
    }

    size_t slotIndex = calculateSlot(wheelIndex, delayMs);

    // 添加到对应槽
    wheels_[wheelIndex]->getSlot(slotIndex).push_back(task);

    // 记录元数据
    TaskMeta meta;
    meta.id = taskId;
    meta.task = task;
    meta.wheelIndex = wheelIndex;
    meta.slotIndex = slotIndex;
    taskMetas_[taskId] = meta;

    taskCount_.fetch_add(1, std::memory_order_relaxed);

    if (config_.enableStats) {
        std::lock_guard statsLock(statsMutex_);
        stats_.totalTasks++;
        stats_.activeTasks = taskCount_.load(std::memory_order_relaxed);
        if (wheelIndex + 1 > stats_.currentWheelSize) {
            stats_.currentWheelSize = wheelIndex + 1;
        }
    }

    return taskId;
}

inline uint64_t TimingWheel::add(int64_t delayMs, std::function<void()> callback) {
    auto task = std::make_shared<TimerTask>(
        getCurrentTimeMs() + delayMs,
        std::move(callback)
    );
    return add(delayMs, task);
}

inline bool TimingWheel::cancel(uint64_t taskId) {
    std::lock_guard lock(tasksMutex_);

    auto it = taskMetas_.find(taskId);
    if (it == taskMetas_.end()) {
        return false;
    }

    it->second.task->cancel();
    taskMetas_.erase(it);
    taskCount_.fetch_sub(1, std::memory_order_relaxed);

    if (config_.enableStats) {
        std::lock_guard statsLock(statsMutex_);
        stats_.cancelledTasks++;
        stats_.activeTasks = taskCount_.load(std::memory_order_relaxed);
    }

    return true;
}

inline std::vector<std::shared_ptr<ITimerTask>> TimingWheel::tick(int64_t currentMs) {
    std::vector<std::shared_ptr<ITimerTask>> expiredTasks;

    // 计算经过的 tick 数
    int64_t elapsedTicks = (currentMs - lastTickMs_) / config_.tickMs;
    if (elapsedTicks <= 0) {
        return expiredTasks;
    }

    // 执行相应次数的 tick
    for (int64_t i = 0; i < elapsedTicks && !wheels_.empty(); ++i) {
        // 推进第一层
        wheels_[0]->advance();

        // 当第一层转完一圈时，处理层级下降
        if (wheels_[0]->currentSlot() == 0) {
            cascadeTasks(0);
        }

        // 收集到期任务
        TimerSlot& slot = wheels_[0]->getSlot(wheels_[0]->currentSlot());

        for (auto it = slot.begin(); it != slot.end(); ) {
            auto task = *it;

            if (!task->isCancelled()) {
                expiredTasks.push_back(task);

                if (config_.enableStats) {
                    std::lock_guard statsLock(statsMutex_);
                    stats_.expiredTasks++;
                }
            }

            // 清理元数据
            // 注意：这里需要更复杂的查找逻辑
            ++it;
        }

        slot.clear();
    }

    lastTickMs_ += elapsedTicks * config_.tickMs;

    return expiredTasks;
}

inline int64_t TimingWheel::getNextExpiry() const {
    // 简化实现：返回第一个非空槽的时间
    std::lock_guard lock(tasksMutex_);

    if (taskCount_.load(std::memory_order_relaxed) == 0) {
        return 0;
    }

    // 检查第一层
    for (size_t i = 0; i < wheels_[0]->size(); ++i) {
        size_t slotIdx = (wheels_[0]->currentSlot() + i) % wheels_[0]->size();
        if (!wheels_[0]->getSlot(slotIdx).empty()) {
            return lastTickMs_ + i * config_.tickMs;
        }
    }

    return lastTickMs_ + wheels_[0]->size() * config_.tickMs;
}

inline TimingWheel::Stats TimingWheel::getStats() const {
    std::lock_guard lock(statsMutex_);
    return stats_;
}

inline void TimingWheel::cascadeTasks(size_t wheelIndex) {
    if (wheelIndex + 1 >= wheels_.size()) {
        return;
    }

    size_t nextWheelIndex = wheelIndex + 1;
    wheels_[nextWheelIndex]->advance();

    // 当下一层也转完时，继续级联
    if (wheels_[nextWheelIndex]->currentSlot() == 0) {
        cascadeTasks(nextWheelIndex);
    }

    // 将下一层当前槽的任务移到当前层
    TimerSlot& sourceSlot = wheels_[nextWheelIndex]->getSlot(
        wheels_[nextWheelIndex]->currentSlot());

    for (auto& task : sourceSlot) {
        if (!task->isCancelled()) {
            // 重新计算该任务应该放在当前层的哪个槽
            int64_t remainingMs = task->getExpiryMs() - lastTickMs_;
            size_t targetSlot = (remainingMs / wheels_[wheelIndex]->intervalMs())
                                % wheels_[wheelIndex]->size();

            wheels_[wheelIndex]->getSlot(targetSlot).push_back(task);

            if (config_.enableStats) {
                std::lock_guard statsLock(statsMutex_);
                stats_.cascadeCount++;
            }
        }
    }

    sourceSlot.clear();
}

inline size_t TimingWheel::calculateWheel(int64_t delayMs) const {
    size_t wheel = 0;
    int64_t maxDelay = config_.tickMs * config_.wheelSize;

    while (delayMs > maxDelay && wheel < wheels_.size() - 1) {
        wheel++;
        maxDelay *= config_.wheelSize;
    }

    return wheel;
}

inline size_t TimingWheel::calculateSlot(size_t wheelIndex, int64_t delayMs) const {
    // 计算在该层的槽索引
    int64_t baseInterval = config_.tickMs;
    for (size_t i = 0; i < wheelIndex; ++i) {
        baseInterval *= config_.wheelSize;
    }

    return static_cast<size_t>((delayMs / baseInterval) % config_.wheelSize);
}

inline uint64_t TimingWheel::generateTaskId() {
    return nextTaskId_.fetch_add(1, std::memory_order_relaxed);
}

//==============================================================================
// 辅助函数
//==============================================================================

namespace {
    inline int64_t getCurrentTimeMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }
}

} // namespace utils
} // namespace apollo
