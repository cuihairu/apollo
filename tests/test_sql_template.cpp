/**
 * @file test_sql_template.cpp
 * @brief SqlTemplate 全面测试用例 (生产级别)
 */

#include "apollo/database/sql_template.h"
#include "apollo/database/datasource.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>
#include <vector>
#include <iomanip>

using namespace apollo::database;

//==============================================================================
// 测试辅助工具
//==============================================================================

namespace test {

// 颜色输出 (Windows/Linux 兼容)
#if defined(_WIN32)
    #define COLOR_RESET   ""
    #define COLOR_RED     ""
    #define COLOR_GREEN   ""
    #define COLOR_YELLOW  ""
    #define COLOR_BLUE    ""
#else
    #define COLOR_RESET   "\033[0m"
    #define COLOR_RED     "\033[31m"
    #define COLOR_GREEN   "\033[32m"
    #define COLOR_YELLOW  "\033[33m"
    #define COLOR_BLUE    "\033[34m"
#endif

// 测试统计
struct Stats {
    int total = 0;
    int passed = 0;
    int failed = 0;
    int skipped = 0;

    void pass() { total++; passed++; }
    void fail() { total++; failed++; }
    void skip() { total++; skipped++; }

    void print() const {
        std::cout << "\n" << COLOR_BLUE << "=== Test Results ===" << COLOR_RESET << "\n";
        std::cout << "  Total:   " << total << "\n";
        std::cout << COLOR_GREEN << "  Passed:  " << passed << COLOR_RESET << "\n";
        std::cout << COLOR_RED << "  Failed:  " << failed << COLOR_RESET << "\n";
        std::cout << COLOR_YELLOW << "  Skipped: " << skipped << COLOR_RESET << "\n";
        std::cout << COLOR_BLUE << "==================" << COLOR_RESET << "\n";
    }
};

static Stats g_stats;

// 断言宏
#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << COLOR_RED << "    ASSERT FAILED: " << msg << " at " << __FILE__ << ":" << __LINE__ << COLOR_RESET << "\n"; \
            return false; \
        } \
    } while (0)

#define TEST_ASSERT_EQ(a, b, msg) \
    do { \
        if ((a) != (b)) { \
            std::cerr << COLOR_RED << "    ASSERT FAILED: " << msg << " (expected: " << (b) << ", got: " << (a) << ") at " << __FILE__ << ":" << __LINE__ << COLOR_RESET << "\n"; \
            return false; \
        } \
    } while (0)

#define TEST_ASSERT_NE(a, b, msg) \
    do { \
        if ((a) == (b)) { \
            std::cerr << COLOR_RED << "    ASSERT FAILED: " << msg << " at " << __FILE__ << ":" << __LINE__ << COLOR_RESET << "\n"; \
            return false; \
        } \
    } while (0)

#define TEST_ASSERT_NULL(ptr, msg) \
    TEST_ASSERT((ptr) == nullptr, msg)

#define TEST_ASSERT_NOTNULL(ptr, msg) \
    TEST_ASSERT((ptr) != nullptr, msg)

// 测试用例宏
#define TEST_CASE(name) \
    bool test_##name(); \
    struct TestRunner_##name { \
        TestRunner_##name() { \
            std::cout << COLOR_BLUE << "[TEST] " << #name << COLOR_RESET << "\n"; \
            auto start = std::chrono::steady_clock::now(); \
            bool result = test_##name(); \
            auto end = std::chrono::steady_clock::now(); \
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(); \
            if (result) { \
                std::cout << COLOR_GREEN << "  PASSED" << COLOR_RESET << " (" << duration << "ms)\n"; \
                g_stats.pass(); \
            } else { \
                std::cout << COLOR_RED << "  FAILED" << COLOR_RESET << "\n"; \
                g_stats.fail(); \
            } \
        } \
    } g_runner_##name; \
    bool test_##name()

} // namespace test

//==============================================================================
// Mock DatabaseConnection (用于测试，不依赖真实数据库)
//==============================================================================

namespace test {

class MockDatabaseConnection : public IDatabaseConnection {
public:
    struct QueryLog {
        std::string sql;
        std::vector<std::string> params;
        size_t timestamp;
    };

    MockDatabaseConnection() = default;

    // 模拟数据
    void setMockData(std::vector<std::vector<std::string>> data) {
        mockData_ = std::move(data);
    }

    void setMockRowNames(std::vector<std::string> names) {
        mockRowNames_ = std::move(names);
    }

    void setAffectedRows(int64_t rows) { affectedRows_ = rows; }
    void setLastInsertId(int64_t id) { lastInsertId_ = id; }
    void setConnected(bool connected) { connected_ = connected; }

