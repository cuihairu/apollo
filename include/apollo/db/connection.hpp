#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <thread>

namespace apollo::db {

/// 数据库连接状态
enum class ConnectionState {
    DISCONNECTED = 0,
    CONNECTING = 1,
    CONNECTED = 2,
    ERROR = 3
};

/// 查询结果集
class QueryResult {
public:
    QueryResult() = default;
    ~QueryResult() = default;

    struct Row {
        std::vector<std::string> columns;
        std::vector<std::string> values;
    };

    /// 获取行数
    size_t GetRowCount() const { return rows_.size(); }

    /// 获取列数
    size_t GetColumnCount() const {
        return rows_.empty() ? 0 : rows_[0].columns.size();
    }

    /// 获取指定行
    const Row& GetRow(size_t index) const {
        return rows_[index];
    }

    /// 获取指定行列的值
    const std::string& GetValue(size_t row, size_t column) const {
        return rows_[row].values[column];
    }

    /// 获取指定列名的值
    std::string GetValue(const std::string& columnName, size_t row = 0) const {
        // 简化实现，实际项目需要列名映射
        return GetValue(row, 0);
    }

    /// 添加行
    void AddRow(const Row& row) {
        rows_.push_back(row);
    }

    /// 清空结果
    void Clear() {
        rows_.clear();
    }

    /// 是否有数据
    bool HasData() const {
        return !rows_.empty();
    }

private:
    std::vector<Row> rows_;
};

/// 数据库连接接口
class IDBConnection {
public:
    virtual ~IDBConnection() = default;

    /// 连接到数据库
    virtual bool Connect(const std::string& host, int port,
                          const std::string& database,
                          const std::string& username,
                          const std::string& password) = 0;

    /// 断开连接
    virtual bool Disconnect() = 0;

    /// 执行查询（返回结果集）
    virtual std::shared_ptr<QueryResult> ExecuteQuery(const std::string& sql) = 0;

    /// 执行更新/插入/删除
    virtual int ExecuteUpdate(const std::string& sql) = 0;

    /// 开始事务
    virtual bool BeginTransaction() = 0;

    /// 提交事务
    virtual bool CommitTransaction() = 0;

    /// 回滚事务
    virtual bool RollbackTransaction() = 0;

    /// 获取连接状态
    virtual ConnectionState GetState() const = 0;

    /// 检查连接是否有效
    virtual bool IsConnected() const = 0;

    /// 获取错误信息
    virtual std::string GetLastError() const = 0;

    /// Ping测试
    virtual bool Ping() = 0;
};

/// 连接池配置
struct ConnectionPoolConfig {
    std::string host = "localhost";
    int port = 3306;
    std::string database;
    std::string username;
    std::string password;
    size_t minConnections = 2;
    size_t maxConnections = 10;
    int connectionTimeout = 5;  // 秒
    int queryTimeout = 30;     // 秒
    bool autoReconnect = true;
    int checkInterval = 30;    // 秒
};

/// 数据库连接池
class ConnectionPool {
public:
    explicit ConnectionPool(const ConnectionPoolConfig& config);
    ~ConnectionPool();

    // 禁止拷贝
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;

    /// 初始化连接池
    bool Initialize();

    /// 获取连接
    std::shared_ptr<IDBConnection> GetConnection();

    /// 归还连接
    void ReturnConnection(std::shared_ptr<IDBConnection> conn);

    /// 关闭所有连接
    void CloseAll();

    /// 获取配置
    const ConnectionPoolConfig& GetConfig() const { return config_; }

    /// 获取统计信息
    struct Stats {
        size_t totalConnections;
        size_t activeConnections;
        size_t idleConnections;
        size_t createdConnections;
        size_t destroyedConnections;
    };

    Stats GetStats() const;

private:
    void CreateConnection();
    void CheckConnections();
    bool ValidateConnection(std::shared_ptr<IDBConnection> conn);

    ConnectionPoolConfig config_;
    std::queue<std::shared_ptr<IDBConnection>> idleConnections_;
    std::vector<std::shared_ptr<IDBConnection>> allConnections_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::atomic<bool> shutdown_{false};

    Stats stats_;
    std::thread checkThread_;
};

/// 连接池工厂
class ConnectionPoolFactory {
public:
    virtual ~ConnectionPoolFactory() = default;

    /// 创建连接池
    virtual std::unique_ptr<ConnectionPool> CreatePool(const ConnectionPoolConfig& config) = 0;

    /// 创建数据库连接
    virtual std::shared_ptr<IDBConnection> CreateConnection(const ConnectionPoolConfig& config) = 0;
};

}  // namespace apollo::db