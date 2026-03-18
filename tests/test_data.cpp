/**
 * @file test_data.cpp
 * @brief Data module unit tests
 */

#include "apollo/data/cache/cache_manager.hpp"
#include "apollo/data/cache/primitive_cache.hpp"
#include "apollo/data/core/connection.hpp"
#include "apollo/data/core/data_source.hpp"
#include "apollo/data/orm/memory_connection.hpp"
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

using namespace apollo::data::cache;
using namespace apollo::data::core;
using namespace apollo::data::orm;

namespace {

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

//==============================================================================
// QueryResult Tests
//==============================================================================

bool test_query_result_default() {
    std::cout << "Running: test_query_result_default..." << std::endl;

    QueryResult result;
    TEST_ASSERT(result.ok, "Default ok is true");
    TEST_ASSERT(result.error.empty(), "Default error is empty");
    TEST_ASSERT(result.affected_rows == 0, "Default affected_rows is 0");
    TEST_ASSERT(result.rows.empty(), "Default rows is empty");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_query_result_with_rows() {
    std::cout << "Running: test_query_result_with_rows..." << std::endl;

    QueryResult result;
    QueryRow row1;
    row1["id"] = "1";
    row1["name"] = "Alice";

    QueryRow row2;
    row2["id"] = "2";
    row2["name"] = "Bob";

    result.rows.push_back(row1);
    result.rows.push_back(row2);
    result.affected_rows = 2;

    TEST_ASSERT(result.rows.size() == 2, "Two rows");
    TEST_ASSERT(result.affected_rows == 2, "affected_rows is 2");
    TEST_ASSERT(result.rows[0]["id"] == "1", "First row id");
    TEST_ASSERT(result.rows[1]["name"] == "Bob", "Second row name");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// IConnection Tests
//==============================================================================

bool test_connection_interface() {
    std::cout << "Running: test_connection_interface..." << std::endl;

    MemoryConnection conn;

    TEST_ASSERT(!conn.is_connected(), "Not connected initially");

    bool connected = conn.connect();
    TEST_ASSERT(connected, "Connect succeeds");
    TEST_ASSERT(conn.is_connected(), "Connected after connect()");

    conn.disconnect();
    TEST_ASSERT(!conn.is_connected(), "Not connected after disconnect()");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_connection_execute_query() {
    std::cout << "Running: test_connection_execute_query..." << std::endl;

    MemoryConnection conn;
    conn.connect();

    // Seed some test data
    QueryRow row1;
    row1["id"] = "1";
    row1["value"] = "test1";

    QueryRow row2;
    row2["id"] = "2";
    row2["value"] = "test2";

    conn.seed_query("SELECT * FROM test", {row1, row2});

    QueryResult result = conn.execute_query("SELECT * FROM test");

    TEST_ASSERT(result.ok, "Query ok");
    TEST_ASSERT(result.rows.size() == 2, "Two rows returned");
    TEST_ASSERT(result.rows[0]["value"] == "test1", "First row value");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_connection_execute_update() {
    std::cout << "Running: test_connection_execute_update..." << std::endl;

    MemoryConnection conn;
    conn.connect();

    QueryResult result = conn.execute_update("INSERT INTO test VALUES (1, 'test')");

    TEST_ASSERT(result.ok, "Update ok");
    // MemoryConnection doesn't track affected_rows by default
    TEST_ASSERT(result.affected_rows == 0, "affected_rows initialized to 0");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// IDataSource Tests
//==============================================================================

bool test_simple_data_source() {
    std::cout << "Running: test_simple_data_source..." << std::endl;

    int factory_calls = 0;
    ConnectionPtr test_conn;

    auto factory = [&]() -> ConnectionPtr {
        factory_calls++;
        test_conn = std::make_shared<MemoryConnection>();
        return test_conn;
    };

    SimpleDataSource source(factory);

    auto conn = source.acquire();
    TEST_ASSERT(factory_calls == 1, "Factory called once");
    TEST_ASSERT(conn != nullptr, "Connection returned");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// ICacheProvider Tests
//==============================================================================

class TestCacheProvider : public ICacheProvider {
public:
    std::unordered_map<std::string, std::string> storage;
    int get_calls = 0;
    int set_calls = 0;
    int remove_calls = 0;
    int exists_calls = 0;

    bool get(const std::string& key, std::string& value) override {
        get_calls++;
        auto it = storage.find(key);
        if (it != storage.end()) {
            value = it->second;
            return true;
        }
        return false;
    }

    void set(const std::string& key, const std::string& value, int ttl_seconds) override {
        set_calls++;
        storage[key] = value;
    }

    void remove(const std::string& key) override {
        remove_calls++;
        storage.erase(key);
    }

    bool exists(const std::string& key) override {
        exists_calls++;
        return storage.find(key) != storage.end();
    }
};

bool test_cache_provider_interface() {
    std::cout << "Running: test_cache_provider_interface..." << std::endl;

    TestCacheProvider provider;

    // Initially empty
    std::string value;
    TEST_ASSERT(!provider.get("key", value), "Get returns false for missing key");
    TEST_ASSERT(!provider.exists("key"), "Exists returns false for missing key");

    // Set and get
    provider.set("key", "value", 0);
    TEST_ASSERT(provider.exists("key"), "Exists returns true after set");
    TEST_ASSERT(provider.get("key", value), "Get returns true after set");
    TEST_ASSERT(value == "value", "Value matches");

    // Remove
    provider.remove("key");
    TEST_ASSERT(!provider.exists("key"), "Exists returns false after remove");

    // Method call counts
    TEST_ASSERT(provider.get_calls == 2, "Get calls");
    TEST_ASSERT(provider.set_calls == 1, "Set calls");
    TEST_ASSERT(provider.remove_calls == 1, "Remove calls");
    TEST_ASSERT(provider.exists_calls == 3, "Exists calls");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// PrimitiveCache Tests
//==============================================================================

bool test_primitive_cache_set_get() {
    std::cout << "Running: test_primitive_cache_set_get..." << std::endl;

    PrimitiveCache cache;

    cache.set("key1", "value1", 0);

    std::string value;
    TEST_ASSERT(cache.get("key1", value), "Get returns true");
    TEST_ASSERT(value == "value1", "Value matches");

    TEST_ASSERT(cache.exists("key1"), "Key exists");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_primitive_cache_get_missing() {
    std::cout << "Running: test_primitive_cache_get_missing..." << std::endl;

    PrimitiveCache cache;

    std::string value;
    TEST_ASSERT(!cache.get("missing", value), "Get returns false for missing key");
    TEST_ASSERT(!cache.exists("missing"), "Exists returns false for missing key");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_primitive_cache_remove() {
    std::cout << "Running: test_primitive_cache_remove..." << std::endl;

    PrimitiveCache cache;

    cache.set("key", "value", 0);
    TEST_ASSERT(cache.exists("key"), "Key exists before remove");

    cache.remove("key");
    TEST_ASSERT(!cache.exists("key"), "Key doesn't exist after remove");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_primitive_cache_ttl() {
    std::cout << "Running: test_primitive_cache_ttl..." << std::endl;

    PrimitiveCache cache;

    // Set with short TTL
    cache.set("temp_key", "temp_value", 1);  // 1 second

    std::string value;
    TEST_ASSERT(cache.get("temp_key", value), "Get returns true immediately");

    // Wait for expiration
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    cache.cleanup_expired();

    TEST_ASSERT(!cache.get("temp_key", value), "Get returns false after TTL");
    TEST_ASSERT(!cache.exists("temp_key"), "Key doesn't exist after TTL");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_primitive_cache_cleanup() {
    std::cout << "Running: test_primitive_cache_cleanup..." << std::endl;

    PrimitiveCache cache;

    cache.set("permanent", "value", 0);  // No TTL
    cache.set("temporary", "value", 1);  // 1 second TTL

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    cache.cleanup_expired();

    std::string value;
    TEST_ASSERT(cache.get("permanent", value), "Permanent entry still exists");
    TEST_ASSERT(!cache.get("temporary", value), "Temporary entry removed");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_primitive_cache_overwrite() {
    std::cout << "Running: test_primitive_cache_overwrite..." << std::endl;

    PrimitiveCache cache;

    cache.set("key", "value1", 0);
    cache.set("key", "value2", 0);  // Overwrite

    std::string value;
    cache.get("key", value);

    TEST_ASSERT(value == "value2", "Value is overwritten");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// CacheManager Tests
//==============================================================================

bool test_cache_manager_singleton() {
    std::cout << "Running: test_cache_manager_singleton..." << std::endl;

    auto& mgr1 = CacheManager::instance();
    auto& mgr2 = CacheManager::instance();

    TEST_ASSERT(&mgr1 == &mgr2, "Same instance");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_cache_manager_set_provider() {
    std::cout << "Running: test_cache_manager_set_provider..." << std::endl;

    auto& mgr = CacheManager::instance();
    auto provider = std::make_shared<TestCacheProvider>();

    mgr.set_provider(provider);
    // Should not crash

    TEST_ASSERT(true, "Provider set without crash");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_cache_manager_get_set() {
    std::cout << "Running: test_cache_manager_get_set..." << std::endl;

    auto& mgr = CacheManager::instance();
    auto provider = std::make_shared<TestCacheProvider>();
    mgr.set_provider(provider);

    mgr.set("key", "value", 0);

    std::string result;
    TEST_ASSERT(mgr.get("key", result), "Get returns true");
    TEST_ASSERT(result == "value", "Value matches");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_cache_manager_remove() {
    std::cout << "Running: test_cache_manager_remove..." << std::endl;

    auto& mgr = CacheManager::instance();
    auto provider = std::make_shared<TestCacheProvider>();
    mgr.set_provider(provider);

    mgr.set("key", "value", 0);
    TEST_ASSERT(mgr.exists("key"), "Key exists");

    mgr.remove("key");
    TEST_ASSERT(!mgr.exists("key"), "Key doesn't exist after remove");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// MemoryConnection Tests
//==============================================================================

bool test_memory_connection_seed() {
    std::cout << "Running: test_memory_connection_seed..." << std::endl;

    MemoryConnection conn;
    conn.connect();

    QueryRow row;
    row["id"] = "123";
    row["data"] = "test_data";

    conn.seed_query("SELECT * FROM test", {row});

    QueryResult result = conn.execute_query("SELECT * FROM test");

    TEST_ASSERT(result.ok, "Query successful");
    TEST_ASSERT(result.rows.size() == 1, "One row");
    TEST_ASSERT(result.rows[0]["id"] == "123", "ID matches");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_memory_connection_empty_query() {
    std::cout << "Running: test_memory_connection_empty_query..." << std::endl;

    MemoryConnection conn;
    conn.connect();

    QueryResult result = conn.execute_query("SELECT * FROM empty");

    TEST_ASSERT(result.ok, "Query ok even when empty");
    TEST_ASSERT(result.rows.empty(), "No rows returned");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// Integration Tests
//==============================================================================

bool test_cache_with_data_integration() {
    std::cout << "Running: test_cache_with_data_integration..." << std::endl;

    // Set up cache manager
    auto& cache_mgr = CacheManager::instance();
    auto cache_provider = std::make_shared<PrimitiveCache>();
    cache_mgr.set_provider(cache_provider);

    // Cache some query results
    cache_mgr.set("user:1:name", "Alice", 60);
    cache_mgr.set("user:1:email", "alice@example.com", 60);

    // Retrieve from cache
    std::string name, email;
    cache_mgr.get("user:1:name", name);
    cache_mgr.get("user:1:email", email);

    TEST_ASSERT(name == "Alice", "Name cached correctly");
    TEST_ASSERT(email == "alice@example.com", "Email cached correctly");

    std::cout << "  PASSED" << std::endl;
    return true;
}

} // namespace

//==============================================================================
// Main Test Runner
//==============================================================================

int main() {
    std::cout << "=== Apollo Data Module Test Suite ===" << std::endl;

    int total = 0;
    int passed = 0;

    auto run = [&](bool (*fn)()) {
        total++;
        if (fn()) {
            passed++;
        }
    };

    // QueryResult tests
    run(test_query_result_default);
    run(test_query_result_with_rows);

    // IConnection tests
    run(test_connection_interface);
    run(test_connection_execute_query);
    run(test_connection_execute_update);

    // IDataSource tests
    run(test_simple_data_source);

    // ICacheProvider tests
    run(test_cache_provider_interface);

    // PrimitiveCache tests
    run(test_primitive_cache_set_get);
    run(test_primitive_cache_get_missing);
    run(test_primitive_cache_remove);
    run(test_primitive_cache_ttl);
    run(test_primitive_cache_cleanup);
    run(test_primitive_cache_overwrite);

    // CacheManager tests
    run(test_cache_manager_singleton);
    run(test_cache_manager_set_provider);
    run(test_cache_manager_get_set);
    run(test_cache_manager_remove);

    // MemoryConnection tests
    run(test_memory_connection_seed);
    run(test_memory_connection_empty_query);

    // Integration tests
    run(test_cache_with_data_integration);

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total: " << total << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << (total - passed) << std::endl;

    return (total == passed) ? 0 : 1;
}

