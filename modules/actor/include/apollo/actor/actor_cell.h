#pragma once

#include "apollo/actor/actor_ref.h"
#include "apollo/actor/actor.h"
#include "apollo/actor/message.h"
#include "apollo/ipc/channel.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <unordered_set>

namespace apollo {
namespace actor {

//==============================================================================
// 消息包装（带发送者信息）
//==============================================================================

struct Envelope {
    Message message;
    ActorRef sender;
    ActorRef receiver;

    Envelope() = default;

    Envelope(const Message& msg, const ActorRef& s, const ActorRef& r)
        : message(msg), sender(s), receiver(r) {}
};

//==============================================================================
// Mailbox 配置
//==============================================================================

struct MailboxConfig {
    size_t capacity = 256;              // 邮箱容量
    size_t highWatermark = 192;         // 高水位线（75%）
    size_t lowWatermark = 64;           // 低水位线（25%）

    // 背压策略
    ipc::BackpressureStrategy strategy = ipc::BackpressureStrategy::Buffer;

    // 超时配置
    int64_t recvTimeoutMs = 1000;       // 接收超时
};

//==============================================================================
// Mailbox（消息队列，支持背压）
//==============================================================================

class Mailbox {
public:
    explicit Mailbox(const MailboxConfig& config = MailboxConfig{});
    ~Mailbox();

    // 发送消息到邮箱（异步）
    bool send(const Envelope& envelope);

    // 从邮箱接收消息（阻塞）
    bool receive(Envelope& envelope, int64_t timeoutMs = -1);

    // 尝试接收（非阻塞）
    bool tryReceive(Envelope& envelope);

    // 获取待处理消息数
    size_t size() const {
        return queueSize_.load(std::memory_order_acquire);
    }

    // 是否为空
    bool isEmpty() const {
        return queueSize_.load(std::memory_order_acquire) == 0;
    }

    // 是否达到高水位线
    bool isHighWatermark() const {
        return queueSize_.load(std::memory_order_acquire) >= config_.highWatermark;
    }

    // 是否达到低水位线
    bool isLowWatermark() const {
        return queueSize_.load(std::memory_order_acquire) <= config_.lowWatermark;
    }

    // 关闭邮箱
    void close();

    // 是否已关闭
    bool isClosed() const {
        return closed_.load(std::memory_order_acquire);
    }

    // 清空邮箱
    void clear();

private:
    MailboxConfig config_;
    std::queue<Envelope> queue_;
    std::atomic<size_t> queueSize_{0};
    std::atomic<bool> closed_{false};
    mutable std::mutex mutex_;
    std::condition_variable cond_;
};

//==============================================================================
// Actor 状态
//==============================================================================

enum class ActorState : uint8_t {
    Starting   = 0,  // 启动中
    Running    = 1,  // 运行中
    Suspending = 2,  // 暂停中
    Stopped    = 3,  // 已停止
    Terminated = 4   // 已终止
};

inline const char* stateToString(ActorState state) {
    switch (state) {
        case ActorState::Starting:    return "Starting";
        case ActorState::Running:     return "Running";
        case ActorState::Suspending:  return "Suspending";
        case ActorState::Stopped:     return "Stopped";
        case ActorState::Terminated:  return "Terminated";
        default: return "Unknown";
    }
}

//==============================================================================
// Actor Cell（Actor 执行单元）
//==============================================================================

class ActorCell : public std::enable_shared_from_this<ActorCell> {
public:
    ActorCell(const ActorPath& path,
              std::unique_ptr<Actor> actor,
              ActorSystem& system,
              const MailboxConfig& mailboxConfig = MailboxConfig{});

    ~ActorCell();

    //==========================================================================
    // 访问器
    //==========================================================================

    const ActorPath& path() const { return path_; }
    Actor* actor() const { return actor_.get(); }
    ActorSystem& system() const { return system_; }

    ActorState state() const {
        return state_.load(std::memory_order_acquire);
    }

    ActorRef self() const { return self_; }
    Mailbox& mailbox() { return mailbox_; }

    //==========================================================================
    // 生命周期控制
    //==========================================================================

    // 启动 Actor
    void start();

    // 停止 Actor
    void stop();

    // 暂停 Actor（暂时不处理消息）
    void suspend();

    // 恢复 Actor
    void resume();

    //==========================================================================
    // 消息处理
    //==========================================================================

    // 发送消息到 Actor
    bool tell(const Message& msg, const ActorRef& sender);

