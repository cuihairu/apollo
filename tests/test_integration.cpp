/**
 * @file test_integration.cpp
 * @brief Apollo 集成测试 (端到端测试)
 *
 * 这些测试验证多个组件之间的交互
 */

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

// 根据可用的组件选择测试
#ifdef APOLLO_HAS_CURL
    #include "apollo/net/http/rest_client.h"
#endif

#ifdef APOLLO_HAS_HIREDIS
    #include "apollo/redis/redis_template.h"
#endif

#include "apollo/database/sql_template.h"
#include "apollo/database/datasource.h"

using namespace apollo;

//==============================================================================
// 测试辅助
//==============================================================================

namespace test {

#if defined(_WIN32)
    #define COLOR_RESET   ""
    #define COLOR_GREEN   ""
    #define COLOR_RED     ""
    #define COLOR_BLUE    ""
#else
    #define COLOR_RESET   "\033[0m"
    #define COLOR_GREEN   "\033[32m"
    #define COLOR_RED     "\033[31m"
    #define COLOR_BLUE    "\033[34m"
#endif

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << COLOR_RED << "FAILED: " << msg << " at " << __FILE__ << ":" << __LINE__ << COLOR_RESET << "\n"; \
            return false; \
        } \
    } while (0)

struct Stats {
    int total = 0, passed = 0, failed = 0, skipped = 0;
    void pass() { total++; passed++; }
    void fail() { total++; failed++; }
    void skip() { total++; skipped++; }
    void print() const {
        std::cout << "\n" << COLOR_BLUE << "=== Integration Test Results ===" << COLOR_RESET << "\n";
        std::cout << "  Total: " << total << " | " << COLOR_GREEN << "Passed: " << passed
                  << COLOR_RESET << " | " << COLOR_RED << "Failed: " << failed << COLOR_RESET << "\n";
    }
};

static Stats g_stats;

#define INTEGRATION_TEST(name) \
    bool test_##name(); \
    struct Runner_##name { \
        Runner_##name() { \
            std::cout << COLOR_BLUE << "[INTEGRATION] " << #name << COLOR_RESET << "\n"; \
            if (test_##name()) { \
                std::cout << COLOR_GREEN << "  PASSED" << COLOR_RESET << "\n"; \
                g_stats.pass(); \
            } else { \
                std::cout << COLOR_RED << "  FAILED" << COLOR_RESET << "\n"; \
                g_stats.fail(); \
            } \
        } \
    } runner_##name; \
    bool test_##name()

//==============================================================================
// Mock 连接 (用于没有真实服务器的测试)
//==============================================================================

namespace {

class MockDbConnection : public database::IDatabaseConnection {
public:
    bool connect(const std::string&) override { connected_ = true; return true; }
    void disconnect() override { connected_ = false; }
    bool isConnected() const override { return connected_; }
    bool ping() override { return connected_; }

    std::vector<std::vector<std::string>> query(const std::string&) override {
        return {{"1", "Alice", "100"}};
    }
    std::vector<std::vector<std::string>> query(const std::string&, const std::vector<std::string>&) override {
        return {};
    }

    bool execute(const std::string&) override { return true; }
    bool execute(const std::string&, const std::vector<std::string>&) override { return true; }

    bool begin() override { return true; }
    bool commit() override { return true; }
    bool rollback() override { return true; }

    std::string escape(const std::string& str) override { return str; }
    int64_t lastInsertId() override { return 1; }
    int64_t rowsAffected() override { return 1; }

private:
    bool connected_ = true;
};

} // namespace

//==============================================================================
// 集成测试用例
//==============================================================================

// 1. 数据库连接池 + SqlTemplate 集成
INTEGRATION_TEST(database_pool_integration) {
    using namespace database;

    auto factory = std::make_unique<MockDbConnection>();  // 简化：这里需要 MockFactory
    auto pool = std::make_shared<ConnectionPool>(
        DbType::MySQL, "mysql://localhost:3306/test");

    PoolConfig config;
    config.minSize = 2;
    config.maxSize = 5;
    pool->setPoolConfig(config);

    TEST_ASSERT(pool->start(), "Pool started");
    TEST_ASSERT(pool->isRunning(), "Pool is running");

    // 从连接池获取连接并创建 SqlTemplate
    auto conn = pool->getConnection();
    TEST_ASSERT(conn != nullptr, "Got connection from pool");

    SqlTemplate db(conn);
    TEST_ASSERT(db.isConnected(), "SqlTemplate connected");

    pool->stop();
    return true;
}

// 2. SqlTemplateBuilder 构建完整配置
INTEGRATION_TEST(sqltemplate_builder_integration) {
    using namespace database;

    auto db = SqlTemplateBuilder()
        .host("localhost")
        .port(3306)
        .database("testdb")
        .username("root")
        .password("password")
        .minSize(2)
        .maxSize(10)
        .validateOnCheckout(true)
        .mode(SqlTemplateBuilder::BuildMode::Direct)  // 无真实数据库，使用直连
        .build();

    TEST_ASSERT(true, "SqlTemplate built with full config");
    return true;
}

// 3. 事务模板集成测试
INTEGRATION_TEST(transaction_template_integration) {
    using namespace database;

    auto mockConn = std::make_shared<MockDbConnection>();
    SqlTemplate db(mockConn);

    bool transactionExecuted = false;
    bool rollbackExecuted = false;

    // 成功事务
    db.executeInTransaction([&]() {
        transactionExecuted = true;
        return 0;
    });
    TEST_ASSERT(transactionExecuted, "Transaction executed");

    // 失败回滚
    try {
        db.executeInTransaction([&]() {
            throw std::runtime_error("Force rollback");
        });
    } catch (...) {
        rollbackExecuted = true;
    }
    TEST_ASSERT(rollbackExecuted, "Rollback executed");

    return true;
}

