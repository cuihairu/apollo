/**
 * @file sqlite_service_discovery_balancer.cpp
 * @brief LoadBalancer 工厂与四种负载均衡策略实现
 */

#include "apollo/ipc/sqlite_service_discovery.h"
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace apollo {
namespace ipc {

//==============================================================================
// LoadBalancer 实现
//==============================================================================

ServiceEndpoint LoadBalancer::select(const std::vector<ServiceEndpoint>& endpoints,
                                    LoadBalanceStrategy strategy,
                                    const std::string& key) {
    if (endpoints.empty()) return {};

    // 过滤健康节点
    std::vector<ServiceEndpoint> healthy;
    for (const auto& ep : endpoints) {
        if (ep.healthy) {
            healthy.push_back(ep);
        }
    }

    if (healthy.empty()) return {};

    static std::mt19937 rng(std::random_device{}());

    switch (strategy) {
        case LoadBalanceStrategy::RoundRobin: {
            static size_t index = 0;
            return healthy[index++ % healthy.size()];
        }

        case LoadBalanceStrategy::Random: {
            std::uniform_int_distribution<size_t> dist(0, healthy.size() - 1);
            return healthy[dist(rng)];
        }

        case LoadBalanceStrategy::FirstAvailable:
            return healthy[0];

        case LoadBalanceStrategy::Weighted: {
            // 计算总权重
            int totalWeight = 0;
            for (const auto& ep : healthy) {
                totalWeight += ep.weight;
            }
            if (totalWeight <= 0) return healthy[0];

            // 加权随机
            std::uniform_int_distribution<int> dist(1, totalWeight);
            int random = dist(rng);
            int sum = 0;
            for (const auto& ep : healthy) {
                sum += ep.weight;
                if (random <= sum) return ep;
            }
            return healthy.back();
        }

        case LoadBalanceStrategy::LeastConnection: {
            // 简化版本：随机选择
            std::uniform_int_distribution<size_t> dist(0, healthy.size() - 1);
            return healthy[dist(rng)];
        }

        case LoadBalanceStrategy::ConsistentHash: {
            // 简化版本：使用 key 的哈希
            size_t hash = std::hash<std::string>{}(key);
            return healthy[hash % healthy.size()];
        }

        default:
            return healthy[0];
    }
}

std::unique_ptr<ILoadBalancer> LoadBalancer::create(
    LoadBalanceStrategy strategy,
    const std::vector<ServiceEndpoint>& endpoints) {

    switch (strategy) {
        case LoadBalanceStrategy::RoundRobin:
            return std::make_unique<RoundRobinBalancer>(endpoints);
        case LoadBalanceStrategy::Random:
            return std::make_unique<RandomBalancer>(endpoints);
        case LoadBalanceStrategy::Weighted:
            return std::make_unique<WeightedRandomBalancer>(endpoints);
        case LoadBalanceStrategy::ConsistentHash:
            return std::make_unique<ConsistentHashBalancer>(endpoints);
        default:
            return std::make_unique<RoundRobinBalancer>(endpoints);
    }
}

//==============================================================================
// RoundRobinBalancer
//==============================================================================

RoundRobinBalancer::RoundRobinBalancer(const std::vector<ServiceEndpoint>& endpoints)
    : endpoints_(endpoints) {}

ServiceEndpoint RoundRobinBalancer::next() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (endpoints_.empty()) return {};

    for (size_t i = 0; i < endpoints_.size(); ++i) {
        size_t idx = (currentIndex_ + i) % endpoints_.size();
        if (endpoints_[idx].healthy) {
            currentIndex_ = (idx + 1) % endpoints_.size();
            return endpoints_[idx];
        }
    }

    return {};
}

void RoundRobinBalancer::updateEndpoints(const std::vector<ServiceEndpoint>& endpoints) {
    std::lock_guard<std::mutex> lock(mutex_);
    endpoints_ = endpoints;
    currentIndex_ = 0;
}

//==============================================================================
// RandomBalancer
//==============================================================================

RandomBalancer::RandomBalancer(const std::vector<ServiceEndpoint>& endpoints)
    : endpoints_(endpoints) {
    rng_.seed(std::random_device{}());
}

ServiceEndpoint RandomBalancer::next() {
    std::lock_guard<std::mutex> lock(mutex_);

    // 过滤健康节点
    std::vector<ServiceEndpoint> healthy;
    for (const auto& ep : endpoints_) {
        if (ep.healthy) healthy.push_back(ep);
    }

    if (healthy.empty()) return {};

    std::uniform_int_distribution<size_t> dist(0, healthy.size() - 1);
    return healthy[dist(rng_)];
}

void RandomBalancer::updateEndpoints(const std::vector<ServiceEndpoint>& endpoints) {
    std::lock_guard<std::mutex> lock(mutex_);
    endpoints_ = endpoints;
}

//==============================================================================
// WeightedRandomBalancer
//==============================================================================

WeightedRandomBalancer::WeightedRandomBalancer(const std::vector<ServiceEndpoint>& endpoints) {
    updateEndpoints(endpoints);
}

ServiceEndpoint WeightedRandomBalancer::next() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (endpoints_.empty()) return {};

    std::vector<ServiceEndpoint> healthy;
    for (const auto& ep : endpoints_) {
        if (ep.healthy) healthy.push_back(ep);
    }

    if (healthy.empty()) return {};

    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(1, totalWeight_);
    int random = dist(rng);

    int sum = 0;
    for (const auto& ep : healthy) {
        sum += ep.weight;
        if (random <= sum) return ep;
    }

    return healthy.back();
}

