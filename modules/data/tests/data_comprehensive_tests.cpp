/**
 * @file data_comprehensive_tests.cpp
 * @brief Comprehensive test suite for apollo::data module
 *
 * Coverage targets:
 * - MemoryConnection: 95%+
 * - SqlTemplate: 90%+
 * - IDataSource/SimpleDataSource: 90%+
 * - QueryResult: 100%
 */

#include "apollo/data/core/connection.hpp"
#include "apollo/data/core/data_source.hpp"
#include "apollo/data/orm/memory_connection.hpp"
#include "apollo/data/orm/sql_template.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>

// ============================================================================
// Test Framework
// ============================================================================

#define TEST(name) bool test_##name()
#define ASSERT_TRUE(expr) do { if (!(expr)) { std::cerr << "FAILED: " #expr << " at " << __FILE__ << ":" << __LINE__ << std::endl; return false; } } while(0)
#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_NE(a, b) ASSERT_TRUE((a) != (b))
#define ASSERT_LT(a, b) ASSERT_TRUE((a) < (b))
#define ASSERT_GT(a, b) ASSERT_TRUE((a) > (b))
#define ASSERT_GE(a, b) ASSERT_TRUE((a) >= (b))
#define ASSERT_LE(a, b) ASSERT_TRUE((a) <= (b))
#define ASSERT_STREQ(a, b) ASSERT_TRUE(std::string(a) == std::string(b))
#define ASSERT_STRNE(a, b) ASSERT_TRUE(std::string(a) != std::string(b))
#define ASSERT_NULL(ptr) ASSERT_TRUE((ptr) == nullptr)
#define ASSERT_NOT_NULL(ptr) ASSERT_TRUE((ptr) != nullptr)

// ============================================================================
// QueryResult Tests
// ============================================================================

TEST(query_result_default) {
    apollo::data::core::QueryResult result;

    ASSERT_TRUE(result.ok);
    ASSERT_TRUE(result.error.empty());
    ASSERT_EQ(result.affected_rows, 0);
    ASSERT_TRUE(result.rows.empty());

    return true;
}

TEST(query_result_error_state) {
    apollo::data::core::QueryResult result;

    result.ok = false;
    result.error = "Connection failed";

    ASSERT_FALSE(result.ok);
    ASSERT_EQ(result.error, "Connection failed");

    return true;
}

TEST(query_result_with_rows) {
    apollo::data::core::QueryResult result;

    apollo::data::core::QueryRow row1;
    row1["id"] = "1";
    row1["name"] = "Alice";

    apollo::data::core::QueryRow row2;
    row2["id"] = "2";
    row2["name"] = "Bob";

    result.rows.push_back(row1);
    result.rows.push_back(row2);

    ASSERT_EQ(result.rows.size(), 2);
    ASSERT_EQ(result.rows[0]["id"], "1");
    ASSERT_EQ(result.rows[1]["name"], "Bob");

    return true;
}

TEST(query_result_affected_rows) {
    apollo::data::core::QueryResult result;

    result.affected_rows = 5;

    ASSERT_EQ(result.affected_rows, 5);

    return true;
}

// ============================================================================
// MemoryConnection Tests
// ============================================================================

TEST(memory_connection_connect_disconnect) {
    apollo::data::orm::MemoryConnection conn;

    ASSERT_FALSE(conn.is_connected());

    ASSERT_TRUE(conn.connect());
    ASSERT_TRUE(conn.is_connected());

    conn.disconnect();
    ASSERT_FALSE(conn.is_connected());

    return true;
}

TEST(memory_connection_execute_query_empty) {
    apollo::data::orm::MemoryConnection conn;

    ASSERT_TRUE(conn.connect());

    auto result = conn.execute_query("SELECT * FROM users WHERE id = 1");

    ASSERT_TRUE(result.ok);
    ASSERT_TRUE(result.rows.empty());

    conn.disconnect();

    return true;
}

