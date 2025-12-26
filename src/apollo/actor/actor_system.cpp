/**
 * @file actor_system.cpp
 * @brief Actor 系统实现
 */

#include "apollo/actor/actor_system.h"
#include "apollo/actor/actor_cell.h"
#include "apollo/actor/message_bus.h"
#include "apollo/ipc/sqlite_service_discovery.h"
#include "apollo/ipc/redis_service_discovery.h"
#include <random>
#include <sstream>

namespace apollo {
namespace actor {

//==============================================================================
// ActorSystem 实现
//==============================================================================

ActorSystem::ActorSystem(const ActorSystemConfig& config)
    : config_(config)
    , localSelector_(config.localSelectorConfig) {

    // 初始化服务发现
    initDiscovery();

    // 初始化消息总线
    initMessageBus();

    // 初始化注册表
    registry_ = std::make_unique<ActorRegistry>();
}

ActorSystem::~ActorSystem() {
    shutdown();
}

bool ActorSystem::start() {
    if (running_.load()) {
        return true;
    }

    // 启动服务发现
    if (discovery_ && !discovery_->start()) {
        return false;
    }

    // 启动消息总线
    if (messageBus_) {
        messageBus_->start(getSystemPath());
    }

    // 初始化调度器
    if (!initDispatcher()) {
        shutdown();
        return false;
    }

    // 注册到服务发现
    registerToDiscovery();

    running_.store(true);
    return true;
}

void ActorSystem::shutdown() {
    if (!running_.exchange(false)) {
        return;
    }

    // 从服务发现注销
    deregisterFromDiscovery();

    // 停止所有 Actor
    if (registry_) {
        auto names = registry_->getAllNames();
        for (const auto& name : names) {
            auto cell = registry_->find(name);
            if (cell) {
                cell->stop();
            }
        }
    }

    // 停止消息总线
    if (messageBus_) {
        messageBus_->stop();
    }

    // 停止服务发现
    if (discovery_) {
        discovery_->stop();
    }

    // 等待工作线程
    for (auto& thread : workerThreads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    workerThreads_.clear();

    // 等待心跳线程
    if (heartbeatThread_.joinable()) {
        heartbeatThread_.join();
    }
}

void ActorSystem::awaitTermination() {
    for (auto& thread : workerThreads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

ActorRef ActorSystem::spawn(const std::string& name, Actor::Factory factory) {
    if (!running_.load()) {
        return ActorRef{};  // 系统未启动
    }

    // 检查名称是否已存在
    if (registry_->find(name)) {
        return ActorRef{};  // 名称冲突
    }

    // 构建 Actor 路径
    ActorPath path;
    path.system = config_.systemName;
    path.address = config_.address;
    path.name = name;
    path.instance = 0;

    // 创建 Actor
    auto actor = factory();
    if (!actor) {
        return ActorRef{};
    }

    // 创建 ActorCell
    MailboxConfig mailboxConfig;
    mailboxConfig.capacity = config_.mailboxSize;

    auto cell = std::make_shared<ActorCell>(path, std::move(actor), *this, mailboxConfig);

    // 注册到注册表
    if (!registry_->registerActor(name, cell)) {
        return ActorRef{};
    }

    // 启动 Actor
    cell->start();

    // 注册到消息总线
    if (messageBus_) {
        messageBus_->registerHandler(name, [cell](const Message& msg, const ActorRef& sender) {
            cell->tell(msg, sender);
        });
    }

    return cell->self();
}

std::shared_ptr<ActorCell> ActorSystem::findLocal(const std::string& name) const {
    return registry_->find(name);
}

std::future<ActorRef> ActorSystem::lookup(const std::string& path) const {
    return std::async(std::launch::async, [this, path]() -> ActorRef {
        ActorPath actorPath = resolvePath(path);

        // 优先查找本地
        auto cell = registry_->find(actorPath.name);
        if (cell) {
            return cell->self();
        }

        // 从服务发现查找远程
        if (discovery_) {
            auto endpoints = discovery_->discover(actorPath.name);
            if (!endpoints.empty()) {
                // 选择最佳端点
                ipc::ServiceEndpointEx ex = ipc::ServiceEndpointEx::fromBase(endpoints[0]);
                ex.calculatePriority(config_.dataCenter, config_.hostId);

                return remoteRef(actorPath);
            }
        }

        return ActorRef{};  // 未找到
    });
}

ActorPath ActorSystem::resolvePath(const std::string& path) const {
    return ActorPath::fromString(path);
}

void ActorSystem::tell(const ActorPath& target, const Message& msg) {
    if (!running_.load() || !messageBus_) {
        return;
    }

    messageBus_->send(target, msg);
}

void ActorSystem::tell(const ActorRef& target, const Message& msg) {
    tell(target.path(), msg);
}

std::future<Message> ActorSystem::ask(const ActorPath& target,
                                     const Message& msg,
                                     int64_t timeoutMs) {
    return std::async(std::launch::async, [this, target, msg, timeoutMs]() -> Message {
        // TODO: 实现临时回复 Actor 和超时机制
        tell(target, msg);
        return Message{};
    });
}

ActorRef ActorSystem::remoteRef(const ActorPath& path) const {
    return ActorRef(path, nullptr);
}

void ActorSystem::watchRemote(const std::string& serviceName,
                              std::function<void(const ActorRef&)> onAdded,
                              std::function<void(const ActorRef&)> onRemoved) {
    if (!discovery_) {
        return;
    }

    discovery_->watch(serviceName,
        [this, onAdded](const ipc::DiscoveryEventInfo& event) {
            if (event.type == ipc::DiscoveryEvent::Added && onAdded) {
                ipc::ServiceEndpointEx ex = ipc::ServiceEndpointEx::fromBase(event.endpoint);
                ActorPath path;
                path.name = event.endpoint.id;
                onAdded(remoteRef(path));
            } else if (event.type == ipc::DiscoveryEvent::Removed && onRemoved) {
                ActorPath path;
                path.name = event.endpoint.id;
                onRemoved(remoteRef(path));
            }
        });
}

ActorSystem::Stats ActorSystem::getStats() const {
    Stats stats;
    stats.totalActors = registry_->size();
    stats.localActors = stats.totalActors;
    // TODO: 统计远程 Actor 和消息数
    return stats;
}

std::string ActorSystem::generateActorName(const std::string& prefix) {
    uint64_t id = nameCounter_.fetch_add(1, std::memory_order_relaxed);
    std::ostringstream oss;
    oss << prefix << "-" << id;
    return oss.str();
}

bool ActorSystem::initDispatcher() {
    size_t numThreads = config_.dispatcherThreads;
    if (numThreads == 0) {
        numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4;
    }

    // 创建工作线程
    for (size_t i = 0; i < numThreads; ++i) {
        workerThreads_.emplace_back([this, i]() {
            // TODO: 实现调度循环
            // 这里应该从注册表中获取有消息待处理的 Actor 并调用 processMessages
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        });
    }

    // 创建心跳线程
    heartbeatThread_ = std::thread([this]() {
        while (running_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));

            // 更新所有 Actor 的心跳
            auto names = registry_->getAllNames();
            for (const auto& name : names) {
                auto cell = registry_->find(name);
                if (cell && cell->state() == ActorState::Running) {
                    cell->updateEndpoint();
                }
            }
        }
    });

    return true;
}

bool ActorSystem::initDiscovery() {
    switch (config_.discoveryConfig.backend) {
        case ipc::DiscoveryBackend::SQLite:
            discovery_ = std::make_unique<ipc::SQLiteServiceDiscovery>(
                config_.discoveryConfig);
            break;

        case ipc::DiscoveryBackend::Redis:
            discovery_ = std::make_unique<ipc::RedisServiceDiscovery>(
                config_.discoveryConfig);
            break;

        case ipc::DiscoveryBackend::Memory:
            discovery_ = std::make_unique<ipc::MemoryServiceDiscovery>();
            break;

        default:
            discovery_ = std::make_unique<ipc::SQLiteServiceDiscovery>(
                config_.discoveryConfig);
            break;
    }

    return discovery_ != nullptr;
}

bool ActorSystem::initMessageBus() {
    MessageBusConfig busConfig;

    busConfig.enableSharedMemory = config_.enableSharedMemory;
    busConfig.enableUnixSocket = config_.enableUnixSocket;
    busConfig.enableTcp = config_.enableTcp;

    busConfig.channelConfig.defaultFormat = static_cast<ipc::MessageFormat>(
        config_.defaultMessageFormat);

    messageBus_ = std::make_unique<MessageBus>(busConfig);
    return messageBus_ != nullptr;
}

void ActorSystem::registerToDiscovery() {
    // 注册系统本身到服务发现
    ipc::ServiceEndpoint endpoint;
    endpoint.id = config_.systemName;
    endpoint.host = config_.address;
    endpoint.port = 0;
    endpoint.protocol = "actor-system";
    endpoint.healthy = true;

    if (discovery_) {
        discovery_->registerService(config_.systemName, endpoint);
    }
}

void ActorSystem::deregisterFromDiscovery() {
    if (discovery_) {
        discovery_->deregister(config_.systemName, config_.systemName);
    }
}

//==============================================================================
// 系统单例实现
//==============================================================================

namespace system {

static std::unique_ptr<ActorSystem> g_instance;
static std::once_flag g_initFlag;

ActorSystem& instance() {
    std::call_once(g_initFlag, []() {
        if (!g_instance) {
            g_instance = std::make_unique<ActorSystem>();
        }
    });
    return *g_instance;
}

bool init(const ActorSystemConfig& config) {
    std::call_once(g_initFlag, [&]() {
        g_instance = std::make_unique<ActorSystem>(config);
    });
    return g_instance->start();
}

void shutdown() {
    if (g_instance) {
        g_instance->shutdown();
    }
}

std::future<ActorRef> lookup(const std::string& path) {
    return instance().lookup(path);
}

} // namespace system

//==============================================================================
// 便捷函数实现
//==============================================================================

std::unique_ptr<ActorSystem> createActorSystem(
    const std::string& name,
    const std::string& address) {

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
