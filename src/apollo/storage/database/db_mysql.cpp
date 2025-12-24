/**
 * @file db_mysql.cpp
 * @brief MySQL 数据库连接实现
 */

#ifdef APOLLO_USE_MYSQL_CONNECTOR

#include "apollo/storage/database/db_mysql.h"
#include <stdexcept>

namespace apollo {
namespace storage {
namespace database {

//==============================================================================
// 辅助函数
//==============================================================================

std::string MySQLConnection::valueToString(const DbValue& value) {
    if (std::holds_alternative<std::monostate>(value)) {
        return "NULL";
    }
    if (std::holds_alternative<bool>(value)) {
        return std::get<bool>(value) ? "1" : "0";
    }
    if (std::holds_alternative<int32_t>(value)) {
        return std::to_string(std::get<int32_t>(value));
    }
    if (std::holds_alternative<int64_t>(value)) {
        return std::to_string(std::get<int64_t>(value));
    }
    if (std::holds_alternative<double>(value)) {
        return std::to_string(std::get<double>(value));
    }
    if (std::holds_alternative<std::string>(value)) {
        return "'" + std::get<std::string>(value) + "'";  // 简单转义
    }
    if (std::holds_alternative<std::vector<uint8_t>>(value)) {
        return "''";  // BLOB 暂不支持
    }
    return "NULL";
}

void MySQLConnection::setError(int code, const std::string& msg) {
    errorCode_ = code;
    errorMessage_ = msg;
}

void MySQLConnection::clearError() {
    errorCode_ = 0;
    errorMessage_.clear();
}

//==============================================================================
// MySQLConnection 实现
//==============================================================================

MySQLConnection::MySQLConnection() = default;

MySQLConnection::~MySQLConnection() {
    disconnect();
}

bool MySQLConnection::connect(const DbConfig& config) {
    try {
        config_ = config;

        // 创建驱动
        sql::Driver* driver = get_driver_instance();
        if (!driver) {
            setError(-1, "Failed to get MySQL driver");
            return false;
        }

        // 构建连接 URL
        std::string url = "tcp://" + config.host + ":" + std::to_string(config.port);
        if (!config.database.empty()) {
            url += "/" + config.database;
        }

        // 连接属性
        sql::Properties properties;
        properties["hostName"] = config.host;
        properties["port"] = std::to_string(config.port);
        properties["userName"] = config.username;
        properties["password"] = config.password;
        properties["schema"] = config.database;
        properties["characterSet"] = config.charset;
        properties["autoReconnect"] = config.autoReconnect ? "true" : "false";

        // 创建连接
        connection_.reset(driver->connect(properties));

        if (!connection_) {
            setError(-1, "Failed to create connection");
            return false;
        }

        return true;
    } catch (const sql::SQLException& e) {
        setError(e.getErrorCode(), e.what());
        return false;
    } catch (const std::exception& e) {
        setError(-1, e.what());
        return false;
    }
}

void MySQLConnection::disconnect() {
    try {
        if (connection_) {
            connection_->close();
            connection_.reset();
        }
        statement_.reset();
        preparedStatement_.reset();
    } catch (...) {
        connection_.reset();
    }
    transactionState_ = TransactionState::None;
}

bool MySQLConnection::isConnected() const {
    return connection_ != nullptr && !connection_->isClosed();
}

bool MySQLConnection::ping() {
    if (!connection_) return false;
    try {
        return !connection_->isClosed();
    } catch (...) {
        return false;
    }
}

DbResult MySQLConnection::executeQuery(const std::string& sql) {
    clearError();
    return executeQueryInternal(sql);
}

DbResult MySQLConnection::executeQuery(const std::string& sql, const DbParams& params) {
    clearError();

    // 简单实现：替换参数
    std::string processedSql = sql;
    for (const auto& param : params) {
        std::string placeholder = ":" + param.first;
        std::string value = valueToString(param.second);
        size_t pos = processedSql.find(placeholder);
        while (pos != std::string::npos) {
            processedSql.replace(pos, placeholder.length(), value);
            pos = processedSql.find(placeholder, pos + value.length());
        }
    }

    return executeQueryInternal(processedSql);
}

int64_t MySQLConnection::executeUpdate(const std::string& sql) {
    clearError();

    try {
        if (!connection_ || connection_->isClosed()) {
            setError(-1, "Not connected");
            return -1;
        }

        std::unique_ptr<sql::Statement> stmt(connection_->createStatement());
        return stmt->executeUpdate(sql);
    } catch (const sql::SQLException& e) {
        setError(e.getErrorCode(), e.what());
        return -1;
    }
}

int64_t MySQLConnection::executeUpdate(const std::string& sql, const DbParams& params) {
    clearError();

    // 简单实现：替换参数
    std::string processedSql = sql;
    for (const auto& param : params) {
        std::string placeholder = ":" + param.first;
        std::string value = valueToString(param.second);
        size_t pos = processedSql.find(placeholder);
        while (pos != std::string::npos) {
            processedSql.replace(pos, placeholder.length(), value);
            pos = processedSql.find(placeholder, pos + value.length());
        }
    }

    return executeUpdate(processedSql);
}

int64_t MySQLConnection::lastInsertId() {
    try {
        if (!connection_ || connection_->isClosed()) {
            return 0;
        }

        std::unique_ptr<sql::Statement> stmt(connection_->createStatement());
        auto result = stmt->executeQuery("SELECT LAST_INSERT_ID()");
        if (result->next()) {
            return result->getInt64(1);
        }
        return 0;
    } catch (...) {
        return 0;
    }
}

int64_t MySQLConnection::affectedRows() {
    // 在 executeUpdate 中已经返回了影响行数
    // 这里只是占位
    return 0;
}

bool MySQLConnection::beginTransaction() {
    try {
        if (!connection_ || connection_->isClosed()) {
            return false;
        }

        connection_->setAutoCommit(false);
        transactionState_ = TransactionState::Started;
        return true;
    } catch (...) {
        return false;
    }
}

bool MySQLConnection::commit() {
    try {
        if (!connection_ || connection_->isClosed()) {
            return false;
        }

        connection_->commit();
        connection_->setAutoCommit(true);
        transactionState_ = TransactionState::Committed;
        return true;
    } catch (...) {
        transactionState_ = TransactionState::RolledBack;
        return false;
    }
}

bool MySQLConnection::rollback() {
    try {
        if (!connection_ || connection_->isClosed()) {
            return false;
        }

        connection_->rollback();
        connection_->setAutoCommit(true);
        transactionState_ = TransactionState::RolledBack;
        return true;
    } catch (...) {
        return false;
    }
}

TransactionState MySQLConnection::getTransactionState() const {
    return transactionState_;
}

std::shared_ptr<IPreparedStatement> MySQLConnection::prepare(const std::string& sql) {
    try {
        if (!connection_ || connection_->isClosed()) {
            return nullptr;
        }

        auto pstmt = connection_->prepareStatement(sql);
        return std::make_shared<MySQLPreparedStatement>(pstmt);
    } catch (...) {
        return nullptr;
    }
}

int MySQLConnection::getErrorCode() const {
    return errorCode_;
}

std::string MySQLConnection::getErrorMessage() const {
    return errorMessage_;
}

bool MySQLConnection::hasError() const {
    return errorCode_ != 0;
}

std::vector<std::string> MySQLConnection::getTables() {
    std::vector<std::string> tables;

    try {
        if (!connection_ || connection_->isClosed()) {
            return tables;
        }

        auto result = executeQueryInternal("SHOW TABLES");
        for (size_t i = 0; i < result.rowCount(); ++i) {
            auto value = result[i].getString(0);
            if (value) {
                tables.push_back(*value);
            }
        }
    } catch (...) {
        // 忽略错误
    }

    return tables;
}

std::vector<std::string> MySQLConnection::getColumns(const std::string& table) {
    std::vector<std::string> columns;

    try {
        if (!connection_ || connection_->isClosed()) {
            return columns;
        }

        auto result = executeQueryInternal("SHOW COLUMNS FROM " + table);
        for (size_t i = 0; i < result.rowCount(); ++i) {
            auto value = result[i].getString("Field");
            if (value) {
                columns.push_back(*value);
            }
        }
    } catch (...) {
        // 忽略错误
    }

    return columns;
}

DbResult MySQLConnection::executeQueryInternal(const std::string& sql) {
    DbResult result;

    try {
        if (!connection_ || connection_->isClosed()) {
            setError(-1, "Not connected");
            return result;
        }

        std::unique_ptr<sql::Statement> stmt(connection_->createStatement());
        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery(sql));

        // 获取元数据
        sql::ResultSetMetaData* meta = rs->getMetaData();
        size_t columnCount = meta->getColumnCount();

        while (rs->next()) {
            DbRow row;
            for (size_t i = 1; i <= columnCount; ++i) {
                std::string columnName = meta->getColumnName(i);

                int sqlType = meta->getColumnType(i);
                DbValue value;

                switch (sqlType) {
                    case sql::DataType::BIT:
                    case sql::DataType::BOOLEAN:
                        value = rs->getBoolean(i);
                        break;
                    case sql::DataType::TINYINT:
                    case sql::DataType::SMALLINT:
                    case sql::DataType::INTEGER:
                        value = static_cast<int32_t>(rs->getInt(i));
                        break;
                    case sql::DataType::BIGINT:
                        value = rs->getInt64(i);
                        break;
                    case sql::DataType::REAL:
                    case sql::DataType::DOUBLE:
                    case sql::DataType::FLOAT:
                        value = rs->getDouble(i);
                        break;
                    case sql::DataType::CHAR:
                    case sql::DataType::VARCHAR:
                    case sql::DataType::LONGVARCHAR:
                    case sql::DataType::TEXT:
                        value = rs->getString(i);
                        break;
                    case sql::DataType::BINARY:
                    case sql::DataType::VARBINARY:
                    case sql::DataType::LONGVARBINARY:
                    case sql::DataType::BLOB:
                        // BLOB 处理
                        value = std::string("");
                        break;
                    default:
                        if (rs->isNull(i)) {
                            value = std::monostate{};
                        } else {
                            value = rs->getString(i);
                        }
                        break;
                }

                row.addColumn(columnName, value);
            }
            result.addRow(row);
        }
    } catch (const sql::SQLException& e) {
        setError(e.getErrorCode(), e.what());
    } catch (const std::exception& e) {
        setError(-1, e.what());
    }

