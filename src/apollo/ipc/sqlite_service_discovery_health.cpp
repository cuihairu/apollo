/**
 * @file sqlite_service_discovery_health.cpp
 * @brief SQLite 服务健康检查与后台线程实现
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

} // namespace ipc
} // namespace apollo
