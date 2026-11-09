/**
 * @file sqlite_service_discovery.cpp
 * @brief SQLite 服务发现实现 - 轻量级本地服务注册中心
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
// 辅助函数
//==============================================================================
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

} // namespace ipc
} // namespace apollo
