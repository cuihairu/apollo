/**
 * @file endpoint.cpp
 * @brief Endpoint and ServiceRegistry implementation
 */

#include "apollo/net/protocol/endpoint.hpp"
#include <sstream>
#include <algorithm>
#include <thread>
#include <mutex>

#ifdef APOLLO_USE_REDIS_PP
#include <sw/redis++/redis++.h>
#endif

namespace apollo {
namespace net {
namespace protocol {

//==============================================================================
// Endpoint implementation
//==============================================================================

std::string Endpoint::toUrl(Protocol proto) const {
    switch (proto) {
        case Protocol::Ipc:
            return "ipc://" + ipcPath;
        case Protocol::Tcp:
            return "tcp://" + host + ":" + std::to_string(port);
        case Protocol::InProc:
            return "inproc://" + service;
        case Protocol::Ws:
            return wsUrl;
        default:
            return "";
    }
}

bool Endpoint::isLocal() const {
    // Local if host is localhost, 127.0.0.1, or empty
    return host.empty() || host == "localhost" || host == "127.0.0.1";
}

std::string Endpoint::serialize() const {
    std::ostringstream oss;
    oss << service << "|"
         << host << "|"
         << port << "|"
         << ipcPath << "|"
         << wsUrl << "|";

    for (auto p : protocols) {
        oss << static_cast<int>(p) << ",";
    }

    oss << "|" << lastSeen << "|"
         << load << "|"
         << version;
    return oss.str();
}

Endpoint Endpoint::deserialize(const std::string& data) {
    Endpoint ep;
    // Simple parsing (in production, use JSON/FlatBuffers)
    std::istringstream iss(data);
    std::string token;
    int field = 0;

    while (std::getline(iss, token, '|')) {
        switch (field++) {
            case 0: ep.service = token; break;
            case 1: ep.host = token; break;
            case 2: ep.port = std::stoi(token); break;
            case 3: ep.ipcPath = token; break;
            case 4: ep.wsUrl = token; break;
            // Parse protocols...
            default: break;
        }
    }

    return ep;
}

//==============================================================================
// MemoryServiceRegistry implementation
//==============================================================================

bool MemoryServiceRegistry::registerService(const Endpoint& endpoint) {
    std::lock_guard<std::mutex> lock(mutex_);

    services_[endpoint.service].push_back(endpoint);

    // Update best endpoint (prefer local, lowest load)
    auto& endpoints = services_[endpoint.service];
    auto bestIt = std::min_element(endpoints.begin(), endpoints.end(),
        [](const Endpoint& a, const Endpoint& b) {
            if (a.isLocal() && !b.isLocal()) return true;
            if (!a.isLocal() && b.isLocal()) return false;
            return a.load < b.load;
        });

    bestEndpoints_[endpoint.service] = *bestIt;
    return true;
}

bool MemoryServiceRegistry::unregister(const std::string& service) {
    std::lock_guard<std::mutex> lock(mutex_);

    services_.erase(service);
    bestEndpoints_.erase(service);
    return true;
}

std::vector<Endpoint> MemoryServiceRegistry::discover(const std::string& service) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = services_.find(service);
    if (it != services_.end()) {
        return it->second;
    }
    return {};
}

bool MemoryServiceRegistry::getBestEndpoint(const std::string& service, Endpoint& out) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = bestEndpoints_.find(service);
    if (it != bestEndpoints_.end()) {
        out = it->second;
        return true;
    }
    return false;
}

bool MemoryServiceRegistry::heartbeat(const std::string& service) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = bestEndpoints_.find(service);
    if (it != bestEndpoints_.end()) {
        it->second.lastSeen = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        return true;
    }
    return false;
}

//==============================================================================
// RedisServiceRegistry implementation
//==============================================================================

#ifdef APOLLO_USE_REDIS_PP

class RedisServiceRegistry::Impl {
public:
    std::shared_ptr<sw::redis::Redis> redis_;
};

