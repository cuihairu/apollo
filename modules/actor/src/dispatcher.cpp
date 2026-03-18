/**
 * @file dispatcher.cpp
 * @brief 消息调度器实现
 */

#include "apollo/actor/dispatcher.h"
#include <random>
#include <algorithm>

namespace apollo {
namespace actor {

//==============================================================================
// MessageDispatcher::Worker 实现
//==============================================================================

MessageDispatcher::Worker::Worker(MessageDispatcher& dispatcher, size_t id)
    : dispatcher_(dispatcher), id_(id) {
}

MessageDispatcher::Worker::~Worker() {
    stop();
}

void MessageDispatcher::Worker::start() {
    if (thread_.joinable()) {
        return;
    }

    state_.store(WorkerState::Running);
    thread_ = std::thread([this]() {
        detail::setCurrentWorkerId(id_);
        dispatcher_.workerLoop(*this);
    });
}

void MessageDispatcher::Worker::stop() {
    state_.store(WorkerState::Stopping);
}

void MessageDispatcher::Worker::join() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

//==============================================================================
// MessageDispatcher 实现
//==============================================================================

MessageDispatcher::MessageDispatcher(const DispatcherConfig& config)
    : config_(config) {
}

MessageDispatcher::~MessageDispatcher() {
    stop();
}

bool MessageDispatcher::start(ActorSystem& system) {
    if (running_.exchange(true)) {
        return true;
    }

    system_ = &system;

    // 确定线程数
    size_t numThreads = config_.numThreads;
    if (numThreads == 0) {
        numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4;
    }

    // 创建工作线程
    workers_.reserve(numThreads);
    for (size_t i = 0; i < numThreads; ++i) {
        auto worker = std::make_unique<Worker>(*this, i);
        worker->start();
        workers_.push_back(std::move(worker));
    }

    return true;
}

void MessageDispatcher::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    // 停止所有工作线程
    for (auto& worker : workers_) {
        worker->stop();
    }

    // 唤醒所有等待的线程
    queueCond_.notify_all();

    // 等待线程结束
    for (auto& worker : workers_) {
        worker->join();
    }