TEST(memory_connection_execute_query_with_data) {
    apollo::data::orm::MemoryConnection conn;

    // Seed some test data
    std::vector<apollo::data::core::QueryRow> rows;
    apollo::data::core::QueryRow row1;
    row1["id"] = "1";
    row1["name"] = "Alice";
    rows.push_back(row1);

    conn.seed_query("SELECT * FROM users", rows);

    ASSERT_TRUE(conn.connect());

    auto result = conn.execute_query("SELECT * FROM users");

    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.rows.size(), 1);
    ASSERT_EQ(result.rows[0]["id"], "1");
    ASSERT_EQ(result.rows[0]["name"], "Alice");

    conn.disconnect();

    return true;
}

TEST(memory_connection_execute_query_error) {
    apollo::data::orm::MemoryConnection conn;

    ASSERT_TRUE(conn.connect());

    // Unseeded query returns empty result (no error in this implementation)
    auto result = conn.execute_query("INVALID SQL");

    // MemoryConnection doesn't validate SQL, so this should return empty result
    ASSERT_TRUE(result.ok);

    conn.disconnect();

    return true;
}

TEST(memory_connection_execute_update) {
    apollo::data::orm::MemoryConnection conn;

    ASSERT_TRUE(conn.connect());

    auto result = conn.execute_update("UPDATE users SET name = 'Bob' WHERE id = 1");

    ASSERT_TRUE(result.ok);

    conn.disconnect();

    return true;
}

TEST(memory_connection_seed_query) {
    apollo::memory_connection conn;

    // Seed multiple rows
    std::vector<apollo::data::core::QueryRow> rows;
    for (int i = 1; i <= 3; ++i) {
        apollo::data::core::QueryRow row;
        row["id"] = std::to_string(i);
        row["value"] = "value" + std::to_string(i);
        rows.push_back(row);
    }

    conn.seed_query("SELECT * FROM test", rows);

    ASSERT_TRUE(conn.connect());

    auto result = conn.execute_query("SELECT * FROM test");

    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.rows.size(), 3);

    conn.disconnect();

    return true;
}

TEST(memory_connection_multiple_seeds) {
    apollo::data::orm::MemoryConnection conn;

    // Seed first query
    std::vector<apollo::data::core::QueryRow> rows1;
    apollo::data::core::QueryRow row1;
    row1["id"] = "1";
    rows1.push_back(row1);
    conn.seed_query("SELECT * FROM a", rows1);

    // Seed second query
    std::vector<apollo::data::core::QueryRow> rows2;
    apollo::data::core::QueryRow row2;
    row2["id"] = "2";
    rows2.push_back(row2);
    conn.seed_query("SELECT * FROM b", rows2);

    ASSERT_TRUE(conn.connect());

    auto result1 = conn.execute_query("SELECT * FROM a");
    auto result2 = conn.execute_query("SELECT * FROM b");

    ASSERT_EQ(result1.rows.size(), 1);
    ASSERT_EQ(result2.rows.size(), 1);
    ASSERT_EQ(result1.rows[0]["id"], "1");
    ASSERT_EQ(result2.rows[0]["id"], "2");

    conn.disconnect();

    return true;
}

TEST(memory_connection_reconnect) {
    apollo::data::orm::MemoryConnection conn;

    ASSERT_TRUE(conn.connect());
    ASSERT_TRUE(conn.is_connected());

    conn.disconnect();
    ASSERT_FALSE(conn.is_connected());

    // Reconnect
    ASSERT_TRUE(conn.connect());
    ASSERT_TRUE(conn.is_connected());

    conn.disconnect();

    return true;
}

TEST(memory_connection_query_without_connect) {
    apollo::data::orm::MemoryConnection conn;

    // Query without connect should still work (returns empty result)
    auto result = conn.execute_query("SELECT 1");

    ASSERT_TRUE(result.ok);
    ASSERT_TRUE(result.rows.empty());

    return true;
}

// ============================================================================
// SimpleDataSource Tests
// ============================================================================

struct TestConnection : public apollo::data::core::IConnection {
    bool connected = false;

    bool connect() override {
        connected = true;
        return true;
    }

    void disconnect() override {
        connected = false;
    }

    bool is_connected() const override {
        return connected;
    }