void WeightedRandomBalancer::updateEndpoints(const std::vector<ServiceEndpoint>& endpoints) {
    std::lock_guard<std::mutex> lock(mutex_);
    endpoints_ = endpoints;
    recalculateWeight();
}

void WeightedRandomBalancer::recalculateWeight() {
    totalWeight_ = 0;
    for (const auto& ep : endpoints_) {
        if (ep.healthy) {
            totalWeight_ += ep.weight;
        }
    }
}

//==============================================================================
// ConsistentHashBalancer
//==============================================================================

ConsistentHashBalancer::ConsistentHashBalancer(const std::vector<ServiceEndpoint>& endpoints,
                                               uint32_t virtualNodes)
    : virtualNodes_(virtualNodes) {
    buildRing(endpoints);
}

void ConsistentHashBalancer::buildRing(const std::vector<ServiceEndpoint>& endpoints) {
    ring_.clear();

    for (const auto& ep : endpoints) {
        if (!ep.healthy) continue;

        for (uint32_t i = 0; i < virtualNodes_; ++i) {
            std::string virtualKey = ep.id + "#" + std::to_string(i);
            std::hash<std::string> hasher;
            uint32_t hash = static_cast<uint32_t>(hasher(virtualKey));

            HashNode node;
            node.hash = hash;
            node.endpoint = ep;
            ring_.push_back(node);
        }
    }

    std::sort(ring_.begin(), ring_.end(),
        [](const HashNode& a, const HashNode& b) { return a.hash < b.hash; });
}

ServiceEndpoint ConsistentHashBalancer::next(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (ring_.empty()) return {};

    std::hash<std::string> hasher;
    uint32_t hash = static_cast<uint32_t>(hasher(key));

    // 二分查找
    auto it = std::lower_bound(ring_.begin(), ring_.end(), hash,
        [](const HashNode& node, uint32_t h) { return node.hash < h; });

    if (it == ring_.end()) {
        return ring_.front().endpoint;
    }

    return it->endpoint;
}

void ConsistentHashBalancer::updateEndpoints(const std::vector<ServiceEndpoint>& endpoints) {
    std::lock_guard<std::mutex> lock(mutex_);
    buildRing(endpoints);
}

} // namespace ipc
} // namespace apollo