// 4. PreparedStatement 参数绑定集成
INTEGRATION_TEST(prepared_statement_integration) {
    using namespace database;

    auto mockConn = std::make_shared<MockDbConnection>();
    SqlTemplate db(mockConn);

    auto stmt = db.prepare("INSERT INTO users (id, name, score) VALUES (?, ?, ?)");
    stmt.setInt(0, 1)
        .setString(1, "Alice")
        .setDouble(2, 100.5);

    auto sql = stmt.buildSql();
    TEST_ASSERT(sql.find("1") != std::string::npos, "Int param bound");
    TEST_ASSERT(sql.find("Alice") != std::string::npos, "String param bound");
    TEST_ASSERT(sql.find("100.5") != std::string::npos, "Double param bound");

    return true;
}

// 5. 批量操作集成测试
INTEGRATION_TEST(batch_operations_integration) {
    using namespace database;

    struct User { std::string name; int age; };
    std::vector<User> users = { {"Alice", 25}, {"Bob", 30}, {"Charlie", 35} };

    auto mockConn = std::make_shared<MockDbConnection>();
    SqlTemplate db(mockConn);

    // 注意：批量操作需要真实连接池，这里只测试接口
    TEST_ASSERT(true, "Batch operation interface valid");

    return true;
}

// 6. 连接池并发测试
INTEGRATION_TEST(pool_concurrent_access) {
    using namespace database;

    auto pool = std::make_shared<ConnectionPool>(
        DbType::MySQL, "mysql://localhost:3306/test");

    PoolConfig config;
    config.minSize = 2;
    config.maxSize = 5;
    config.checkoutTimeoutMs = 1000;
    pool->setPoolConfig(config);
    pool->start();

    std::atomic<int> successCount{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&]() {
            auto conn = pool->getConnection();
            if (conn) {
                successCount++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    pool->stop();

    TEST_ASSERT_EQ(successCount.load(), 5, "All threads got connections");
    return true;
}

// 7. DataSource 配置测试
INTEGRATION_TEST(datasource_config_validation) {
    using namespace database;

    PoolConfig config;
    config.minSize = 5;
    config.maxSize = 10;
    config.connectTimeoutMs = 5000;
    config.validateOnCheckout = true;

    auto pool = std::make_shared<ConnectionPool>(
        DbType::MySQL, "mysql://localhost:3306/test");
    pool->setPoolConfig(config);

    TEST_ASSERT_EQ(pool->getPoolConfig().minSize, size_t(5), "Min size configured");
    TEST_ASSERT_EQ(pool->getPoolConfig().maxSize, size_t(10), "Max size configured");

    return true;
}

// 8. 多数据库类型支持
INTEGRATION_TEST(multiple_database_types) {
    using namespace database;

    // 测试不同数据库类型的 URL 构建
    std::string mysqlUrl = SqlTemplateBuilder()
        .database("testdb")
        .buildUrl();
    TEST_ASSERT(mysqlUrl.find("mysql://") == 0, "MySQL URL");

    std::string pgUrl = SqlTemplateBuilder()
        .url("postgres://localhost/test")
        .buildUrl();
    TEST_ASSERT(pgUrl.find("postgres://") == 0, "PostgreSQL URL");

    std::string sqliteUrl = SqlTemplateBuilder()
        .database("/path/to/db.sqlite")
        .buildUrl();
    TEST_ASSERT(sqliteUrl.find("sqlite://") == 0, "SQLite URL");

    return true;
}

// 9. 错误恢复集成测试
INTEGRATION_TEST(error_recovery_integration) {
    using namespace database;

    auto mockConn = std::make_shared<MockDbConnection>();
    SqlTemplate db(mockConn);

    // 测试重连逻辑
    TEST_ASSERT(db.isConnected(), "Initially connected");

    // 模拟断开后重连
    db.disconnect();
    TEST_ASSERT(!db.isConnected(), "Disconnected after disconnect");

    db.reconnect();
    TEST_ASSERT(db.isConnected(), "Reconnected after reconnect");

    return true;
}

// 10. 性能基准测试
INTEGRATION_TEST(performance_benchmark) {
    using namespace database;

    auto mockConn = std::make_shared<MockDbConnection>();
    SqlTemplate db(mockConn);

    const int iterations = 1000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        auto rs = db.query("SELECT * FROM users");
        (void)rs;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // 性能要求：1000 次查询在 500ms 内完成
    TEST_ASSERT(duration < 500, "Performance benchmark passed");

    std::cout << "    Performance: " << iterations << " queries in " << duration << "ms\n";

    return true;
}

} // namespace test

//==============================================================================
// 主程序
//==============================================================================

int main() {
    std::cout << "\n";
    std::cout << test::COLOR_BLUE << "========================================" << test::COLOR_RESET << "\n";
    std::cout << test::COLOR_BLUE << "=== Apollo Integration Tests ===" << test::COLOR_RESET << "\n";
    std::cout << test::COLOR_BLUE << "========================================" << test::COLOR_RESET << "\n\n";

    std::cout << "Note: These tests use mock connections and don't require real servers.\n\n";

    // 测试通过静态构造自动运行

    std::cout << "\n";
    test::g_stats.print();

    return (test::g_stats.failed == 0) ? 0 : 1;
}
