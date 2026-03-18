/**
 * @file actor_cell.cpp
 * @brief Actor 执行单元实现
 */

#include "apollo/actor/actor_cell.h"
#include "apollo/actor/actor_system.h"
#include "apollo/actor/actor_utils.h"
#include <chrono>

namespace apollo {
namespace actor {

//==============================================================================
// Mailbox 实现
//==============================================================================

Mailbox::Mailbox(const MailboxConfig& config)
    : config_(config) {
}

Mailbox::~Mailbox() {
    close();
}

bool Mailbox::send(const Envelope& envelope) {
    if (closed_.load(std::memory_order_acquire)) {
        return false;
    }

    std::unique_lock lock(mutex_);

    // 检查容量
    if (queue_.size() >= config_.capacity) {
        switch (config_.strategy) {
            case ipc::BackpressureStrategy::Drop:
                return false;  // 丢弃消息
            case ipc::BackpressureStrategy::Fail:
                throw std::runtime_error("Mailbox overflow");
            case ipc::BackpressureStrategy::Block:
                // 等待空间
                cond_.wait(lock, [this] {
                    return queue_.size() < config_.capacity || closed_.load();
                });
                break;
            case ipc::BackpressureStrategy::Buffer:
            default:
                // 允许超出容量
                break;
        }
    }

    if (closed_.load(std::memory_order_acquire)) {
        return false;
    }

    queue_.push(envelope);
    queueSize_.fetch_add(1, std::memory_order_release);
    cond_.notify_one();
    return true;
}

bool Mailbox::receive(Envelope& envelope, int64_t timeoutMs) {
    std::unique_lock lock(mutex_);

    auto predicate = [this] {
        return !queue_.empty() || closed_.load(std::memory_order_acquire);
    };

    if (timeoutMs < 0) {
        cond_.wait(lock, predicate);
    } else {
        cond_.wait_for(lock, std::chrono::milliseconds(timeoutMs), predicate);
    }

    if (queue_.empty()) {
        return false;
    }

    envelope = queue_.front();
    queue_.pop();
    queueSize_.fetch_sub(1, std::memory_order_release);
    cond_.notify_one();  // 通知可能等待的发送者
    return true;
}

bool Mailbox::tryReceive(Envelope& envelope) {
    std::unique_lock lock(mutex_);

    if (queue_.empty()) {
        return false;
    }

    envelope = queue_.front();
    queue_.pop();
    queueSize_.fetch_sub(1, std::memory_order_release);
    cond_.notify_one();
    return true;
}

void Mailbox::close() {
    closed_.store(true, std::memory_order_release);
    cond_.notify_all();
}

void Mailbox::clear() {
    std::unique_lock lock(mutex_);
    while (!queue_.empty()) {
        queue_.pop();
    }
    queueSize_.store(0, std::memory_order_release);
}

//==============================================================================
// ActorCell 实现
//==============================================================================

ActorCell::ActorCell(const ActorPath& path,
                     std::unique_ptr<Actor> actor,
                     ActorSystem& system,
                     const MailboxConfig& mailboxConfig)
    : path_(path)
    , actor_(std::move(actor))
    , system_(system)
    , self_(path, shared_from_this())
    , mailbox_(mailboxConfig)
    , context_(std::make_unique<ActorContextImpl>(this)) {

    // 设置 Actor 的路径和引用
    actor_->path_ = path_;
    actor_->self_ = self_;
    actor_->context_ = context_.get();
}

ActorCell::~ActorCell() {
    stop();
}

void ActorCell::start() {
    ActorState expected = ActorState::Starting;
    if (!state_.compare_exchange_strong(expected, ActorState::Running,
                                        std::memory_order_acq_rel)) {
        return;  // 已经启动或停止
    }

    try {
        // 调用 Actor 的 onStart 回调
        actor_->onStart();
    } catch (const std::exception& e) {
        handleError(e);
        stop();
    }
}

void ActorCell::stop() {
    ActorState expected = ActorState::Running;
    if (state_.compare_exchange_strong(expected, ActorState::Stopped,
                                       std::memory_order_acq_rel)) {
        // 停止所有子 Actor
        std::vector<std::shared_ptr<ActorCell>> children;
        {
            std::unique_lock lock(childrenMutex_);
            children.reserve(children_.size());
            for (auto& [name, child] : children_) {
                children.push_back(child);
            }
        }

        for (auto& child : children) {
            child->stop();
        }

        // 关闭邮箱
        mailbox_.close();

        // 调用 Actor 的 onStop 回调
        try {
            actor_->onStop();
        } catch (const std::exception& e) {
            // 忽略停止时的异常
        }

        setState(ActorState::Terminated);
    }
}

void ActorCell::suspend() {
    setState(ActorState::Suspending);
}

void ActorCell::resume() {
    ActorState expected = ActorState::Suspending;
    if (state_.compare_exchange_strong(expected, ActorState::Running,
                                       std::memory_order_acq_rel)) {
        // 恢复处理
    }
}

bool ActorCell::tell(const Message& msg, const ActorRef& sender) {
    if (state_.load(std::memory_order_acquire) != ActorState::Running) {
        return false;
    }

    Envelope envelope;
    envelope.message = msg;
    envelope.sender = sender;
    envelope.receiver = self_;

    return mailbox_.send(envelope);
}

void ActorCell::processOneMessage() {
    if (state_.load(std::memory_order_acquire) != ActorState::Running) {
        return;
    }

    Envelope envelope;
    if (mailbox_.tryReceive(envelope)) {
        try {
            actor_->receive(envelope.message);
        } catch (const std::exception& e) {
            handleError(e);
        }
    }
}

void ActorCell::processMessages() {
    if (state_.load(std::memory_order_acquire) != ActorState::Running) {
        return;
    }

    // 批量处理消息（最多处理 100 条）
    for (int i = 0; i < 100; ++i) {
        if (state_.load(std::memory_order_acquire) != ActorState::Running) {
            break;
        }

        Envelope envelope;
        if (!mailbox_.tryReceive(envelope)) {
            break;
        }

        try {
            actor_->receive(envelope.message);
        } catch (const std::exception& e) {
            handleError(e);
        }
    }
}

ActorRef ActorCell::spawnChild(const std::string& name, Actor::Factory factory) {
    // 构建子 Actor 路径
    ActorPath childPath = path_;
    childPath.name = path_.name + "/" + name;

    // 创建子 Actor
    auto childCell = std::make_shared<ActorCell>(
        childPath, factory(), system_);

    childCell->start();

    // 注册到子 Actor 列表
    {
        std::unique_lock lock(childrenMutex_);
        children_[name] = childCell;
    }

    return childCell->self();
}

std::shared_ptr<ActorCell> ActorCell::findChild(const std::string& name) const {
    std::shared_lock lock(childrenMutex_);
    auto it = children_.find(name);
    return it != children_.end() ? it->second : nullptr;
}

void ActorCell::stopChild(const std::string& name) {
    std::shared_ptr<ActorCell> child;
    {
        std::unique_lock lock(childrenMutex_);
        auto it = children_.find(name);
        if (it != children_.end()) {
            child = it->second;
            children_.erase(it);
        }
    }

    if (child) {
        child->stop();
    }
}

void ActorCell::watch(const ActorRef& child) {
    std::unique_lock lock(watchingMutex_);
    watching_.insert(child.path().toString());
}

void ActorCell::unwatch(const ActorRef& child) {
    std::unique_lock lock(watchingMutex_);
    watching_.erase(child.path().toString());
}

uint64_t ActorCell::scheduleOnce(int64_t delayMs, std::function<void()> fn) {
    uint64_t timerId = nextTimerId_++;

    // TODO: 实现定时器调度
    // 这里应该使用系统的定时器服务

    return timerId;
}

uint64_t ActorCell::scheduleRepeated(int64_t intervalMs, std::function<void()> fn) {
    uint64_t timerId = nextTimerId_++;

    // TODO: 实现重复定时器

    return timerId;
}

void ActorCell::cancelTimer(uint64_t timerId) {
    std::unique_lock lock(timersMutex_);
    timers_.erase(timerId);
}

void ActorCell::registerToDiscovery() {
    // 构建服务端点
    endpoint_.id = ipc::ServiceId::generateId(path_);
    endpoint_.host = system_.getConfig().address;
    endpoint_.port = 0;  // 由 MessageBus 分配
    endpoint_.protocol = "actor";
    endpoint_.healthy = true;

    // 设置支持的传输类型
    endpoint_.transportMask = ipc::TransportMask::SharedMemory |
                             ipc::TransportMask::UnixSocket |
                             ipc::TransportMask::Tcp;

    // 设置共享内存地址
    endpoint_.shmAddress.shmName = system_.getConfig().systemName + "_" + path_.name;
    endpoint_.shmAddress.protocol = "shm";

    // 设置 Unix Socket 地址
    endpoint_.unixAddress.unixPath = "/tmp/" +
        system_.getConfig().systemName + "_" + path_.name + ".sock";
    endpoint_.unixAddress.protocol = "unix";

    // 设置 TCP 地址
    endpoint_.tcpAddress.serverHost = "127.0.0.1";
    endpoint_.tcpAddress.protocol = "tcp";

    // 注册到服务发现
    system_.discovery().registerService(path_.name, endpoint_);
}

void ActorCell::deregisterFromDiscovery() {
    system_.discovery().deregister(path_.name, endpoint_.id);
}

void ActorCell::updateEndpoint() {
    endpoint_.lastHeartbeat = currentTimeMs();
    system_.discovery().heartbeat(path_.name, endpoint_.id);
}

void ActorCell::handleError(const std::exception& e) {
    try {
        actor_->onError(e);
    } catch (...) {
        // 忽略 onError 中的异常
    }
}

void ActorCell::notifyChildTerminated(const ActorRef& child) {
    try {
        actor_->onChildTerminated(child);
    } catch (...) {
        // 忽略异常
    }
}

void ActorCell::setState(ActorState newState) {
    state_.store(newState, std::memory_order_release);
}

//==============================================================================
// ActorRegistry 实现
//==============================================================================

bool ActorRegistry::registerActor(const std::string& name,
                                  std::shared_ptr<ActorCell> cell) {
    std::unique_lock lock(mutex_);
    auto result = actors_.emplace(name, std::move(cell));
    return result.second;
}

bool ActorRegistry::unregisterActor(const std::string& name) {
    std::unique_lock lock(mutex_);
    return actors_.erase(name) > 0;
}

std::shared_ptr<ActorCell> ActorRegistry::find(const std::string& name) const {
    std::shared_lock lock(mutex_);
    auto it = actors_.find(name);
    return it != actors_.end() ? it->second : nullptr;
}

std::vector<std::string> ActorRegistry::getAllNames() const {
    std::shared_lock lock(mutex_);
    std::vector<std::string> names;
    names.reserve(actors_.size());
    for (const auto& [name, _] : actors_) {
        names.push_back(name);
    }
    return names;
}

} // namespace actor
} // namespace apollo
