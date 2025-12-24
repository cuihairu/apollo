#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>

namespace apollo {
namespace core {

/// 服务器类型
enum class ServerType : uint8_t {
    Unknown = 0,
    Login = 1,
    Gateway = 2,
    Game = 3,
    Battle = 4,
    Chat = 5,
    Social = 6,
    Match = 7,
    DBProxy = 8,
    World = 9,
    Cross = 10
};

/// 服务器状态
enum class ServerStatus : uint8_t {
    Starting = 0,     // 启动中
    Running = 1,      // 运行中
    Stopping = 2,     // 关闭中
    Maintenance = 3,  // 维护中
    Offline = 4       // 离线
};

/// 服务器节点信息
struct ServerNode {
    uint32_t id;              // 服务器 ID
    ServerType type;          // 服务器类型
    std::string name;         // 服务器名称
    std::string host;         // 主机地址
    uint16_t port;            // 端口
    ServerStatus status;      // 状态
    uint32_t currentLoad;     // 当前负载 (0-100)
    uint32_t maxLoad;         // 最大负载
    uint64_t startTime;       // 启动时间（毫秒时间戳）
    uint64_t lastHeartbeat;   // 最后心跳时间

    // 扩展信息
    std::unordered_map<std::string, std::string> metadata;

    ServerNode()
        : id(0), type(ServerType::Unknown), port(0)
        , status(ServerStatus::Offline)
        , currentLoad(0), maxLoad(1000)
        , startTime(0), lastHeartbeat(0) {}

    bool isAvailable() const {
        return status == ServerStatus::Running &&
               currentLoad < maxLoad;
    }

    float getLoadPercent() const {
        return maxLoad > 0 ? (currentLoad * 100.0f / maxLoad) : 0.0f;
    }
};

/// 服务发现配置
struct ServiceDiscoveryConfig {
    uint32_t heartbeatIntervalMs = 5000;     // 心跳间隔
    uint32_t heartbeatTimeoutMs = 15000;     // 心跳超时
    bool enableAutoDiscovery = true;         // 启用自动发现
    std::string discoveryEndpoint;           // 发现服务端点
};

/// 服务发现监听器
class IServiceDiscoveryListener {
public:
    virtual ~IServiceDiscoveryListener() = default;

    /// 服务器上线
    virtual void onServerUp(const ServerNode& node) {}

    /// 服务器下线
    virtual void onServerDown(const ServerNode& node) {}

    /// 服务器状态变化
    virtual void onServerStatusChanged(const ServerNode& node, ServerStatus oldStatus) {}

    /// 服务器负载变化
    virtual void onServerLoadChanged(const ServerNode& node, uint32_t oldLoad) {}
};

/// 服务发现接口
class IServiceDiscovery {
public:
    virtual ~IServiceDiscovery() = default;

    /// 注册服务
    virtual bool registerServer(const ServerNode& node) = 0;

    /// 注销服务
    virtual bool unregisterServer(uint32_t serverId) = 0;

    /// 发送心跳
    virtual bool sendHeartbeat(uint32_t serverId) = 0;

    /// 发现服务（按类型）
    virtual std::vector<ServerNode> discoverServers(ServerType type) = 0;

    /// 获取指定服务器
    virtual bool getServer(uint32_t serverId, ServerNode& node) = 0;

    /// 选择负载最低的服务器
    virtual bool selectLeastLoaded(ServerType type, ServerNode& node) = 0;

    /// 随机选择可用服务器
    virtual bool selectRandom(ServerType type, ServerNode& node) = 0;

    /// 设置监听器
    virtual void setListener(IServiceDiscoveryListener* listener) = 0;

    /// 启动
    virtual bool start(const ServiceDiscoveryConfig& config) = 0;

    /// 停止
    virtual void stop() = 0;
};

/// 本地服务发现实现（内存存储）
class LocalServiceDiscovery : public IServiceDiscovery {
public:
    LocalServiceDiscovery();
    ~LocalServiceDiscovery() override;

    // IServiceDiscovery 实现
    bool registerServer(const ServerNode& node) override;
    bool unregisterServer(uint32_t serverId) override;
    bool sendHeartbeat(uint32_t serverId) override;
    std::vector<ServerNode> discoverServers(ServerType type) override;
    bool getServer(uint32_t serverId, ServerNode& node) override;
    bool selectLeastLoaded(ServerType type, ServerNode& node) override;
    bool selectRandom(ServerType type, ServerNode& node) override;
    void setListener(IServiceDiscoveryListener* listener) override;
    bool start(const ServiceDiscoveryConfig& config) override;
    void stop() override;

    /// 更新服务器负载
    bool updateLoad(uint32_t serverId, uint32_t currentLoad);

    /// 更新服务器状态
    bool updateStatus(uint32_t serverId, ServerStatus status);

    /// 获取所有服务器
    std::vector<ServerNode> getAllServers() const;

    /// 获取服务器数量
    size_t getServerCount() const;
    size_t getServerCount(ServerType type) const;

private:
    void heartbeatThread();
    void checkTimeouts();

    std::unordered_map<uint32_t, ServerNode> servers_;
    ServiceDiscoveryConfig config_;
    IServiceDiscoveryListener* listener_;
    std::atomic<bool> running_;
    std::thread heartbeatThread_;
    mutable std::mutex mutex_;
};

/// 服务注册器（服务器端使用）
class ServiceRegistry {
public:
    static ServiceRegistry& instance();

    /// 注册当前服务器
    bool registerSelf(const ServerNode& node);

    /// 注销当前服务器
    bool unregisterSelf();

    /// 启动心跳
    bool startHeartbeat();

    /// 停止心跳
    void stopHeartbeat();

    /// 更新负载
    void updateLoad(uint32_t currentLoad);

    /// 更新状态
    void updateStatus(ServerStatus status);

    /// 获取当前节点
    const ServerNode& getCurrentNode() const { return currentNode_; }

    /// 设置服务发现后端
    void setDiscovery(IServiceDiscovery* discovery);

private:
    ServiceRegistry();
    ~ServiceRegistry();

    void heartbeatLoop();

    ServerNode currentNode_;
    IServiceDiscovery* discovery_;
    std::atomic<bool> heartbeatRunning_;
    std::thread heartbeatThread_;
    std::mutex mutex_;
};

/// 服务客户端（用于发现和连接服务）
class ServiceClient {
public:
    ServiceClient(IServiceDiscovery* discovery);

    /// 获取游戏服务器列表
    std::vector<ServerNode> getGameServers();

    /// 获取网关服务器列表
    std::vector<ServerNode> getGatewayServers();

    /// 连接到负载最低的游戏服务器
    bool connectToGameServer(std::function<bool(const ServerNode&)> connector);

    /// 连接到负载最低的网关服务器
    bool connectToGateway(std::function<bool(const ServerNode&)> connector);

    /// 监听服务器变化
    void setListener(IServiceDiscoveryListener* listener);

private:
    IServiceDiscovery* discovery_;
    IServiceDiscoveryListener* listener_;
};

/// 辅助函数
namespace ServiceHelper {

/// 服务器类型转字符串
const char* serverTypeToString(ServerType type);

/// 字符串转服务器类型
ServerType stringToServerType(const std::string& str);

/// 服务器状态转字符串
const char* statusToString(ServerStatus status);

/// 创建默认服务器节点
ServerNode createDefaultNode(ServerType type, uint32_t id,
                            const std::string& host = "0.0.0.0",
                            uint16_t port = 0);

} // namespace ServiceHelper

} // namespace core
} // namespace apollo