RedisServiceRegistry::RedisServiceRegistry(const std::string& redisUrl)
    : impl_(std::make_unique<Impl>())
{
    sw::redis::ConnectionOptions opts;
    sw::redis::ConnectionPoolOptions poolOpts;
    poolOpts.size = 4;

    try {
        impl_->redis_ = std::make_shared<sw::redis::Redis>(opts, poolOpts);
        // Test connection
        impl_->redis_->ping();
    } catch (...) {
        // Connection failed, will use stub
    }
}

bool RedisServiceRegistry::registerService(const Endpoint& endpoint) {
    if (!impl_->redis_) return false;

    try {
        auto key = "service:" + endpoint.service;
        std::string value = endpoint.serialize();

        impl_->redis_->set(key, value);
        impl_->redis_->expire(key, std::chrono::seconds(60));

        // Add to service list
        impl_->redis_->sadd("services", endpoint.service);
        return true;
    } catch (...) {
        return false;
    }
}

bool RedisServiceRegistry::unregister(const std::string& service) {
    if (!impl_->redis_) return false;

    try {
        impl_->redis_->del("service:" + service);
        impl_->redis_->srem("services", service);
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<Endpoint> RedisServiceRegistry::discover(const std::string& service) {
    std::vector<Endpoint> result;

    if (!impl_->redis_) return result;

    try {
        auto key = "service:" + service;
        auto value = impl_->redis_->get(key);

        if (value) {
            result.push_back(Endpoint::deserialize(*value));
        }

        // Also check for multiple instances (service:name:1, service:name:2, etc.)
        std::vector<std::string> keys;
        impl_->redis_->keys("service:" + service + ":*", std::back_inserter(keys));

        for (const auto& k : keys) {
            auto v = impl_->redis_->get(k);
            if (v) {
                result.push_back(Endpoint::deserialize(*v));
            }
        }
    } catch (...) {
        // Fall through
    }

    return result;
}

bool RedisServiceRegistry::getBestEndpoint(const std::string& service, Endpoint& out) {
    auto endpoints = discover(service);
    if (endpoints.empty()) return false;

    // Select best: local > lowest load
    auto bestIt = std::min_element(endpoints.begin(), endpoints.end(),
        [](const Endpoint& a, const Endpoint& b) {
            if (a.isLocal() && !b.isLocal()) return true;
            if (!a.isLocal() && b.isLocal()) return false;
            return a.load < b.load;
        });

    out = *bestIt;
    return true;
}

bool RedisServiceRegistry::heartbeat(const std::string& service) {
    if (!impl_->redis_) return false;

    try {
        // Update heartbeat timestamp
        auto key = "service:" + service + ":heartbeat";
        impl_->redis_->set(key, std::to_string(std::time(nullptr)));
        impl_->redis_->expire(key, std::chrono::seconds(10));
        return true;
    } catch (...) {
        return false;
    }
}

#else // Stub when Redis not available

RedisServiceRegistry::RedisServiceRegistry(const std::string&) {}
RedisServiceRegistry::~RedisServiceRegistry() = default;

bool RedisServiceRegistry::registerService(const Endpoint&) { return false; }
bool RedisServiceRegistry::unregister(const std::string&) { return false; }
std::vector<Endpoint> RedisServiceRegistry::discover(const std::string&) { return {}; }
bool RedisServiceRegistry::getBestEndpoint(const std::string&, Endpoint&) { return false; }
bool RedisServiceRegistry::heartbeat(const std::string&) { return false; }

#endif

//==============================================================================
// Global registry
//==============================================================================

namespace {
    std::unique_ptr<ServiceRegistry> g_registry;
    std::mutex g_registryMutex;
}

ServiceRegistry& getRegistry() {
    std::lock_guard<std::mutex> lock(g_registryMutex);

    if (!g_registry) {
        // Default to memory registry
        g_registry = std::make_unique<MemoryServiceRegistry>();
    }

    return *g_registry;
}

void setRegistry(std::unique_ptr<ServiceRegistry> registry) {
    std::lock_guard<std::mutex> lock(g_registryMutex);
    g_registry = std::move(registry);
}

} // namespace protocol
} // namespace net
} // namespace apollo
