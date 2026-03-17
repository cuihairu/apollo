/**
 * @file actor_manager.cpp
 * @brief Actor 管理器实现
 */

#include "apollo/actor/actor_manager.h"
#include "apollo/actor/actor_system.h"
#include "apollo/actor/actor_utils.h"
#include <sstream>
#include <algorithm>

namespace apollo {
namespace actor {

//==============================================================================
// ActorManager 实现
//==============================================================================

ActorManager::ActorManager(const ThreadPoolConfig& config)
    : config_(config) {
}

ActorManager::~ActorManager() {
    stop();
}

bool ActorManager::start(ActorSystem& system) {
    if (running_.exchange(true)) {
        return true;
    }

    // 初始化调度器
    if (!initDispatcher(system)) {
        running_.store(false);
        return false;
    }

    // 初始化线程池
    if (!initIoPool()) {
        running_.store(false);
        return false;
    }

    if (!initBackgroundPool()) {
        running_.store(false);
        return false;
    }

    return true;
}

void ActorManager::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    // 停止调度器
    if (dispatcher_) {
        dispatcher_->stop();
    }

    // 停止 IO 线程池
    {
        std::unique_lock lock(ioQueue_.mutex);
        ioQueue_.running.store(false);
        ioQueue_.cond.notify_all();
    }

    // 停止后台线程池
    {
        std::unique_lock lock(bgQueue_.mutex);
        bgQueue_.running.store(false);
        bgQueue_.cond.notify_all();
    }

