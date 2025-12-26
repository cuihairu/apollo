#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <optional>
#include <variant>
#include <iterator>

namespace apollo {
namespace database {

//==============================================================================
// 数据库类型
//==============================================================================

enum class DbType {
    MySQL,
    PostgreSQL,
    SQLite,
    ODBC
};

//==============================================================================
// 参数类型 - 类型安全的参数绑定
//==============================================================================

class Parameter {
public:
    using Value = std::variant<
        std::monostate,
        bool,
        int32_t,
        int64_t,
        double,
        std::string
    >;

    Parameter() = default;
    template<typename T>
    Parameter(T value) : value_(value) {}

    Value get() const { return value_; }
    std::string toString() const;

private:
    Value value_;
};

//==============================================================================
// ResultSet - 流式结果集 (JDBC 风格)
//==============================================================================

class ResultSet {
public:
    struct Row {
        std::vector<std::string> values;
        std::vector<std::string> columnNames;

        // 按索引获取
        std::string_view operator[](size_t index) const {
            return index < values.size() ? std::string_view(values[index]) : std::string_view();
        }

        // 按列名获取
        std::string_view operator[](const std::string& name) const {
            for (size_t i = 0; i < columnNames.size(); ++i) {
                if (columnNames[i] == name) {
                    return std::string_view(values[i]);
                }
            }
            return {};
        }

        // 类型安全获取
        template<typename T>
        T get(size_t index) const;

        template<typename T>
        T get(const std::string& name) const;

        int getInt(size_t index) const { return std::stoi(get<std::string>(index)); }
        int64_t getInt64(size_t index) const { return std::stoll(get<std::string>(index)); }
        double getDouble(size_t index) const { return std::stod(get<std::string>(index)); }
    };

    ResultSet(std::vector<std::vector<std::string>> rows,
              std::vector<std::string> columnNames);

    // 迭代器支持
    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Row;
        using difference_type = ptrdiff_t;
        using pointer = const Row*;
        using reference = const Row&;

        iterator(const ResultSet* rs, size_t pos);
        reference operator*() const;
        pointer operator->() const;
        iterator& operator++();
        iterator operator++(int);
        bool operator==(const iterator& other) const;
        bool operator!=(const iterator& other) const;

    private:
        const ResultSet* rs_;
        size_t pos_;
        Row currentRow_;
    };

    iterator begin() const;
    iterator end() const;

    size_t size() const { return rows_.size(); }
    bool empty() const { return rows_.empty(); }
    const std::vector<std::string>& columnNames() const { return columnNames_; }

private:
    std::vector<std::vector<std::string>> rows_;
    std::vector<std::string> columnNames_;
};

//==============================================================================
// RowMapper - ORM 映射接口 (Spring JDBC 风格)
//==============================================================================

template<typename T>
using RowMapper = std::function<T(const ResultSet::Row&)>;

// 便捷宏：自动生成 RowMapper
#define APOLLO_ROW_MAPPER(Type, ...) \
    [](const apollo::database::ResultSet::Row& row) -> Type { \
        Type obj; \
        __VA_ARGS__; \
        return obj; \
    }

//==============================================================================
// PreparedStatement - 参数化查询 (JDBC 风格)
//==============================================================================

class PreparedStatement {
public:
    PreparedStatement(std::shared_ptr<IDatabaseConnection> conn,
                      std::string sql);
    ~PreparedStatement();

    // 链式参数绑定
    PreparedStatement& setBool(size_t index, bool value);
    PreparedStatement& setInt(size_t index, int32_t value);
    PreparedStatement& setLong(size_t index, int64_t value);
    PreparedStatement& setDouble(size_t index, double value);
    PreparedStatement& setString(size_t index, const std::string& value);
    PreparedStatement& setNull(size_t index);

