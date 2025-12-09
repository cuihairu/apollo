#include "apollo/BaseComponent.h"
#include "apollo/ConfigManager.h"
#include "apollo/ConfigProperty.h"
#include <iostream>
#include <cassert>
#include <fstream>
#include <thread>
#include <chrono>

using namespace Apollo;

// External test runner
extern class TestRunner {
public:
    static void assert_true(bool condition, const std::string& message);
    static void assert_equals(int expected, int actual, const std::string& message);
    static void print_summary();
    static int get_result();
};

void test_basic_config_operations() {
    std::cout << "\nTesting Basic Config Operations..." << std::endl;

    auto config = ConfigManager::getInstance();

    // Test setting and getting values
    config->setValue("test.int", 42);
    config->setValue("test.string", "hello");
    config->setValue("test.bool", true);

    TestRunner::assert_equals(42, config->getValue<int>("test.int"),
        "Should retrieve correct int value");
    TestRunner::assert_equals(std::string("hello"), config->getValue<std::string>("test.string"),
        "Should retrieve correct string value");
    TestRunner::assert_true(config->getValue<bool>("test.bool"),
        "Should retrieve correct bool value");

    // Test default values
    TestRunner::assert_equals(100, config->getValue<int>("nonexistent", 100),
        "Should return default value for nonexistent key");
    TestRunner::assert_true(!config->hasValue("nonexistent"),
        "Should not have value for nonexistent key");

    // Test removing values
    config->removeValue("test.int");
    TestRunner::assert_true(!config->hasValue("test.int"),
        "Should not have value after removal");
}

void test_config_property() {
    std::cout << "\nTesting ConfigProperty..." << std::endl;

    auto config = ConfigManager::getInstance();
    config->setValue("app.port", 8080);
    config->setValue("app.name", "TestApp");

    // Create ConfigProperty instances
    ConfigProperty<int> portProp("app.port", 3000);
    ConfigProperty<std::string> nameProp("app.name", "DefaultApp");

    // Test values are loaded from config
    TestRunner::assert_equals(8080, portProp.get(),
        "ConfigProperty should load value from config");
    TestRunner::assert_equals(std::string("TestApp"), nameProp.get(),
        "ConfigProperty should load string value from config");

    // Test modifying property
    portProp = 9090;
    TestRunner::assert_equals(9090, config->getValue<int>("app.port"),
        "ConfigProperty should update config when modified");

    // Test default value
    ConfigProperty<int> defaultProp("nonexistent.key", 123);
    TestRunner::assert_equals(123, defaultProp.get(),
        "ConfigProperty should use default value when key not in config");
}

void test_json_persistence() {
    std::cout << "\nTesting JSON Persistence..." << std::endl;

    auto config = ConfigManager::getInstance();
    const std::string testFile = "test_config.json";

    // Clean up any existing test file
    std::remove(testFile.c_str());

    // Set some values
    config->setValue("persist.int", 123);
    config->setValue("persist.float", 3.14);
    config->setValue("persist.bool", false);
    config->setValue("persist.string", "persistent");

    // Save to file
    TestRunner::assert_true(config->saveToFile(testFile),
        "Should be able to save config to file");

    // Clear config
    config->clear();
    TestRunner::assert_true(!config->hasValue("persist.int"),
        "Config should be cleared");

    // Load from file
    TestRunner::assert_true(config->loadFromFile(testFile),
        "Should be able to load config from file");

    // Verify values
    TestRunner::assert_equals(123, config->getValue<int>("persist.int"),
        "Should load int value correctly");
    TestRunner::assert_true(std::abs(config->getValue<double>("persist.float") - 3.14) < 0.001,
        "Should load float value correctly");
    TestRunner::assert_true(!config->getValue<bool>("persist.bool"),
        "Should load bool value correctly");
    TestRunner::assert_equals(std::string("persistent"), config->getValue<std::string>("persist.string"),
        "Should load string value correctly");

    // Cleanup
    std::remove(testFile.c_str());
}

void test_change_listeners() {
    std::cout << "\nTesting Change Listeners..." << std::endl;

    auto config = ConfigManager::getInstance();

    int callbackCount = 0;
    std::string lastKey;

    // Add global listener
    config->addGlobalChangeListener([&](const std::string& key) {
        callbackCount++;
        lastKey = key;
    });

    // Add specific listener
    config->addChangeListener("specific.key", [&](const std::string& key) {
        callbackCount++;
    });

    // Change values
    config->setValue("test.key", 1);
    TestRunner::assert_equals(1, callbackCount,
        "Global listener should be called once");

    config->setValue("specific.key", 2);
    TestRunner::assert_equals(3, callbackCount,
        "Both listeners should be called for specific key");

    TestRunner::assert_equals(std::string("specific.key"), lastKey,
        "Last key should be specific.key");
}

void test_config_property_listeners() {
    std::cout << "\nTesting ConfigProperty Listeners..." << std::endl;

    ConfigProperty<int> testProp("listener.test", 0);

    int notifiedValue = -1;
    testProp.addChangeListener([&](int value) {
        notifiedValue = value;
    });

    // Change the property
    testProp = 42;
    TestRunner::assert_equals(42, notifiedValue,
        "Listener should be notified of property change");

    // Reset and test notification
    testProp.reset();
    notifiedValue = -1;

    // Change again
    testProp = 99;
    TestRunner::assert_equals(99, notifiedValue,
        "Listener should be notified after reset");
}

void test_config_json_format() {
    std::cout << "\nTesting JSON Format..." << std::endl;

    auto config = ConfigManager::getInstance();

    // Set various types
    config->setValue("json.int", 42);
    config->setValue("json.float", 3.14159);
    config->setValue("json.bool_true", true);
    config->setValue("json.bool_false", false);
    config->setValue("json.string", "test string");

    // Get JSON string
    std::string jsonStr = config->saveToJson();

    // Basic validation
    TestRunner::assert_true(!jsonStr.empty(),
        "JSON string should not be empty");
    TestRunner::assert_true(jsonStr.find("\"json.int\": 42") != std::string::npos,
        "JSON should contain int value");
    TestRunner::assert_true(jsonStr.find("\"json.bool_true\": true") != std::string::npos,
        "JSON should contain true boolean");
    TestRunner::assert_true(jsonStr.find("\"json.bool_false\": false") != std::string::npos,
        "JSON should contain false boolean");

    // Test parsing back
    config->clear();
    TestRunner::assert_true(config->loadFromJson(jsonStr),
        "Should be able to parse generated JSON");

    TestRunner::assert_equals(42, config->getValue<int>("json.int"),
        "Should restore int value from JSON");
    TestRunner::assert_true(config->getValue<bool>("json.bool_true"),
        "Should restore true boolean from JSON");
}

void run_config_tests() {
    std::cout << "\n=== Config Tests ===" << std::endl;

    test_basic_config_operations();
    test_config_property();
    test_json_persistence();
    test_change_listeners();
    test_config_property_listeners();
    test_config_json_format();

    std::cout << "Config tests completed." << std::endl;
}