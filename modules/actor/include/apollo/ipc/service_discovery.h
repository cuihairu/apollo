#pragma once

/**
 * @file service_discovery.h
 * @brief Service discovery stub - to be integrated with NNG-based implementation
 */

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>

namespace apollo {
namespace ipc {

// Forward declarations
class ServiceEndpoint;

enum class DiscoveryBackend {
    Memory,
    Redis,
    SQLite,
    NNG  // Using NNG for peer discovery
};

/**
 * @brief Service discovery configuration
 */
struct ServiceDiscoveryConfig {
    DiscoveryBackend backend = DiscoveryBackend::Memory;
    std::string connectionString;

    // For SQLite backend
    std::string dbPath = "/tmp/apollo_services.db";

    // For NNG-based discovery
    std::string multicastGroup = "224.0.0.1";
    int discoveryPort = 7654;
};

/**
 * @brief Service discovery interface - stub implementation
 * TODO: Integrate with NNG-based peer discovery from protocol module
 */
class IServiceDiscovery {
public:
    virtual ~IServiceDiscovery() = default;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool registerService(const std::string& name, const ServiceEndpoint& endpoint) = 0;
    virtual bool unregisterService(const std::string& name) = 0;
    virtual std::vector<ServiceEndpoint> discover(const std::string& name) = 0;
};
class ServiceDiscovery {
public:
    using ChangeCallback = std::function<void(const std::string& serviceName, bool added)>;

    ServiceDiscovery() = default;
    virtual ~ServiceDiscovery() = default;

    // Stub methods
    virtual bool start() { return false; }
    virtual bool stop() { return false; }
    virtual bool registerService(const std::string& name, const ServiceEndpoint& endpoint) { return false; }
    virtual bool unregisterService(const std::string& name) { return false; }
    virtual std::vector<ServiceEndpoint> discover(const std::string& name) { return {}; }
    virtual void watch(const std::string& name, ChangeCallback callback) {}

protected:
    ServiceDiscoveryConfig config_;
};

/**
 * @brief Service ID for actor messaging
 */
struct ServiceId {
    std::string name;
    uint64_t instanceId;

    ServiceId() : instanceId(0) {}
    ServiceId(const std::string& n, uint64_t id = 0) : name(n), instanceId(id) {}

    bool operator==(const ServiceId& other) const {
        return name == other.name && instanceId == other.instanceId;
    }

    // Helper for generating unique IDs
    static ServiceId generate() {
        static uint64_t counter = 0;
        return ServiceId("service", ++counter);
    }

    // Alias for compatibility
    static ServiceId generateId() {
        return generate();
    }
};

/**
 * @brief Local-first endpoint selector for actor messaging
 */
class LocalFirstSelector {
public:
    struct Config {
        bool preferLocal = true;
        int maxRetries = 3;
        int64_t timeoutMs = 5000;
    };

    LocalFirstSelector() = default;
    explicit LocalFirstSelector(const Config& config) : config_(config) {}

    // Stub methods
    template<typename T>
    T select(const std::vector<T>& endpoints) {
        if (config_.preferLocal && !endpoints.empty()) {
            return endpoints[0];  // Return first (local) endpoint
        }
        if (!endpoints.empty()) {
            return endpoints[0];
        }
        return T{};
    }

    const Config& config() const { return config_; }

private:
    Config config_;
};

} // namespace ipc
} // namespace apollo