    // 通用设置
    template<typename T>
    PreparedStatement& set(size_t index, T value) {
        if constexpr (std::is_same_v<T, bool>) {
            return setBool(index, value);
        } else if constexpr (std::is_integral_v<T>) {
            return setLong(index, static_cast<int64_t>(value));
        } else if constexpr (std::is_floating_point_v<T>) {
            return setDouble(index, static_cast<double>(value));
        } else {
            return setString(index, std::string(value));
        }
    }

    // 执行查询
    ResultSet query();
    std::vector<std::map<std::string, std::string>> queryForMap();

    template<typename T>
    std::vector<T> query(RowMapper<T> mapper) {
        auto rs = query();
        std::vector<T> result;
        for (const auto& row : rs) {
            result.push_back(mapper(row));
        }
        return result;
    }

    template<typename T>
    std::optional<T> queryForObject(RowMapper<T> mapper) {
        auto results = query(mapper);
        return results.empty() ? std::nullopt : std::optional<T>(results[0]);
    }

    // 执行更新
    int executeUpdate();
    int64_t executeInsert();

private:
    std::shared_ptr<IDatabaseConnection> conn_;
    std::string sql_;
    std::vector<Parameter> params_;
    std::string buildSql() const;
};

//==============================================================================
// 统一的数据库接口 (核心库)
//==============================================================================

class IDatabaseConnection {
public:
    virtual ~IDatabaseConnection() = default;

    virtual bool connect(const std::string& connStr) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual bool ping() = 0;

    // 执行查询
    virtual std::vector<std::vector<std::string>> query(
        const std::string& sql) = 0;
    virtual std::vector<std::vector<std::string>> query(
        const std::string& sql,
        const std::vector<std::string>& params) = 0;

    // 执行更新
    virtual bool execute(const std::string& sql) = 0;
    virtual bool execute(const std::string& sql,
                        const std::vector<std::string>& params) = 0;

    // 事务
    virtual bool begin() = 0;
    virtual bool commit() = 0;
    virtual bool rollback() = 0;

    // 工具
    virtual std::string escape(const std::string& str) = 0;
    virtual int64_t lastInsertId() = 0;
    virtual int64_t rowsAffected() = 0;
};

//==============================================================================
// 连接工厂接口
//==============================================================================

class IDatabaseConnectionFactory {
public:
    virtual ~IDatabaseConnectionFactory() = default;
    virtual std::unique_ptr<IDatabaseConnection> create() = 0;
    virtual DbType getType() const = 0;
    virtual const char* getTypeName() const = 0;
};

//==============================================================================
// 连接注册中心 (核心库)
//==============================================================================

class DatabaseRegistry {
public:
    static void registerFactory(std::unique_ptr<IDatabaseConnectionFactory> factory);
    static std::unique_ptr<IDatabaseConnection> create(DbType type);
    static std::unique_ptr<IDatabaseConnection> create(const std::string& typeStr);
    static std::vector<DbType> getSupportedTypes();
    static bool isSupported(DbType type);
    static void loadPlugins(const std::string& pluginDir = "./plugins");
};

//==============================================================================
// 自动注册宏
//==============================================================================

#define APOLLO_REGISTER_DATABASE(FactoryClass) \
    namespace { \
        struct AutoRegister { \
            AutoRegister() { \
                apollo::database::DatabaseRegistry::registerFactory( \
                    std::make_unique<FactoryClass>()); \
            } \
        }; \
        static AutoRegister g_autoRegister; \
    }

//==============================================================================
// SqlTemplate - 高级封装 (JdbcTemplate 风格)
//==============================================================================

class SqlTemplate {
public:
    explicit SqlTemplate(std::unique_ptr<IDatabaseConnection> conn);
    explicit SqlTemplate(std::shared_ptr<IDatabaseConnection> conn);
    SqlTemplate(const std::string& connectionString);
    ~SqlTemplate();

    //======================================================================
    // PreparedStatement 支持 (JDBC 最优雅的部分)
    //======================================================================

    PreparedStatement prepare(const std::string& sql);

    //======================================================================
    // 便捷查询方法
    //======================================================================