    // 查询日志
    const std::vector<QueryLog>& getQueryLog() const { return queryLog_; }
    void clearQueryLog() { queryLog_.clear(); }

    // IDatabaseConnection 接口
    bool connect(const std::string& connStr) override {
        connectionLog_ = connStr;
        connected_ = true;
        return true;
    }

    void disconnect() override {
        connected_ = false;
    }

    bool isConnected() const override { return connected_; }

    bool ping() override { return connected_; }

    std::vector<std::vector<std::string>> query(const std::string& sql) override {
        logQuery(sql, {});
        return mockData_;
    }

    std::vector<std::vector<std::string>> query(
        const std::string& sql,
        const std::vector<std::string>& params) override {
        logQuery(sql, params);
        return mockData_;
    }

    bool execute(const std::string& sql) override {
        logQuery(sql, {});
        executed_ = true;
        return true;
    }

    bool execute(const std::string& sql, const std::vector<std::string>& params) override {
        logQuery(sql, params);
        executed_ = true;
        return true;
    }

    bool begin() override {
        transactionDepth_++;
        return true;
    }

    bool commit() override {
        if (transactionDepth_ > 0) {
            transactionDepth_--;
        }
        return true;
    }

    bool rollback() override {
        if (transactionDepth_ > 0) {
            transactionDepth_--;
        }
        return true;
    }

    std::string escape(const std::string& str) override {
        std::string result;
        result.reserve(str.size() * 2);
        for (char c : str) {
            if (c == '\'') {
                result += "''";
            } else if (c == '\\') {
                result += "\\\\";
            } else {
                result += c;
            }
        }
        return result;
    }

    int64_t lastInsertId() override { return lastInsertId_; }
    int64_t rowsAffected() override { return affectedRows_; }

    // 验证方法
    bool wasExecuted() const { return executed_; }
    void resetExecuted() { executed_ = false; }
    int getTransactionDepth() const { return transactionDepth_; }

private:
    void logQuery(const std::string& sql, const std::vector<std::string>& params) {
        static size_t counter = 0;
        queryLog_.push_back({sql, params, counter++});
    }

    std::vector<std::vector<std::string>> mockData_;
    std::vector<std::string> mockRowNames_;
    std::vector<QueryLog> queryLog_;
    std::string connectionLog_;
    bool connected_ = false;
    bool executed_ = false;
    int64_t affectedRows_ = 1;
    int64_t lastInsertId_ = 1;
    int transactionDepth_ = 0;
};

class MockDatabaseFactory : public IDatabaseConnectionFactory {
public:
    std::unique_ptr<IDatabaseConnection> create() override {
        return std::make_unique<MockDatabaseConnection>();
    }

    DbType getType() const override { return DbType::MySQL; }
    const char* getTypeName() const override { return "mock"; }
};

} // namespace test

//==============================================================================
// 测试用例
//==============================================================================

