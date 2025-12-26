/**
 * @file service_discovery.cpp
 * @brief 服务发现基础实现
 */

#include "apollo/ipc/service_discovery.h"
#include <random>
#include <algorithm>

namespace apollo {
namespace ipc {

//==============================================================================
// 辅助函数
//==============================================================================

namespace {

// 生成服务 ID
std::string generateServiceId(const std::string& serviceName,
                              const std::string& host,
                              uint16_t port) {
    // 格式: serviceName-host:port-timestamp
    char buf[256];
    snprintf(buf, sizeof(buf), "%s-%s:%u-%lu",
             serviceName.c_str(), host.c_str(), port,
             static_cast<unsigned long>(currentTimeMs()));
    return std::string(buf);
}

// 随机数生成器
thread_local std::random_device rd;
thread_local std::mt19937 gen(rd());

} // anonymous namespace

//==============================================================================
// ServiceEndpoint 实现
//==============================================================================

bool ServiceEndpoint::isExpired(int64_t heartbeatTimeoutMs) const {
    int64_t now = currentTimeMs();
    return (now - lastHeartbeat) > heartbeatTimeoutMs;
}

//==============================================================================
// ILoadBalancer 实现
//==============================================================================

class RoundRobinBalancer : public ILoadBalancer {
public:
    explicit RoundRobinBalancer(const std::vector<ServiceEndpoint>& endpoints)
        : endpoints_(endpoints), index_(0) {}

    ServiceEndpoint next() override {
        if (endpoints_.empty()) {
            return ServiceEndpoint{};
        }

        size_t i = index_.fetch_add(1, std::memory_order_relaxed) % endpoints_.size();
        return endpoints_[i];
    }

    void update(const std::vector<ServiceEndpoint>& endpoints) override {
        endpoints_ = endpoints;
    }

private:
    std::vector<ServiceEndpoint> endpoints_;
    std::atomic<size_t> index_{0};
};

class RandomBalancer : public ILoadBalancer {
public:
    explicit RandomBalancer(const std::vector<ServiceEndpoint>& endpoints)
        : endpoints_(endpoints) {}

    ServiceEndpoint next() override {
        if (endpoints_.empty()) {
            return ServiceEndpoint{};
        }

        std::uniform_int_distribution<size_t> dist(0, endpoints_.size() - 1);
        return endpoints_[dist(gen)];
    }

    void update(const std::vector<ServiceEndpoint>& endpoints) override {
        endpoints_ = endpoints;
    }

private:
    std::vector<ServiceEndpoint> endpoints_;
    std::mutex mutex_;
};

class WeightedBalancer : public ILoadBalancer {
public:
    explicit WeightedBalancer(const std::vector<ServiceEndpoint>& endpoints) {
        update(endpoints);
    }

    ServiceEndpoint next() override {
        std::lock_guard lock(mutex_);

        if (weightedEndpoints_.empty()) {
            return ServiceEndpoint{};
        }

        // 计算总权重
        int totalWeight = 0;
        for (const auto& we : weightedEndpoints_) {
            totalWeight += we.effectiveWeight;
        }

        if (totalWeight <= 0) {
            return ServiceEndpoint{};
        }

        // 随机选择
        std::uniform_int_distribution<int> dist(1, totalWeight);
        int random = dist(gen);

        int sum = 0;
        for (auto& we : weightedEndpoints_) {
            sum += we.effectiveWeight;
            if (random <= sum) {
                // 降低有效权重
                we.currentWeight--;
                if (we.currentWeight < 0) {
                    we.currentWeight = 0;
                }
                return we.endpoint;
            }
        }

        return weightedEndpoints_[0].endpoint;
    }

    void update(const std::vector<ServiceEndpoint>& endpoints) override {
        std::lock_guard lock(mutex_);

        weightedEndpoints_.clear();
        for (const auto& ep : endpoints) {
            WeightedEndpoint we;
            we.endpoint = ep;
            we.weight = ep.weight;
            we.currentWeight = ep.weight;
            we.effectiveWeight = ep.weight;
            weightedEndpoints_.push_back(we);
        }
    }

private:
    struct WeightedEndpoint {
        ServiceEndpoint endpoint;
        int weight = 1;
        int currentWeight = 1;
        int effectiveWeight = 1;
    };

    std::vector<WeightedEndpoint> weightedEndpoints_;
    std::mutex mutex_;
};

class LeastConnectionBalancer : public ILoadBalancer {
public:
    explicit LeastConnectionBalancer(const std::vector<ServiceEndpoint>& endpoints) {
        update(endpoints);
    }

    ServiceEndpoint next() override {
        std::lock_guard lock(mutex_);

        if (endpoints_.empty()) {
            return ServiceEndpoint{};
        }

        // 选择连接数最少的
        auto it = std::min_element(endpoints_.begin(), endpoints_.end(),
            [](const ServiceEndpoint& a, const ServiceEndpoint& b) {
                return a.activeConnections < b.activeConnections;
            });

        if (it != endpoints_.end()) {
            it->activeConnections++;
            return *it;
        }

        return ServiceEndpoint{};
    }

    void update(const std::vector<ServiceEndpoint>& endpoints) override {
        std::lock_guard lock(mutex_);
        endpoints_ = endpoints;
    }

private:
    std::vector<ServiceEndpoint> endpoints_;
    std::mutex mutex_;
};

//==============================================================================
// LoadBalancer 工厂
//==============================================================================

std::unique_ptr<ILoadBalancer> LoadBalancer::create(
    LoadBalanceStrategy strategy,
    const std::vector<ServiceEndpoint>& endpoints) {

    switch (strategy) {
        case LoadBalanceStrategy::RoundRobin:
            return std::make_unique<RoundRobinBalancer>(endpoints);

        case LoadBalanceStrategy::Random:
            return std::make_unique<RandomBalancer>(endpoints);

        case LoadBalanceStrategy::Weighted:
            return std::make_unique<WeightedBalancer>(endpoints);

        case LoadBalanceStrategy::LeastConnection:
            return std::make_unique<LeastConnectionBalancer>(endpoints);

        default:
            return std::make_unique<RoundRobinBalancer>(endpoints);
    }
}

//==============================================================================
// ServiceDiscoveryConfig 实现
//==============================================================================

ServiceDiscoveryConfig::ServiceDiscoveryConfig()
    : backend(DiscoveryBackend::SQLite)
    , heartbeatIntervalMs(5000)
    , heartbeatTimeoutMs(15000)
    , enableCache(true)
    , cacheTtlMs(60000) {
}

//==============================================================================
// DiscoveryEvent 转字符串
//==============================================================================

const char* discoveryEventToString(DiscoveryEvent event) {
    switch (event) {
        case DiscoveryEvent::Added:    return "Added";
        case DiscoveryEvent::Removed:  return "Removed";
        case DiscoveryEvent::Updated:  return "Updated";
        case DiscoveryEvent::Healthy:  return "Healthy";
        case DiscoveryEvent::Unhealthy: return "Unhealthy";
        default: return "Unknown";
    }
}

} // namespace ipc
} // namespace apollo
