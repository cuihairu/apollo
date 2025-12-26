#pragma once

#include "apollo/actor/message.h"
#include "apollo/actor/actor_ref.h"
#include <string>
#include <memory>
#include <future>
#include <unordered_map>
#include <functional>
#include <atomic>

namespace apollo {
namespace actor {

//==============================================================================
// 前向声明
//==============================================================================

class ActorSystem;
class ActorCell;
class ActorContext;

//==============================================================================
// Actor 上下文（提供给 Actor 的运行时接口）
//==============================================================================

class ActorContext {
public:
    virtual ~ActorContext() = default;

    // 获取自身引用
    virtual ActorRef self() const = 0;

    // 获取发送者引用
    virtual ActorRef sender() const = 0;

    // 获取系统引用
    virtual ActorSystem& system() const = 0;

    // 创建子 Actor
    template<typename T, typename... Args>
    ActorRef spawn(const std::string& name, Args&&... args) {
        return spawnActor(name, [](Args&&... args) -> T* {
            return new T(std::forward<Args>(args)...);
        }, std::forward<Args>(args)...);
    }

    // 查找 Actor
    virtual std::future<ActorRef> lookup(const std::string& path) const = 0;

    // 定时任务
    virtual void scheduleOnce(int64_t delayMs, std::function<void()> fn) = 0;
    virtual void scheduleRepeated(int64_t intervalMs, std::function<void()> fn) = 0;

    // 取消定时任务
    virtual void cancelTimer(uint64_t timerId) = 0;

    // 停止自身
    virtual void stop() = 0;

    // 观察子 Actor
    virtual void watch(const ActorRef& child) = 0;
    virtual void unwatch(const ActorRef& child) = 0;

private:
    virtual ActorRef spawnActor(const std::string& name,
                                std::function<void*()> factory,
                                const std::vector<std::shared_ptr<void>>& args) = 0;
};

//==============================================================================
// Actor 基类
//==============================================================================

class Actor {
    friend class ActorCell;
    friend class ActorContextImpl;

public:
    virtual ~Actor() = default;

    // 创建 Actor 的工厂函数类型
    using Factory = std::function<std::unique_ptr<Actor>()>;

    //==========================================================================
    // 生命周期回调（可选重写）
    //==========================================================================

    // Actor 启动时调用
    virtual void onStart() {
        // 默认：注册到服务发现
        registerSelf();
    }

    // Actor 停止时调用
    virtual void onStop() {
        // 默认：从服务发现注销
        deregisterSelf();
    }

    // 异常处理
    virtual void onError(const std::exception& e) {
        // 默认：记录日志并停止
        // TODO: 日志
    }

    // 子 Actor 终止通知
    virtual void onChildTerminated(const ActorRef& child) {
        // 默认：不做处理
    }

    //==========================================================================
    // 消息处理（必须重写）
    //==========================================================================

    // 处理接收到的消息
    virtual void receive(const Message& msg) = 0;

    //==========================================================================
    // 辅助方法
    //==========================================================================

    // 获取上下文
    ActorContext* context() { return context_.get(); }

    // 获取自身引用
    ActorRef self() const { return self_; }

    // 获取路径
    const ActorPath& path() const { return path_; }

protected:
    // 加载配置
    template<typename T>
    T loadConfig(const std::string& key) {
        // TODO: 从 ConfigManager 加载
        return T{};
    }

    // 设置 Actor 名称
    void setName(const std::string& name) {
        path_.name = name;
    }

    // 设置支持的传输类型
    void setTransports(uint32_t transportMask) {
        transportMask_ = transportMask;
    }

private:
    // 注册到服务发现
    void registerSelf();

    // 从服务发现注销
    void deregisterSelf();

    ActorPath path_;
    ActorRef self_;
    std::unique_ptr<ActorContext> context_;
    uint32_t transportMask_ = 0;
};

//==============================================================================
// 内建消息类型
//==============================================================================

namespace Messages {

// 启动消息
struct Start {
    APOLLO_MESSAGE_TYPE(Start)
};

// 停止消息
struct Stop {
    APOLLO_MESSAGE_TYPE(Stop)
};

// 终止消息（已停止）
struct Terminated {
    ActorRef actor;
    APOLLO_MESSAGE_TYPE(Terminated)
};

// 异常消息
struct Error {
    std::string what;
    APOLLO_MESSAGE_TYPE(Error)
};

// 心跳消息
struct Heartbeat {
    int64_t timestamp;
    APOLLO_POD_MESSAGE(Heartbeat)
};

// 请求消息基类
template<typename T>
struct Request {
    ActorRef replyTo;
    T payload;

    static const char* typeName() { return T::typeName(); }
};

// 响应消息基类
template<typename T>
struct Response {
    bool success;
    T payload;
    std::string error;

    static const char* typeName() { return T::typeName(); }
};

} // namespace Messages

//==============================================================================
// Actor 创建辅助宏
//==============================================================================

#define APOLLO_ACTOR_DECLARE(Type) \
    using Factory = std::function<std::unique_ptr<Type>()>; \
    static std::unique_ptr<Actor> create() { return std::make_unique<Type>(); }

} // namespace actor
} // namespace apollo
