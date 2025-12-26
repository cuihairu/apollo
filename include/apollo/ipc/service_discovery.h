#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <chrono>
#include <vector>

namespace apollo {
namespace ipc {

//==============================================================================
// 服务端点信息
//==============================================================================

struct ServiceEndpoint {
    std::string id;              // 唯一标识
    std::string host;
    uint16_t port = 0;
    std::string protocol = "tcp";  // tcp, udp, unix
    std::map<std::string, std::string> metadata;  // 自定义元数据

    int64_t lastHeartbeat = 0;   // 最后心跳时间 (unix timestamp ms)
    bool healthy = true;
    int weight = 1;              // 负载均衡权重

    // 构造函数
    ServiceEndpoint() = default;
    ServiceEndpoint(std::string id, std::string host, uint16_t port)
        : id(std::move(id)), host(std::move(host)), port(port) {}

    // 序列化为 JSON
    std::string toJson() const;

    // 从 JSON 反序列化
    static ServiceEndpoint fromJson(const std::string& json);

    // 获取地址字符串
    std::string address() const { return host + ":" + std::to_string(port); }

    // 比较操作
    bool operator==(const ServiceEndpoint& other) const {
        return id == other.id;
    }
};

//==============================================================================
// 服务发现事件
//==============================================================================

enum class DiscoveryEvent : uint8_t {
    Added,      // 新服务上线
    Removed,    // 服务下线
    Updated,    // 元数据变更
    Healthy,    // 恢复健康
    Unhealthy   // 不健康
};

struct DiscoveryEventInfo {
    DiscoveryEvent type;
    ServiceEndpoint endpoint;
    int64_t timestamp;
};

//==============================================================================
// 负载均衡策略
//==============================================================================

enum class LoadBalanceStrategy : uint8_t {
    RoundRobin,       // 轮询
    Random,           // 随机
    LeastConnection,  // 最少连接
    Weighted,         // 加权随机
    ConsistentHash,   // 一致性哈希
    FirstAvailable,   // 第一个可用
    Priority          // 按优先级
};

//==============================================================================
// 服务发现配置
//==============================================================================

struct ServiceDiscoveryConfig {
    // 心跳配置
    uint32_t heartbeatIntervalMs = 10000;     // 心跳间隔
    uint32_t heartbeatTimeoutMs = 30000;      // 心跳超时
    uint32_t cleanupIntervalMs = 60000;       // 清理过期服务间隔

    // 缓存配置
    bool enableCache = true;
    uint32_t cacheTtlMs = 5000;               // 本地缓存过期时间

    // 重试配置
    uint32_t maxRetries = 3;
    uint32_t retryDelayMs = 1000;

    // SQLite 特定配置
    std::string dbPath = "./services.db";

    // Redis 特定配置
    std::string redisHost = "127.0.0.1";
    uint16_t redisPort = 6379;
    std::string redisPassword;
    int redisDb = 0;
    uint32_t redisPoolSize = 10;
};

//==============================================================================
// 服务回调类型
//==============================================================================

using ServiceChangeCallback = std::function<void(const DiscoveryEventInfo&)>;
using ServiceListCallback = std::function<void(const std::vector<ServiceEndpoint>&)>;

//==============================================================================
// 服务发现抽象接口
//==============================================================================

class IServiceDiscovery {
public:
    virtual ~IServiceDiscovery() = default;

    //==========================================================================
    // 客户端接口：服务发现
    //==========================================================================

    // 同步获取服务端点列表
    virtual std::vector<ServiceEndpoint> discover(const std::string& serviceName) = 0;

    // 异步获取服务端点列表
    virtual void discoverAsync(const std::string& serviceName, ServiceListCallback callback) = 0;

    // 订阅服务变化
    virtual bool watch(const std::string& serviceName, ServiceChangeCallback callback) = 0;

    // 取消订阅
    virtual void unwatch(const std::string& serviceName) = 0;

