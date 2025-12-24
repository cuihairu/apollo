#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <variant>
#include <optional>
#include <functional>

namespace apollo {
namespace storage {
namespace database {

/**
 * @brief 数据库值类型
 */
using DbValue = std::variant<
    std::monostate,   // NULL
    bool,
    int32_t,
    int64_t,
    double,
    std::string,
    std::vector<uint8_t>  // BLOB
>;

/**
 * @brief 数据库行
 */
class DbRow {
public:
    DbRow() = default;

    // 按索引访问
    const DbValue& operator[](size_t index) const;
    DbValue& operator[](size_t index);

    // 按列名访问
    const DbValue& operator[](const std::string& columnName) const;
    DbValue& operator[](const std::string& columnName);

    // 获取值（带类型转换）
    template<typename T>
    std::optional<T> get(size_t index) const {
        if (index >= values_.size()) return std::nullopt;
        return std::get_if<T>(&values_[index]);
    }

    template<typename T>
    std::optional<T> get(const std::string& columnName) const {
        auto it = columnMap_.find(columnName);
        if (it == columnMap_.end()) return std::nullopt;
        return std::get_if<T>(&values_[it->second]);
    }

    // 便捷方法
    std::optional<std::string> getString(size_t index) const;
    std::optional<std::string> getString(const std::string& columnName) const;
    std::optional<int32_t> getInt32(size_t index) const;
    std::optional<int32_t> getInt32(const std::string& columnName) const;
    std::optional<int64_t> getInt64(size_t index) const;
    std::optional<int64_t> getInt64(const std::string& columnName) const;
    std::optional<double> getDouble(size_t index) const;
    std::optional<double> getDouble(const std::string& columnName) const;
    std::optional<bool> getBool(size_t index) const;
    std::optional<bool> getBool(const std::string& columnName) const;

    // 列信息
    size_t columnCount() const { return values_.size(); }
    std::vector<std::string> columnNames() const;

    // 内部方法
    void addColumn(const std::string& name, const DbValue& value);
    void setColumnMapping(const std::map<std::string, size_t>& mapping);

private:
    std::vector<DbValue> values_;
    std::map<std::string, size_t> columnMap_;
    std::vector<std::string> columnNames_;
};

/**
 * @brief 查询结果集
 */
class DbResult {
public:
    DbResult() = default;
    explicit DbResult(std::vector<DbRow> rows) : rows_(std::move(rows)) {}

    size_t rowCount() const { return rows_.size(); }
    size_t columnCount() const { return rows_.empty() ? 0 : rows_[0].columnCount(); }
    bool isEmpty() const { return rows_.empty(); }

    const DbRow& operator[](size_t index) const { return rows_[index]; }
    DbRow& operator[](size_t index) { return rows_[index]; }

    // 迭代器支持
    auto begin() const { return rows_.begin(); }
    auto end() const { return rows_.end(); }

    // 添加行
    void addRow(const DbRow& row) { rows_.push_back(row); }
    void clear() { rows_.clear(); }

private:
    std::vector<DbRow> rows_;
};

/**
 * @brief 参数绑定
 */
using DbParams = std::vector<std::pair<std::string, DbValue>>;

/**
 * @brief 数据库类型
 */
enum class DbType : int {
    Unknown,
    MySQL,
    PostgreSQL,
    SQLite,
    ODBC
};

/**
 * @brief 数据库配置
 */
struct DbConfig {
    DbType type = DbType::MySQL;
    std::string host = "localhost";
    uint16_t port = 3306;
    std::string database;
    std::string username;
    std::string password;
    std::string charset = "utf8mb4";

    // 连接池配置
    size_t poolSize = 8;
    uint32_t connectTimeoutMs = 5000;
    uint32_t queryTimeoutMs = 30000;

    // 其他选项
    bool autoReconnect = true;
    bool multiStatements = false;
};

/**
 * @brief 事务状态
 */
enum class TransactionState : int {
    None,
    Started,
    Committed,
    RolledBack
};

/**
 * @brief 数据库连接接口
 *
 * 抽象接口，支持多种数据库实现
 */
class IDbConnection {
public:
    virtual ~IDbConnection() = default;

    /**
     * @brief 连接到数据库
     */
    virtual bool connect(const DbConfig& config) = 0;

