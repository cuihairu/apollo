/**
 * @file timer_service.cpp
 * @brief 定时器服务实现
 */

#include "apollo/actor/timer_service.h"
#include "apollo/actor/actor_system.h"
#include "apollo/actor/actor_utils.h"
#include <algorithm>

namespace apollo {
namespace actor {

//==============================================================================
// TimerService 实现
//==============================================================================

TimerService::TimerService(const TimerServiceConfig& config)
    : config_(config) {
}

TimerService::~TimerService() {
    stop();
}

bool TimerService::start() {
    if (running_.exchange(true)) {
        return true;
    }

    // 确定线程数
    size_t numThreads = config_.workerThreads;
    if (numThreads == 0) {
        numThreads = 1;
    }

    // 启动工作线程
    workerThreads_.reserve(numThreads);
    for (size_t i = 0; i < numThreads; ++i) {
        workerThreads_.emplace_back([this]() {
            workerLoop();
        });
    }

    return true;
}

void TimerService::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    // 唤醒所有等待的线程
    queueCond_.notify_all();

    // 等待线程结束
    for (auto& thread : workerThreads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    workerThreads_.clear();

    // 清空队列
    {
        std::unique_lock lock(queueMutex_);
        while (!taskQueue_.empty()) {
            taskQueue_.pop();
        }
    }

    {
        std::unique_lock lock(indexMutex_);
        taskIndex_.clear();
    }

    {
        std::unique_lock lock(actorTimersMutex_);
        actorTimers_.clear();
    }
}

uint64_t TimerService::scheduleOnce(int64_t delayMs,
                                    std::function<void()> callback) {
    if (!running_.load()) {
        return 0;
    }

    int64_t now = currentTimeMs();
    int64_t executeTime = now + delayMs;

    TimerTask task;
    task.id = generateId();
    task.executeTime = executeTime;
    task.interval = 0;  // 单次
    task.callback = std::move(callback);

    return addTask(std::move(task));
}

uint64_t TimerService::scheduleOnce(int64_t delayMs,
                                    const std::string& targetActor,
                                    const Message& message) {
    if (!running_.load()) {
        return 0;
    }

    return scheduleOnce(delayMs, [this, targetActor, message]() {
        // 查找目标 Actor 并发送消息
        // TODO: 需要 ActorSystem 引用
        // auto cell = system_->findLocal(targetActor);
        // if (cell) {
        //     cell->tell(message, ActorRef{});
        // }
    });
}

uint64_t TimerService::scheduleRepeated(int64_t intervalMs,
                                       std::function<void()> callback) {
    if (!running_.load()) {
        return 0;
    }

    if (intervalMs <= 0) {
        return 0;
    }

    int64_t now = currentTimeMs();
    int64_t executeTime = now + intervalMs;

    TimerTask task;
    task.id = generateId();
    task.executeTime = executeTime;
    task.interval = intervalMs;
    task.callback = std::move(callback);

    return addTask(std::move(task));
}

uint64_t TimerService::scheduleRepeated(int64_t intervalMs,
                                       const std::string& targetActor,
                                       const Message& message) {
    if (!running_.load()) {
        return 0;
    }

    return scheduleRepeated(intervalMs, [this, targetActor, message]() {
        // 发送消息到目标 Actor
        // TODO: 需要 ActorSystem 引用
    });
}

uint64_t TimerService::scheduleAtFixedRate(int64_t intervalMs,
                                          std::function<void()> callback) {
    // 固定速率和重复定时器的区别在于：
    // - scheduleRepeated: 在上次执行完成后等待 intervalMs
    // - scheduleAtFixedRate: 不管上次执行多久，每 intervalMs 触发一次

    if (!running_.load()) {
        return 0;
    }

    if (intervalMs <= 0) {
        return 0;
    }

    int64_t now = currentTimeMs();
    int64_t executeTime = now + intervalMs;

    TimerTask task;
    task.id = generateId();
    task.executeTime = executeTime;
    task.interval = intervalMs;
    task.callback = [this, intervalMs, cb = std::move(callback)]() mutable {
        // 执行回调
        cb();

        // 立即安排下一次（不管执行时间）
        return scheduleAtFixedRate(intervalMs, std::move(cb));
    };

    return addTask(std::move(task));
}

bool TimerService::cancel(uint64_t timerId) {
    if (timerId == 0) {
        return false;
    }

    // 从索引中移除
    {
        std::unique_lock lock(indexMutex_);
        auto it = taskIndex_.find(timerId);
        if (it != taskIndex_.end()) {
            it->second.cancelled = true;

            // 从 Actor 映射中移除
            if (!it->second.targetActor.empty()) {
                std::unique_lock lock2(actorTimersMutex_);
                auto actorIt = actorTimers_.find(it->second.targetActor);
                if (actorIt != actorTimers_.end()) {
                    actorIt->second.erase(timerId);
                }
            }

            taskIndex_.erase(it);
            totalCancelled_.fetch_add(1);
            return true;
        }
    }

    return false;
}

size_t TimerService::cancelAllForActor(const std::string& actorName) {
    size_t count = 0;

    std::unique_lock lock(actorTimersMutex_);
    auto it = actorTimers_.find(actorName);
    if (it != actorTimers_.end()) {
        for (uint64_t timerId : it->second) {
            cancel(timerId);
            ++count;
        }
        actorTimers_.erase(it);
    }

    return count;
}

TimerStats TimerService::getStats() const {
    TimerStats stats;
    stats.activeTimers = activeTimers_.load();
    stats.totalTimersCreated = totalCreated_.load();
    stats.totalTimersFired = totalFired_.load();
    stats.totalTimersCancelled = totalCancelled_.load();
    stats.totalExecutedCallbacks = totalExecuted_.load();
    stats.totalCallbackErrors = totalErrors_.load();
    return stats;
}

size_t TimerService::getActiveCount() const {
    return activeTimers_.load();
}

uint64_t TimerService::scheduleAt(int64_t timestampMs,
                                 std::function<void()> callback) {
    if (!running_.load()) {
        return 0;
    }

    int64_t now = currentTimeMs();
    if (timestampMs <= now) {
        // 已经过期，立即执行
        callback();
        return 0;
    }

    int64_t delayMs = timestampMs - now;

    TimerTask task;
    task.id = generateId();
    task.executeTime = timestampMs;
    task.interval = 0;
    task.callback = std::move(callback);

    return addTask(std::move(task));
}

uint64_t TimerService::countdown(int64_t durationMs,
                                std::function<void(int64_t)> tickCallback,
                                std::function<void()> completeCallback) {
    if (!running_.load()) {
        return 0;
    }

    int64_t startTime = currentTimeMs();
    int64_t endTime = startTime + durationMs;

    // 创建倒计时定时器
    struct CountdownState {
        int64_t startTime;
        int64_t endTime;
        int64_t duration;
        std::function<void(int64_t)> tick;
        std::function<void()> complete;
        uint64_t timerId;
    };

    auto state = std::make_shared<CountdownState>();
    state->startTime = startTime;
    state->endTime = endTime;
    state->duration = durationMs;
    state->tick = std::move(tickCallback);
    state->complete = std::move(completeCallback);

    // 创建重复定时器，每秒触发一次
    uint64_t timerId = scheduleRepeated(1000,
        [this, state]() {
            int64_t now = currentTimeMs();
            int64_t remaining = state->endTime - now;

            if (remaining <= 0) {
                // 倒计时结束
                if (state->complete) {
                    state->complete();
                }
                cancel(state->timerId);
            } else {
                // 触发 tick
                if (state->tick) {
                    state->tick(remaining);
                }
            }
        });

    if (state) {
        state->timerId = timerId;
    }

    return timerId;
}

void TimerService::workerLoop() {
    while (running_.load()) {
        std::unique_lock lock(queueMutex_);

        // 计算等待时间
        int64_t waitMs = config_.precisionMs;
        if (!taskQueue_.empty()) {
            int64_t now = currentTimeMs();
            int64_t nextExecuteTime = taskQueue_.top().executeTime;
            waitMs = std::max<int64_t>(0, nextExecuteTime - now);
            waitMs = std::min(waitMs, config_.precisionMs);
        }

        // 等待任务到期或被通知
        queueCond_.wait_for(lock, std::chrono::milliseconds(waitMs),
            [this]() {
                return !running_.load() ||
                       (!taskQueue_.empty() &&
                        taskQueue_.top().executeTime <= currentTimeMs());
            });

        if (!running_.load()) {
            break;
        }

        // 处理到期的定时器
        processExpiredTimers();
    }
}

void TimerService::processExpiredTimers() {
    int64_t now = currentTimeMs();

    while (!taskQueue_.empty() && running_.load()) {
        std::unique_lock lock(queueMutex_);

        if (taskQueue_.empty()) {
            break;
        }

        TimerTask task = taskQueue_.top();

        // 检查是否到期
        if (task.executeTime > now) {
            break;  // 还没到期
        }

        // 从队列中移除
        taskQueue_.pop();

        // 从索引中检查是否被取消
        {
            std::shared_lock indexLock(indexMutex_);
            auto it = taskIndex_.find(task.id);
            if (it == taskIndex_.end() || it->second.cancelled) {
                continue;  // 已取消
            }
            // 从索引中移除
            const_cast<std::unordered_map<uint64_t, TimerTask>&>(
                taskIndex_).erase(it);
        }

        activeTimers_.fetch_sub(1);
        totalFired_.fetch_add(1);

        // 释放锁后执行
        lock.unlock();

        // 执行任务
        executeTask(task);

        lock.lock();
    }
}

void TimerService::executeTask(TimerTask& task) {
    if (task.cancelled || !task.callback) {
        return;
    }

    try {
        task.callback();
        totalExecuted_.fetch_add(1);
    } catch (const std::exception& e) {
        totalErrors_.fetch_add(1);
        // TODO: 记录错误
    } catch (...) {
        totalErrors_.fetch_add(1);
    }

    // 如果是重复定时器，重新安排
    if (task.interval > 0 && !task.cancelled && running_.load()) {
        int64_t now = currentTimeMs();
        task.executeTime = now + task.interval;

        addTask(task);
    }
}

uint64_t TimerService::generateId() {
    return nextId_.fetch_add(1);
}

uint64_t TimerService::addTask(TimerTask task) {
    if (task.id == 0) {
        task.id = generateId();
    }

    {
        std::unique_lock lock(queueMutex_);
        taskQueue_.push(std::move(task));
    }
    queueCond_.notify_one();

    {
        std::unique_lock lock(indexMutex_);
        taskIndex_[task.id] = task;
    }

    if (!task.targetActor.empty()) {
        std::unique_lock lock(actorTimersMutex_);
        actorTimers_[task.targetActor].insert(task.id);
    }

    activeTimers_.fetch_add(1);
    totalCreated_.fetch_add(1);

    return task.id;
}

//==============================================================================
// ActorScheduler 实现
//==============================================================================

ActorScheduler::ActorScheduler(const ActorPath& actorPath,
                             TimerService& timerService,
                             ActorSystem& system)
    : actorPath_(actorPath)
    , timerService_(timerService)
    , system_(system) {
}

uint64_t ActorScheduler::scheduleOnce(int64_t delayMs,
                                     std::function<void()> callback) {
    uint64_t timerId = timerService_.scheduleOnce(delayMs, std::move(callback));

    std::unique_lock lock(mutex_);
    timerIds_.insert(timerId);

    return timerId;
}

uint64_t ActorScheduler::scheduleRepeated(int64_t intervalMs,
                                         std::function<void()> callback) {
    uint64_t timerId = timerService_.scheduleRepeated(intervalMs, std::move(callback));

    std::unique_lock lock(mutex_);
    timerIds_.insert(timerId);

    return timerId;
}

uint64_t ActorScheduler::tellLater(int64_t delayMs, const Message& msg) {
    return scheduleOnce(delayMs, [this, msg]() {
        // 发送消息给自己
        // TODO: 需要通过 ActorSystem 发送
    });
}

uint64_t ActorScheduler::tellRepeated(int64_t intervalMs, const Message& msg) {
    return scheduleRepeated(intervalMs, [this, msg]() {
        // 发送消息给自己
    });
}

bool ActorScheduler::cancel(uint64_t timerId) {
    bool success = timerService_.cancel(timerId);

    if (success) {
        std::unique_lock lock(mutex_);
        timerIds_.erase(timerId);
    }

    return success;
}

void ActorScheduler::cancelAll() {
    std::unordered_set<uint64_t> ids;

    {
        std::unique_lock lock(mutex_);
        ids = std::move(timerIds_);
        timerIds_.clear();
    }

    for (uint64_t id : ids) {
        timerService_.cancel(id);
    }
}

//==============================================================================
// 全局定时器服务实现
//==============================================================================

namespace timer {

static std::unique_ptr<TimerService> g_instance;
static std::once_flag g_initFlag;

TimerService& instance() {
    std::call_once(g_initFlag, []() {
        if (!g_instance) {
            g_instance = std::make_unique<TimerService>();
        }
    });
    return *g_instance;
}

bool init(const TimerServiceConfig& config) {
    std::call_once(g_initFlag, [&]() {
        g_instance = std::make_unique<TimerService>(config);
    });
    return g_instance->start();
}

void shutdown() {
    if (g_instance) {
        g_instance->stop();
    }
}

} // namespace timer

} // namespace actor
} // namespace apollo
