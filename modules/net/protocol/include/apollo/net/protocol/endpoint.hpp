#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <set>
#include <cstdint>
#include <memory>
#include <mutex>
#include "apollo/net/protocol/channel.hpp"

namespace apollo {
namespace net {
namespace protocol {

/**
 * @brief Service endpoint information
 */
struct Endpoint {
    std::string service;          // Service name
    std::string host;             // Host address
    int port = 0;                  // TCP port
    std::string ipcPath;          // Shared memory path
    std::string wsUrl;            // WebSocket URL
    std::set<Protocol> protocols;  // Supported protocols

    // Metadata
    uint64_t lastSeen = 0;         // Last heartbeat timestamp
    uint32_t load = 0;             // Server load (0-100)
    std::string version;           // Service version

    // Convert to URL for specific protocol
    std::string toUrl(Protocol proto) const;

    // Check if protocol is supported
    bool supports(Protocol proto) const {
        return protocols.find(proto) != protocols.end();
    }

    // Check if local (same machine)
    bool isLocal() const;

    // Serialization for registry
    std::string serialize() const;
    static Endpoint deserialize(const std::string& data);
};

/**
 * @brief Service discovery options
 */
struct DiscoveryOptions {
    bool useRedis = true;          // Use Redis as registry
    bool useBroadcast = false;      // Use UDP broadcast (local network)
    int cacheTimeoutMs = 30000;     // Cache timeout
    int heartbeatIntervalMs = 5000; // Heartbeat interval
};

/**
 * @brief Service registry interface
 *
 * Used for:
 * - Registering services
 * - Discovering services
 * - Protocol negotiation
 */
class ServiceRegistry {
public:
    virtual ~ServiceRegistry() = default;

    // Register a service
    virtual bool registerService(const Endpoint& endpoint) = 0;

    // Unregister a service
    virtual bool unregister(const std::string& service) = 0;

    // Discover a service (returns all endpoints)
    virtual std::vector<Endpoint> discover(const std::string& service) = 0;

    // Get best endpoint for a service
    virtual bool getBestEndpoint(const std::string& service, Endpoint& out) = 0;

    // Heartbeat (keep alive)
    virtual bool heartbeat(const std::string& service) = 0;
};

/**
 * @brief In-memory service registry (for testing/fallback)
 */
class MemoryServiceRegistry : public ServiceRegistry {
public:
    MemoryServiceRegistry() = default;

    bool registerService(const Endpoint& endpoint) override;
    bool unregister(const std::string& service) override;
    std::vector<Endpoint> discover(const std::string& service) override;
    bool getBestEndpoint(const std::string& service, Endpoint& out) override;
    bool heartbeat(const std::string& service) override;

private:
    std::unordered_map<std::string, std::vector<Endpoint>> services_;
    std::unordered_map<std::string, Endpoint> bestEndpoints_;
    mutable std::mutex mutex_;
};

/**
 * @brief Redis-based service registry implementation
 */
class RedisServiceRegistry : public ServiceRegistry {
public:
    explicit RedisServiceRegistry(const std::string& redisUrl = "redis://127.0.0.1:6379");

    bool registerService(const Endpoint& endpoint) override;
    bool unregister(const std::string& service) override;
    std::vector<Endpoint> discover(const std::string& service) override;
    bool getBestEndpoint(const std::string& service, Endpoint& out) override;
    bool heartbeat(const std::string& service) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief Global registry accessor
 */
ServiceRegistry& getRegistry();
void setRegistry(std::unique_ptr<ServiceRegistry> registry);

} // namespace protocol
} // namespace net
} // namespace apollo
