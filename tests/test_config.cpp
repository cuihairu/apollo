/**
 * @file test_config.cpp
 * @brief 配置管理系统测试
 */

#include "apollo/core/config/config_manager.h"
#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <chrono>

using namespace apollo::core::config;

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
 * @brief 测试INI文件加载
 */
bool test_ini_load() {
    std::cout << "Running: test_ini_load..." << std::endl;

    std::string iniContent = R"(
        ; This is a comment
        [Server]
        host = 127.0.0.1
        port = 7700
        max_connections = 10000
        enabled = true

        [Database]
        host = localhost
        port = 3306
        username = root
        password = "secret123"
        timeout = 30.5
    )";

    auto& manager = ConfigManager::instance();
    manager.clear();

    bool success = manager.loadString(iniContent, ConfigFormat::Ini, "test");
    TEST_ASSERT(success, "Load INI content");

    // 测试读取值
    TEST_ASSERT(manager.getString("Server/host", "", "test") == "127.0.0.1", "Server host");
    TEST_ASSERT(manager.getInt("Server/port", 0, "test") == 7700, "Server port");
    TEST_ASSERT(manager.getInt("Server/max_connections", 0, "test") == 10000, "Max connections");
    TEST_ASSERT(manager.getBool("Server/enabled", false, "test") == true, "Server enabled");

    TEST_ASSERT(manager.getString("Database/host", "", "test") == "localhost", "DB host");
    TEST_ASSERT(manager.getInt("Database/port", 0, "test") == 3306, "DB port");
    TEST_ASSERT(manager.getString("Database/username", "", "test") == "root", "DB username");
    TEST_ASSERT(manager.getString("Database/password", "", "test") == "secret123", "DB password");
    TEST_ASSERT(manager.getDouble("Database/timeout", 0, "test") == 30.5, "DB timeout");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试默认值
 */