    // 选择一个端点（带负载均衡）
    virtual ServiceEndpoint selectOne(const std::string& serviceName,
                                     LoadBalanceStrategy strategy = LoadBalanceStrategy::RoundRobin) = 0;

    //==========================================================================
    // 服务端接口：服务注册
    //==========================================================================

    // 注册服务
    virtual bool registerService(const std::string& serviceName,
                                const ServiceEndpoint& endpoint) = 0;

    // 注销服务
    virtual bool deregister(const std::string& serviceName,
                           const std::string& id) = 0;

    // 发送心跳
    virtual bool heartbeat(const std::string& serviceName,
                          const std::string& id) = 0;

    // 更新服务元数据
    virtual bool updateService(const std::string& serviceName,
                              const std::string& id,
                              const std::map<std::string, std::string>& metadata) = 0;

    //==========================================================================
    // 生命周期
    //==========================================================================

    // 启动服务发现
    virtual bool start() = 0;

    // 停止服务发现
    virtual void stop() = 0;

    // 是否运行中
    virtual bool isRunning() const = 0;

    // 获取配置
    virtual const ServiceDiscoveryConfig& getConfig() const = 0;

    //==========================================================================
    // 健康检查
    //==========================================================================

    // 标记服务为不健康
    virtual bool markUnhealthy(const std::string& serviceName,
                               const std::string& id) = 0;

    // 标记服务为健康
    virtual bool markHealthy(const std::string& serviceName,
                             const std::string& id) = 0;

    // 获取所有服务名称
    virtual std::vector<std::string> getServiceNames() const = 0;
};

//==============================================================================
// 服务发现工厂
//==============================================================================

class ServiceDiscoveryFactory {
public:
    // 创建类型
    enum class Type : uint8_t {
        SQLite,    // 轻量级本地存储
        Redis,     // Redis 存储中心
        Memory,    // 内存存储（测试用）
        Consul,    // 预留
        Etcd,      // 预留
        Nacos      // 预留
    };

    // 创建服务发现实例
    static std::unique_ptr<IServiceDiscovery> create(
        Type type,
        const ServiceDiscoveryConfig& config = ServiceDiscoveryConfig{}
    );

    // 从配置字符串创建 (格式: "sqlite:./services.db" 或 "redis:localhost:6379")
    static std::unique_ptr<IServiceDiscovery> createFromString(
        const std::string& connectionString,
        const ServiceDiscoveryConfig& config = ServiceDiscoveryConfig{}
    );

    // 获取类型名称
    static const char* typeName(Type type);
    static Type typeFromName(const std::string& name);
};

//==============================================================================
// 负载均衡器
//==============================================================================

class LoadBalancer {
public:
    // 选择一个端点
    static ServiceEndpoint select(
        const std::vector<ServiceEndpoint>& endpoints,
        LoadBalanceStrategy strategy,
        const std::string& key = ""  // 用于一致性哈希的 key
    );

    // 创建带状态的负载均衡器（用于轮询等）
    static std::unique_ptr<class ILoadBalancer> create(
        LoadBalanceStrategy strategy,
        const std::vector<ServiceEndpoint>& endpoints
    );
};

//==============================================================================
// 有状态的负载均衡器接口
//==============================================================================

class ILoadBalancer {
public:
    virtual ~ILoadBalancer() = default;

    // 选择下一个端点
    virtual ServiceEndpoint next() = 0;

    // 更新端点列表
    virtual void updateEndpoints(const std::vector<ServiceEndpoint>& endpoints) = 0;

    // 获取当前策略
    virtual LoadBalanceStrategy getStrategy() const = 0;
};

//==============================================================================
// 轮询负载均衡器
//==============================================================================

class RoundRobinBalancer : public ILoadBalancer {
public:
    explicit RoundRobinBalancer(const std::vector<ServiceEndpoint>& endpoints);