    // 等待线程结束
    for (auto& thread : ioWorkers_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    ioWorkers_.clear();

    for (auto& thread : bgWorkers_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    bgWorkers_.clear();

    // 停止所有 Actor
    std::vector<std::shared_ptr<ActorCell>> actorsToStop;
    {
        std::unique_lock lock(actorsMutex_);
        for (auto& [name, cell] : actors_) {
            actorsToStop.push_back(cell);
        }
        actors_.clear();
    }

    for (auto& cell : actorsToStop) {
        cell->stop();
    }
}

bool ActorManager::registerActor(const std::shared_ptr<ActorCell>& cell) {
    if (!cell) {
        return false;
    }

    const std::string& name = cell->path().name;

    {
        std::unique_lock lock(actorsMutex_);
        auto result = actors_.emplace(name, cell);
        if (!result.second) {
            return false;  // 名称冲突
        }
    }

    // 发布事件
    Observation obs;
    obs.event = ObservationEvent::ActorCreated;
    obs.actorPath = cell->path();
    obs.timestamp = currentTimeMs();
    publish(obs);

    return true;
}

bool ActorManager::unregisterActor(const std::string& name) {
    std::shared_ptr<ActorCell> cell;

    {
        std::unique_lock lock(actorsMutex_);
        auto it = actors_.find(name);
        if (it == actors_.end()) {
            return false;
        }
        cell = it->second;
        actors_.erase(it);
    }

    // 清除标签索引
    // TODO

    // 发布事件
    Observation obs;
    obs.event = ObservationEvent::ActorTerminated;
    obs.actorPath = cell->path();
    obs.timestamp = currentTimeMs();
    publish(obs);

    return true;
}

std::shared_ptr<ActorCell> ActorManager::getActor(const std::string& name) const {
    std::shared_lock lock(actorsMutex_);
    auto it = actors_.find(name);
    return it != actors_.end() ? it->second : nullptr;
}

std::vector<std::string> ActorManager::getAllActorNames() const {
    std::shared_lock lock(actorsMutex_);
    std::vector<std::string> names;
    names.reserve(actors_.size());
    for (const auto& [name, _] : actors_) {
        names.push_back(name);
    }
    return names;
}

std::vector<std::string> ActorManager::findActorsByPrefix(const std::string& prefix) const {
    std::shared_lock lock(actorsMutex_);
    std::vector<std::string> result;

    for (const auto& [name, _] : actors_) {
        if (name.size() >= prefix.size() &&
            name.compare(0, prefix.size(), prefix) == 0) {
            result.push_back(name);
        }
    }

    return result;
}

std::vector<std::string> ActorManager::findActorsByTag(const std::string& key,
                                                        const std::string& value) const {
    std::shared_lock lock(tagIndexMutex_);
    std::vector<std::string> result;

    auto keyIt = tagIndex_.find(key);
    if (keyIt != tagIndex_.end()) {
        auto valueIt = keyIt->second.find(value);
        if (valueIt != keyIt->second.end()) {
            result.assign(valueIt->second.begin(), valueIt->second.end());
        }
    }

    return result;
}

bool ActorManager::startActor(const std::string& name) {
    auto cell = getActor(name);
    if (!cell) {
        return false;
    }

    cell->start();

    Observation obs;
    obs.event = ObservationEvent::ActorStarted;
    obs.actorPath = cell->path();
    obs.timestamp = currentTimeMs();
    publish(obs);

    return true;
}

bool ActorManager::stopActor(const std::string& name) {
    auto cell = getActor(name);
    if (!cell) {
        return false;
    }

    cell->stop();

    Observation obs;
    obs.event = ObservationEvent::ActorStopped;
    obs.actorPath = cell->path();
    obs.timestamp = currentTimeMs();
    publish(obs);

    return true;
}

bool ActorManager::restartActor(const std::string& name) {
    // TODO: 实现 Actor 重启（需要保存工厂函数）
    return false;
}

bool ActorManager::suspendActor(const std::string& name) {
    auto cell = getActor(name);
    if (!cell) {
        return false;
    }
    cell->suspend();
    return true;
}

bool ActorManager::resumeActor(const std::string& name) {
    auto cell = getActor(name);
    if (!cell) {
        return false;
    }
    cell->resume();
    return true;
}

uint64_t ActorManager::addObserver(const std::string& name, ObserverCallback callback,
                                  const std::unordered_set<ObservationEvent>& events) {
    uint64_t id = nextObserverId_.fetch_add(1);

    Observer obs;
    obs.id = id;
    obs.name = name;
    obs.callback = std::move(callback);
    obs.events = events;

    {
        std::unique_lock lock(observersMutex_);
        observers_[id] = std::move(obs);
        observerNameMap_[name] = id;
    }

    return id;
}

void ActorManager::removeObserver(uint64_t id) {
    std::unique_lock lock(observersMutex_);

    auto it = observers_.find(id);
    if (it != observers_.end()) {
        observerNameMap_.erase(it->second.name);
        observers_.erase(it);
    }
}

void ActorManager::removeObserver(const std::string& name) {
    std::unique_lock lock(observersMutex_);

    auto it = observerNameMap_.find(name);
    if (it != observerNameMap_.end()) {
        observers_.erase(it->second);
        observerNameMap_.erase(it);
    }
}

void ActorManager::publish(const Observation& event) {
    notifyObservers(event);
}

ActorInfo ActorManager::getActorInfo(const std::string& name) const {
    ActorInfo info;
    info.path.name = name;

    auto cell = getActor(name);
    if (!cell) {
        return info;
    }

    info.path = cell->path();
    info.state = cell->state();
    info.type = "";  // TODO: 获取类型名
    info.mailboxSize = cell->mailbox().size();
    info.processedMessages = 0;  // TODO
    info.droppedMessages = 0;
    info.cpuTimeUs = 0;
    info.lastActivityTime = currentTimeMs();

    return info;
}

std::vector<ActorInfo> ActorManager::getAllActorInfo() const {
    std::vector<ActorInfo> result;

    auto names = getAllActorNames();
    result.reserve(names.size());

    for (const auto& name : names) {
        result.push_back(getActorInfo(name));
    }

    return result;
}

ActorManager::ManagerStats ActorManager::getStats() const {
    ManagerStats stats;

    auto names = getAllActorNames();
    stats.totalActors = names.size();

    for (const auto& name : names) {
        auto cell = getActor(name);
        if (cell) {
            switch (cell->state()) {
                case ActorState::Running:
                    ++stats.runningActors;
                    break;
                case ActorState::Suspending:
                    ++stats.suspendedActors;
                    break;
                case ActorState::Stopped:
                case ActorState::Terminated:
                    ++stats.stoppedActors;
                    break;
                default:
                    break;
            }
        }
    }

    stats.totalMessages = totalMessages_.load();
    stats.processedMessages = processedMessages_.load();
    stats.droppedMessages = droppedMessages_.load();

    // 调度器统计
    if (dispatcher_) {
        auto dispStats = dispatcher_->getStats();
        stats.dispatcherQueueSize = dispStats.pendingTasks;
        stats.activeDispatcherThreads = dispStats.pendingTasks > 0 ?
            std::min(size_t(1), dispStats.pendingTasks) : 0;  // 简化
    }

    return stats;
}

void ActorManager::setConfig(const ThreadPoolConfig& config) {
    config_ = config;
    // 重启时应用新配置
}

ActorManager::HealthStatus ActorManager::checkHealth() const {
    HealthStatus status;
    status.healthy = true;

    auto names = getAllActorNames();
    for (const auto& name : names) {
        auto cell = getActor(name);
        if (!cell) {
            continue;
        }

        // 检查邮箱溢出
        if (cell->mailbox().isHighWatermark()) {
            status.healthy = false;
            status.unhealthyActors.push_back(name + " (mailbox overflow)");
        }

        // 检查是否长时间无响应
        auto info = getActorInfo(name);
        if (currentTimeMs() - info.lastActivityTime > 60000) {  // 1 分钟
            status.warnings.push_back(name + " (no recent activity)");
        }
    }

    return status;
}

std::vector<std::string> ActorManager::detectDeadlock() const {
    std::vector<std::string> deadlocked;

    // 检查是否有 Actor 在等待响应但目标已停止
    std::shared_lock lock(actorsMutex_);
    for (const auto& [name, cell] : actors_) {
        if (cell->state() == ActorState::Running &&
            !cell->mailbox().isEmpty() &&
            cell->mailbox().isHighWatermark()) {
            // 可能的死锁
            deadlocked.push_back(name);
        }
    }

    return deadlocked;
}

bool ActorManager::initDispatcher(ActorSystem& system) {
    DispatcherConfig config;
    config.numThreads = config_.dispatcherThreads;
    config.strategy = config_.dispatchStrategy;

    dispatcher_ = std::make_unique<MessageDispatcher>(config);
    return dispatcher_->start(system);
}

bool ActorManager::initIoPool() {
    if (config_.ioThreads == 0) {
        return true;
    }

    startIoWorkers();
    return true;
}

bool ActorManager::initBackgroundPool() {
    if (config_.backgroundThreads == 0) {
        return true;
    }

    startBackgroundWorkers();
    return true;
}

void ActorManager::startIoWorkers() {
    for (size_t i = 0; i < config_.ioThreads; ++i) {
        ioWorkers_.emplace_back([this, i]() {
            std::string threadName = config_.ioThreadPrefix + "-" + std::to_string(i);
            // TODO: 设置线程名称

            while (ioQueue_.running.load()) {
                std::function<void()> task;

                {
                    std::unique_lock lock(ioQueue_.mutex);
                    ioQueue_.cond.wait(lock, [this]() {
                        return !ioQueue_.queue.empty() || !ioQueue_.running.load();
                    });

                    if (!ioQueue_.running.load()) {
                        break;
                    }

                    if (ioQueue_.queue.empty()) {
                        continue;
                    }

                    task = std::move(ioQueue_.queue.front());
                    ioQueue_.queue.pop();
                }

                // 执行任务
                task();
            }
        });
    }
}

void ActorManager::startBackgroundWorkers() {
    for (size_t i = 0; i < config_.backgroundThreads; ++i) {
        bgWorkers_.emplace_back([this, i]() {
            std::string threadName = config_.backgroundThreadPrefix + "-" + std::to_string(i);
            // TODO: 设置线程名称

            while (bgQueue_.running.load()) {
                std::function<void()> task;

                {
                    std::unique_lock lock(bgQueue_.mutex);
                    bgQueue_.cond.wait(lock, [this]() {
                        return !bgQueue_.queue.empty() || !bgQueue_.running.load();
                    });

                    if (!bgQueue_.running.load()) {
                        break;
                    }

                    if (bgQueue_.queue.empty()) {
                        continue;
                    }

                    task = std::move(bgQueue_.queue.front());
                    bgQueue_.queue.pop();
                }

                // 执行任务
                task();
            }
        });
    }
}

void ActorManager::notifyObservers(const Observation& event) {
    std::shared_lock lock(observersMutex_);

    for (auto& [id, observer] : observers_) {
        // 检查是否订阅了此事件
        if (!observer.events.empty() &&
            observer.events.find(event.event) == observer.events.end()) {
            continue;
        }

        // 异步通知（避免阻塞）
        // TODO: 使用后台线程池
        try {
            observer.callback(event);
        } catch (...) {
            // 忽略观察者异常
        }
    }
}

//==============================================================================
// ActorMonitor 实现
//==============================================================================

std::string ActorMonitor::exportStatsAsJson() const {
    std::ostringstream oss;
    auto stats = manager_.getStats();

    oss << "{"
        << "\"totalActors\":" << stats.totalActors << ","
        << "\"runningActors\":" << stats.runningActors << ","
        << "\"suspendedActors\":" << stats.suspendedActors << ","
        << "\"stoppedActors\":" << stats.stoppedActors << ","
        << "\"totalMessages\":" << stats.totalMessages << ","
        << "\"processedMessages\":" << stats.processedMessages << ","
        << "\"droppedMessages\":" << stats.droppedMessages << ","
        << "\"activeDispatcherThreads\":" << stats.activeDispatcherThreads << ","
        << "\"activeIoThreads\":" << stats.activeIoThreads << ","
        << "\"activeBackgroundThreads\":" << stats.activeBackgroundThreads << ","
        << "\"dispatcherQueueSize\":" << stats.dispatcherQueueSize << ","
        << "\"ioQueueSize\":" << stats.ioQueueSize << ","
        << "\"backgroundQueueSize\":" << stats.backgroundQueueSize
        << "}";

    return oss.str();
}

std::string ActorMonitor::exportActorInfoAsJson(const std::string& name) const {
    std::ostringstream oss;
    auto info = manager_.getActorInfo(name);

    oss << "{"
        << "\"name\":\"" << info.path.name << "\","
        << "\"path\":\"" << info.path.toString() << "\","
        << "\"state\":\"" << stateToString(info.state) << "\","
        << "\"type\":\"" << info.type << "\","
        << "\"mailboxSize\":" << info.mailboxSize << ","
        << "\"processedMessages\":" << info.processedMessages << ","
        << "\"droppedMessages\":" << info.droppedMessages << ","
        << "\"cpuTimeUs\":" << info.cpuTimeUs << ","
        << "\"lastActivityTime\":" << info.lastActivityTime << ","
        << "\"children\":[";

    for (size_t i = 0; i < info.children.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << info.children[i] << "\"";
    }

    oss << "],\"tags\":{";

    bool first = true;
    for (const auto& [k, v] : info.tags) {
        if (!first) oss << ",";
        oss << "\"" << k << "\":\"" << v << "\"";
        first = false;
    }

    oss << "}}";
    return oss.str();
}

std::string ActorMonitor::exportAllActorsAsJson() const {
    std::ostringstream oss;
    auto actors = manager_.getAllActorInfo();

    oss << "[";
    for (size_t i = 0; i < actors.size(); ++i) {
        if (i > 0) oss << ",";
        oss << exportActorInfoAsJson(actors[i].path.name);
    }
    oss << "]";

    return oss.str();
}

} // namespace actor
} // namespace apollo