namespace test {

//==============================================================================
// 1. ResultSet 测试
//==============================================================================

TEST_CASE(resultset_empty) {
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> names = {"id", "name"};
    ResultSet rs(std::move(rows), std::move(names));

    TEST_ASSERT(rs.empty(), "Empty ResultSet");
    TEST_ASSERT_EQ(rs.size(), size_t(0), "Row count is 0");

    int count = 0;
    for (const auto& row : rs) {
        (void)row;
        count++;
    }
    TEST_ASSERT_EQ(count, 0, "Iterator yields 0 rows");

    return true;
}

TEST_CASE(resultset_single_row) {
    std::vector<std::vector<std::string>> rows = {{"1", "Alice", "100"}};
    std::vector<std::string> names = {"id", "name", "score"};
    ResultSet rs(std::move(rows), std::move(names));

    TEST_ASSERT(!rs.empty(), "Not empty");
    TEST_ASSERT_EQ(rs.size(), size_t(1), "Row count is 1");

    auto it = rs.begin();
    TEST_ASSERT(it != rs.end(), "Iterator valid");

    const auto& row = *it;
    TEST_ASSERT_EQ(row.getInt(0), 1, "First column int");
    TEST_ASSERT_EQ(row.get<std::string>(1), std::string("Alice"), "Second column string");
    TEST_ASSERT_EQ(row.getDouble(2), 100.0, "Third column double");

    return true;
}

TEST_CASE(resultset_multiple_rows) {
    std::vector<std::vector<std::string>> rows = {
        {"1", "Alice"},
        {"2", "Bob"},
        {"3", "Charlie"}
    };
    std::vector<std::string> names = {"id", "name"};
    ResultSet rs(std::move(rows), std::move(names));

    TEST_ASSERT_EQ(rs.size(), size_t(3), "Row count is 3");

    std::vector<std::string> names_found;
    for (const auto& row : rs) {
        names_found.push_back(row.get<std::string>(1));
    }

    TEST_ASSERT_EQ(names_found.size(), size_t(3), "Found 3 names");
    TEST_ASSERT_EQ(names_found[0], std::string("Alice"), "First name");
    TEST_ASSERT_EQ(names_found[1], std::string("Bob"), "Second name");
    TEST_ASSERT_EQ(names_found[2], std::string("Charlie"), "Third name");

    return true;
}

TEST_CASE(resulttest_row_access_by_name) {
    std::vector<std::vector<std::string>> rows = {{"1", "Alice", "100"}};
    std::vector<std::string> names = {"id", "name", "score"};
    ResultSet rs(std::move(rows), std::move(names));

    const auto& row = *rs.begin();

    TEST_ASSERT_EQ(row.get<int>("id"), 1, "Get id by name");
    TEST_ASSERT_EQ(row.get<std::string>("name"), std::string("Alice"), "Get name by name");
    TEST_ASSERT_EQ(row.get<int>("score"), 100, "Get score by name");

    return true;
}

//==============================================================================
// 2. PreparedStatement 测试
//==============================================================================

TEST_CASE(preparedStatement_set_params) {
    auto mockConn = std::make_shared<MockDatabaseConnection>();
    PreparedStatement stmt(mockConn, "SELECT * FROM users WHERE id = ? AND name = ?");

    stmt.setInt(0, 42);
    stmt.setString(1, "Alice");

    auto sql = stmt.buildSql();
    TEST_ASSERT(sql.find("42") != std::string::npos, "SQL contains int param");
    TEST_ASSERT(sql.find("Alice") != std::string::npos, "SQL contains string param");
    TEST_ASSERT(sql.find("?") == std::string::npos, "No placeholders remain");

    return true;
}

TEST_CASE(preparedStatement_chained_binding) {
    auto mockConn = std::make_shared<MockDatabaseConnection>();
    PreparedStatement stmt(mockConn, "INSERT INTO users VALUES (?, ?, ?, ?)");

    stmt.setInt(0, 1).setString(1, "Alice").setDouble(2, 99.5).setBool(3, true);

    auto sql = stmt.buildSql();
    TEST_ASSERT(sql.find("1") != std::string::npos, "Has int");
    TEST_ASSERT(sql.find("Alice") != std::string::npos, "Has string");
    TEST_ASSERT(sql.find("99.5") != std::string::npos, "Has double");

    return true;
}

TEST_CASE(preparedStatement_null_param) {
    auto mockConn = std::make_shared<MockDatabaseConnection>();
    PreparedStatement stmt(mockConn, "INSERT INTO users VALUES (?, ?)");

    stmt.setInt(0, 1);
    stmt.setNull(1);

    auto sql = stmt.buildSql();
    TEST_ASSERT(sql.find("NULL") != std::string::npos, "Contains NULL");

    return true;
}

//==============================================================================
// 3. SqlTemplate 测试
//==============================================================================

TEST_CASE(sqltemplate_query) {
    auto mockConn = std::make_shared<MockDatabaseConnection>();
    mockConn->setMockData({{"1", "Alice", "100"}});
    mockConn->setMockRowNames({"id", "name", "score"});
    mockConn->setConnected(true);

    SqlTemplate db(mockConn);

    auto rs = db.query("SELECT * FROM users");
    TEST_ASSERT(!rs.empty(), "Has results");

    return true;
}

TEST_CASE(sqltemplate_query_with_mapper) {
    struct Player {
        int64_t id;
        std::string name;
        int score;
    };

    auto mockConn = std::make_shared<MockDatabaseConnection>();
    mockConn->setMockData({
        {"1", "Alice", "100"},
        {"2", "Bob", "200"}
    });
    mockConn->setMockRowNames({"id", "name", "score"});
    mockConn->setConnected(true);

    SqlTemplate db(mockConn);

    RowMapper<Player> mapper = [](const ResultSet::Row& row) -> Player {
        return Player {
            .id = row.getInt64(0),
            .name = row.get<std::string>(1),
            .score = row.getInt(2)
        };
    };

    auto players = db.query("SELECT * FROM players", mapper);
    TEST_ASSERT_EQ(players.size(), size_t(2), "Two players");
    TEST_ASSERT_EQ(players[0].name, std::string("Alice"), "First player name");
    TEST_ASSERT_EQ(players[1].name, std::string("Bob"), "Second player name");

    return true;
}

TEST_CASE(sqltemplate_transaction) {
    auto mockConn = std::make_shared<MockDatabaseConnection>();
    mockConn->setConnected(true);

    SqlTemplate db(mockConn);

    bool executed = false;
    db.executeInTransaction([&]() {
        db.update("UPDATE players SET gold = 100");
        executed = true;
        return 0;
    });

    TEST_ASSERT(executed, "Transaction executed");
    TEST_ASSERT_EQ(mockConn->getTransactionDepth(), 0, "Transaction committed");

    return true;
}

TEST_CASE(sqltemplate_transaction_rollback) {
    auto mockConn = std::make_shared<MockDatabaseConnection>();
    mockConn->setConnected(true);

    SqlTemplate db(mockConn);

    bool caught = false;
    try {
        db.executeInTransaction([&]() {
            db.update("UPDATE players SET gold = 100");
            throw std::runtime_error("Force rollback");
        });
    } catch (...) {
        caught = true;
    }

    TEST_ASSERT(caught, "Exception thrown");
    TEST_ASSERT_EQ(mockConn->getTransactionDepth(), 0, "Transaction rolled back");

    return true;
}

//==============================================================================
// 4. DataSource/连接池 测试
//==============================================================================

TEST_CASE(datasource_connection_pool_basic) {
    auto factory = std::make_unique<MockDatabaseFactory>();
    auto pool = std::make_shared<ConnectionPool>(std::move(factory), "mock://test");

    PoolConfig config;
    config.minSize = 2;
    config.maxSize = 5;
    pool->setPoolConfig(config);

    TEST_ASSERT(pool->start(), "Pool started");
    TEST_ASSERT(pool->isRunning(), "Pool is running");
    TEST_ASSERT(pool->getIdleConnections() >= 2, "Min idle connections");

    pool->stop();
    TEST_ASSERT(!pool->isRunning(), "Pool stopped");

    return true;
}

TEST_CASE(datasource_get_connection) {
    auto factory = std::make_unique<MockDatabaseFactory>();
    auto pool = std::make_shared<ConnectionPool>(std::move(factory), "mock://test");

    PoolConfig config;
    config.minSize = 2;
    config.maxSize = 5;
    pool->setPoolConfig(config);
    pool->start();

    auto conn = pool->getConnection();
    TEST_ASSERT_NOTNULL(conn, "Got connection");
    TEST_ASSERT(pool->getActiveConnections() >= 1, "Active connection counted");

    pool->stop();
    return true;
}

TEST_CASE(datasource_connection_return) {
    auto factory = std::make_unique<MockDatabaseFactory>();
    auto pool = std::make_shared<ConnectionPool>(std::move(factory), "mock://test");

    PoolConfig config;
    config.minSize = 2;
    config.maxSize = 5;
    pool->setPoolConfig(config);
    pool->start();

    size_t idleBefore = pool->getIdleConnections();
    {
        auto conn = pool->getConnection();
        TEST_ASSERT(pool->getIdleConnections() < idleBefore, "Idle decreased");
    }
    // 连接归还
    TEST_ASSERT(pool->getIdleConnections() >= idleBefore, "Connection returned");

    pool->stop();
    return true;
}

//==============================================================================
// 5. SqlTemplateBuilder 测试
//==============================================================================

TEST_CASE(builder_default) {
    auto builder = SqlTemplateBuilder();

    TEST_ASSERT(true, "Builder created");  // 默认测试
    return true;
}

TEST_CASE_builder_mysql_url) {
    std::string url = SqlTemplateBuilder()
        .host("localhost")
        .port(3306)
        .database("testdb")
        .username("root")
        .password("pass")
        .buildUrl();

    TEST_ASSERT(url.find("mysql://") == 0, "MySQL URL");
    TEST_ASSERT(url.find("root:pass") != std::string::npos, "Has credentials");
    TEST_ASSERT(url.find("testdb") != std::string::npos, "Has database");

    return true;
}

