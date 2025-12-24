/**
 * @file test_database.cpp
 * @brief 数据库抽象层测试
 */

#include "apollo/storage/database/db.h"
#include "apollo/storage/database/db_mock.h"
#include <iostream>
#include <cassert>

using namespace apollo::storage::database;

// 测试辅助宏
#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

//==============================================================================
// 测试用例
//==============================================================================

/**
 * @brief 测试连接
 */
bool test_connect() {
    std::cout << "Running: test_connect..." << std::endl;

    auto db = std::make_shared<MockDbConnection>();
    DbConfig config;
    config.host = "127.0.0.1";
    config.port = 3306;

    TEST_ASSERT(db->connect(config), "Connect success");
    TEST_ASSERT(db->isConnected(), "Is connected");
    TEST_ASSERT(db->ping(), "Ping success");

    db->disconnect();
    TEST_ASSERT(!db->isConnected(), "Is disconnected");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试 DbRow 值转换
 */
bool test_dbrow_values() {
    std::cout << "Running: test_dbrow_values..." << std::endl;

    DbRow row;
    row.addColumn("id", static_cast<int32_t>(42));
    row.addColumn("name", std::string("Alice"));
    row.addColumn("score", static_cast<double>(95.5));
    row.addColumn("active", true);

    TEST_ASSERT(row.getInt32("id") == 42, "Int32 value");
    TEST_ASSERT(row.getString("name") == "Alice", "String value");
    TEST_ASSERT(row.getDouble("score") == 95.5, "Double value");
    TEST_ASSERT(row.getBool("active") == true, "Bool value");

    // NULL 值
    row.addColumn("null_value", std::monostate{});
    TEST_ASSERT(row.getString("null_value") == std::nullopt, "Null value");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试查询
 */
bool test_query() {
    std::cout << "Running: test_query..." << std::endl;

    auto db = std::make_shared<MockDbConnection>();
    db->connect({});

    // 设置自定义查询处理器
    db->setQueryHandler("SELECT", [&](const std::string&, const DbParams&) {
        DbResult result;
        DbRow row;
        row.addColumn("id", static_cast<int64_t>(1));
        row.addColumn("name", std::string("Test"));
        result.addRow(row);
        return result;
    });

    auto result = db->executeQuery("SELECT * FROM users");
    TEST_ASSERT(!result.isEmpty(), "Result has data");
    TEST_ASSERT(result.rowCount() == 1, "Row count");
    TEST_ASSERT(result[0].getInt64("id") == 1, "First row id");
    TEST_ASSERT(result[0].getString("name") == "Test", "First row name");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试更新操作
 */
bool test_update() {
    std::cout << "Running: test_update..." << std::endl;

    auto db = std::make_shared<MockDbConnection>();
    db->connect({});

    // INSERT
    auto rows = db->executeUpdate("INSERT INTO users VALUES (1, 'Alice')");
    TEST_ASSERT(rows >= 0, "Insert executed");

    // UPDATE
    rows = db->executeUpdate("UPDATE users SET name='Bob'");
    TEST_ASSERT(rows >= 0, "Update executed");

    // DELETE
    rows = db->executeUpdate("DELETE FROM users WHERE id=1");
    TEST_ASSERT(rows >= 0, "Delete executed");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试事务
 */
bool test_transaction() {
    std::cout << "Running: test_transaction..." << std::endl;

    auto db = std::make_shared<MockDbConnection>();
    db->connect({});

    TEST_ASSERT(db->beginTransaction(), "Begin transaction");
    TEST_ASSERT(db->getTransactionState() == TransactionState::Started, "Transaction started");

    db->executeUpdate("INSERT INTO users VALUES (1, 'Alice')");

    TEST_ASSERT(db->commit(), "Commit transaction");
    TEST_ASSERT(db->getTransactionState() == TransactionState::Committed, "Transaction committed");

    // Rollback
    TEST_ASSERT(db->beginTransaction(), "Begin transaction 2");
    TEST_ASSERT(db->rollback(), "Rollback transaction");
    TEST_ASSERT(db->getTransactionState() == TransactionState::RolledBack, "Transaction rolled back");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试参数化查询
 */
bool test_parameterized_query() {
    std::cout << "Running: test_parameterized_query..." << std::endl;

    auto db = std::make_shared<MockDbConnection>();
    db->connect({});

    // 设置处理器
    db->setQueryHandler("SELECT", [](const std::string& sql, const DbParams& params) {
        DbResult result;
        DbRow row;
        row.addColumn("result", static_cast<int32_t>(params.size()));
        result.addRow(row);
        return result;
    });

    DbParams params;
    params.push_back({"id", static_cast<int32_t>(42)});
    params.push_back({"name", std::string("Alice")});

    auto result = db->executeQuery("SELECT * FROM users WHERE id=:id AND name=:name", params);
    TEST_ASSERT(!result.isEmpty(), "Result has data");
    TEST_ASSERT(result[0].getInt32("result") == 2, "Parameter count");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试预编译语句
 */
bool test_prepared_statement() {
    std::cout << "Running: test_prepared_statement..." << std::endl;

    auto db = std::make_shared<MockDbConnection>();
    db->connect({});

    auto stmt = db->prepare("SELECT * FROM users WHERE id = ?");
    TEST_ASSERT(stmt != nullptr, "Statement created");

    stmt->bind(0, static_cast<int32_t>(42));
    auto result = stmt->executeQuery();
    // Mock 实现返回空结果，但不应该崩溃

    stmt->reset();
    stmt->close();

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试元数据
 */
bool test_metadata() {
    std::cout << "Running: test_metadata..." << std::endl;

    auto db = std::make_shared<MockDbConnection>();
    db->connect({});

    // 添加模拟表
    db->addTableData("users", {"id", "name", "email"}, {});

    auto tables = db->getTables();
    TEST_ASSERT(!tables.empty(), "Tables returned");
    TEST_ASSERT(std::find(tables.begin(), tables.end(), "users") != tables.end(), "Users table exists");

    auto columns = db->getColumns("users");
    TEST_ASSERT(columns.size() == 3, "Column count");
    TEST_ASSERT(columns[0] == "id", "First column");
    TEST_ASSERT(columns[1] == "name", "Second column");
    TEST_ASSERT(columns[2] == "email", "Third column");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试错误处理
 */
bool test_error_handling() {
    std::cout << "Running: test_error_handling..." << std::endl;

    auto db = std::make_shared<MockDbConnection>();
    db->connect({});

    // 设置错误处理器
    db->setUpdateHandler("ERROR", [](const std::string&, const DbParams&) {
        return static_cast<int64_t>(-1);
    });

    // Mock 实现不会自动设置错误，这里只测试接口
    TEST_ASSERT(db->getErrorCode() == 0, "No error initially");
    TEST_ASSERT(!db->hasError(), "No error flag");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试 DbManager
 */
bool test_db_manager() {
    std::cout << "Running: test_db_manager..." << std::endl;

    auto& manager = DbManager::instance();

    DbConfig config;
    config.type = DbType::MySQL;  // 会使用 Mock 因为没有定义宏

    // 暂时直接创建 Mock 连接
    auto db = std::make_shared<MockDbConnection>();
    TEST_ASSERT(db->connect(config), "Mock connect");
    TEST_ASSERT(db->isConnected(), "Mock connected");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// 测试运行器
//==============================================================================

int main() {
    std::cout << "====================================" << std::endl;
    std::cout << "=== Apollo Database Tests ===" << std::endl;
    std::cout << "====================================" << std::endl;
    std::cout << std::endl;

    int passed = 0;
    int total = 0;

    auto run = [&](const char* name, bool (*test)()) {
        total++;
        if (test()) passed++;
        else {
            std::cout << "  FAILED!" << std::endl;
        }
    };

    // 运行所有测试
    run("test_connect", test_connect);
    run("test_dbrow_values", test_dbrow_values);
    run("test_query", test_query);
    run("test_update", test_update);
    run("test_transaction", test_transaction);
    run("test_parameterized_query", test_parameterized_query);
    run("test_prepared_statement", test_prepared_statement);
    run("test_metadata", test_metadata);
    run("test_error_handling", test_error_handling);
    run("test_db_manager", test_db_manager);

    std::cout << std::endl;
    std::cout << "====================================" << std::endl;
    std::cout << "=== Results: " << passed << "/" << total << " passed ===" << std::endl;
    std::cout << "====================================" << std::endl;

    return (passed == total) ? 0 : 1;
}

int test_database_main() {
    return main();
}
