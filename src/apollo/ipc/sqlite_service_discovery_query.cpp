/**
 * @file sqlite_service_discovery_query.cpp
 * @brief SQLite 服务发现与 Watch 订阅实现
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

} // namespace ipc
} // namespace apollo