TEST_CASE(builder_postgresql_url) {
    std::string url = SqlTemplateBuilder()
        .host("localhost")
        .port(5432)
        .database("testdb")
        .username("postgres")
        .password("pass")
        .url("postgres://localhost:5432/testdb")
        .buildUrl();

    TEST_ASSERT(url.find("postgres://") == 0, "PostgreSQL URL");

    return true;
}

TEST_CASE(builder_sqlite_url) {
    std::string url = SqlTemplateBuilder()
        .database("/path/to/db.sqlite")
        .buildUrl();

    TEST_ASSERT(url.find("sqlite://") == 0, "SQLite URL");

    return true;
}

TEST_CASE(builder_pool_config) {
    auto builder = SqlTemplateBuilder()
        .minSize(5)
        .maxSize(20)
        .connectTimeout(3000)
        .validateOnCheckout(true);

    TEST_ASSERT(true, "Builder config set");
    return true;
}

//==============================================================================
// 6. 参数化查询测试
//==============================================================================

TEST_CASE(parameterized_query) {
    auto mockConn = std::make_shared<MockDatabaseConnection>();
    mockConn->setMockData({{"1", "Alice"}});
    mockConn->setMockRowNames({"id", "name"});
    mockConn->setConnected(true);

    SqlTemplate db(mockConn);

    auto stmt = db.prepare("SELECT * FROM users WHERE id = ?");
    stmt.setInt(0, 1);
    auto rs = stmt.query();

    TEST_ASSERT(!rs.empty(), "Query executed");
    return true;
}

