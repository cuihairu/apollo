/**
 * @file datasource.cpp
 * @brief DataSource 和连接池实现 (JDBC 风格)
 */

#include "apollo/database/datasource.h"
#include <algorithm>
#include <random>
#include <thread>

namespace apollo {
namespace database {

//==============================================================================
// ConnectionPool::PooledConnection 实现
//==============================================================================

ConnectionPool::PooledConnection::PooledConnection(
    std::unique_ptr<IDatabaseConnection> conn,
    ConnectionPool* pool)
    : conn_(std::move(conn))
    , pool_(pool) {
}

ConnectionPool::PooledConnection::~PooledConnection() {
    if (pool_ && conn_) {
        pool_->returnConnection(std::move(conn_));
    }
}

bool ConnectionPool::PooledConnection::connect(const std::string& connStr) {
    return conn_ ? conn_->connect(connStr) : false;
}

void ConnectionPool::PooledConnection::disconnect() {
    if (conn_) {
        conn_->disconnect();
    }
}

bool ConnectionPool::PooledConnection::isConnected() const {
    return conn_ ? conn_->isConnected() : false;
}

bool ConnectionPool::PooledConnection::ping() {
    return conn_ ? conn_->ping() : false;
}

std::vector<std::vector<std::string>> ConnectionPool::PooledConnection::query(
    const std::string& sql) {
    return conn_ ? conn_->query(sql) : std::vector<std::vector<std::string>>();
}

std::vector<std::vector<std::string>> ConnectionPool::PooledConnection::query(
    const std::string& sql,
    const std::vector<std::string>& params) {
    return conn_ ? conn_->query(sql, params) : std::vector<std::vector<std::string>>();
}

bool ConnectionPool::PooledConnection::execute(const std::string& sql) {
    return conn_ ? conn_->execute(sql) : false;
}

bool ConnectionPool::PooledConnection::execute(
    const std::string& sql,
    const std::vector<std::string>& params) {
    return conn_ ? conn_->execute(sql, params) : false;
}

bool ConnectionPool::PooledConnection::begin() {
    return conn_ ? conn_->begin() : false;
}

bool ConnectionPool::PooledConnection::commit() {
    return conn_ ? conn_->commit() : false;
}

bool ConnectionPool::PooledConnection::rollback() {
    return conn_ ? conn_->rollback() : false;
}

std::string ConnectionPool::PooledConnection::escape(const std::string& str) {
    return conn_ ? conn_->escape(str) : str;
}

int64_t ConnectionPool::PooledConnection::lastInsertId() {
    return conn_ ? conn_->lastInsertId() : 0;
}

int64_t ConnectionPool::PooledConnection::rowsAffected() {
    return conn_ ? conn_->rowsAffected() : 0;
}

//==============================================================================
// ConnectionPool 实现
//==============================================================================

ConnectionPool::ConnectionPool(DbType type, const std::string& connectionString)
    : connectionString_(connectionString) {

    // 从注册表获取对应类型的工厂
    // 这里简化处理，实际应该有工厂创建逻辑
    (void)type;
}

ConnectionPool::ConnectionPool(
    std::unique_ptr<IDatabaseConnectionFactory> factory,
    const std::string& connectionString)
    : factory_(std::move(factory))
    , connectionString_(connectionString) {
}

ConnectionPool::~ConnectionPool() {
    stop();
}

bool ConnectionPool::start() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (running_) {
        return true;
    }

    // 预创建最小连接数
    for (size_t i = 0; i < config_.minSize; ++i) {
        auto conn = createConnection();
        if (conn) {
            idleConnections_.push_back(std::move(conn));
            ++idleCount_;
        }
    }

    running_ = true;

    // 启动维护线程
    stopMaintenance_ = false;
    maintenanceThread_ = std::thread([this]() { maintenanceThread(); });

    return true;
}

void ConnectionPool::stop() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!running_) {
        return;
    }

    running_ = false;
    stopMaintenance_ = true;
    cv_.notify_all();

    if (maintenanceThread_.joinable()) {
        maintenanceThread_.join();
    }

    // 关闭所有连接
    idleConnections_.clear();
    idleCount_ = 0;
    activeConnections_.clear();
    activeCount_ = 0;
}

