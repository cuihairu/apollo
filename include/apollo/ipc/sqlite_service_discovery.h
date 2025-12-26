#pragma once

#include "apollo/ipc/service_discovery.h"
#include <sqlite3.h>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <condition_variable>
#include <mutex>

namespace apollo {
namespace ipc {

//==============================================================================
// SQLite 服务发现实现
//==============================================================================

class SQLiteServiceDiscovery : public IServiceDiscovery {
public:
    explicit SQLiteServiceDiscovery(const ServiceDiscoveryConfig& config = ServiceDiscoveryConfig{});
    ~SQLiteServiceDiscovery() override;

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
    // SQLite 特定方法
    //==========================================================================

    // 获取数据库连接
    sqlite3* getDb() const { return db_; }

    // 执行健康检查（清理过期服务）
    void cleanupExpiredServices();

private:
    // 数据库初始化
    bool initDatabase();
    bool createTables();

    // 数据库操作
    bool insertService(const std::string& serviceName, const ServiceEndpoint& endpoint);
    bool updateHeartbeat(const std::string& serviceName, const std::string& id);
    bool deleteService(const std::string& serviceName, const std::string& id);
    ServiceEndpoint queryService(const std::string& serviceName, const std::string& id);

    // 后台线程
    void heartbeatThreadFunc();
    void cleanupThreadFunc();
    void notifyWatchers(const DiscoveryEventInfo& event);

    // 成员变量
    ServiceDiscoveryConfig config_;
    sqlite3* db_ = nullptr;
    std::atomic<bool> running_{false};

    // 心跳线程
    std::thread heartbeatThread_;
    std::unordered_map<std::string, std::pair<std::string, ServiceEndpoint>> registeredServices_;
    std::mutex registeredMutex_;

    // 清理线程
    std::thread cleanupThread_;

    // Watch 订阅
    struct Watcher {
        ServiceChangeCallback callback;
        std::thread notifyThread;
        std::atomic<bool> active{true};
        std::vector<ServiceEndpoint> lastEndpoints;
    };
    std::unordered_map<std::string, std::unique_ptr<Watcher>> watchers_;
    std::mutex watchersMutex_;

    // 负载均衡器
    struct LoadBalancerState {
        std::unique_ptr<ILoadBalancer> balancer;
        std::vector<ServiceEndpoint> endpoints;
        int64_t lastUpdate = 0;
    };
    mutable std::unordered_map<std::string, LoadBalancerState> balancers_;
    mutable std::mutex balancersMutex_;
};

//==============================================================================
// 内存服务发现实现（用于测试）
//==============================================================================

class MemoryServiceDiscovery : public IServiceDiscovery {
public:
    MemoryServiceDiscovery();
    ~MemoryServiceDiscovery() override;

    std::vector<ServiceEndpoint> discover(const std::string& serviceName) override;
    void discoverAsync(const std::string& serviceName, ServiceListCallback callback) override;
    bool watch(const std::string& serviceName, ServiceChangeCallback callback) override;
    void unwatch(const std::string& serviceName) override;

    ServiceEndpoint selectOne(const std::string& serviceName,
                             LoadBalanceStrategy strategy = LoadBalanceStrategy::RoundRobin) override;

    bool registerService(const std::string& serviceName,
                        const ServiceEndpoint& endpoint) override;
    bool deregister(const std::string& serviceName,
                   const std::string& id) override;
    bool heartbeat(const std::string& serviceName,
                  const std::string& id) override;
    bool updateService(const std::string& serviceName,
                      const std::string& id,
                      const std::map<std::string, std::string>& metadata) override;

    bool start() override;
    void stop() override;
    bool isRunning() const override { return running_; }
    const ServiceDiscoveryConfig& getConfig() const override { return config_; }

    bool markUnhealthy(const std::string& serviceName, const std::string& id) override;
    bool markHealthy(const std::string& serviceName, const std::string& id) override;
    std::vector<std::string> getServiceNames() const override;

private:
    struct ServiceInfo {
        ServiceEndpoint endpoint;
        int64_t lastHeartbeat;
    };

    std::unordered_map<std::string, std::unordered_map<std::string, ServiceInfo>> services_;
    mutable std::shared_mutex servicesMutex_;

    std::unordered_map<std::string, ServiceChangeCallback> watchers_;
    std::mutex watchersMutex_;

    ServiceDiscoveryConfig config_;
    std::atomic<bool> running_{false};
    std::thread cleanupThread_;

    void cleanupThreadFunc();
    void notifyWatchers(const std::string& serviceName, const DiscoveryEventInfo& event);
};

} // namespace ipc
} // namespace apollo
