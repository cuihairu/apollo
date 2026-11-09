/**
 * @file memory_service_discovery.cpp
 * @brief SQLite 服务发现实现（内存版服务发现，用于测试）
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
// MemoryServiceDiscovery 实现（用于测试）
//==============================================================================

MemoryServiceDiscovery::MemoryServiceDiscovery() {
    config_.dbPath = ":memory:";
}

MemoryServiceDiscovery::~MemoryServiceDiscovery() {
    stop();
}

bool MemoryServiceDiscovery::start() {
    running_.store(true);

    // 启动清理线程
    cleanupThread_ = std::thread(&MemoryServiceDiscovery::cleanupThreadFunc, this);

    return true;
}

void MemoryServiceDiscovery::stop() {
    running_.store(false);

    if (cleanupThread_.joinable()) {
        cleanupThread_.join();
    }
}

std::vector<ServiceEndpoint> MemoryServiceDiscovery::discover(const std::string& serviceName) {
    std::shared_lock<std::shared_mutex> lock(servicesMutex_);

    std::vector<ServiceEndpoint> result;
    auto it = services_.find(serviceName);
    if (it != services_.end()) {
        int64_t now = currentTimeMs();
        for (const auto& [id, info] : it->second) {
            if (info.endpoint.healthy &&
                (now - info.lastHeartbeat) < config_.heartbeatTimeoutMs) {
                result.push_back(info.endpoint);
            }
        }
    }

    return result;
}

void MemoryServiceDiscovery::discoverAsync(const std::string& serviceName,
                                           ServiceListCallback callback) {
    auto endpoints = discover(serviceName);
    if (callback) callback(endpoints);
}

ServiceEndpoint MemoryServiceDiscovery::selectOne(const std::string& serviceName,
                                                   LoadBalanceStrategy strategy) {
    return LoadBalancer::select(discover(serviceName), strategy);
}

bool MemoryServiceDiscovery::registerService(const std::string& serviceName,
                                            const ServiceEndpoint& endpoint) {
    std::unique_lock<std::shared_mutex> lock(servicesMutex_);

    ServiceInfo info;
    info.endpoint = endpoint;
    info.lastHeartbeat = currentTimeMs();
    services_[serviceName][endpoint.id] = info;

    DiscoveryEventInfo event;
    event.type = DiscoveryEvent::Added;
    event.endpoint = endpoint;
    event.timestamp = info.lastHeartbeat;
    notifyWatchers(serviceName, event);

    return true;
}

bool MemoryServiceDiscovery::deregister(const std::string& serviceName,
                                        const std::string& id) {
    std::unique_lock<std::shared_mutex> lock(servicesMutex_);

    auto it = services_.find(serviceName);
    if (it != services_.end()) {
        it->second.erase(id);

        DiscoveryEventInfo event;
        event.type = DiscoveryEvent::Removed;
        event.endpoint.id = id;
        event.timestamp = currentTimeMs();
        notifyWatchers(serviceName, event);
    }

    return true;
}

bool MemoryServiceDiscovery::heartbeat(const std::string& serviceName,
                                      const std::string& id) {
    std::unique_lock<std::shared_mutex> lock(servicesMutex_);

    auto it = services_.find(serviceName);
    if (it != services_.end()) {
        auto it2 = it->second.find(id);
        if (it2 != it->second.end()) {
            it2->second.lastHeartbeat = currentTimeMs();
            it2->second.endpoint.healthy = true;
            return true;
        }
    }

    return false;
}

bool MemoryServiceDiscovery::updateService(const std::string& serviceName,
                                          const std::string& id,
                                          const std::map<std::string, std::string>& metadata) {
    std::unique_lock<std::shared_mutex> lock(servicesMutex_);

    auto it = services_.find(serviceName);
    if (it != services_.end()) {
        auto it2 = it->second.find(id);
        if (it2 != it->second.end()) {
            it2->second.endpoint.metadata = metadata;

            DiscoveryEventInfo event;
            event.type = DiscoveryEvent::Updated;
            event.endpoint = it2->second.endpoint;
            event.timestamp = currentTimeMs();
            notifyWatchers(serviceName, event);

            return true;
        }
    }

    return false;
}

bool MemoryServiceDiscovery::watch(const std::string& serviceName,
                                  ServiceChangeCallback callback) {
    std::lock_guard<std::mutex> lock(watchersMutex_);
    watchers_[serviceName] = callback;
    return true;
}

void MemoryServiceDiscovery::unwatch(const std::string& serviceName) {
    std::lock_guard<std::mutex> lock(watchersMutex_);
    watchers_.erase(serviceName);
}

void MemoryServiceDiscovery::notifyWatchers(const std::string& serviceName,
                                            const DiscoveryEventInfo& event) {
    std::lock_guard<std::mutex> lock(watchersMutex_);
    auto it = watchers_.find(serviceName);
    if (it != watchers_.end() && it->second) {
        it->second(event);
    }
}

bool MemoryServiceDiscovery::markUnhealthy(const std::string& serviceName,
                                          const std::string& id) {
    std::unique_lock<std::shared_mutex> lock(servicesMutex_);

    auto it = services_.find(serviceName);
    if (it != services_.end()) {
        auto it2 = it->second.find(id);
        if (it2 != it->second.end()) {
            it2->second.endpoint.healthy = false;
            return true;
        }
    }

    return false;
}

bool MemoryServiceDiscovery::markHealthy(const std::string& serviceName,
                                        const std::string& id) {
    std::unique_lock<std::shared_mutex> lock(servicesMutex_);

    auto it = services_.find(serviceName);
    if (it != services_.end()) {
        auto it2 = it->second.find(id);
        if (it2 != it->second.end()) {
            it2->second.endpoint.healthy = true;
            it2->second.lastHeartbeat = currentTimeMs();
            return true;
        }
    }

    return false;
}

std::vector<std::string> MemoryServiceDiscovery::getServiceNames() const {
    std::shared_lock<std::shared_mutex> lock(servicesMutex_);

    std::vector<std::string> names;
    for (const auto& [name, services] : services_) {
        if (!services.empty()) {
            names.push_back(name);
        }
    }

    return names;
}

void MemoryServiceDiscovery::cleanupThreadFunc() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.cleanupIntervalMs));

        int64_t expiredBefore = currentTimeMs() - config_.heartbeatTimeoutMs;

        std::unique_lock<std::shared_mutex> lock(servicesMutex_);
        for (auto& [name, services] : services_) {
            auto it = services.begin();
            while (it != services.end()) {
                if (it->second.lastHeartbeat < expiredBefore) {
                    it = services.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }
}

} // namespace ipc
} // namespace apollo
