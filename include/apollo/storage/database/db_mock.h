/**
 * @file db_mock.h
 * @brief 内存 Mock 数据库实现（用于测试）
 */

#pragma once

#include "apollo/storage/database/db.h"
#include <unordered_map>
#include <functional>

namespace apollo {
namespace storage {
namespace database {

/**
 * @brief 内存 Mock 数据库
 *
 * 用于单元测试，不需要真实数据库
 */
class MockDbConnection : public IDbConnection {
public:
    MockDbConnection() = default;
    ~MockDbConnection() override = default;

    bool connect(const DbConfig& config) override {
        config_ = config;
        connected_ = true;
        return true;
    }

    void disconnect() override {
        connected_ = false;
    }

    bool isConnected() const override {
        return connected_;
    }

    bool ping() override {
        return connected_;
    }

    DbResult executeQuery(const std::string& sql) override;
    DbResult executeQuery(const std::string& sql, const DbParams& params) override;

    int64_t executeUpdate(const std::string& sql) override;
    int64_t executeUpdate(const std::string& sql, const DbParams& params) override;

    int64_t lastInsertId() override { return lastInsertId_; }
    int64_t affectedRows() override { return affectedRows_; }

    bool beginTransaction() override {
        transactionState_ = TransactionState::Started;
        return true;
    }

    bool commit() override {
        transactionState_ = TransactionState::Committed;
        return true;
    }

    bool rollback() override {
        transactionState_ = TransactionState::RolledBack;
        return true;
    }

    TransactionState getTransactionState() const override {
        return transactionState_;
    }

    std::shared_ptr<IPreparedStatement> prepare(const std::string& sql) override;

    int getErrorCode() const override { return errorCode_; }
    std::string getErrorMessage() const override { return errorMessage_; }
    bool hasError() const override { return errorCode_ != 0; }

    std::vector<std::string> getTables() override;
    std::vector<std::string> getColumns(const std::string& table) override;

    // ========== Mock 特有方法 ==========

    /**
     * @brief 添加模拟表数据
     */
    void addTableData(const std::string& table, const std::vector<std::string>& columns,
                      const std::vector<std::vector<DbValue>>& rows);

    /**
     * @brief 设置自定义查询处理器
     */
    using QueryHandler = std::function<DbResult(const std::string&, const DbParams&)>;
    void setQueryHandler(const std::string& pattern, QueryHandler handler);

    /**
     * @brief 设置更新处理器
     */
    using UpdateHandler = std::function<int64_t(const std::string&, const DbParams&)>;
    void setUpdateHandler(const std::string& pattern, UpdateHandler handler);

    /**
     * @brief 清空所有数据
     */
    void clear();

private:
    bool connected_ = true;
    DbConfig config_;
    TransactionState transactionState_ = TransactionState::None;
    int errorCode_ = 0;
    std::string errorMessage_;
    int64_t lastInsertId_ = 1;
    int64_t affectedRows_ = 0;

    // 模拟数据存储
    std::unordered_map<std::string, std::vector<std::string>> tables_;
    std::unordered_map<std::string, std::vector<DbValue>> tableData_;

    // 自定义处理器
    std::vector<std::pair<std::string, QueryHandler>> queryHandlers_;
    std::vector<std::pair<std::string, UpdateHandler>> updateHandlers_;

    // 辅助函数
    std::string extractTableName(const std::string& sql);
};

/**
 * @brief Mock 预编译语句
 */
class MockPreparedStatement : public IPreparedStatement {
public:
    explicit MockPreparedStatement(std::weak_ptr<MockDbConnection> conn)
        : connection_(conn) {}

    void bind(size_t index, const DbValue& value) override {
        if (params_.size() <= index) {
            params_.resize(index + 1);
        }
        params_[index] = value;
    }

    void bind(const std::string& name, const DbValue& value) override {
        namedParams_[name] = value;
    }

    DbResult executeQuery() override {
        if (auto conn = connection_.lock()) {
            return conn->executeQuery(sql_, namedParams_);
        }
        return DbResult{};
    }

    int64_t executeUpdate() override {
        if (auto conn = connection_.lock()) {
            return conn->executeUpdate(sql_, namedParams_);
        }
        return -1;
    }

    void reset() override {
        params_.clear();
        namedParams_.clear();
    }

    void close() override {
        params_.clear();
        namedParams_.clear();
    }

    void setSql(const std::string& sql) { sql_ = sql; }

private:
    std::weak_ptr<MockDbConnection> connection_;
    std::string sql_;
    std::vector<DbValue> params_;
    DbParams namedParams_;
};

} // namespace database
} // namespace storage
} // namespace apollo