    workers_.clear();
}

void MessageDispatcher::schedule(const std::shared_ptr<ActorCell>& cell,
                                  uint32_t priority) {
    if (!running_.load() || !cell) {
        return;
    }

    // 如果启用了优先级，使用优先级队列
    if (config_.enablePriority) {
        DispatchTask task(cell, priority, nextSequence_.fetch_add(1));

        {
            std::unique_lock lock(queueMutex_);

            if (config_.maxQueueSize > 0 &&
                taskQueue_.size() >= config_.maxQueueSize) {
                // 队列已满，丢弃任务（或可以阻塞）
                return;
            }

            taskQueue_.push(task);
            totalTasks_.fetch_add(1);
        }

        queueCond_.notify_one();
    } else {
        // 不使用优先级，直接处理
        scheduleImmediate(cell);
    }
}

void MessageDispatcher::scheduleImmediate(const std::shared_ptr<ActorCell>& cell) {
    if (!cell) {
        return;
    }

    // 如果当前在调度器线程中，立即处理
    size_t currentWorkerId = detail::getCurrentWorkerId();
    if (currentWorkerId < workers_.size()) {
        DispatchTask task(cell, 0, nextSequence_.fetch_add(1));
        processTask(task);
    } else {
        // 否则加入队列
        schedule(cell, 0);
    }
}

void MessageDispatcher::unschedule(const std::string& actorName) {
    // 移除 Actor 与 Worker 的绑定
    std::unique_lock lock(actorWorkerMutex_);
    actorWorkerMap_.erase(actorName);
}

MessageDispatcher::Stats MessageDispatcher::getStats() const {
    Stats stats;
    stats.totalTasks = totalTasks_.load();
    stats.processedTasks = processedTasks_.load();

    {
        std::unique_lock lock(queueMutex_);
        stats.pendingTasks = taskQueue_.size();
    }

    size_t idleCount = 0;
    for (const auto& worker : workers_) {
        if (worker->isIdle()) {
            ++idleCount;
        }
    }
    stats.idleThreads = idleCount;

    return stats;
}

void MessageDispatcher::workerLoop(Worker& worker) {
    while (worker.state_ != WorkerState::Stopping) {
        DispatchTask task;

        if (getNextTask(task, worker.id_)) {
            worker.state_.store(WorkerState::Running);

            auto startTime = std::chrono::steady_clock::now();
            bool success = processTask(task);
            auto endTime = std::chrono::steady_clock::now();

            if (success) {
                processedTasks_.fetch_add(1);
                worker.processedCount_.fetch_add(1);
            }

            worker.state_.store(WorkerState::Idle);
        } else {
            // 没有任务，等待
            std::unique_lock lock(queueMutex_);
            queueCond_.wait_for(lock, std::chrono::milliseconds(config_.idleTimeoutMs),
                [this]() {
                    return !taskQueue_.empty() || !running_.load();
                });
        }
    }
}

bool MessageDispatcher::processTask(DispatchTask& task) {
    if (!task.cell) {
        return false;
    }

    try {
        // 处理 Actor 的所有待处理消息
        task.cell->processMessages();
        return true;
    } catch (const std::exception& e) {
        // 记录异常
        // TODO: 日志
        return false;
    }
}

bool MessageDispatcher::getNextTask(DispatchTask& task, size_t workerId) {
    std::unique_lock lock(queueMutex_);

    if (taskQueue_.empty()) {
        return false;
    }

    // 根据策略选择任务
    switch (config_.strategy) {
        case DispatchStrategy::RoundRobin:
        case DispatchStrategy::Random:
        case DispatchStrategy::LeastLoaded:
            // 优先队列已按优先级排序，直接取顶部
            task = taskQueue_.top();
            taskQueue_.pop();
            return true;

        case DispatchStrategy::Pinned: {
            // 取出任务直到找到分配给当前 Worker 的
            std::vector<DispatchTask> temp;
            bool found = false;

            while (!taskQueue_.empty()) {
                task = taskQueue_.top();
                taskQueue_.pop();

                std::string actorName = task.cell->path().name;
                size_t assignedWorker = 0;

                {
                    std::shared_lock rlock(actorWorkerMutex_);
                    auto it = actorWorkerMap_.find(actorName);
                    if (it != actorWorkerMap_.end()) {
                        assignedWorker = it->second;
                    }
                }

                if (assignedWorker == workerId) {
                    found = true;
                    // 将临时任务放回队列
                    while (!temp.empty()) {
                        taskQueue_.push(temp.back());
                        temp.pop_back();
                    }
                    break;
                }

                temp.push_back(task);
            }

            if (!found) {
                // 放回临时任务
                for (auto& t : temp) {
                    taskQueue_.push(t);
                }
                return false;
            }

            return true;
        }

        default:
            return false;
    }
}

size_t MessageDispatcher::selectWorker(const std::string& actorName) {
    size_t numWorkers = workers_.size();
    if (numWorkers == 0) {
        return 0;
    }

    switch (config_.strategy) {
        case DispatchStrategy::RoundRobin: {
            size_t index = roundRobinIndex_.fetch_add(1) % numWorkers;
            return index;
        }

        case DispatchStrategy::LeastLoaded: {
            size_t minIndex = 0;
            size_t minCount = workers_[0]->processedCount();

            for (size_t i = 1; i < numWorkers; ++i) {
                size_t count = workers_[i]->processedCount();
                if (count < minCount) {
                    minCount = count;
                    minIndex = i;
                }
            }
            return minIndex;
        }

        case DispatchStrategy::Random: {
            static thread_local std::random_device rd;
            static thread_local std::mt19937 gen(rd());
            std::uniform_int_distribution<size_t> dist(0, numWorkers - 1);
            return dist(gen);
        }

        case DispatchStrategy::Pinned: {
            // 使用哈希固定分配
            std::hash<std::string> hasher;
            size_t hash = hasher(actorName);
            return hash % numWorkers;
        }

        default:
            return 0;
    }
}

//==============================================================================
// DispatcherFactory 实现
//==============================================================================

std::unique_ptr<MessageDispatcher> DispatcherFactory::createDefault() {
    DispatcherConfig config;
    return create(config);
}

std::unique_ptr<MessageDispatcher> DispatcherFactory::create(
    const DispatcherConfig& config) {

    return std::make_unique<MessageDispatcher>(config);
}

} // namespace actor
} // namespace apollo