bool test_default_values() {
    std::cout << "Running: test_default_values..." << std::endl;

    auto& manager = ConfigManager::instance();
    manager.clear();

    manager.loadString("[Test]\nkey=value", ConfigFormat::Ini);

    // 存在的键
    TEST_ASSERT(manager.getString("Test/key", "default") == "value", "Existing key");
    TEST_ASSERT(manager.getInt("Test/key", 99) == 99, "Non-int key returns default");
    TEST_ASSERT(manager.getBool("Test/key", true) == false, "Non-bool key returns default");

    // 不存在的键
    TEST_ASSERT(manager.getString("Test/nonexistent", "default") == "default", "Non-existent string");
    TEST_ASSERT(manager.getInt("Test/nonexistent", 42) == 42, "Non-existent int");
    TEST_ASSERT(manager.getBool("Test/nonexistent", true) == true, "Non-existent bool");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试多配置节
 */
bool test_multiple_sections() {
    std::cout << "Running: test_multiple_sections..." << std::endl;

    auto& manager = ConfigManager::instance();
    manager.clear();

    manager.loadString("[App]\nname=App1", ConfigFormat::Ini, "section1");
    manager.loadString("[App]\nname=App2", ConfigFormat::Ini, "section2");

    TEST_ASSERT(manager.getString("App/name", "", "section1") == "App1", "Section1 value");
    TEST_ASSERT(manager.getString("App/name", "", "section2") == "App2", "Section2 value");

    auto sections = manager.getSections();
    TEST_ASSERT(sections.size() >= 2, "Multiple sections exist");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试配置节点
 */
bool test_config_node() {
    std::cout << "Running: test_config_node..." << std::endl;

    ConfigNode node;

    // 测试基本类型
    node.setInt(42);
    TEST_ASSERT(node.asInt() == 42, "Int value");
    TEST_ASSERT(node.asString() == "42", "Int to string");

    node.setDouble(3.14);
    TEST_ASSERT(node.asDouble() == 3.14, "Double value");

    node.setBool(true);
    TEST_ASSERT(node.asBool() == true, "Bool value");

    node.setString("hello");
    TEST_ASSERT(node.asString() == "hello", "String value");

    // 测试子节点
    ConfigNode child;
    child.setInt(100);
    node.setChild("child", child);

    TEST_ASSERT(node.hasChild("child"), "Has child");
    TEST_ASSERT(node.getChild("child").asInt() == 100, "Child value");

    // 测试路径访问
    ConfigNode root;
    root.setChild("a", ConfigNode{});
    root.getChild("a").setChild("b", ConfigNode{});
    root.getByPath("a/b")->setInt(123);

    TEST_ASSERT(root.getByPath("a/b") != nullptr, "Path exists");
    TEST_ASSERT(root.getByPath("a/b")->asInt() == 123, "Path value");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试数组
 */
bool test_array() {
    std::cout << "Running: test_array..." << std::endl;

    ConfigNode node;
    std::vector<std::string> arr = {"a", "b", "c"};
    node.setArray(arr);

    auto result = node.asArray();
    TEST_ASSERT(result.size() == 3, "Array size");
    TEST_ASSERT(result[0] == "a", "Array element 0");
    TEST_ASSERT(result[1] == "b", "Array element 1");
    TEST_ASSERT(result[2] == "c", "Array element 2");

    // 测试字符串分割
    ConfigNode strNode;
    strNode.setString("x,y,z");
    auto splitResult = strNode.asArray();
    TEST_ASSERT(splitResult.size() == 3, "Split array size");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试配置合并
 */
bool test_merge() {
    std::cout << "Running: test_merge..." << std::endl;

    ConfigNode node1;
    node1.setInt(10);
    node1.setChild("a", ConfigNode{});
    node1.getChild("a").setInt(100);

    ConfigNode node2;
    node2.setInt(20);
    node2.setChild("b", ConfigNode{});
    node2.getChild("b").setInt(200);

    node1.merge(node2);

    // node2的值应该覆盖node1
    TEST_ASSERT(node1.asInt() == 20, "Value merged");
    TEST_ASSERT(node1.hasChild("a"), "Child a exists");
    TEST_ASSERT(node1.hasChild("b"), "Child b exists");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试JSON格式
 */
bool test_json_format() {
    std::cout << "Running: test_json_format..." << std::endl;

    std::string jsonContent = R"(
        {
            "name": "TestApp",
            "version": "1.0.0",
            "debug": true,
            "port": 8080
        }
    )";

    auto& manager = ConfigManager::instance();
    manager.clear();

    bool success = manager.loadString(jsonContent, ConfigFormat::Json, "json_test");
    TEST_ASSERT(success, "Load JSON content");

    TEST_ASSERT(manager.getString("name", "", "json_test") == "TestApp", "JSON name");
    TEST_ASSERT(manager.getString("version", "", "json_test") == "1.0.0", "JSON version");
    TEST_ASSERT(manager.getBool("debug", false, "json_test") == true, "JSON debug");
    TEST_ASSERT(manager.getInt("port", 0, "json_test") == 8080, "JSON port");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试便捷宏
 */
bool test_macros() {
    std::cout << "Running: test_macros..." << std::endl;

    auto& manager = ConfigManager::instance();
    manager.clear();

    manager.loadString("[Test]\nstr=value\nint=42\nbool=true\ndbl=3.14",
                      ConfigFormat::Ini);

    TEST_ASSERT(APOLLO_CONFIG_STR("Test/str", "def") == "value", "STR macro");
    TEST_ASSERT(APOLLO_CONFIG_INT("Test/int", 0) == 42, "INT macro");
    TEST_ASSERT(APOLLO_CONFIG_BOOL("Test/bool", false) == true, "BOOL macro");
    TEST_ASSERT(APOLLO_CONFIG_DOUBLE("Test/dbl", 0.0) == 3.14, "DOUBLE macro");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试转储
 */
bool test_dump() {
    std::cout << "Running: test_dump..." << std::endl;

    auto& manager = ConfigManager::instance();
    manager.clear();

    manager.loadString("[Section1]\nkey1=value1\nkey2=42", ConfigFormat::Ini, "dump_test");

    std::string dump = manager.dump("dump_test");

    TEST_ASSERT(dump.find("Section1") != std::string::npos, "Dump contains section");
    TEST_ASSERT(dump.find("key1") != std::string::npos, "Dump contains key1");
    TEST_ASSERT(dump.find("value1") != std::string::npos, "Dump contains value1");

    std::cout << dump << std::endl;
    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试loadFile + reload（确保记录filePath/format）
 */
bool test_loadfile_reload() {
    std::cout << "Running: test_loadfile_reload..." << std::endl;

    auto& manager = ConfigManager::instance();
    manager.clear();

    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("apollo_config_test_" + std::to_string(now) + ".ini");

    {
        std::ofstream out(path, std::ios::binary);
        TEST_ASSERT(out.is_open(), "Open temp ini file for write");
        out << "[Test]\nvalue=1\n";
    }

    bool success = manager.loadFile(path.string(), ConfigFormat::Auto, "file_test");
    TEST_ASSERT(success, "Load INI file with Auto format");
    TEST_ASSERT(manager.getInt("Test/value", 0, "file_test") == 1, "Read initial value");

    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        TEST_ASSERT(out.is_open(), "Open temp ini file for rewrite");
        out << "[Test]\nvalue=2\n";
    }

    success = manager.reload("file_test");
    TEST_ASSERT(success, "Reload file_test section");
    TEST_ASSERT(manager.getInt("Test/value", 0, "file_test") == 2, "Read reloaded value");

    std::error_code ec;
    std::filesystem::remove(path, ec);

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// 测试运行器
//==============================================================================

int main() {
    std::cout << "====================================" << std::endl;
    std::cout << "=== Apollo Config System Tests ===" << std::endl;
    std::cout << "====================================" << std::endl;
    std::cout << std::endl;

    int passed = 0;
    int total = 0;

    auto run = [&](const char*, bool (*test)()) {
        total++;
        if (test()) passed++;
        else {
            std::cout << "  FAILED!" << std::endl;
        }
    };

    // 运行所有测试
    run("test_ini_load", test_ini_load);
    run("test_default_values", test_default_values);
    run("test_multiple_sections", test_multiple_sections);
    run("test_config_node", test_config_node);
    run("test_array", test_array);
    run("test_merge", test_merge);
    run("test_json_format", test_json_format);
    run("test_macros", test_macros);
    run("test_dump", test_dump);
    run("test_loadfile_reload", test_loadfile_reload);

    std::cout << std::endl;
    std::cout << "====================================" << std::endl;
    std::cout << "=== Results: " << passed << "/" << total << " passed ===" << std::endl;
    std::cout << "====================================" << std::endl;

    return (passed == total) ? 0 : 1;
}