std::shared_ptr<IDatabaseConnection> ConnectionPool::getConnection() {
    return getConnection(config_.checkoutTimeoutMs);
}

std::shared_ptr<IDatabaseConnection> ConnectionPool::getConnection(uint32_t timeoutMs) {
    std::unique_lock<std::mutex> lock(mutex_);

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

    // 等待可用连接
    while (running_) {
        // 尝试从空闲池获取
        if (!idleConnections_.empty()) {
            auto conn = std::move(idleConnections_.back());
            idleConnections_.pop_back();
            --idleCount_;

            // 验证连接
            if (validateConnection(conn.get())) {
                // 包装为 PooledConnection
                auto pooled = std::make_shared<PooledConnection>(std::move(conn), this);
                activeConnections_.push_back(pooled.get());
                ++activeCount_;
                return pooled;
            } else {
                // 连接无效，创建新连接
                conn = createConnection();
                if (conn && validateConnection(conn.get())) {
                    auto pooled = std::make_shared<PooledConnection>(std::move(conn), this);
                    activeConnections_.push_back(pooled.get());
                    ++activeCount_;
                    return pooled;
                }
            }
        }

        // 检查是否可以创建新连接
        if (getTotalConnections() < config_.maxSize) {
            auto conn = createConnection();
            if (conn && validateConnection(conn.get())) {
                auto pooled = std::make_shared<PooledConnection>(std::move(conn), this);
                activeConnections_.push_back(pooled.get());
                ++activeCount_;
                return pooled;
            }
        }

        // 等待连接归还
        if (cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
            break;
        }
    }

    return nullptr;
}

void ConnectionPool::returnConnection(std::unique_ptr<IDatabaseConnection> conn) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 从活跃列表移除
    auto it = std::find(activeConnections_.begin(), activeConnections_.end(), conn.get());
    if (it != activeConnections_.end()) {
        activeConnections_.erase(it);
        --activeCount_;
    }

    // 归还验证
    if (config_.validateOnReturn && !validateConnection(conn.get())) {
        return;  // 丢弃无效连接
    }

    // 放回空闲池
    if (idleCount_ < config_.maxSize) {
        idleConnections_.push_back(std::move(conn));
        ++idleCount_;
    }

    cv_.notify_one();
}

std::unique_ptr<IDatabaseConnection> ConnectionPool::createConnection() {
    if (!factory_) {
        return nullptr;
    }

    auto conn = factory_->create();
    if (conn && conn->connect(connectionString_)) {
        return conn;
    }

    return nullptr;
}

bool ConnectionPool::validateConnection(IDatabaseConnection* conn) {
    if (!conn || !conn->isConnected()) {
        return false;
    }

    if (config_.validateOnCheckout || config_.validateOnBorrow) {
        return conn->ping();
    }

    return true;
}

