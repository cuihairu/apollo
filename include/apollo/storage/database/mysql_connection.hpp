#pragma once

#include "connection.hpp"

#ifdef HAVE_MYSQL
#include <mysql/mysql.h>
#endif

namespace apollo::db {

#ifdef HAVE_MYSQL

/// MySQL连接实现
class MySQLConnection : public IDBConnection {
public:
    MySQLConnection();
    ~MySQLConnection();

    bool Connect(const std::string& host, int port,
                  const std::string& database,
                  const std::string& username,
                  const std::string& password) override;

    bool Disconnect() override;

    std::shared_ptr<QueryResult> ExecuteQuery(const std::string& sql) override;
    int ExecuteUpdate(const std::string& sql) override;

    bool BeginTransaction() override;
    bool CommitTransaction() override;
    bool RollbackTransaction() override;

    ConnectionState GetState() const override;
    bool IsConnected() const override;
    std::string GetLastError() const override;

    bool Ping() override;

    /// 获取MySQL句柄
    MYSQL* GetMySQL() { return mysql_; }

private:
    MYSQL* mysql_;
    ConnectionState state_;
    std::string lastError_;
};

#else

// MySQL未找到时的空实现
class MySQLConnection : public IDBConnection {
public:
    MySQLConnection() {}
    ~MySQLConnection() = default;

    bool Connect(const std::string&, int, const std::string&,
                  const std::string&, const std::string&) override { return false; }

    bool Disconnect() override { return false; }
    std::shared_ptr<QueryResult> ExecuteQuery(const std::string&) override { return nullptr; }
    int ExecuteUpdate(const std::string&) override { return -1; }
    bool BeginTransaction() override { return false; }
    bool CommitTransaction() override { return false; }
    bool RollbackTransaction() override { return false; }
    ConnectionState GetState() const override { return ConnectionState::DISCONNECTED; }
    bool IsConnected() const override { return false; }
    std::string GetLastError() const override { return "MySQL not supported"; }
    bool Ping() override { return false; }
};

#endif  // HAVE_MYSQL

/// MySQL连接池工厂
class MySQLConnectionFactory : public ConnectionPoolFactory {
public:
    std::unique_ptr<ConnectionPool> CreatePool(const ConnectionPoolConfig& config) override;
    std::shared_ptr<IDBConnection> CreateConnection(const ConnectionPoolConfig& config) override;
};

}  // namespace apollo::db