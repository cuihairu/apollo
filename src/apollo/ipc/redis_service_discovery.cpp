/**
 * @file redis_service_discovery.cpp
 * @brief Redis 服务发现实现（基于 RedisTemplate）
 */

#include "apollo/ipc/redis_service_discovery.h"
#include <sstream>
#include <algorithm>
#include <random>
#include <cstring>

namespace apollo {
namespace ipc {

//==============================================================================
// 辅助函数：JSON 序列化/反序列化
//==============================================================================

namespace {

// 简单 JSON 转义
std::string jsonEscape(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (c < 32) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    result += buf;
                } else {
                    result += c;
                }
        }
    }
    return result;
}

// metadata -> JSON
std::string metadataToJson(const std::map<std::string, std::string>& metadata) {
    if (metadata.empty()) return "{}";
    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (const auto& [k, v] : metadata) {
        if (!first) oss << ",";
        oss << "\"" << jsonEscape(k) << "\":\"" << jsonEscape(v) << "\"";
        first = false;
    }
    oss << "}";
    return oss.str();
}

// JSON -> metadata (简化版)
std::map<std::string, std::string> jsonToMetadata(const std::string& json) {
    std::map<std::string, std::string> result;
    if (json.empty() || json == "{}") return result;

    // 简化解析，生产环境应使用 nlohmann/json
    // TODO: 完整 JSON 解析
    return result;
}

// DiscoveryEvent -> JSON
std::string eventToJson(const DiscoveryEventInfo& event) {
    std::ostringstream oss;
    oss << "{"
        << "\"type\":";
    switch (event.type) {
        case DiscoveryEvent::Added:    oss << "\"Added\""; break;
        case DiscoveryEvent::Removed:  oss << "\"Removed\""; break;
        case DiscoveryEvent::Updated:  oss << "\"Updated\""; break;
        case DiscoveryEvent::Healthy:  oss << "\"Healthy\""; break;
        case DiscoveryEvent::Unhealthy: oss << "\"Unhealthy\""; break;
    }
    oss << ","
        << "\"endpoint\":{"
        << "\"id\":\"" << event.endpoint.id << "\","
        << "\"host\":\"" << event.endpoint.host << "\","
        << "\"port\":" << event.endpoint.port << ","
        << "\"healthy\":" << (event.endpoint.healthy ? "true" : "false")
        << "},"
        << "\"timestamp\":" << event.timestamp
        << "}";
    return oss.str();
}

// JSON -> DiscoveryEventInfo (简化版)
DiscoveryEventInfo jsonToEvent(const std::string& json) {
    DiscoveryEventInfo info;
    // TODO: 完整 JSON 解析
    return info;
}

} // anonymous namespace

//==============================================================================
// RedisServiceDiscovery 实现
//==============================================================================

RedisServiceDiscovery::RedisServiceDiscovery(apollo::net::redis::RedisTemplate redis)
    : redis_(std::move(redis)), ownsRedis_(false) {
}

RedisServiceDiscovery::RedisServiceDiscovery(const ServiceDiscoveryConfig& config)
    : config_(config), ownsRedis_(true) {
    // 内部创建 RedisTemplate
    redis_.connect(config_.redisHost, config_.redisPort,
                  config_.redisPassword, config_.redisDb);
}

RedisServiceDiscovery::~RedisServiceDiscovery() {
    stop();
}

//==============================================================================
// 序列化/反序列化
//==============================================================================

std::map<std::string, std::string> RedisServiceDiscovery::endpointToHash(
    const ServiceEndpoint& ep) const {

    return {
        {"id", ep.id},
        {"host", ep.host},
        {"port", std::to_string(ep.port)},
        {"protocol", ep.protocol},
        {"weight", std::to_string(ep.weight)},
        {"healthy", ep.healthy ? "1" : "0"},
        {"lastHeartbeat", std::to_string(ep.lastHeartbeat)},
        {"metadata", metadataToJson(ep.metadata)}
    };
}