    // 查询多行
    ResultSet query(const std::string& sql);
    std::vector<std::map<std::string, std::string>> queryForMap(const std::string& sql);

    // 使用 RowMapper 映射对象
    template<typename T>
    std::vector<T> query(const std::string& sql, RowMapper<T> mapper) {
        auto rs = query(sql);
        std::vector<T> result;
        for (const auto& row : rs) {
            result.push_back(mapper(row));
        }
        return result;
    }

    // 查询单行
    std::optional<ResultSet::Row> queryForRow(const std::string& sql);
    std::optional<std::map<std::string, std::string>> queryForMapRow(const std::string& sql);

    template<typename T>
    std::optional<T> queryForObject(const std::string& sql, RowMapper<T> mapper) {
        auto results = query(sql, mapper);
        return results.empty() ? std::nullopt : std::optional<T>(results[0]);
    }

    // 查询单个值
    template<typename T = std::string>
    std::optional<T> queryForValue(const std::string& sql) {
        auto row = queryForRow(sql);
        if (!row || row->values.empty()) {
            return std::nullopt;
        }
        if constexpr (std::is_same_v<T, std::string>) {
            return std::string((*row)[0]);
        } else if constexpr (std::is_integral_v<T>) {
            return static_cast<T>(std::stoll(std::string((*row)[0])));
        } else if constexpr (std::is_floating_point_v<T>) {
            return static_cast<T>(std::stod(std::string((*row)[0])));
        }
    }

    //======================================================================
    // 更新方法
    //======================================================================

    int update(const std::string& sql);
    int insert(const std::string& sql);

    //======================================================================
    // 事务模板 (Spring 风格)
    //======================================================================

    template<typename F>
    auto executeInTransaction(F&& func) -> decltype(func()) {
        begin();
        try {
            auto result = func();
            commit();
            return result;
        } catch (...) {
            rollback();
            throw;
        }
    }

    void begin();
    void commit();
    void rollback();

    //======================================================================
    // 批量操作
    //======================================================================

    template<typename T>
    std::vector<int> batchUpdate(const std::string& sql,
                                  const std::vector<T>& items,
                                  std::function<void(PreparedStatement&, const T&)> binder) {
        std::vector<int> results;
        executeInTransaction([&]() {
            for (const auto& item : items) {
                auto stmt = prepare(sql);
                binder(stmt, item);
                results.push_back(stmt.executeUpdate());
            }
            return 0;
        });
        return results;
    }

    //======================================================================
    // 元数据
    //======================================================================

    std::vector<std::string> getTableNames();
    std::vector<std::string> getColumnNames(const std::string& table);
    bool tableExists(const std::string& table);

    //======================================================================
    // 连接管理
    //======================================================================

    bool isConnected() const;
    void reconnect();

private:
    std::shared_ptr<IDatabaseConnection> conn_;
    std::string connectionString_;
};

//==============================================================================
// 便捷全局函数
//==============================================================================

inline std::unique_ptr<SqlTemplate> createMySQL(
    const std::string& host, uint16_t port,
    const std::string& database,
    const std::string& username,
    const std::string& password) {

    std::string connStr = "mysql://" + username + ":" + password + "@" +
                         host + ":" + std::to_string(port) + "/" + database;
    return std::make_unique<SqlTemplate>(connStr);
}

inline std::unique_ptr<SqlTemplate> createPostgreSQL(
    const std::string& host, uint16_t port,
    const std::string& database,
    const std::string& username,
    const std::string& password) {

    std::string connStr = "postgres://" + username + ":" + password + "@" +
                         host + ":" + std::to_string(port) + "/" + database;
    return std::make_unique<SqlTemplate>(connStr);
}

inline std::unique_ptr<SqlTemplate> createSQLite(
    const std::string& path) {

    std::string connStr = "sqlite://" + path;
    return std::make_unique<SqlTemplate>(connStr);
}

} // namespace database
} // namespace apollo