    ServiceEndpoint next() override;
    void updateEndpoints(const std::vector<ServiceEndpoint>& endpoints) override;
    LoadBalanceStrategy getStrategy() const override { return LoadBalanceStrategy::RoundRobin; }

private:
    std::vector<ServiceEndpoint> endpoints_;
    size_t currentIndex_ = 0;
    mutable std::mutex mutex_;
};

//==============================================================================
// 随机负载均衡器
//==============================================================================

class RandomBalancer : public ILoadBalancer {
public:
    explicit RandomBalancer(const std::vector<ServiceEndpoint>& endpoints);

    ServiceEndpoint next() override;
    void updateEndpoints(const std::vector<ServiceEndpoint>& endpoints) override;
    LoadBalanceStrategy getStrategy() const override { return LoadBalanceStrategy::Random; }

private:
    std::vector<ServiceEndpoint> endpoints_;
    mutable std::mutex mutex_;
};

//==============================================================================
// 最少连接负载均衡器
//==============================================================================

class LeastConnectionBalancer : public ILoadBalancer {
public:
    explicit LeastConnectionBalancer(const std::vector<ServiceEndpoint>& endpoints);

    ServiceEndpoint next() override;
    void updateEndpoints(const std::vector<ServiceEndpoint>& endpoints) override;

    // 记录连接数
    void increment(const std::string& id);
    void decrement(const std::string& id);

    LoadBalancerStrategy getStrategy() const override { return LoadBalanceStrategy::LeastConnection; }

private:
    struct EndpointWithConn {
        ServiceEndpoint endpoint;
        std::atomic<int> connections{0};
    };

    std::vector<std::shared_ptr<EndpointWithConn>> endpoints_;
    mutable std::shared_mutex mutex_;  // 读写锁
};

//==============================================================================
// 加权随机负载均衡器
//==============================================================================

class WeightedRandomBalancer : public ILoadBalancer {
public:
    explicit WeightedRandomBalancer(const std::vector<ServiceEndpoint>& endpoints);

    ServiceEndpoint next() override;
    void updateEndpoints(const std::vector<ServiceEndpoint>& endpoints) override;
    LoadBalanceStrategy getStrategy() const override { return LoadBalanceStrategy::Weighted; }

private:
    std::vector<ServiceEndpoint> endpoints_;
    int totalWeight_ = 0;
    mutable std::mutex mutex_;

    void recalculateWeight();
};

//==============================================================================
// 一致性哈希负载均衡器
//==============================================================================

class ConsistentHashBalancer : public ILoadBalancer {
public:
    explicit ConsistentHashBalancer(const std::vector<ServiceEndpoint>& endpoints,
                                   uint32_t virtualNodes = 150);

    ServiceEndpoint next(const std::string& key);

    // ILoadBalancer 接口（使用默认 key）
    ServiceEndpoint next() override {
        return next(defaultKey_);
    }

    void updateEndpoints(const std::vector<ServiceEndpoint>& endpoints) override;
    LoadBalanceStrategy getStrategy() const override { return LoadBalanceStrategy::ConsistentHash; }

    // 设置默认 key
    void setDefaultKey(const std::string& key) { defaultKey_ = key; }

private:
    struct HashNode {
        uint32_t hash;
        ServiceEndpoint endpoint;
    };

    std::vector<HashNode> ring_;
    uint32_t virtualNodes_;
    std::string defaultKey_;
    mutable std::mutex mutex_;

    void buildRing(const std::vector<ServiceEndpoint>& endpoints);
};

//==============================================================================
// 辅助函数
//==============================================================================

// 生成唯一服务 ID
inline std::string generateServiceId(const std::string& serviceName,
                                    const std::string& host,
                                    uint16_t port) {
    return serviceName + "@" + host + ":" + std::to_string(port);
}

// 获取当前时间戳 (毫秒)
inline int64_t currentTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

} // namespace ipc
} // namespace apollo