ServiceEndpoint RedisServiceDiscovery::hashToEndpoint(
    const std::map<std::string, std::string>& hash) const {

    ServiceEndpoint ep;

    auto it = hash.find("id");
    if (it != hash.end()) ep.id = it->second;

    it = hash.find("host");
    if (it != hash.end()) ep.host = it->second;

    it = hash.find("port");
    if (it != hash.end()) ep.port = static_cast<uint16_t>(std::stoi(it->second));

    it = hash.find("protocol");
    if (it != hash.end()) ep.protocol = it->second;

    it = hash.find("weight");
    if (it != hash.end()) ep.weight = std::stoi(it->second);

    it = hash.find("healthy");
    if (it != hash.end()) ep.healthy = (it->second == "1");

    it = hash.find("lastHeartbeat");
    if (it != hash.end()) ep.lastHeartbeat = std::stoll(it->second);

    it = hash.find("metadata");
    if (it != hash.end()) ep.metadata = jsonToMetadata(it->second);

    return ep;
}

std::string RedisServiceDiscovery::eventToJson(const DiscoveryEventInfo& event) const {
    return apollo::ipc::eventToJson(event);
}

DiscoveryEventInfo RedisServiceDiscovery::jsonToEvent(const std::string& json) const {
    return apollo::ipc::jsonToEvent(json);
}

//==============================================================================
// 生命周期
//==============================================================================

bool RedisServiceDiscovery::start() {
    if (running_.load()) return true;

    if (!redis_.isConnected()) {
        return false;
    }

    running_.store(true);

    // 启动心跳线程
    heartbeatThread_ = std::thread(&RedisServiceDiscovery::heartbeatThreadFunc, this);

    return true;
}

void RedisServiceDiscovery::stop() {
    if (!running_.load()) return;

    running_.store(false);

    // 停止所有 watcher
    {
        std::lock_guard<std::mutex> lock(watchersMutex_);
        for (auto& [name, info] : watchers_) {
            if (info) {
                info->active = false;
                redis_.unsubscribe(changeChannel(name));

                if (info->subscribeThread.joinable()) {
                    info->subscribeThread.join();
                }
            }
        }
        watchers_.clear();
    }

    // 等待心跳线程
    if (heartbeatThread_.joinable()) {
        heartbeatThread_.join();
    }

    // 注销所有服务
    {
        std::lock_guard<std::mutex> lock(registeredMutex_);
        for (const auto& [serviceName, endpoint] : registeredServices_) {
            deregister(serviceName, endpoint.id);
        }
        registeredServices_.clear();
    }

    // 如果拥有 Redis，断开连接
    if (ownsRedis_) {
        redis_.disconnect();
    }
}

//==============================================================================
// 服务注册
//==============================================================================

bool RedisServiceDiscovery::registerService(const std::string& serviceName,
                                           const ServiceEndpoint& endpoint) {
    if (!redis_.isConnected()) return false;

    std::string id = endpoint.id.empty()
        ? generateServiceId(serviceName, endpoint.host, endpoint.port)
        : endpoint.id;

    ServiceEndpoint ep = endpoint;
    ep.id = id;
    ep.lastHeartbeat = currentTimeMs();

    // 记录注册的服务（用于心跳）
    {
        std::lock_guard<std::mutex> lock(registeredMutex_);
        registeredServices_[serviceName] = ep;
    }

    std::string listKey = serviceListKey(serviceName);
    std::string detailKey = serviceDetailKey(serviceName, id);
    std::string hbKey = heartbeatKey(serviceName, id);
    std::string indexKey = servicesIndexKey();

    // 1. 添加到服务列表
    redis_.sAdd(listKey, id);

    // 2. 设置服务详情
    redis_.hmSet(detailKey, endpointToHash(ep));

    // 3. 设置心跳（带 TTL）
    uint64_t ttlSec = (config_.heartbeatIntervalMs / 1000) + 5;
    redis_.pSetEx(hbKey, ttlSec * 1000, std::to_string(ep.lastHeartbeat));

    // 4. 添加到服务名索引
    redis_.sAdd(indexKey, serviceName);

    // 5. 发布事件
    DiscoveryEventInfo event;
    event.type = DiscoveryEvent::Added;
    event.endpoint = ep;
    event.timestamp = ep.lastHeartbeat;

    redis_.publish(changeChannel(serviceName), eventToJson(event));

    return true;
}