    apollo::data::core::QueryResult execute_query(const std::string& sql) override {
        apollo::data::core::QueryResult result;
        if (sql.find("error") != std::string::npos) {
            result.ok = false;
            result.error = "SQL error";
        }
        return result;
    }

    apollo::data::core::QueryResult execute_update(const std::string& sql) override {
        apollo::data::core::QueryResult result;
        result.affected_rows = 1;
        return result;
    }
};

TEST(simple_data_source_basic) {
    int connection_count = 0;

    apollo::data::core::SimpleDataSource source([&]() -> apollo::data::core::ConnectionPtr {
        ++connection_count;
        return std::make_shared<TestConnection>();
    });

    auto conn1 = source.acquire();
    auto conn2 = source.acquire();

    ASSERT_NOT_NULL(conn1);
    ASSERT_NOT_NULL(conn2);
    ASSERT_EQ(connection_count, 2);
    ASSERT_NE(conn1, conn2);  // Should create new connection each time

    return true;
}

TEST(simple_data_source_null_factory) {
    apollo::data::core::SimpleDataSource source(nullptr);

    auto conn = source.acquire();

    ASSERT_NULL(conn);

    return true;
}

TEST(simple_data_source_connection_lifecycle) {
    apollo::data::core::SimpleDataSource source([]() -> apollo::data::core::ConnectionPtr {
        return std::make_shared<TestConnection>();
    });

    auto conn = source.acquire();
    ASSERT_NOT_NULL(conn);

    auto* test_conn = static_cast<TestConnection*>(conn.get());
    ASSERT_FALSE(test_conn->connected);

    ASSERT_TRUE(test_conn->connect());
    ASSERT_TRUE(test_conn->is_connected());

    test_conn->disconnect();
    ASSERT_FALSE(test_conn->is_connected());

    return true;
}

// ============================================================================
// SqlTemplate Tests
// ============================================================================

TEST(sql_template_query_basic) {
    auto conn = std::make_shared<apollo::data::orm::MemoryConnection>();

    // Seed test data
    std::vector<apollo::data::core::QueryRow> rows;
    apollo::data::core::QueryRow row;
    row["id"] = "1";
    row["username"] = "alice";
    row["email"] = "alice@example.com";
    rows.push_back(row);

    conn->seed_query("SELECT * FROM users", rows);
    ASSERT_TRUE(conn->connect());

    apollo::data::orm::SqlTemplate templ(conn);

    auto result = templ.query("SELECT * FROM users");

    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.rows.size(), 1);
    ASSERT_EQ(result.rows[0]["username"], "alice");

    conn->disconnect();

    return true;
}

TEST(sql_template_query_with_mapper) {
    auto conn = std::make_shared<apollo::apollo::data::orm::MemoryConnection>();

    struct User {
        int id;
        std::string username;
        std::string email;
    };

    // Seed test data
    std::vector<apollo::data::core::QueryRow> rows;
    apollo::data::core::QueryRow row;
    row["id"] = "42";
    row["username"] = "bob";
    row["email"] = "bob@test.com";
    rows.push_back(row);

    conn->seed_query("SELECT * FROM users", rows);
    ASSERT_TRUE(conn->connect());

    apollo::data::orm::SqlTemplate templ(conn);

    auto users = templ.query<User>("SELECT * FROM users", [](const apollo::data::core::QueryRow& row) {
        User user;
        user.id = std::stoi(row.at("id"));
        user.username = row.at("username");
        user.email = row.at("email");
        return user;
    });

    ASSERT_EQ(users.size(), 1);
    ASSERT_EQ(users[0].id, 42);
    ASSERT_EQ(users[0].username, "bob");
    ASSERT_EQ(users[0].email, "bob@test.com");

    conn->disconnect();

    return true;
}

TEST(sql_template_query_empty_result) {
    auto conn = std::make_shared<apollo::data::orm::MemoryConnection>();
    ASSERT_TRUE(conn->connect());

    apollo::data::orm::SqlTemplate templ(conn);

    auto result = templ.query("SELECT * FROM empty_table");

    ASSERT_TRUE(result.ok);
    ASSERT_TRUE(result.rows.empty());

    conn->disconnect();

    return true;
}

