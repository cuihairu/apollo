#include "apollo/storage/database/connection.hpp"
#include <thread>
#include <algorithm>
#include <queue>

namespace apollo::db {

ConnectionPool::ConnectionPool(const ConnectionPoolConfig& config)
    : config_(config) {
    stats_.totalConnections = 0;
    stats_.activeConnections = 0;
    stats_.idleConnections = 0;
    stats_.createdConnections = 0;
    stats_.destroyedConnections = 0;
}

ConnectionPool::~ConnectionPool() {
    CloseAll();
}

bool ConnectionPool::Initialize() {
    // 创建最小数量的连接
    for (size_t i = 0; i < config_.minConnections; ++i) {
        CreateConnection();
    }

    // 启动连接检查线程
    checkThread_ = std::thread(&ConnectionPool::CheckConnections, this);

    return true;
}

std::shared_ptr<IDBConnection> ConnectionPool::GetConnection() {
    std::unique_lock<std::mutex> lock(mutex_);

    // 如果有空闲连接，直接返回
    if (!idleConnections_.empty()) {
        auto conn = idleConnections_.front();
        idleConnections_.pop();
        stats_.idleConnections--;
        stats_.activeConnections++;

        // 检查连接是否有效
        if (!ValidateConnection(conn)) {
            // 连接无效，重新创建
            CreateConnection();
            conn = idleConnections_.front();
            idleConnections_.pop();
            stats_.idleConnections--;
            stats_.activeConnections++;
        }

        return conn;
    }

    // 如果还能创建新连接
    if (allConnections_.size() < config_.maxConnections) {
        CreateConnection();
        auto conn = allConnections_.back();
        allConnections_.pop_back();
        stats_.activeConnections++;

        return conn;
    }

    // 等待空闲连接
    condition_.wait(lock, [this]() {
        return !idleConnections_.empty() || shutdown_;
    });

    if (shutdown_) {
        return nullptr;
    }

    auto conn = idleConnections_.front();
    idleConnections_.pop();
    stats_.idleConnections--;
    stats_.activeConnections++;

    if (!ValidateConnection(conn)) {
        CreateConnection();
        conn = idleConnections_.front();
        idleConnections_.pop();
        stats_.idleConnections--;
        stats_.activeConnections++;
    }

    return conn;
}

void ConnectionPool::ReturnConnection(std::shared_ptr<IDBConnection> conn) {
    if (!conn) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    if (shutdown_) {
        // 连接池正在关闭，直接销毁连接
        allConnections_.erase(
            std::remove(allConnections_.begin(), allConnections_.end(), conn)
        );
        stats_.activeConnections--;
        stats_.destroyedConnections++;
        return;
    }

    // 连接有效，放回空闲队列
    idleConnections_.push(conn);
    stats_.idleConnections++;
    stats_.activeConnections--;

    condition_.notify_one();
}

void ConnectionPool::CloseAll() {
    shutdown_ = true;
    condition_.notify_all();

    if (checkThread_.joinable()) {
        checkThread_.join();
    }

    std::lock_guard<std::mutex> lock(mutex_);
    // 销毁所有连接
    allConnections_.clear();
    std::queue<std::shared_ptr<IDBConnection>> empty;
    std::swap(idleConnections_, empty);

    stats_.totalConnections = 0;
    stats_.activeConnections = 0;
    stats_.idleConnections = 0;
}

ConnectionPool::Stats ConnectionPool::GetStats() const {
    return stats_;
}

void ConnectionPool::CreateConnection() {
    // 这里需要根据配置创建实际的连接
    // 简化实现，实际项目中需要依赖注入
    // TODO: 实现工厂模式创建不同类型的数据库连接
    // auto factory = std::make_unique<MySQLConnectionFactory>();
    // auto conn = factory->CreateConnection(config_);

    // 暂时返回，等待MySQL连接实现
    return;
}

void ConnectionPool::CheckConnections() {
    while (!shutdown_) {
        std::this_thread::sleep_for(std::chrono::seconds(config_.checkInterval));

        if (shutdown_) {
            break;
        }

        std::unique_lock<std::mutex> lock(mutex_);
        size_t i = 0;
        while (i < idleConnections_.size()) {
            auto conn = idleConnections_.front();
            idleConnections_.pop();

            if (!ValidateConnection(conn)) {
                // 连接无效，移除
                allConnections_.erase(
                    std::remove(allConnections_.begin(), allConnections_.end(), conn)
                );
                stats_.totalConnections--;
                stats_.idleConnections--;
                stats_.destroyedConnections++;

                // 尝试创建新连接
                CreateConnection();
            } else {
                // 连接有效，放回队列
                idleConnections_.push(conn);
                stats_.idleConnections++;
                i++;
            }
        }
        condition_.notify_all();
    }
}

bool ConnectionPool::ValidateConnection(std::shared_ptr<IDBConnection> conn) {
    if (!conn) {
        return false;
    }

    // 检查连接状态
    if (!conn->IsConnected()) {
        return false;
    }

    // Ping测试
    return conn->Ping();
}

}  // namespace apollo::db