bool RedisServiceDiscovery::deregister(const std::string& serviceName,
                                       const std::string& id) {
    if (!redis_.isConnected()) return false;

    std::string listKey = serviceListKey(serviceName);
    std::string detailKey = serviceDetailKey(serviceName, id);
    std::string hbKey = heartbeatKey(serviceName, id);

    // 批量删除
    redis_.sRem(listKey, id);
    redis_.del(detailKey);
    redis_.del(hbKey);

    // 检查服务是否还有实例
    auto members = redis_.sMembers(listKey);
    if (members.empty()) {
        // 没有实例了，从索引中移除服务名
        std::string indexKey = servicesIndexKey();
        redis_.sRem(indexKey, serviceName);
    }

    {
        std::lock_guard<std::mutex> lock(registeredMutex_);
        registeredServices_.erase(serviceName);
    }

    DiscoveryEventInfo event;
    event.type = DiscoveryEvent::Removed;
    event.endpoint.id = id;
    event.timestamp = currentTimeMs();

    redis_.publish(changeChannel(serviceName), eventToJson(event));

    return true;
}

bool RedisServiceDiscovery::heartbeat(const std::string& serviceName,
                                     const std::string& id) {
    if (!redis_.isConnected()) return false;

    int64_t now = currentTimeMs();
    std::string detailKey = serviceDetailKey(serviceName, id);
    std::string hbKey = heartbeatKey(serviceName, id);

    // 更新详情
    redis_.hSet(detailKey, "lastHeartbeat", std::to_string(now));
    redis_.hSet(detailKey, "healthy", "1");

    // 刷新心跳 TTL
    uint64_t ttlSec = (config_.heartbeatIntervalMs / 1000) + 5;
    redis_.pSetEx(hbKey, ttlSec * 1000, std::to_string(now));

    return true;
}

bool RedisServiceDiscovery::updateService(const std::string& serviceName,
                                         const std::string& id,
                                         const std::map<std::string, std::string>& metadata) {
    if (!redis_.isConnected()) return false;

    std::string detailKey = serviceDetailKey(serviceName, id);
    redis_.hSet(detailKey, "metadata", metadataToJson(metadata));

    DiscoveryEventInfo event;
    event.type = DiscoveryEvent::Updated;
    event.endpoint.id = id;
    event.endpoint.metadata = metadata;
    event.timestamp = currentTimeMs();

    redis_.publish(changeChannel(serviceName), eventToJson(event));

    return true;
}

//==============================================================================
// 服务发现
//==============================================================================

std::vector<ServiceEndpoint> RedisServiceDiscovery::discover(const std::string& serviceName) {
    if (!redis_.isConnected()) return {};

    // 检查缓存
    if (config_.enableCache) {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        auto it = cache_.find(serviceName);
        if (it != cache_.end() && it->second.expiryTime > currentTimeMs()) {
            return it->second.endpoints;
        }
    }

    std::vector<ServiceEndpoint> endpoints;
    std::string listKey = serviceListKey(serviceName);

    // 获取所有服务 ID
    auto ids = redis_.sMembers(listKey);

    // 获取每个服务的详情
    for (const auto& id : ids) {
        std::string detailKey = serviceDetailKey(serviceName, id);

        // 检查心跳是否存在
        std::string hbKey = heartbeatKey(serviceName, id);
        if (!redis_.exists(hbKey)) {
            continue;  // 心跳过期，跳过
        }

        // 获取服务详情
        auto hash = redis_.hGetAll(detailKey);
        if (!hash.empty()) {
            auto ep = hashToEndpoint(hash);
            // 只返回健康的服务
            if (ep.healthy) {
                endpoints.push_back(ep);
            }
        }
    }

    // 更新缓存
    if (config_.enableCache) {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        CacheEntry entry;
        entry.endpoints = endpoints;
        entry.expiryTime = currentTimeMs() + config_.cacheTtlMs;
        cache_[serviceName] = entry;
    }

    return endpoints;
}

