/**
 * @file sqlite_service_discovery.cpp
 * @brief SQLite 服务发现实现 - 轻量级本地服务注册中心
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
// 辅助函数
//==============================================================================

namespace {

std::string metadataToJson(const std::map<std::string, std::string>& metadata) {
    if (metadata.empty()) return "{}";
    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (const auto& [k, v] : metadata) {
        if (!first) oss << ",";
        oss << "\"" << k << "\":\"" << v << "\"";
        first = false;
    }
    oss << "}";
    return oss.str();
}

std::map<std::string, std::string> jsonToMetadata(const std::string& json) {
    std::map<std::string, std::string> result;
    // 简单解析（生产环境应使用 nlohmann/json 或类似库）
    if (json.empty() || json == "{}") return result;
    // TODO: 完整的 JSON 解析
    return result;
}

} // anonymous namespace

//==============================================================================
// ServiceEndpoint 序列化
//==============================================================================

std::string ServiceEndpoint::toJson() const {
    std::ostringstream oss;
    oss << "{"
        << "\"id\":\"" << id << "\","
        << "\"host\":\"" << host << "\","
        << "\"port\":" << port << ","
        << "\"protocol\":\"" << protocol << "\","
        << "\"weight\":" << weight << ","
        << "\"healthy\":" << (healthy ? "true" : "false") << ","
        << "\"lastHeartbeat\":" << lastHeartbeat << ","
        << "\"metadata\":" << metadataToJson(metadata)
        << "}";
    return oss.str();
}

ServiceEndpoint ServiceEndpoint::fromJson(const std::string& json) {
    ServiceEndpoint ep;
    // TODO: 完整的 JSON 解析
    // 简化版本：假设 json 格式正确
    return ep;
}

//==============================================================================
// SQLiteServiceDiscovery 实现
//==============================================================================

SQLiteServiceDiscovery::SQLiteServiceDiscovery(const ServiceDiscoveryConfig& config)
    : config_(config) {
}

SQLiteServiceDiscovery::~SQLiteServiceDiscovery() {
    stop();
}

bool SQLiteServiceDiscovery::start() {
    if (running_.load()) return true;

    if (!initDatabase()) {
        return false;
    }

    running_.store(true);

    // 启动心跳线程
    heartbeatThread_ = std::thread(&SQLiteServiceDiscovery::heartbeatThreadFunc, this);

    // 启动清理线程
    cleanupThread_ = std::thread(&SQLiteServiceDiscovery::cleanupThreadFunc, this);

    return true;
}

void SQLiteServiceDiscovery::stop() {
    if (!running_.load()) return;

    running_.store(false);

    // 停止所有 watcher
    {
        std::lock_guard<std::mutex> lock(watchersMutex_);
        for (auto& [name, watcher] : watchers_) {
            if (watcher) {
                watcher->active.store(false);
                if (watcher->notifyThread.joinable()) {
                    watcher->notifyThread.join();
                }
            }
        }
        watchers_.clear();
    }

    // 等待线程结束
    if (heartbeatThread_.joinable()) {
        heartbeatThread_.join();
    }
    if (cleanupThread_.joinable()) {
        cleanupThread_.join();
    }

    // 关闭数据库
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool SQLiteServiceDiscovery::initDatabase() {
    int rc = sqlite3_open(config_.dbPath.c_str(), &db_);
    if (rc != SQLITE_OK) {
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }

    // 启用外键约束
    sqlite3_exec(db_, "PRAGMA foreign_keys = ON", nullptr, nullptr, nullptr);

    // 创建表
    return createTables();
}

bool SQLiteServiceDiscovery::createTables() {
    const char* createServicesTable = R"(
        CREATE TABLE IF NOT EXISTS services (
            id TEXT PRIMARY KEY,
            service_name TEXT NOT NULL,
            host TEXT NOT NULL,
            port INTEGER NOT NULL,
            protocol TEXT DEFAULT 'tcp',
            metadata TEXT DEFAULT '{}',
            weight INTEGER DEFAULT 1,
            created_at INTEGER NOT NULL,
            last_heartbeat INTEGER NOT NULL
        )
    )";

    const char* createHealthTable = R"(
        CREATE TABLE IF NOT EXISTS service_health (
            id TEXT PRIMARY KEY,
            healthy INTEGER DEFAULT 1,
            last_check INTEGER NOT NULL,
            FOREIGN KEY(id) REFERENCES services(id) ON DELETE CASCADE
        )
    )";

    const char* createIndexes = R"(
        CREATE INDEX IF NOT EXISTS idx_service_name ON services(service_name);
        CREATE INDEX IF NOT EXISTS idx_last_heartbeat ON services(last_heartbeat);
    )";

    char* errMsg = nullptr;
    int rc;

    rc = sqlite3_exec(db_, createServicesTable, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        sqlite3_free(errMsg);
        return false;
    }

    rc = sqlite3_exec(db_, createHealthTable, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        sqlite3_free(errMsg);
        return false;
    }

    rc = sqlite3_exec(db_, createIndexes, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        sqlite3_free(errMsg);
        return false;
    }

    return true;
}

//==============================================================================
// 服务注册
//==============================================================================

bool SQLiteServiceDiscovery::registerService(const std::string& serviceName,
                                            const ServiceEndpoint& endpoint) {
    if (!db_) return false;

    std::lock_guard<std::mutex> lock(registeredMutex_);

    // 记录注册的服务（用于心跳）
    registeredServices_[endpoint.id] = {serviceName, endpoint};

    return insertService(serviceName, endpoint);
}

bool SQLiteServiceDiscovery::insertService(const std::string& serviceName,
                                          const ServiceEndpoint& endpoint) {
    const char* sql = R"(
        INSERT OR REPLACE INTO services
        (id, service_name, host, port, protocol, metadata, weight, created_at, last_heartbeat)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    int64_t now = currentTimeMs();

    sqlite3_bind_text(stmt, 1, endpoint.id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, serviceName.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, endpoint.host.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, endpoint.port);
    sqlite3_bind_text(stmt, 5, endpoint.protocol.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, metadataToJson(endpoint.metadata).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, endpoint.weight);
    sqlite3_bind_int64(stmt, 8, now);
    sqlite3_bind_int64(stmt, 9, now);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return false;

    // 初始化健康状态
    const char* healthSql = "INSERT OR REPLACE INTO service_health (id, healthy, last_check) VALUES (?, 1, ?)";
    rc = sqlite3_prepare_v2(db_, healthSql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, endpoint.id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, now);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    // 发布事件给 watcher
    DiscoveryEventInfo event;
    event.type = DiscoveryEvent::Added;
    event.endpoint = endpoint;
    event.timestamp = now;
    notifyWatchers(event);

    return rc == SQLITE_DONE;
}

bool SQLiteServiceDiscovery::deregister(const std::string& serviceName,
                                        const std::string& id) {
    if (!db_) return false;

    std::lock_guard<std::mutex> lock(registeredMutex_);
    registeredServices_.erase(id);

    const char* sql = "DELETE FROM services WHERE id = ?";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    DiscoveryEventInfo event;
    event.type = DiscoveryEvent::Removed;
    event.endpoint.id = id;
    event.timestamp = currentTimeMs();
    notifyWatchers(event);

    return rc == SQLITE_DONE;
}

bool SQLiteServiceDiscovery::heartbeat(const std::string& serviceName,
                                      const std::string& id) {
    if (!db_) return false;

    return updateHeartbeat(serviceName, id);
}

bool SQLiteServiceDiscovery::updateHeartbeat(const std::string& serviceName,
                                            const std::string& id) {
    const char* sql = "UPDATE services SET last_heartbeat = ? WHERE id = ?";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    int64_t now = currentTimeMs();
    sqlite3_bind_int64(stmt, 1, now);
    sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool SQLiteServiceDiscovery::updateService(const std::string& serviceName,
                                          const std::string& id,
                                          const std::map<std::string, std::string>& metadata) {
    if (!db_) return false;

    const char* sql = "UPDATE services SET metadata = ? WHERE id = ?";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    std::string metaJson = metadataToJson(metadata);
    sqlite3_bind_text(stmt, 1, metaJson.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    DiscoveryEventInfo event;
    event.type = DiscoveryEvent::Updated;
    event.endpoint.id = id;
    event.timestamp = currentTimeMs();
    notifyWatchers(event);

    return rc == SQLITE_DONE;
}

//==============================================================================
// 服务发现
//==============================================================================

std::vector<ServiceEndpoint> SQLiteServiceDiscovery::discover(const std::string& serviceName) {
    if (!db_) return {};

    const char* sql = R"(
        SELECT s.id, s.host, s.port, s.protocol, s.metadata, s.weight,
               s.last_heartbeat, h.healthy
        FROM services s
        LEFT JOIN service_health h ON s.id = h.id
        WHERE s.service_name = ? AND h.healthy = 1
        ORDER BY s.weight DESC
    )";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return {};

    sqlite3_bind_text(stmt, 1, serviceName.c_str(), -1, SQLITE_STATIC);

    std::vector<ServiceEndpoint> endpoints;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ServiceEndpoint ep;
        ep.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        ep.host = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        ep.port = sqlite3_column_int(stmt, 2);
        ep.protocol = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        ep.weight = sqlite3_column_int(stmt, 5);
        ep.lastHeartbeat = sqlite3_column_int64(stmt, 6);
        ep.healthy = sqlite3_column_int(stmt, 7) != 0;

        // 解析 metadata
        const char* metaJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        if (metaJson) {
            ep.metadata = jsonToMetadata(metaJson);
        }

        endpoints.push_back(ep);
    }

    sqlite3_finalize(stmt);
    return endpoints;
}

void SQLiteServiceDiscovery::discoverAsync(const std::string& serviceName,
                                          ServiceListCallback callback) {
    // 简单实现：直接在当前线程执行
    // 生产环境应使用线程池
    auto endpoints = discover(serviceName);
    if (callback) callback(endpoints);
}

ServiceEndpoint SQLiteServiceDiscovery::selectOne(const std::string& serviceName,
                                                  LoadBalanceStrategy strategy) {
    auto endpoints = discover(serviceName);
    if (endpoints.empty()) return {};

    // 获取或创建负载均衡器
    std::lock_guard<std::mutex> lock(balancersMutex_);

    auto& state = balancers_[serviceName];
    int64_t now = currentTimeMs();

    // 检查是否需要更新
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

bool SQLiteServiceDiscovery::watch(const std::string& serviceName,
                                  ServiceChangeCallback callback) {
    std::lock_guard<std::mutex> lock(watchersMutex_);

    if (watchers_.find(serviceName) != watchers_.end()) {
        return true;  // 已经订阅
    }

    auto watcher = std::make_unique<Watcher>();
    watcher->callback = callback;
    watcher->active = true;
    watcher->lastEndpoints = discover(serviceName);

    // 启动通知线程（轮询模式）
    watcher->notifyThread = std::thread([this, serviceName, watcherPtr = watcher.get()]() {
        while (watcherPtr->active.load() && running_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));

            auto currentEndpoints = discover(serviceName);

            // 检测变化
            // 简化版本：只检测数量变化
            if (currentEndpoints.size() != watcherPtr->lastEndpoints.size()) {
                DiscoveryEventInfo event;
                event.type = currentEndpoints.size() > watcherPtr->lastEndpoints.size()
                    ? DiscoveryEvent::Added : DiscoveryEvent::Removed;
                event.timestamp = currentTimeMs();

                if (watcherPtr->callback) {
                    for (const auto& ep : currentEndpoints) {
                        event.endpoint = ep;
                        watcherPtr->callback(event);
                    }
                }

                watcherPtr->lastEndpoints = currentEndpoints;
            }
        }
    });

    watchers_[serviceName] = std::move(watcher);
    return true;
}

void SQLiteServiceDiscovery::unwatch(const std::string& serviceName) {
    std::lock_guard<std::mutex> lock(watchersMutex_);

    auto it = watchers_.find(serviceName);
    if (it != watchers_.end()) {
        it->second->active = false;
        if (it->second->notifyThread.joinable()) {
            it->second->notifyThread.join();
        }
        watchers_.erase(it);
    }
}

void SQLiteServiceDiscovery::notifyWatchers(const DiscoveryEventInfo& event) {
    // 简化实现：通过轮询，watcher 会自动检测变化
    // 生产环境可以使用条件变量来立即通知
}

//==============================================================================
// 健康检查
//==============================================================================

bool SQLiteServiceDiscovery::markUnhealthy(const std::string& serviceName,
                                          const std::string& id) {
    if (!db_) return false;

    const char* sql = "UPDATE service_health SET healthy = 0, last_check = ? WHERE id = ?";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    int64_t now = currentTimeMs();
    sqlite3_bind_int64(stmt, 1, now);
    sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool SQLiteServiceDiscovery::markHealthy(const std::string& serviceName,
                                        const std::string& id) {
    if (!db_) return false;

    const char* sql = "UPDATE service_health SET healthy = 1, last_check = ? WHERE id = ?";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    int64_t now = currentTimeMs();
    sqlite3_bind_int64(stmt, 1, now);
    sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

std::vector<std::string> SQLiteServiceDiscovery::getServiceNames() const {
    if (!db_) return {};

    const char* sql = "SELECT DISTINCT service_name FROM services";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return {};

    std::vector<std::string> names;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        names.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
    }

    sqlite3_finalize(stmt);
    return names;
}

//==============================================================================
// 后台线程
//==============================================================================

void SQLiteServiceDiscovery::heartbeatThreadFunc() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.heartbeatIntervalMs));

        std::lock_guard<std::mutex> lock(registeredMutex_);
        for (const auto& [id, info] : registeredServices_) {
            updateHeartbeat(info.first, id);
        }
    }
}

void SQLiteServiceDiscovery::cleanupThreadFunc() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.cleanupIntervalMs));
        cleanupExpiredServices();
    }
}

void SQLiteServiceDiscovery::cleanupExpiredServices() {
    if (!db_) return;

    int64_t expiredBefore = currentTimeMs() - config_.heartbeatTimeoutMs;

    const char* sql = R"(
        UPDATE service_health SET healthy = 0
        WHERE id IN (
            SELECT s.id FROM services s
            WHERE s.last_heartbeat < ?
        )
    )";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return;

    sqlite3_bind_int64(stmt, 1, expiredBefore);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

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
