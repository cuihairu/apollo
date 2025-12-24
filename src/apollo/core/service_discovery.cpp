/**
 * @file service_discovery.cpp
 * @brief 服务发现实现
 */

#include "apollo/core/service_discovery.h"
#include "apollo/utils/time.h"
#include "apollo/utils/random.h"
#include <algorithm>
#include <cstring>
#include <chrono>
#include <thread>

namespace apollo {
namespace core {

//==============================================================================
// LocalServiceDiscovery 实现
//==============================================================================

LocalServiceDiscovery::LocalServiceDiscovery()
    : listener_(nullptr), running_(false) {
}

LocalServiceDiscovery::~LocalServiceDiscovery() {
    stop();
}

bool LocalServiceDiscovery::registerServer(const ServerNode& node) {
    std::lock_guard<std::mutex> lock(mutex_);

    ServerNode new_node = node;
    new_node.startTime = utils::Time::now();
    new_node.lastHeartbeat = new_node.startTime;

    bool isNew = servers_.find(node.id) == servers_.end();
    servers_[node.id] = new_node;

    if (isNew && listener_) {
        listener_->onServerUp(new_node);
    }

    return true;
}

bool LocalServiceDiscovery::unregisterServer(uint32_t serverId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = servers_.find(serverId);
    if (it == servers_.end()) {
        return false;
    }

    if (listener_) {
        listener_->onServerDown(it->second);
    }

    servers_.erase(it);
    return true;
}

bool LocalServiceDiscovery::sendHeartbeat(uint32_t serverId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = servers_.find(serverId);
    if (it == servers_.end()) {
        return false;
    }

    it->second.lastHeartbeat = utils::Time::now();
    return true;
}

std::vector<ServerNode> LocalServiceDiscovery::discoverServers(ServerType type) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<ServerNode> result;
    uint64_t now = utils::Time::now();

    for (const auto& pair : servers_) {
        const auto& node = pair.second;

        // 检查心跳超时
        if (now - node.lastHeartbeat > config_.heartbeatTimeoutMs) {
            continue;  // 跳过超时的服务器
        }

        if (node.type == type) {
            result.push_back(node);
        }
    }

    return result;
}

bool LocalServiceDiscovery::getServer(uint32_t serverId, ServerNode& node) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = servers_.find(serverId);
    if (it == servers_.end()) {
        return false;
    }

    node = it->second;
    return true;
}

bool LocalServiceDiscovery::selectLeastLoaded(ServerType type, ServerNode& node) {
    std::lock_guard<std::mutex> lock(mutex_);

    ServerNode* bestNode = nullptr;
    uint32_t minLoad = UINT32_MAX;
    uint64_t now = utils::Time::now();

    for (auto& pair : servers_) {
        auto& server = pair.second;

        // 检查心跳超时和可用性
        if (now - server.lastHeartbeat > config_.heartbeatTimeoutMs ||
            !server.isAvailable()) {
            continue;
        }

        if (server.type == type && server.currentLoad < minLoad) {
            minLoad = server.currentLoad;
            bestNode = &server;
        }
    }

    if (bestNode) {
        node = *bestNode;
        return true;
    }

    return false;
}

bool LocalServiceDiscovery::selectRandom(ServerType type, ServerNode& node) {
    auto servers = discoverServers(type);
    if (servers.empty()) {
        return false;
    }

    // 过滤可用服务器
    std::vector<ServerNode> availableServers;
    for (const auto& s : servers) {
        if (s.isAvailable()) {
            availableServers.push_back(s);
        }
    }

    if (availableServers.empty()) {
        return false;
    }

    // 随机选择
    utils::Random rng(static_cast<uint32_t>(utils::Time::now()));
    size_t idx = rng.next(static_cast<uint32_t>(availableServers.size()));
    node = availableServers[idx];

    return true;
}

void LocalServiceDiscovery::setListener(IServiceDiscoveryListener* listener) {
    listener_ = listener;
}

bool LocalServiceDiscovery::start(const ServiceDiscoveryConfig& config) {
    config_ = config;
    running_ = true;

    if (config_.enableAutoDiscovery) {
        heartbeatThread_ = std::thread(&LocalServiceDiscovery::heartbeatThread, this);
    }

    return true;
}