void RedisServiceDiscovery::discoverAsync(const std::string& serviceName,
                                         ServiceListCallback callback) {
    auto endpoints = discover(serviceName);
    if (callback) callback(endpoints);
}

ServiceEndpoint RedisServiceDiscovery::selectOne(const std::string& serviceName,
                                                 LoadBalanceStrategy strategy) {
    auto endpoints = discover(serviceName);
    if (endpoints.empty()) return {};

    // 获取或创建负载均衡器
    std::lock_guard<std::mutex> lock(balancersMutex_);

    auto& state = balancers_[serviceName];
    int64_t now = currentTimeMs();

    if (!state.balancer || now - state.lastUpdate > config_.cacheTtlMs) {
        state.endpoints = endpoints;
        state.balancer = LoadBalancer::create(strategy, endpoints);
        state.lastUpdate = now;
    }

    return state.balancer->next();
}

//==============================================================================
// Watch 订阅
//==============================================================================

bool RedisServiceDiscovery::watch(const std::string& serviceName,
                                 ServiceChangeCallback callback) {
    std::lock_guard<std::mutex> lock(watchersMutex_);

    if (watchers_.find(serviceName) != watchers_.end()) {
        return true;  // 已经订阅
    }

    auto info = std::make_unique<WatcherInfo>();
    info->callback = callback;
    info->active = true;
    info->serviceName = serviceName;

    // 启动订阅线程
    info->subscribeThread = std::thread([this, serviceName, infoPtr = info.get()]() {
        std::string channel = changeChannel(serviceName);

        redis_.subscribe(channel, [this, serviceName, infoPtr](const std::string& ch,
                                                                const std::string& msg) {
            if (!infoPtr->active.load() || ch != channel) return;

            // 解析事件
            auto event = jsonToEvent(msg);

            if (infoPtr->callback) {
                infoPtr->callback(event);
            }
        });
    });

    watchers_[serviceName] = std::move(info);
    return true;
}

void RedisServiceDiscovery::unwatch(const std::string& serviceName) {
    std::lock_guard<std::mutex> lock(watchersMutex_);

    auto it = watchers_.find(serviceName);
    if (it != watchers_.end()) {
        it->second->active = false;
        redis_.unsubscribe(changeChannel(serviceName));

        if (it->second->subscribeThread.joinable()) {
            it->second->subscribeThread.join();
        }

        watchers_.erase(it);
    }
}

void RedisServiceDiscovery::subscribeThreadFunc(const std::string& serviceName,
                                                ServiceChangeCallback callback) {
    // 这个函数不再使用，逻辑移到 watch() 中
}

//==============================================================================
// 健康检查
//==============================================================================

bool RedisServiceDiscovery::markUnhealthy(const std::string& serviceName,
                                          const std::string& id) {
    if (!redis_.isConnected()) return false;

    std::string detailKey = serviceDetailKey(serviceName, id);
    redis_.hSet(detailKey, "healthy", "0");

    return true;
}

bool RedisServiceDiscovery::markHealthy(const std::string& serviceName,
                                        const std::string& id) {
    if (!redis_.isConnected()) return false;

    std::string detailKey = serviceDetailKey(serviceName, id);
    redis_.hSet(detailKey, "healthy", "1");

    // 刷新心跳
    int64_t now = currentTimeMs();
    std::string hbKey = heartbeatKey(serviceName, id);
    uint64_t ttlSec = (config_.heartbeatIntervalMs / 1000) + 5;
    redis_.pSetEx(hbKey, ttlSec * 1000, std::to_string(now));

    return true;
}

std::vector<std::string> RedisServiceDiscovery::getServiceNames() const {
    if (!redis_.isConnected()) return {};

    std::string indexKey = servicesIndexKey();
    return redis_.sMembers(indexKey);
}

//==============================================================================
// 后台线程
//==============================================================================

void RedisServiceDiscovery::heartbeatThreadFunc() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.heartbeatIntervalMs));

        std::lock_guard<std::mutex> lock(registeredMutex_);
        for (const auto& [serviceName, endpoint] : registeredServices_) {
            heartbeat(serviceName, endpoint.id);
        }
    }
}

} // namespace ipc
} // namespace apollo
