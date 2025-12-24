/**
 * @file db_mysql.h
 * @brief MySQL 数据库连接实现
 *
 * 支持 MySQL Connector/C++ 或 MariaDB Connector/C++
 */

#pragma once

#include "apollo/storage/database/db.h"

#ifdef APOLLO_USE_MYSQL_CONNECTOR

#include <mysql/jdbc.h>
#include <memory>
#include <mutex>

namespace apollo {
namespace storage {
namespace database {

/**
 * @brief MySQL 连接实现
 */
class MySQLConnection : public IDbConnection {
public:
    MySQLConnection();
    ~MySQLConnection() override;

    // 禁止拷贝
    MySQLConnection(const MySQLConnection&) = delete;
    MySQLConnection& operator=(const MySQLConnection&) = delete;

    bool connect(const DbConfig& config) override;
    void disconnect() override;
    bool isConnected() const override;
    bool ping() override;

    DbResult executeQuery(const std::string& sql) override;
    DbResult executeQuery(const std::string& sql, const DbParams& params) override;

    int64_t executeUpdate(const std::string& sql) override;
    int64_t executeUpdate(const std::string& sql, const DbParams& params) override;

    int64_t lastInsertId() override;
    int64_t affectedRows() override;

    bool beginTransaction() override;
    bool commit() override;
    bool rollback() override;
    TransactionState getTransactionState() const override;

    std::shared_ptr<IPreparedStatement> prepare(const std::string& sql) override;

    int getErrorCode() const override;
    std::string getErrorMessage() const override;
    bool hasError() const override;

    std::vector<std::string> getTables() override;
    std::vector<std::string> getColumns(const std::string& table) override;

private:
    std::shared_ptr<sql::Connection> connection_;
    std::shared_ptr<sql::Statement> statement_;
    std::shared_ptr<sql::PreparedStatement> preparedStatement_;
    TransactionState transactionState_ = TransactionState::None;
    DbConfig config_;
    mutable std::mutex mutex_;

    int errorCode_ = 0;
    std::string errorMessage_;

    void setError(int code, const std::string& msg);
    void clearError();

    // 辅助函数：将 DbValue 转换为 SQL 字符串
    std::string valueToString(const DbValue& value);

    // 辅助函数：执行查询并转换结果
    DbResult executeQueryInternal(const std::string& sql);
};

/**
 * @brief MySQL 预编译语句实现
 */
class MySQLPreparedStatement : public IPreparedStatement {
public:
    explicit MySQLPreparedStatement(std::shared_ptr<sql::PreparedStatement> stmt);
    ~MySQLPreparedStatement() override = default;

    void bind(size_t index, const DbValue& value) override;
    void bind(const std::string& name, const DbValue& value) override;

    DbResult executeQuery() override;
    int64_t executeUpdate() override;

    void reset() override;
    void close() override;

private:
    std::shared_ptr<sql::PreparedStatement> statement_;
    DbParams boundParams_;

    void bindValue(size_t index, const DbValue& value);
};

} // namespace database
} // namespace storage
} // namespace apollo

#endif // APOLLO_USE_MYSQL_CONNECTOR