void LocalServiceDiscovery::stop() {
    running_ = false;

    if (heartbeatThread_.joinable()) {
        heartbeatThread_.join();
    }
}

bool LocalServiceDiscovery::updateLoad(uint32_t serverId, uint32_t currentLoad) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = servers_.find(serverId);
    if (it == servers_.end()) {
        return false;
    }

    uint32_t oldLoad = it->second.currentLoad;
    it->second.currentLoad = currentLoad;
    it->second.lastHeartbeat = utils::Time::now();

    if (listener_ && oldLoad != currentLoad) {
        listener_->onServerLoadChanged(it->second, oldLoad);
    }

    return true;
}

bool LocalServiceDiscovery::updateStatus(uint32_t serverId, ServerStatus status) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = servers_.find(serverId);
    if (it == servers_.end()) {
        return false;
    }

    ServerStatus oldStatus = it->second.status;
    it->second.status = status;
    it->second.lastHeartbeat = utils::Time::now();

    if (listener_ && oldStatus != status) {
        listener_->onServerStatusChanged(it->second, oldStatus);
    }

    return true;
}

std::vector<ServerNode> LocalServiceDiscovery::getAllServers() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<ServerNode> result;
    result.reserve(servers_.size());

    for (const auto& pair : servers_) {
        result.push_back(pair.second);
    }

    return result;
}

size_t LocalServiceDiscovery::getServerCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return servers_.size();
}

size_t LocalServiceDiscovery::getServerCount(ServerType type) const {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t count = 0;
    for (const auto& pair : servers_) {
        if (pair.second.type == type) {
            ++count;
        }
    }

    return count;
}

void LocalServiceDiscovery::heartbeatThread() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.heartbeatIntervalMs));
        checkTimeouts();
    }
}

void LocalServiceDiscovery::checkTimeouts() {
    std::vector<uint32_t> timeoutServers;
    uint64_t now = utils::Time::now();

    {
        std::lock_guard<std::mutex> lock(mutex_);

        for (const auto& pair : servers_) {
            if (now - pair.second.lastHeartbeat > config_.heartbeatTimeoutMs) {
                timeoutServers.push_back(pair.first);
            }
        }
    }

    for (uint32_t serverId : timeoutServers) {
        unregisterServer(serverId);
    }
}

//==============================================================================
// ServiceRegistry 实现
//==============================================================================

ServiceRegistry& ServiceRegistry::instance() {
    static ServiceRegistry instance;
    return instance;
}

ServiceRegistry::ServiceRegistry()
    : discovery_(nullptr), heartbeatRunning_(false) {
    std::memset(&currentNode_, 0, sizeof(currentNode_));
}

ServiceRegistry::~ServiceRegistry() {
    stopHeartbeat();
}

bool ServiceRegistry::registerSelf(const ServerNode& node) {
    std::lock_guard<std::mutex> lock(mutex_);

    currentNode_ = node;

    if (discovery_) {
        return discovery_->registerServer(node);
    }

    return true;
}

bool ServiceRegistry::unregisterSelf() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (currentNode_.id == 0) {
        return false;
    }

    if (discovery_) {
        return discovery_->unregisterServer(currentNode_.id);
    }

    currentNode_.id = 0;
    return true;
}

bool ServiceRegistry::startHeartbeat() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (heartbeatRunning_ || currentNode_.id == 0) {
        return false;
    }

    heartbeatRunning_ = true;
    heartbeatThread_ = std::thread(&ServiceRegistry::heartbeatLoop, this);

    return true;
}

void ServiceRegistry::stopHeartbeat() {
    std::lock_guard<std::mutex> lock(mutex_);

    heartbeatRunning_ = false;

    if (heartbeatThread_.joinable()) {
        heartbeatThread_.join();
    }
}

void ServiceRegistry::updateLoad(uint32_t currentLoad) {
    std::lock_guard<std::mutex> lock(mutex_);

    currentNode_.currentLoad = currentLoad;

    if (discovery_) {
        discovery_->updateLoad(currentNode_.id, currentLoad);
    }
}