TEST(sql_template_query_for_one) {
    auto conn = std::make_shared<apollo::data::orm::MemoryConnection>();

    // Seed multiple rows
    std::vector<apollo::data::core::QueryRow> rows;
    for (int i = 1; i <= 3; ++i) {
        apollo::data::core::QueryRow row;
        row["id"] = std::to_string(i);
        rows.push_back(row);
    }

    conn->seed_query("SELECT * FROM nums", rows);
    ASSERT_TRUE(conn->connect());

    apollo::data::orm::SqlTemplate templ(conn);

    auto result = templ.query_for_one<int>("SELECT * FROM nums", [](const apollo::data::core::QueryRow& row) {
        return std::stoi(row.at("id"));
    });

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(*result, 1);  // Should return first row

    conn->disconnect();

    return true;
}

TEST(sql_template_query_for_one_empty) {
    auto conn = std::make_shared<apollo::data::orm::MemoryConnection>();
    ASSERT_TRUE(conn->connect());

    apollo::data::orm::SqlTemplate templ(conn);

    auto result = templ.query_for_one<int>("SELECT * FROM empty", [](const apollo::data::core::QueryRow& row) {
        return 42;
    });

    ASSERT_FALSE(result.has_value());

    conn->disconnect();

    return true;
}

TEST(sql_template_update) {
    auto conn = std::make_shared<apollo::data::orm::MemoryConnection>();
    ASSERT_TRUE(conn->connect());

    apollo::data::orm::SqlTemplate templ(conn);

    auto result = templ.update("UPDATE users SET active = 1");

    ASSERT_TRUE(result.ok);

    conn->disconnect();

    return true;
}

TEST(sql_template_with_null_data_source) {
    // Create SqlTemplate with null data source
    // This tests error handling
    apollo::data::orm::SqlTemplate templ(nullptr);

    // Query should return empty result (no crash)
    auto result = templ.query("SELECT 1");

    ASSERT_TRUE(result.ok);  // May vary based on implementation

    return true;
}