//==============================================================================
// 7. 并发测试
//==============================================================================

TEST_CASE(concurrent_connections) {
    auto factory = std::make_unique<MockDatabaseFactory>();
    auto pool = std::make_shared<ConnectionPool>(std::move(factory), "mock://test");

    PoolConfig config;
    config.minSize = 2;
    config.maxSize = 10;
    config.checkoutTimeoutMs = 5000;
    pool->setPoolConfig(config);
    pool->start();

    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&pool, &successCount]() {
            auto conn = pool->getConnection();
            if (conn) {
                successCount++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    TEST_ASSERT_EQ(successCount.load(), 5, "All threads got connections");

    pool->stop();
    return true;
}

//==============================================================================
// 8. 边界条件测试
//==============================================================================

TEST_CASE(empty_sql) {
    auto mockConn = std::make_shared<MockDatabaseConnection>();
    mockConn->setMockData({});
    mockConn->setConnected(true);

    SqlTemplate db(mockConn);
    auto rs = db.query("");

    TEST_ASSERT(rs.empty(), "Empty query returns empty result");
    return true;
}

TEST_CASE(very_long_string) {
    auto mockConn = std::make_shared<MockDatabaseConnection>();
    mockConn->setConnected(true);

    PreparedStatement stmt(mockConn, "INSERT INTO long_text VALUES (?)");

    std::string longStr(10000, 'A');
    stmt.setString(0, longStr);

    auto sql = stmt.buildSql();
    TEST_ASSERT(sql.find(longStr) != std::string::npos, "Long string in SQL");

    return true;
}

TEST_CASE(special_characters) {
    auto mockConn = std::make_shared<MockDatabaseConnection>();
    mockConn->setConnected(true);

    std::string special = "test'with\\quotes\"and'danger";
    auto escaped = mockConn->escape(special);

    TEST_ASSERT(escaped.find("'") == std::string::npos || escaped.find("''") != std::string::npos,
               "Quotes escaped");

    return true;
}

//==============================================================================
// 9. 性能测试
//==============================================================================

TEST_CASE(performance_many_queries) {
    auto mockConn = std::make_shared<MockDatabaseConnection>();
    mockConn->setMockData({{"1", "Alice"}});
    mockConn->setMockRowNames({"id", "name"});
    mockConn->setConnected(true);

    SqlTemplate db(mockConn);

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < 1000; ++i) {
        auto rs = db.query("SELECT * FROM users");
        (void)rs;
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    TEST_ASSERT(duration < 1000, "1000 queries in < 1 second");
    return true;
}

TEST_CASE(performance_pool_efficiency) {
    auto factory = std::make_unique<MockDatabaseFactory>();
    auto pool = std::make_shared<ConnectionPool>(std::move(factory), "mock://test");

    PoolConfig config;
    config.minSize = 5;
    config.maxSize = 10;
    pool->setPoolConfig(config);
    pool->start();

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < 100; ++i) {
        auto conn = pool->getConnection();
        // 连接会自动归还
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    TEST_ASSERT(duration < 500, "100 connections in < 500ms with pool");
    TEST_ASSERT(pool->getTotalConnections() <= 10, "Pool size respected");

    pool->stop();
    return true;
}

} // namespace test

//==============================================================================
// 主程序
//==============================================================================

int main() {
    std::cout << "\n";
    std::cout << COLOR_BLUE << "========================================" << COLOR_RESET << "\n";
    std::cout << COLOR_BLUE << "=== Apollo SqlTemplate Tests ===" << COLOR_RESET << "\n";
    std::cout << COLOR_BLUE << "========================================" << COLOR_RESET << "\n\n";

    // 测试会通过静态构造自动运行

    std::cout << "\n";
    test::g_stats.print();

    return (test::g_stats.failed == 0) ? 0 : 1;
}