    /**
     * @brief 断开连接
     */
    virtual void disconnect() = 0;

    /**
     * @brief 检查连接状态
     */
    virtual bool isConnected() const = 0;

    /**
     * @brief Ping 测试
     */
    virtual bool ping() = 0;

    /**
     * @brief 执行查询（返回结果集）
     */
    virtual DbResult executeQuery(const std::string& sql) = 0;

    /**
     * @brief 执行查询（带参数）
     */
    virtual DbResult executeQuery(const std::string& sql, const DbParams& params) = 0;

    /**
     * @brief 执行更新（返回影响的行数）
     */
    virtual int64_t executeUpdate(const std::string& sql) = 0;

    /**
     * @brief 执行更新（带参数）
     */
    virtual int64_t executeUpdate(const std::string& sql, const DbParams& params) = 0;

    /**
     * @brief 获取最后插入的ID
     */
    virtual int64_t lastInsertId() = 0;

    /**
     * @brief 获取影响的行数
     */
    virtual int64_t affectedRows() = 0;

    // ========== 事务操作 ==========

    /**
     * @brief 开始事务
     */
    virtual bool beginTransaction() = 0;

    /**
     * @brief 提交事务
     */
    virtual bool commit() = 0;

    /**
     * @brief 回滚事务
     */
    virtual bool rollback() = 0;

    /**
     * @brief 获取事务状态
     */
    virtual TransactionState getTransactionState() const = 0;

    // ========== 预编译语句 ==========

    /**
     * @brief 创建预编译语句
     */
    virtual std::shared_ptr<class IPreparedStatement> prepare(const std::string& sql) = 0;

    // ========== 错误处理 ==========

    /**
     * @brief 获取错误代码
     */
    virtual int getErrorCode() const = 0;

    /**
     * @brief 获取错误消息
     */
    virtual std::string getErrorMessage() const = 0;

    /**
     * @brief 是否有错误
     */
    virtual bool hasError() const = 0;

    // ========== 元数据 ==========

    /**
     * @brief 获取表列表
     */
    virtual std::vector<std::string> getTables() = 0;

    /**
     * @brief 获取表的列信息
     */
    virtual std::vector<std::string> getColumns(const std::string& table) = 0;
};

/**
 * @brief 预编译语句接口
 */
class IPreparedStatement {
public:
    virtual ~IPreparedStatement() = default;

    /**
     * @brief 绑定参数
     */
    virtual void bind(size_t index, const DbValue& value) = 0;
    virtual void bind(const std::string& name, const DbValue& value) = 0;

    /**
     * @brief 执行查询
     */
    virtual DbResult executeQuery() = 0;

    /**
     * @brief 执行更新
     */
    virtual int64_t executeUpdate() = 0;

    /**
     * @brief 重置语句
     */
    virtual void reset() = 0;

    /**
     * @brief 关闭语句
     */
    virtual void close() = 0;
};

/**
 * @brief 数据库连接智能指针类型
 */
using IDbConnectionPtr = std::shared_ptr<IDbConnection>;

/**
 * @brief 数据库工厂
 *
 * 根据配置创建不同类型的数据库连接
 */
class DbFactory {
public:
    /**
     * @brief 创建数据库连接
     */
    static IDbConnectionPtr create(const DbConfig& config);

    /**
     * @brief 获取可用的数据库类型
     */
    static std::vector<DbType> getAvailableTypes();
};

/**
 * @brief 数据库管理器（单例）
 *
 * 管理默认的数据库连接
 */
class DbManager {
public:
    static DbManager& instance();

    /**
     * @brief 初始化
     */
    bool initialize(const DbConfig& config);

    /**
     * @brief 获取连接
     */
    IDbConnectionPtr getConnection();

    /**
     * @brief 关闭连接
     */
    void shutdown();

    /**
     * @brief 检查是否已初始化
     */
    bool isInitialized() const { return connection_ != nullptr; }

private:
    DbManager() = default;
    ~DbManager() = default;

    IDbConnectionPtr connection_;
    DbConfig config_;
};

} // namespace database
} // namespace storage
} // namespace apollo

// 便捷宏
#define APOLLO_DB() apollo::storage::database::DbManager::instance().getConnection()