void ConnectionPool::setPoolConfig(const PoolConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

void ConnectionPool::maintenanceThread() {
    while (!stopMaintenance_) {
        std::this_thread::sleep_for(std::chrono::seconds(30));

        std::lock_guard<std::mutex> lock(mutex_);

        if (!running_) {
            break;
        }

        // 清理超时的空闲连接
        auto now = std::chrono::steady_clock::now();

        auto it = idleConnections_.begin();
        while (it != idleConnections_.end()) {
            // TODO: 检查连接空闲时间和生命周期
            // 简化实现：保留最小连接数
            if (idleConnections_.size() > config_.minSize) {
                it = idleConnections_.erase(it);
                --idleCount_;
            } else {
                ++it;
            }
        }
    }
}

//==============================================================================
// SqlTemplateBuilder 实现
//==============================================================================

SqlTemplateBuilder::SqlTemplateBuilder() = default;

SqlTemplateBuilder& SqlTemplateBuilder::url(const std::string& url) {
    config_.url = url;
    return *this;
}

SqlTemplateBuilder& SqlTemplateBuilder::host(const std::string& host) {
    config_.host = host;
    return *this;
}

SqlTemplateBuilder& SqlTemplateBuilder::port(uint16_t port) {
    config_.port = port;
    return *this;
}

SqlTemplateBuilder& SqlTemplateBuilder::database(const std::string& database) {
    config_.database = database;
    return *this;
}

SqlTemplateBuilder& SqlTemplateBuilder::username(const std::string& username) {
    config_.username = username;
    return *this;
}

SqlTemplateBuilder& SqlTemplateBuilder::password(const std::string& password) {
    config_.password = password;
    return *this;
}

SqlTemplateBuilder& SqlTemplateBuilder::minSize(size_t size) {
    config_.poolConfig.minSize = size;
    return *this;
}

SqlTemplateBuilder& SqlTemplateBuilder::maxSize(size_t size) {
    config_.poolConfig.maxSize = size;
    return *this;
}

SqlTemplateBuilder& SqlTemplateBuilder::connectTimeout(uint32_t timeoutMs) {
    config_.poolConfig.connectTimeoutMs = timeoutMs;
    return *this;
}

SqlTemplateBuilder& SqlTemplateBuilder::idleTimeout(uint32_t timeoutMs) {
    config_.poolConfig.idleTimeoutMs = timeoutMs;
    return *this;
}

SqlTemplateBuilder& SqlTemplateBuilder::maxLifetime(uint32_t lifetimeMs) {
    config_.poolConfig.maxLifetimeMs = lifetimeMs;
    return *this;
}

SqlTemplateBuilder& SqlTemplateBuilder::validateOnCheckout(bool enable) {
    config_.poolConfig.validateOnCheckout = enable;
    return *this;
}

SqlTemplateBuilder& SqlTemplateBuilder::validationQuery(const std::string& sql) {
    config_.poolConfig.validationQuery = sql;
    return *this;
}

SqlTemplateBuilder& SqlTemplateBuilder::mode(BuildMode mode) {
    config_.buildMode = mode;
    return *this;
}

DbType SqlTemplateBuilder::detectType() const {
    if (!config_.url.empty()) {
        if (config_.url.find("sqlite://") == 0) {
            return DbType::SQLite;
        } else if (config_.url.find("postgres") == 0) {
            return DbType::PostgreSQL;
        }
    }
    return DbType::MySQL;  // 默认
}

std::string SqlTemplateBuilder::buildUrl() const {
    if (!config_.url.empty()) {
        return config_.url;
    }

    auto type = detectType();
    if (type == DbType::SQLite) {
        return "sqlite://" + config_.database;
    } else if (type == DbType::PostgreSQL) {
        return "postgres://" + config_.username + ":" + config_.password + "@" +
               config_.host + ":" + std::to_string(config_.port) + "/" + config_.database;
    } else {
        return "mysql://" + config_.username + ":" + config_.password + "@" +
               config_.host + ":" + std::to_string(config_.port) + "/" + config_.database;
    }
}

SqlTemplate SqlTemplateBuilder::build() {
    std::string url = buildUrl();

    // 根据模式选择是否使用连接池
    bool usePool = (config_.buildMode == BuildMode::Pooled);
    if (config_.buildMode == BuildMode::Auto) {
        // 自动选择：生产环境使用连接池
        usePool = (config_.poolConfig.maxSize > 1);
    }

    if (usePool) {
        auto dataSource = std::make_shared<ConnectionPool>(
            detectType(), url);
        dataSource->setPoolConfig(config_.poolConfig);
        dataSource->start();

        return SqlTemplate(dataSource->getConnection());
    }

    return SqlTemplate(url);
}

std::unique_ptr<SqlTemplate> SqlTemplateBuilder::buildUnique() {
    return std::make_unique<SqlTemplate>(build());
}

std::shared_ptr<DataSource> SqlTemplateBuilder::buildDataSource() {
    auto pool = std::make_shared<ConnectionPool>(detectType(), buildUrl());
    pool->setPoolConfig(config_.poolConfig);
    pool->start();
    return pool;
}

} // namespace database
} // namespace apollo
