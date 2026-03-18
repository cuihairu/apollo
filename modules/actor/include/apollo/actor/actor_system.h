#pragma once

#include "apollo/actor/actor_ref.h"
#include "apollo/actor/actor.h"
#include "apollo/ipc/service_discovery.h"
#include "apollo/ipc/service_endpoint_ex.h"
#include <memory>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace apollo {
namespace actor {

//==============================================================================
// Actor 系统配置
//==============================================================================

struct ActorSystemConfig {
    // 系统标识
    std::string systemName = "apollo";
    std::string address = "0";           // 0 表示本地
    std::string dataCenter = "0";
    std::string hostId = "0";
    uint8_t hostIdNum = 0;

    // 线程配置
    size_t dispatcherThreads = 0;        // 0 表示使用 CPU 核心数
    size_t mailboxSize = 256;            // 默认邮箱大小

    // 消息配置
    MessageFormat defaultMessageFormat = MessageFormat::FlatBuffers;

    // 服务发现配置
    ipc::ServiceDiscoveryConfig discoveryConfig;

    // 本地优先配置
    ipc::LocalFirstSelector::Config localSelectorConfig;

    // Channel 配置（用于远程通信）
    bool enableSharedMemory = true;
    bool enableUnixSocket = true;
    bool enableTcp = true;

    ActorSystemConfig() {
        // 默认使用 SQLite 作为本地注册中心
        discoveryConfig.backend = ipc::DiscoveryBackend::SQLite;
        discoveryConfig.dbPath = ":memory:";  // 内存模式，测试用
    }
};

//==============================================================================
// 前向声明
//==============================================================================

class ActorCell;
class MessageDispatcher;
class MessageBus;
class ActorRegistry;

//==============================================================================
// Actor 系统（统一入口）
//==============================================================================

class ActorSystem {
public:
    //==========================================================================
    // 构造/析构
    //==========================================================================

    explicit ActorSystem(const ActorSystemConfig& config = ActorSystemConfig{});
    ~ActorSystem();

    // 禁止拷贝
    ActorSystem(const ActorSystem&) = delete;
    ActorSystem& operator=(const ActorSystem&) = delete;

    //==========================================================================
    // 生命周期
    //==========================================================================

    // 启动系统
    bool start();

    // 关闭系统（优雅关闭所有 Actor）
    void shutdown();

    // 等待系统关闭
    void awaitTermination();

    // 是否运行中
    bool isRunning() const { return running_.load(); }

    //==========================================================================
    // Actor 创建
    //==========================================================================

    // 创建 Actor（使用工厂函数）
    ActorRef spawn(const std::string& name, Actor::Factory factory);

    // 创建 Actor（使用模板）
    template<typename T, typename... Args>
    ActorRef spawn(const std::string& name, Args&&... args) {
        auto factory = [=]() -> std::unique_ptr<Actor> {
            return std::make_unique<T>(args...);
        };
        return spawn(name, factory);
    }

    // 创建匿名 Actor（自动生成名称）
    template<typename T, typename... Args>
    ActorRef spawnAnonymous(Args&&... args) {
        std::string name = generateActorName(T::typeName());
        return spawn<T>(name, std::forward<Args>(args)...);
    }

    //==========================================================================
    // Actor 查找
    //==========================================================================

    // 查找本地 Actor
    std::shared_ptr<ActorCell> findLocal(const std::string& name) const;

    // 查找 Actor（本地或远程）
    std::future<ActorRef> lookup(const std::string& path) const;

    // 解析 Actor 路径
    ActorPath resolvePath(const std::string& path) const;

    //==========================================================================
    // 消息发送
    //==========================================================================

    // 发送消息到指定 Actor
    void tell(const ActorPath& target, const Message& msg);
    void tell(const ActorRef& target, const Message& msg);

    // 请求-响应
    std::future<Message> ask(const ActorPath& target, const Message& msg,
                             int64_t timeoutMs = 5000);

    //==========================================================================
    // 系统信息
    //==========================================================================

    // 获取系统路径
    ActorPath getSystemPath() const {
        return ActorPath(config_.systemName, config_.address, "");
    }

    // 获取配置
    const ActorSystemConfig& getConfig() const { return config_; }

    // 获取服务发现
    ipc::IServiceDiscovery& discovery() { return *discovery_; }

    // 获取消息总线
    MessageBus& messageBus() { return *messageBus_; }

    // 获取本地选择器
    const ipc::LocalFirstSelector& localSelector() const { return localSelector_; }

    //==========================================================================
    // 远程 Actor 处理
    //==========================================================================

    // 创建远程 Actor 引用
    ActorRef remoteRef(const ActorPath& path) const;

    // 订阅远程 Actor 变更
    void watchRemote(const std::string& serviceName,
                    std::function<void(const ActorRef&)> onAdded,
                    std::function<void(const ActorRef&)> onRemoved);

    //==========================================================================
    // 统计信息
    //==========================================================================

    struct Stats {
        size_t totalActors = 0;
        size_t localActors = 0;
        size_t remoteActors = 0;
        size_t pendingMessages = 0;
        size_t processedMessages = 0;
    };

    Stats getStats() const;

private:
    //==========================================================================
    // 内部方法
    //==========================================================================

    // 生成 Actor 名称
    std::string generateActorName(const std::string& prefix);

    // 初始化调度器
    bool initDispatcher();

    // 初始化服务发现
    bool initDiscovery();

    // 初始化消息总线
    bool initMessageBus();

    // 注册到服务发现
    void registerToDiscovery();

    // 从服务发现注销
    void deregisterFromDiscovery();

    //==========================================================================
    // 成员变量
    //==========================================================================

    ActorSystemConfig config_;
    std::atomic<bool> running_{false};

    // 服务发现
    std::unique_ptr<ipc::IServiceDiscovery> discovery_;

    // Actor 注册表（本地 Actor）
    std::unique_ptr<ActorRegistry> registry_;

    // 消息调度器
    std::unique_ptr<MessageDispatcher> dispatcher_;

    // 消息总线
    std::unique_ptr<MessageBus> messageBus_;

    // 本地优先选择器
    ipc::LocalFirstSelector localSelector_;

    // 系统线程
    std::vector<std::thread> workerThreads_;
    std::thread heartbeatThread_;

    // 名称计数器
    std::atomic<uint64_t> nameCounter_{0};
};

//==============================================================================
// Actor 系统单例（可选）
//==============================================================================

namespace system {
    // 获取默认 Actor 系统
    ActorSystem& instance();

    // 初始化默认系统
    bool init(const ActorSystemConfig& config = ActorSystemConfig{});

    // 关闭默认系统
    void shutdown();

    // 快捷创建 Actor
    template<typename T, typename... Args>
    ActorRef spawn(const std::string& name, Args&&... args) {
        return instance().spawn<T>(name, std::forward<Args>(args)...);
    }

    // 快捷查找 Actor
    std::future<ActorRef> lookup(const std::string& path);
}

//==============================================================================
// 便捷函数
//==============================================================================

// 创建并启动 Actor 系统
inline std::unique_ptr<ActorSystem> createActorSystem(
    const std::string& name,
    const std::string& address = "0") {

    ActorSystemConfig config;
    config.systemName = name;
    config.address = address;

    auto system = std::make_unique<ActorSystem>(config);
    if (!system->start()) {
        return nullptr;
    }
    return system;
}

} // namespace actor
} // namespace apollo
