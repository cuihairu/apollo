#pragma once

#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include "apollo/database/sql_template.h"

namespace apollo {
namespace database {

//==============================================================================
// 连接池配置
//==============================================================================

struct PoolConfig {
    size_t minSize = 2;              // 最小连接数
    size_t maxSize = 10;             // 最大连接数
    uint32_t connectTimeoutMs = 5000;    // 连接超时
    uint32_t idleTimeoutMs = 60000;      // 空闲超时
    uint32_t maxLifetimeMs = 1800000;    // 连接最大生命周期 (30分钟)
    uint32_t checkoutTimeoutMs = 30000;  // 获取连接超时

    bool validateOnCheckout = true;  // 获取时验证
    bool validateOnReturn = false;   // 归还时验证
    bool validateOnBorrow = true;    // 借出时验证

    // 验证 SQL (不同数据库不同)
    std::string validationQuery = "SELECT 1";

    // 连接泄漏检测
    bool leakDetection = false;
    uint32_t leakDetectionThresholdMs = 60000;  // 60秒
};

//==============================================================================
// DataSource - JDBC 风格的数据源抽象
//==============================================================================

class DataSource {
public:
    virtual ~DataSource() = default;

    // 获取数据库连接
    virtual std::shared_ptr<IDatabaseConnection> getConnection() = 0;
    virtual std::shared_ptr<IDatabaseConnection> getConnection(uint32_t timeoutMs) = 0;

    // 配置
    virtual void setPoolConfig(const PoolConfig& config) = 0;
    virtual const PoolConfig& getPoolConfig() const = 0;

    // 状态
    virtual size_t getActiveConnections() const = 0;
    virtual size_t getIdleConnections() const = 0;
    virtual size_t getTotalConnections() const = 0;

    // 生命周期
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
};

//==============================================================================
// 连接池实现
//==============================================================================

class ConnectionPool : public DataSource {
public:
    // 构造函数
    ConnectionPool(DbType type, const std::string& connectionString);
    ConnectionPool(std::unique_ptr<IDatabaseConnectionFactory> factory,
                   const std::string& connectionString);

    ~ConnectionPool() override;

    // DataSource 接口实现
    std::shared_ptr<IDatabaseConnection> getConnection() override;
    std::shared_ptr<IDatabaseConnection> getConnection(uint32_t timeoutMs) override;

    void setPoolConfig(const PoolConfig& config) override;
    const PoolConfig& getPoolConfig() const override { return config_; }

    size_t getActiveConnections() const override { return activeCount_; }
    size_t getIdleConnections() const override { return idleCount_; }
    size_t getTotalConnections() const override { return activeCount_ + idleCount_; }

    bool start() override;
    void stop() override;
    bool isRunning() const override { return running_; }

private:
    // 连接包装器 (自动归还)
    class PooledConnection : public IDatabaseConnection {
    public:
        PooledConnection(std::unique_ptr<IDatabaseConnection> conn,
                        ConnectionPool* pool);
        ~PooledConnection() override;

        // 委托给真实连接
        bool connect(const std::string& connStr) override;
        void disconnect() override;
        bool isConnected() const override;
        bool ping() override;

        std::vector<std::vector<std::string>> query(const std::string& sql) override;
        std::vector<std::vector<std::string>> query(
            const std::string& sql,
            const std::vector<std::string>& params) override;

        bool execute(const std::string& sql) override;
        bool execute(const std::string& sql,
                    const std::vector<std::string>& params) override;

        bool begin() override;
        bool commit() override;
        bool rollback() override;

        std::string escape(const std::string& str) override;
        int64_t lastInsertId() override;
        int64_t rowsAffected() override;

        // 禁用拷贝
        PooledConnection(const PooledConnection&) = delete;
        PooledConnection& operator=(const PooledConnection&) = delete;

    private:
        std::unique_ptr<IDatabaseConnection> conn_;
        ConnectionPool* pool_;
    };

    // 内部方法
    std::unique_ptr<IDatabaseConnection> createConnection();
    bool validateConnection(IDatabaseConnection* conn);
    void returnConnection(std::unique_ptr<IDatabaseConnection> conn);
    void maintenanceThread();

    // 成员
    std::unique_ptr<IDatabaseConnectionFactory> factory_;
    std::string connectionString_;
    PoolConfig config_;

    std::vector<std::unique_ptr<IDatabaseConnection>> idleConnections_;
    std::vector<IDatabaseConnection*> activeConnections_;

    size_t activeCount_ = 0;
    size_t idleCount_ = 0;
    bool running_ = false;

    std::mutex mutex_;
    std::condition_variable cv_;

    // 维护线程
    std::thread maintenanceThread_;
    std::atomic<bool> stopMaintenance_{false};

    friend class PooledConnection;
};

//==============================================================================
// SqlTemplateBuilder - Spring 风格的构建器
//==============================================================================

class SqlTemplateBuilder {
public:
    SqlTemplateBuilder();

    // 数据源配置
    SqlTemplateBuilder& url(const std::string& url);
    SqlTemplateBuilder& host(const std::string& host);
    SqlTemplateBuilder& port(uint16_t port);
    SqlTemplateBuilder& database(const std::string& database);
    SqlTemplateBuilder& username(const std::string& username);
    SqlTemplateBuilder& password(const std::string& password);

    // 连接池配置
    SqlTemplateBuilder& minSize(size_t size);
    SqlTemplateBuilder& maxSize(size_t size);
    SqlTemplateBuilder& connectTimeout(uint32_t timeoutMs);
    SqlTemplateBuilder& idleTimeout(uint32_t timeoutMs);
    SqlTemplateBuilder& maxLifetime(uint32_t lifetimeMs);

    // 验证配置
    SqlTemplateBuilder& validateOnCheckout(bool enable);
    SqlTemplateBuilder& validationQuery(const std::string& sql);

    // 构建选项
    enum class BuildMode {
        Direct,         // 直接连接 (无连接池)
        Pooled,         // 使用连接池
        Auto            // 自动选择
    };

    SqlTemplateBuilder& mode(BuildMode mode);

    // 构建
    SqlTemplate build();
    std::unique_ptr<SqlTemplate> buildUnique();

    // 构建 DataSource (高级用法)
    std::shared_ptr<DataSource> buildDataSource();

private:
    struct Config {
        // 连接信息
        std::string url;
        std::string host = "localhost";
        uint16_t port = 3306;
        std::string database;
        std::string username;
        std::string password;

        // 连接池
        PoolConfig poolConfig;

        // 构建模式
        BuildMode buildMode = BuildMode::Auto;
    };

    Config config_;
    DbType detectType() const;
    std::string buildUrl() const;
};

//==============================================================================
// JdbcTemplate 风格的便捷函数
//==============================================================================

// 使用 DataSource 创建 SqlTemplate
inline SqlTemplate createSqlTemplate(std::shared_ptr<DataSource> dataSource) {
    return SqlTemplate(dataSource->getConnection());
}

// 使用连接池创建
inline std::shared_ptr<DataSource> createPoolingDataSource(
    DbType type,
    const std::string& connectionString,
    const PoolConfig& config = {}) {

    auto pool = std::make_shared<ConnectionPool>(type, connectionString);
    pool->setPoolConfig(config);
    pool->start();
    return pool;
}

} // namespace database
} // namespace apollo