TEST(sql_template_multiple_queries) {
    auto conn = std::make_shared<apollo::data::orm::MemoryConnection>();
    ASSERT_TRUE(conn->connect());

    apollo::data::orm::SqlTemplate templ(conn);

    // Seed data
    std::vector<apollo::data::core::QueryRow> rows1;
    apollo::data::core::QueryRow row;
    row["value"] = "100";
    rows1.push_back(row);

    conn->seed_query("SELECT value FROM config", rows1);

    // Execute multiple queries
    auto result1 = templ.query("SELECT value FROM config");
    ASSERT_EQ(result1.rows.size(), 1);

    auto result2 = templ.query("SELECT value FROM config");
    ASSERT_EQ(result2.rows.size(), 1);

    // Execute update
    auto result3 = templ.update("UPDATE config SET value = '200'");
    ASSERT_TRUE(result3.ok);

    conn->disconnect();

    return true;
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST(orm_crud_operations) {
    auto conn = std::make_shared<apollo::data::orm::MemoryConnection>();
    ASSERT_TRUE(conn->connect());

    // CREATE - seed data
    std::vector<apollo::data::core::QueryRow> rows;
    apollo::data::core::QueryRow row1;
    row1["id"] = "1";
    row1["name"] = "Product1";
    row1["price"] = "29.99";
    rows.push_back(row1);

    conn->seed_query("SELECT * FROM products", rows);

    // READ
    apollo::data::orm::SqlTemplate templ(conn);

    struct Product {
        std::string id;
        std::string name;
        double price;
    };

    auto products = templ.query<Product>("SELECT * FROM products", [](const apollo::data::core::QueryRow& row) {
        Product p;
        p.id = row.at("id");
        p.name = row.at("name");
        p.price = std::stod(row.at("price"));
        return p;
    });

    ASSERT_EQ(products.size(), 1);
    ASSERT_EQ(products[0].id, "1");
    ASSERT_EQ(products[0].name, "Product1");
    ASSERT_GT(products[0].price, 29.9);

    // UPDATE
    templ.update("UPDATE products SET price = 19.99");

    conn->disconnect();

    return true;
}

TEST(orm_transaction_simulation) {
    auto conn = std::make_shared<apollo::data::orm::MemoryConnection>();
    ASSERT_TRUE(conn->connect());

    // Seed data for two accounts
    std::vector<apollo::data::core::QueryRow> rows;
    apollo::data::core::QueryRow row1;
    row1["account"] = "A";
    row1["balance"] = "1000";
    rows.push_back(row1);

    apollo::data::core::QueryRow row2;
    row2["account"] = "B";
    row2["balance"] = "500";
    rows.push_back(row2);

    conn->seed_query("SELECT * FROM accounts", rows);

    apollo::data::orm::SqlTemplate templ(conn);

    // Simulate transfer: A -> B (100)
    templ.update("UPDATE accounts SET balance = 900 WHERE account = 'A'");
    templ.update("UPDATE accounts SET balance = 600 WHERE account = 'B'");

    // Verify balances
    auto accounts = templ.query("SELECT * FROM accounts ORDER BY account");

    ASSERT_EQ(accounts.rows.size(), 2);
    ASSERT_EQ(accounts.rows[0]["balance"], "900");   // Account A
    ASSERT_EQ(accounts.rows[1]["balance"], "600");   // Account B

    conn->disconnect();

    return true;
}

TEST(orm_join_query_simulation) {
    auto conn = std::make_shared<apollo::data::orm::MemoryConnection>();
    ASSERT_TRUE(conn->connect());

    // Seed data for users and orders
    std::vector<apollo::apollo::core::QueryRow> rows;
    apollo::data::core::QueryRow row1;
    row1["user_id"] = "1";
    row1["username"] = "alice";
    row1["order_id"] = "101";
    row1["order_total"] = "50.00";
    rows.push_back(row1);

    conn->seed_query("SELECT u.user_id, u.username, o.order_id, o.order_total FROM users u JOIN orders o ON u.user_id = o.user_id", rows);

    apollo::data::orm::SqlTemplate templ(conn);

    struct OrderInfo {
        std::string user_id;
        std::string username;
        std::string order_id;
        double total;
    };

    auto orders = templ.query<OrderInfo>(
        "SELECT u.user_id, u.username, o.order_id, o.order_total FROM users u JOIN orders o ON u.user_id = o.user_id",
        [](const apollo::data::core::QueryRow& row) {
            OrderInfo info;
            info.user_id = row.at("user_id");
            info.username = row.at("username");
            info.order_id = row.at("order_id");
            info.total = std::stod(row.at("order_total"));
            return info;
        }
    );

    ASSERT_EQ(orders.size(), 1);
    ASSERT_EQ(orders[0].username, "alice");
    ASSERT_GT(orders[0].total, 49.9);

    conn->disconnect();

    return true;
}

TEST(orm_aggregate_query_simulation) {
    auto conn = std::make_shared<apollo::data::orm::MemoryConnection>();
    ASSERT_TRUE(conn->connect());

    // Seed aggregate data
    std::vector<apollo::data::core::QueryRow> rows;
    apollo::data::core::QueryRow row;
    row["count"] = "150";
    row["avg_price"] = "25.50";
    row["max_price"] = "99.99";
    rows.push_back(row);

    conn->seed_query("SELECT COUNT(*), AVG(price), MAX(price) FROM products", rows);

    apollo::data::orm::SqlTemplate templ(conn);

    auto result = templ.query("SELECT COUNT(*), AVG(price), MAX(price) FROM products");

    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.rows.size(), 1);
    ASSERT_EQ(result.rows[0]["count"], "150");

    conn->disconnect();

    return true;
}

