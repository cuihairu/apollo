/**
 * @file db_mock.cpp
 * @brief 内存 Mock 数据库实现
 */

#include "apollo/storage/database/db_mock.h"
#include <regex>
#include <algorithm>

namespace apollo {
namespace storage {
namespace database {

//==============================================================================
// MockDbConnection 实现
//==============================================================================

DbResult MockDbConnection::executeQuery(const std::string& sql) {
    errorCode_ = 0;
    errorMessage_.clear();

    // 检查自定义处理器
    for (const auto& pair : queryHandlers_) {
        if (sql.find(pair.first) != std::string::npos) {
            return pair.second(sql, {});
        }
    }

    // 解析 SQL（简单实现）
    std::string upperSql = sql;
    std::transform(upperSql.begin(), upperSql.end(), upperSql.begin(), ::toupper);

    if (upperSql.find("SELECT") == 0) {
        // 解析表名
        std::string table = extractTableName(sql);

        // 返回表数据
        auto it = tableData_.find(table);
        if (it != tableData_.end()) {
            DbResult result;
            // 简化：假设所有值都是字符串
            DbRow row;
            for (const auto& value : it->second) {
                if (std::holds_alternative<std::string>(value)) {
                    row.addColumn("", value);
                }
            }
            result.addRow(row);
            return result;
        }
    }

    return DbResult{};
}

DbResult MockDbConnection::executeQuery(const std::string& sql, const DbParams& params) {
    // 检查自定义处理器
    for (const auto& pair : queryHandlers_) {
        if (sql.find(pair.first) != std::string::npos) {
            return pair.second(sql, params);
        }
    }

    return executeQuery(sql);
}

int64_t MockDbConnection::executeUpdate(const std::string& sql) {
    errorCode_ = 0;
    errorMessage_.clear();
    affectedRows_ = 0;

    // 检查自定义处理器
    for (const auto& pair : updateHandlers_) {
        if (sql.find(pair.first) != std::string::npos) {
            return pair.second(sql, {});
        }
    }

    // 解析 SQL
    std::string upperSql = sql;
    std::transform(upperSql.begin(), upperSql.end(), upperSql.begin(), ::toupper);

    if (upperSql.find("INSERT") == 0) {
        affectedRows_ = 1;
        lastInsertId_++;
        return affectedRows_;
    } else if (upperSql.find("UPDATE") == 0) {
        affectedRows_ = 1;
        return affectedRows_;
    } else if (upperSql.find("DELETE") == 0) {
        affectedRows_ = 1;
        return affectedRows_;
    }

    return 0;
}

int64_t MockDbConnection::executeUpdate(const std::string& sql, const DbParams& params) {
    // 检查自定义处理器
    for (const auto& pair : updateHandlers_) {
        if (sql.find(pair.first) != std::string::npos) {
            return pair.second(sql, params);
        }
    }

    return executeUpdate(sql);
}

std::shared_ptr<IPreparedStatement> MockDbConnection::prepare(const std::string& sql) {
    auto stmt = std::make_shared<MockPreparedStatement>(
        std::static_pointer_cast<MockDbConnection>(shared_from_this()));
    stmt->setSql(sql);
    return stmt;
}

std::vector<std::string> MockDbConnection::getTables() {
    std::vector<std::string> result;
    for (const auto& pair : tables_) {
        result.push_back(pair.first);
    }
    return result;
}

std::vector<std::string> MockDbConnection::getColumns(const std::string& table) {
    auto it = tables_.find(table);
    if (it != tables_.end()) {
        return it->second;
    }
    return {};
}

void MockDbConnection::addTableData(const std::string& table,
                                    const std::vector<std::string>& columns,
                                    const std::vector<std::vector<DbValue>>& rows) {
    tables_[table] = columns;

    // 简化实现：只存储第一行数据
    if (!rows.empty()) {
        tableData_[table] = rows[0];
    }
}

void MockDbConnection::setQueryHandler(const std::string& pattern, QueryHandler handler) {
    queryHandlers_.push_back({pattern, std::move(handler)});
}

void MockDbConnection::setUpdateHandler(const std::string& pattern, UpdateHandler handler) {
    updateHandlers_.push_back({pattern, std::move(handler)});
}

void MockDbConnection::clear() {
    tables_.clear();
    tableData_.clear();
    lastInsertId_ = 1;
    affectedRows_ = 0;
    transactionState_ = TransactionState::None;
}

std::string MockDbConnection::extractTableName(const std::string& sql) {
    // 简单的正则提取 FROM 后的表名
    std::string upperSql = sql;
    std::transform(upperSql.begin(), upperSql.end(), upperSql.begin(), ::toupper);

    size_t fromPos = upperSql.find("FROM ");
    if (fromPos == std::string::npos) {
        fromPos = upperSql.find("FROM\t");
    }

    if (fromPos != std::string::npos) {
        size_t start = fromPos + 5;
        while (start < sql.size() && (sql[start] == ' ' || sql[start] == '\t')) {
            start++;
        }

        size_t end = start;
        while (end < sql.size() && sql[end] != ' ' && sql[end] != '\t' &&
               sql[end] != ',' && sql[end] != ';') {
            end++;
        }

        return sql.substr(start, end - start);
    }

    return "";
}

} // namespace database
} // namespace storage
} // namespace apollo
