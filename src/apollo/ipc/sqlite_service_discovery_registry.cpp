/**
 * @file sqlite_service_discovery_registry.cpp
 * @brief SQLite 服务注册实现（register/deregister/heartbeat/update）
 */

#include "apollo/ipc/sqlite_service_discovery.h"
#include "sqlite_service_discovery_detail.hpp"
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace apollo {
namespace ipc {

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

} // namespace ipc
} // namespace apollo