void ServiceRegistry::updateStatus(ServerStatus status) {
    std::lock_guard<std::mutex> lock(mutex_);

    currentNode_.status = status;

    if (discovery_) {
        discovery_->updateStatus(currentNode_.id, status);
    }
}

void ServiceRegistry::setDiscovery(IServiceDiscovery* discovery) {
    discovery_ = discovery;
}

void ServiceRegistry::heartbeatLoop() {
    while (heartbeatRunning_) {
        if (discovery_) {
            discovery_->sendHeartbeat(currentNode_.id);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    }
}

//==============================================================================
// ServiceClient 实现
//==============================================================================

ServiceClient::ServiceClient(IServiceDiscovery* discovery)
    : discovery_(discovery), listener_(nullptr) {
}

std::vector<ServerNode> ServiceClient::getGameServers() {
    if (discovery_) {
        return discovery_->discoverServers(ServerType::Game);
    }
    return {};
}

std::vector<ServerNode> ServiceClient::getGatewayServers() {
    if (discovery_) {
        return discovery_->discoverServers(ServerType::Gateway);
    }
    return {};
}

bool ServiceClient::connectToGameServer(std::function<bool(const ServerNode&)> connector) {
    if (!discovery_) {
        return false;
    }

    ServerNode node;
    if (discovery_->selectLeastLoaded(ServerType::Game, node)) {
        return connector(node);
    }

    return false;
}

bool ServiceClient::connectToGateway(std::function<bool(const ServerNode&)> connector) {
    if (!discovery_) {
        return false;
    }

    ServerNode node;
    if (discovery_->selectLeastLoaded(ServerType::Gateway, node)) {
        return connector(node);
    }

    return false;
}

void ServiceClient::setListener(IServiceDiscoveryListener* listener) {
    listener_ = listener;
    if (discovery_) {
        discovery_->setListener(listener);
    }
}

//==============================================================================
// ServiceHelper 实现
//==============================================================================

namespace ServiceHelper {

const char* serverTypeToString(ServerType type) {
    switch (type) {
        case ServerType::Login:    return "Login";
        case ServerType::Gateway:  return "Gateway";
        case ServerType::Game:     return "Game";
        case ServerType::Battle:   return "Battle";
        case ServerType::Chat:     return "Chat";
        case ServerType::Social:   return "Social";
        case ServerType::Match:    return "Match";
        case ServerType::DBProxy:  return "DBProxy";
        case ServerType::World:    return "World";
        case ServerType::Cross:    return "Cross";
        default:                   return "Unknown";
    }
}

ServerType stringToServerType(const std::string& str) {
    if (str == "Login")    return ServerType::Login;
    if (str == "Gateway")  return ServerType::Gateway;
    if (str == "Game")     return ServerType::Game;
    if (str == "Battle")   return ServerType::Battle;
    if (str == "Chat")     return ServerType::Chat;
    if (str == "Social")   return ServerType::Social;
    if (str == "Match")    return ServerType::Match;
    if (str == "DBProxy")  return ServerType::DBProxy;
    if (str == "World")    return ServerType::World;
    if (str == "Cross")    return ServerType::Cross;
    return ServerType::Unknown;
}

const char* statusToString(ServerStatus status) {
    switch (status) {
        case ServerStatus::Starting:     return "Starting";
        case ServerStatus::Running:      return "Running";
        case ServerStatus::Stopping:     return "Stopping";
        case ServerStatus::Maintenance:  return "Maintenance";
        case ServerStatus::Offline:      return "Offline";
        default:                         return "Unknown";
    }
}

ServerNode createDefaultNode(ServerType type, uint32_t id,
                             const std::string& host, uint16_t port) {
    ServerNode node;
    node.id = id;
    node.type = type;
    node.host = host;
    node.port = port;
    node.status = ServerStatus::Running;
    node.currentLoad = 0;
    node.maxLoad = 1000;

    char nameBuffer[64];
    std::snprintf(nameBuffer, sizeof(nameBuffer), "%s_%u",
                 serverTypeToString(type), id);
    node.name = nameBuffer;

    return node;
}

} // namespace ServiceHelper

} // namespace core
} // namespace apollo