TEST(orm_error_handling) {
    // Create a connection that returns errors
    class ErrorConnection : public apollo::data::core::IConnection {
    public:
        bool connect() override { return true; }
        void disconnect() override {}
        bool is_connected() const override { return true; }

        apollo::data::core::QueryResult execute_query(const std::string& sql) override {
            apollo::data::core::QueryResult result;
            result.ok = false;
            result.error = "Table not found: " + sql;
            return result;
        }

        apollo::data::core::QueryResult execute_update(const std::string& sql) override {
            apollo::data::core::QueryResult result;
            result.ok = false;
            result.error = "Update failed";
            return result;
        }
    };

    auto conn = std::make_shared<ErrorConnection>();
    apollo::data::orm::SqlTemplate templ(conn);

    auto query_result = templ.query("SELECT * FROM missing_table");

    ASSERT_FALSE(query_result.ok);
    ASSERT_FALSE(query_result.error.empty());

    auto update_result = templ.update("UPDATE missing_table SET x = 1");

    ASSERT_FALSE(update_result.ok);

    return true;
}

// ============================================================================
// Data Type Mapping Tests
// ============================================================================

TEST(orm_type_mapping_string) {
    auto conn = std::make_shared<apollo::data::orm::MemoryConnection>();

    std::vector<apollo::data::core::QueryRow> rows;
    apollo::data::core::QueryRow row;
    row["string_col"] = "hello";
    row["empty_col"] = "";
    row["null_col"] = "";  // Simulated null as empty string
    rows.push_back(row);

    conn->seed_query("SELECT * FROM strings", rows);
    ASSERT_TRUE(conn->connect());

    auto result = conn->execute_query("SELECT * FROM strings");

    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.rows[0]["string_col"], "hello");
    ASSERT_EQ(result.rows[0]["empty_col"], "");

    conn->disconnect();

    return true;
}

TEST(orm_type_mapping_numeric) {
    auto conn = std::make_shared<apollo::data::orm::MemoryConnection>();

    std::vector<apollo::data::core::QueryRow> rows;
    apollo::data::core::QueryRow row;
    row["int_col"] = "42";
    row["double_col"] = "3.14159";
    row["negative_col"] = "-100";
    rows.push_back(row);

    conn->seed_query("SELECT * FROM numbers", rows);
    ASSERT_TRUE(conn->connect());

    auto result = conn->execute_query("SELECT * FROM numbers");

    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.rows[0]["int_col"], "42");
    ASSERT_TRUE(result.rows[0]["negative_col"], "-100");

    conn->disconnect();

    return true;
}

// ============================================================================
// Concurrent Tests
// ============================================================================