    return result;
}

//==============================================================================
// MySQLPreparedStatement 实现
//==============================================================================

MySQLPreparedStatement::MySQLPreparedStatement(std::shared_ptr<sql::PreparedStatement> stmt)
    : statement_(stmt) {
}

void MySQLPreparedStatement::bind(size_t index, const DbValue& value) {
    bindValue(index + 1, value);  // MySQL 索引从 1 开始
}

void MySQLPreparedStatement::bind(const std::string& name, const DbValue& value) {
    // MySQL Connector/C++ 不支持命名参数
    // 需要手动管理参数映射
    boundParams_.push_back({name, value});
}

DbResult MySQLPreparedStatement::executeQuery() {
    DbResult result;
    // 简化实现
    return result;
}

int64_t MySQLPreparedStatement::executeUpdate() {
    if (!statement_) return -1;
    try {
        return statement_->executeUpdate();
    } catch (...) {
        return -1;
    }
}

void MySQLPreparedStatement::reset() {
    // 重置参数
    boundParams_.clear();
}

void MySQLPreparedStatement::close() {
    statement_.reset();
}

void MySQLPreparedStatement::bindValue(size_t index, const DbValue& value) {
    if (!statement_) return;

    try {
        if (std::holds_alternative<std::monostate>(value)) {
            statement_->setNull(index, sql::DataType::VARCHAR);
        } else if (std::holds_alternative<bool>(value)) {
            statement_->setBoolean(index, std::get<bool>(value));
        } else if (std::holds_alternative<int32_t>(value)) {
            statement_->setInt(index, std::get<int32_t>(value));
        } else if (std::holds_alternative<int64_t>(value)) {
            statement_->setInt64(index, std::get<int64_t>(value));
        } else if (std::holds_alternative<double>(value)) {
            statement_->setDouble(index, std::get<double>(value));
        } else if (std::holds_alternative<std::string>(value)) {
            statement_->setString(index, std::get<std::string>(value));
        }
    } catch (...) {
        // 忽略错误
    }
}

} // namespace database
} // namespace storage
} // namespace apollo

#endif // APOLLO_USE_MYSQL_CONNECTOR
