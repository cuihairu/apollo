#pragma once

/**
 * @file service_endpoint_ex.h
 * @brief Service endpoint stub - to be integrated with NNG-based implementation
 */

#include "apollo/ipc/service_discovery.h"
#include <string>
#include <vector>
#include <map>

namespace apollo {
namespace ipc {

/**
 * @brief Extended service endpoint information
 */
struct ServiceEndpoint {
    std::string name;
    std::string host;
    int port;
    std::string protocol;  // "tcp", "ipc", "nng"

    ServiceEndpoint() : port(0) {}
    ServiceEndpoint(const std::string& n, const std::string& h, int p, const std::string& proto = "tcp")
        : name(n), host(h), port(p), protocol(proto) {}

    // Convert to URL format for NNG
    std::string toUrl() const {
        if (protocol == "ipc" || protocol == "nng") {
            return "ipc://" + name;
        }
        return "tcp://" + host + ":" + std::to_string(port);
    }

    bool isValid() const {
        return !name.empty() && port > 0;
    }
};

/**
 * @brief Service endpoint registry - stub implementation
 * TODO: Replace with NNG-based endpoint management
 */
class ServiceEndpointEx {
public:
    ServiceEndpointEx() = default;
    virtual ~ServiceEndpointEx() = default;

    // Stub methods
    virtual bool registerEndpoint(const ServiceEndpoint& endpoint) { return false; }
    virtual bool unregisterEndpoint(const std::string& name) { return false; }
    virtual ServiceEndpoint getEndpoint(const std::string& name) { return {}; }
    virtual std::vector<ServiceEndpoint> getAllEndpoints() { return {}; }

protected:
    std::map<std::string, ServiceEndpoint> endpoints_;
};

} // namespace ipc
} // namespace apollo