    // 处理单条消息（由调度器调用）
    void processOneMessage();

    // 处理所有待处理消息（由调度器调用）
    void processMessages();

    //==========================================================================
    // 子 Actor 管理
    //==========================================================================

    // 创建子 Actor
    ActorRef spawnChild(const std::string& name, Actor::Factory factory);

    // 查找子 Actor
    std::shared_ptr<ActorCell> findChild(const std::string& name) const;

    // 停止子 Actor
    void stopChild(const std::string& name);

    // 监视子 Actor
    void watch(const ActorRef& child);
    void unwatch(const ActorRef& child);

    //==========================================================================
    // 定时器
    //==========================================================================

    // 单次定时器
    uint64_t scheduleOnce(int64_t delayMs, std::function<void()> fn);

    // 重复定时器
    uint64_t scheduleRepeated(int64_t intervalMs, std::function<void()> fn);

    // 取消定时器
    void cancelTimer(uint64_t timerId);

    //==========================================================================
    // 服务发现
    //==========================================================================

    // 注册到服务发现
    void registerToDiscovery();

    // 从服务发现注销
    void deregisterFromDiscovery();

    // 更新服务端点
    void updateEndpoint();

    //==========================================================================
    // 上下文实现
    //==========================================================================

    ActorContext* context() { return context_.get(); }

private:
    //==========================================================================
    // 内部方法
    //==========================================================================

    // 处理异常
    void handleError(const std::exception& e);

    // 通知子 Actor 终止
    void notifyChildTerminated(const ActorRef& child);

    // 设置状态
    void setState(ActorState newState);

    //==========================================================================
    // 成员变量
    //==========================================================================

    ActorPath path_;
    std::unique_ptr<Actor> actor_;
    ActorSystem& system_;
    ActorRef self_;

    std::atomic<ActorState> state_{ActorState::Starting};

    // 邮箱
    Mailbox mailbox_;

    // 子 Actor
    std::unordered_map<std::string, std::shared_ptr<ActorCell>> children_;
    mutable std::shared_mutex childrenMutex_;

    // 监视的子 Actor
    std::unordered_set<std::string> watching_;
    std::mutex watchingMutex_;

    // 定时器
    std::unordered_map<uint64_t, std::function<void()>> timers_;
    std::atomic<uint64_t> nextTimerId_{1};
    std::mutex timersMutex_;

    // 服务发现端点
    ipc::ServiceEndpointEx endpoint_;

    // 上下文
    class ActorContextImpl;
    std::unique_ptr<ActorContext> context_;
};

//==============================================================================
// Actor 上下文实现
//==============================================================================

class ActorCell::ActorContextImpl : public ActorContext {
public:
    explicit ActorContextImpl(ActorCell* cell) : cell_(cell) {}

    ActorRef self() const override {
        return cell_->self();
    }

    ActorRef sender() const override {
        // TODO: 从当前处理的消息中获取
        return ActorRef{};
    }

    ActorSystem& system() const override {
        return cell_->system();
    }

    std::future<ActorRef> lookup(const std::string& path) const override {
        return cell_->system().lookup(path);
    }

    void scheduleOnce(int64_t delayMs, std::function<void()> fn) override {
        cell_->scheduleOnce(delayMs, std::move(fn));
    }

    void scheduleRepeated(int64_t intervalMs, std::function<void()> fn) override {
        cell_->scheduleRepeated(intervalMs, std::move(fn));
    }

    void cancelTimer(uint64_t timerId) override {
        cell_->cancelTimer(timerId);
    }

    void stop() override {
        cell_->stop();
    }

    void watch(const ActorRef& child) override {
        cell_->watch(child);
    }

    void unwatch(const ActorRef& child) override {
        cell_->unwatch(child);
    }

private:
    ActorCell* cell_;
};

//==============================================================================
// Actor 注册表（本地 Actor 索引）
//==============================================================================

class ActorRegistry {
public:
    ActorRegistry() = default;

    // 注册 Actor
    bool registerActor(const std::string& name, std::shared_ptr<ActorCell> cell);

    // 注销 Actor
    bool unregisterActor(const std::string& name);

    // 查找 Actor
    std::shared_ptr<ActorCell> find(const std::string& name) const;

    // 获取所有 Actor 名称
    std::vector<std::string> getAllNames() const;

    // 获取 Actor 数量
    size_t size() const {
        std::shared_lock lock(mutex_);
        return actors_.size();
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<ActorCell>> actors_;
};

} // namespace actor
} // namespace apollo