TEST(orm_concurrent_queries) {
    auto conn = std::make_shared<apollo::data::orm::MemoryConnection>();
    ASSERT_TRUE(conn->connect());

    // Seed data
    std::vector<apollo::data::core::QueryRow> rows;
    for (int i = 0; i < 10; ++i) {
        apollo::data::core::QueryRow row;
        row["id"] = std::to_string(i);
        row["value"] = "value" + std::to_string(i);
        rows.push_back(row);
    }

    conn->seed_query("SELECT * FROM concurrent_test", rows);

    apollo::data::orm::SqlTemplate templ(conn);

    std::atomic<int> query_count{0};

    // Execute multiple queries from "different threads"
    auto query_func = [&]() {
        for (int i = 0; i < 5; ++i) {
            templ.query("SELECT * FROM concurrent_test");
            ++query_count;
        }
    };

    std::thread t1(query_func);
    std::thread t2(query_func);

    t1.join();
    t2.join();

    ASSERT_EQ(query_count, 10);

    conn->disconnect();

    return true;
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST(sql_template_empty_sql) {
    auto conn = std::make_shared<apollo::data::orm::MemoryConnection>();
    ASSERT_TRUE(conn->connect());

    apollo::data::orm::SqlTemplate templ(conn);

    // Empty SQL
    auto result = templ.query("");

    ASSERT_TRUE(result.ok);

    conn->disconnect();

    return true;
}

TEST(memory_connection_seed_overwrite) {
    apollo::memory_connection conn;

    // Seed initial data
    std::vector<apollo::data::core::QueryRow> rows1;
    apollo::data::core::QueryRow row1;
    row1["id"] = "1";
    rows1.push_back(row1);

    conn.seed_query("SELECT * FROM test", rows1);

    // Seed with same SQL again (should overwrite or add)
    std::vector<apollo::data::core::QueryRow> rows2;
    apollo::data::core::QueryRow row2;
    row2["id"] = "2";
    rows2.push_back(row2);

    conn.seed_query("SELECT * FROM test", rows2);

    ASSERT_TRUE(conn.connect());

    auto result = conn.execute_query("SELECT * FROM test");

    // Should have the latest seeded data
    ASSERT_EQ(result.rows.size(), 1);
    ASSERT_EQ(result.rows[0]["id"], "2");

    conn.disconnect();

    return true;
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << "=== Apollo Data Module Comprehensive Tests ===" << std::endl;
    std::cout << std::endl;

    int passed = 0;
    int failed = 0;

    #define RUN_TEST(name) \
        do { \
            std::cout << "Running test_" #name "... "; \
            if (test_##name()) { \
                std::cout << "PASHED" << std::endl; \
                ++passed; \
            } else { \
                std::cout << "FAILED" << std::endl; \
                ++failed; \
            } \
        } while(0)

    // QueryResult Tests
    std::cout << "--- QueryResult Tests ---" << std::endl;
    RUN_TEST(query_result_default);
    RUN_TEST(query_result_error_state);
    RUN_TEST(query_result_with_rows);
    RUN_TEST(query_result_affected_rows);

    // MemoryConnection Tests
    std::cout << "\n--- MemoryConnection Tests ---" << std::endl;
    RUN_TEST(memory_connection_connect_disconnect);
    RUN_TEST(memory_connection_execute_query_empty);
    RUN_TEST(memory_connection_execute_query_with_data);
    RUN_TEST(memory_connection_execute_query_error);
    RUN_TEST(memory_connection_execute_update);
    RUN_TEST(memory_connection_seed_query);
    RUN_TEST(memory_connection_multiple_seeds);
    RUN_TEST(memory_connection_reconnect);
    RUN_TEST(memory_connection_query_without_connect);

    // SimpleDataSource Tests
    std::cout << "\n--- SimpleDataSource Tests ---" << std::endl;
    RUN_TEST(simple_data_source_basic);
    RUN_TEST(simple_data_source_null_factory);
    RUN_TEST(simple_data_source_connection_lifecycle);

    // SqlTemplate Tests
    std::cout << "\n--- SqlTemplate Tests ---" << std::endl;
    RUN_TEST(sql_template_query_basic);
    RUN_TEST(sql_template_query_with_mapper);
    RUN_TEST(sql_template_query_empty_result);
    RUN_TEST(sql_template_query_for_one);
    RUN_TEST(sql_template_query_for_one_empty);
    RUN_TEST(sql_template_update);
    RUN_TEST(sql_template_with_null_data_source);
    RUN_TEST(sql_template_multiple_queries);

    // Integration Tests
    std::cout << "\n--- Integration Tests ---" << std::endl;
    RUN_TEST(orm_crud_operations);
    RUN_TEST(orm_transaction_simulation);
    RUN_TEST(orm_join_query_simulation);
    RUN_TEST(orm_aggregate_query_simulation);
    RUN_TEST(orm_error_handling);

    // Data Type Mapping Tests
    std::cout << "\n--- Data Type Mapping Tests ---" << std::endl;
    RUN_TEST(orm_type_mapping_string);
    RUN_TEST(orm_type_mapping_numeric);

    // Concurrent Tests
    std::cout << "\n--- Concurrent Tests ---" << std::endl;
    RUN_TEST(orm_concurrent_queries);

    // Edge Cases
    std::cout << "\n--- Edge Cases ---" << std::endl;
    RUN_TEST(sql_template_empty_sql);
    RUN_TEST(memory_connection_seed_overwrite);

    #undef RUN_TEST

    std::cout << "\n=== Test Summary ===" << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << failed << std::endl;

    if (failed > 0) {
        std::cerr << "\nSome tests FAILED!" << std::endl;
        return 1;
    }

    std::cout << "\nAll tests PASSED!" << std::endl;
    return 0;
}
