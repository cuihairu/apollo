#pragma once

#include "apollo/ipc/service_discovery.h"
#include "apollo/redis/redis_template.h"
#include <thread>
#include <atomic>
#include <unordered_map>
#include <condition_variable>
#include <mutex>

namespace apollo {
namespace ipc {

//==============================================================================
// Redis 服务发现实现（基于 RedisTemplate）
//==============================================================================

class RedisServiceDiscovery : public IServiceDiscovery {
public:
    // 构造函数：接受外部创建的 RedisTemplate
    explicit RedisServiceDiscovery(apollo::net::redis::RedisTemplate redis);

    // 构造函数：内部创建 RedisTemplate
    explicit RedisServiceDiscovery(const ServiceDiscoveryConfig& config = ServiceDiscoveryConfig{});

    ~RedisServiceDiscovery() override;

    //==========================================================================
    // 客户端接口：服务发现
    //==========================================================================

    std::vector<ServiceEndpoint> discover(const std::string& serviceName) override;
    void discoverAsync(const std::string& serviceName, ServiceListCallback callback) override;
    bool watch(const std::string& serviceName, ServiceChangeCallback callback) override;
    void unwatch(const std::string& serviceName) override;

    ServiceEndpoint selectOne(const std::string& serviceName,
                             LoadBalanceStrategy strategy = LoadBalanceStrategy::RoundRobin) override;

    //==========================================================================
    // 服务端接口：服务注册
    //==========================================================================

    bool registerService(const std::string& serviceName,
                        const ServiceEndpoint& endpoint) override;
    bool deregister(const std::string& serviceName,
                   const std::string& id) override;
    bool heartbeat(const std::string& serviceName,
                  const std::string& id) override;
    bool updateService(const std::string& serviceName,
                      const std::string& id,
                      const std::map<std::string, std::string>& metadata) override;

    //==========================================================================
    // 生命周期
    //==========================================================================

    bool start() override;
    void stop() override;
    bool isRunning() const override { return running_; }
    const ServiceDiscoveryConfig& getConfig() const override { return config_; }

    //==========================================================================
    // 健康检查
    //==========================================================================

    bool markUnhealthy(const std::string& serviceName, const std::string& id) override;
    bool markHealthy(const std::string& serviceName, const std::string& id) override;
    std::vector<std::string> getServiceNames() const override;

    //==========================================================================
    // Redis 特定方法
    //==========================================================================

    // 获取 RedisTemplate 引用
    apollo::net::redis::RedisTemplate& getRedis() { return redis_; }

    // 设置键前缀
    void setKeyPrefix(const std::string& prefix) { keyPrefix_ = prefix; }

    // 获取键前缀
    const std::string& getKeyPrefix() const { return keyPrefix_; }

private:
    //==========================================================================
    // Redis 键名生成
    //==========================================================================

    // 服务列表: {prefix}:{service_name} -> SET
    std::string serviceListKey(const std::string& serviceName) const {
        return keyPrefix_ + ":" + serviceName;
    }

    // 服务详情: {prefix}:{service_name}:{id} -> HASH
    std::string serviceDetailKey(const std::string& serviceName, const std::string& id) const {
        return keyPrefix_ + ":" + serviceName + ":" + id;
    }

    // 心跳: {prefix}:{service_name}:{id}:heartbeat -> STRING (with TTL)
    std::string heartbeatKey(const std::string& serviceName, const std::string& id) const {
        return keyPrefix_ + ":" + serviceName + ":" + id + ":heartbeat";
    }

    // 变更通知: {prefix}:{service_name}:changes -> Pub/Sub Channel
    std::string changeChannel(const std::string& serviceName) const {
        return keyPrefix_ + ":" + serviceName + ":changes";
    }

    // 服务名索引: {prefix}:__services__ -> SET (所有服务名)
    std::string servicesIndexKey() const {
        return keyPrefix_ + ":__services__";
    }

    //==========================================================================
    // 序列化/反序列化
    //==========================================================================

    // 端点转 Hash (用于 HSET)
    std::map<std::string, std::string> endpointToHash(const ServiceEndpoint& ep) const;

    // Hash 转端点 (用于 HGETALL)
    ServiceEndpoint hashToEndpoint(const std::map<std::string, std::string>& hash) const;

    // 事件转 JSON
    std::string eventToJson(const DiscoveryEventInfo& event) const;

    // JSON 转事件
    DiscoveryEventInfo jsonToEvent(const std::string& json) const;

    //==========================================================================
    // 后台线程
    //==========================================================================

    void heartbeatThreadFunc();
    void subscribeThreadFunc(const std::string& serviceName, ServiceChangeCallback callback);

    //==========================================================================
    // 成员变量
    //==========================================================================

    ServiceDiscoveryConfig config_;
    apollo::net::redis::RedisTemplate redis_;
    std::atomic<bool> running_{false};
    std::string keyPrefix_ = "apollo:services";
    bool ownsRedis_ = false;  // 是否拥有 redis_ 的所有权

    // 心跳线程
    std::thread heartbeatThread_;
    std::unordered_map<std::string, ServiceEndpoint> registeredServices_;
    std::mutex registeredMutex_;

    // Watch 订阅
    struct WatcherInfo {
        ServiceChangeCallback callback;
        std::thread subscribeThread;
        std::atomic<bool> active{true};
        std::string serviceName;
    };
    std::unordered_map<std::string, std::unique_ptr<WatcherInfo>> watchers_;
    std::mutex watchersMutex_;

    // 负载均衡器
    struct LoadBalancerState {
        std::unique_ptr<ILoadBalancer> balancer;
        std::vector<ServiceEndpoint> endpoints;
        int64_t lastUpdate = 0;
    };
    mutable std::unordered_map<std::string, LoadBalancerState> balancers_;
    mutable std::mutex balancersMutex_;

    // 本地缓存
    struct CacheEntry {
        std::vector<ServiceEndpoint> endpoints;
        int64_t expiryTime;
    };
    mutable std::unordered_map<std::string, CacheEntry> cache_;
    mutable std::mutex cacheMutex_;
};

//==============================================================================
// Redis 数据结构设计
//==============================================================================
/*
   服务注册:
   --------------------------------------------------
   Key: apollo:services:{service_name}
   Type: SET
   Members: {service_id1, service_id2, ...}

   服务详情:
   --------------------------------------------------
   Key: apollo:services:{service_name}:{service_id}
   Type: HASH
   Fields:
     - id: "user-service-1"
     - host: "192.168.1.10"
     - port: "8080"
     - protocol: "tcp"
     - weight: "1"
     - healthy: "1"
     - metadata: "{\"version\":\"1.0\"}"
     - lastHeartbeat: "1234567890"

   心跳 (带 TTL):
   --------------------------------------------------
   Key: apollo:services:{service_name}:{service_id}:heartbeat
   Type: STRING
   Value: timestamp
   TTL: heartbeatIntervalMs / 1000 + 5s
   (心跳过期则服务视为不健康)

   服务变更通知:
   --------------------------------------------------
   Channel: apollo:services:{service_name}:changes
   Message: JSON encoded DiscoveryEventInfo

   服务名索引:
   --------------------------------------------------
   Key: apollo:services:__services__
   Type: SET
   Members: {service_name1, service_name2, ...}
*/

} // namespace ipc
} // namespace apollo
