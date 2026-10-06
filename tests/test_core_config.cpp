/**
 * @file test_core_config.cpp
 * @brief Core config module unit tests（P3-1 配置统一收口：legacy
 * ConfigManager/ConfigNode/ConfigValue 已删——audit A8 消账，活件唯
 * ConfigRegistry；本文件即其单测）
 */

#include "apollo/core/config/config_registry.hpp"
#include <iostream>
#include <string>

using namespace apollo::core::config;

namespace {

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

//==============================================================================
// ConfigRegistry Tests
//==============================================================================

bool test_config_registry_set_get_string() {
    std::cout << "Running: test_config_registry_set_get_string..." << std::endl;

    auto& registry = global_config();

    registry.set("test.key", "test_value");
    TEST_ASSERT(registry.has("test.key"), "Has test.key");

    std::string value = registry.get_string("test.key", "default");
    TEST_ASSERT(value == "test_value", "Value matches");

    // Default value
    std::string missing = registry.get_string("missing.key", "default");
    TEST_ASSERT(missing == "default", "Default value for missing key");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_config_registry_set_get_int64() {
    std::cout << "Running: test_config_registry_set_get_int64..." << std::endl;

    auto& registry = global_config();

    registry.set("test.port", static_cast<int64_t>(8080));
    int64_t port = registry.get_int64("test.port", 0);
    TEST_ASSERT(port == 8080, "Int64 value correct");

    registry.set("test.negative", static_cast<int64_t>(-42));
    int64_t negative = registry.get_int64("test.negative", 0);
    TEST_ASSERT(negative == -42, "Negative int64 correct");

    // Default value
    int64_t missing = registry.get_int64("missing", 999);
    TEST_ASSERT(missing == 999, "Default value for missing key");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_config_registry_set_get_bool() {
    std::cout << "Running: test_config_registry_set_get_bool..." << std::endl;

    auto& registry = global_config();

    registry.set("test.enabled", true);
    TEST_ASSERT(registry.get_bool("test.enabled", false) == true, "Bool true correct");

    registry.set("test.disabled", false);
    TEST_ASSERT(registry.get_bool("test.disabled", true) == false, "Bool false correct");

    // Default value
    TEST_ASSERT(registry.get_bool("nonexistent", true) == true, "Default value");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_config_registry_has() {
    std::cout << "Running: test_config_registry_has..." << std::endl;

    auto& registry = global_config();

    registry.set("existing.key", "value");

    TEST_ASSERT(registry.has("existing.key"), "Has existing key");
    TEST_ASSERT(!registry.has("nonexistent.key"), "Does not have nonexistent key");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_config_registry_set_overwrite() {
    std::cout << "Running: test_config_registry_set_overwrite..." << std::endl;

    auto& registry = global_config();

    registry.set("test.key", "first");
    TEST_ASSERT(registry.get_string("test.key", "") == "first", "First value");

    registry.set("test.key", "second");
    TEST_ASSERT(registry.get_string("test.key", "") == "second", "Overwritten value");

    std::cout << "  PASSED" << std::endl;
    return true;
}

} // namespace

//==============================================================================
// Main Test Runner
//==============================================================================

int main() {
    std::cout << "=== Apollo Core Config Module Test Suite ===" << std::endl;

    int total = 0;
    int passed = 0;

    auto run = [&](bool (*fn)()) {
        total++;
        if (fn()) {
            passed++;
        }
    };

    // ConfigRegistry tests
    run(test_config_registry_set_get_string);
    run(test_config_registry_set_get_int64);
    run(test_config_registry_set_get_bool);
    run(test_config_registry_has);
    run(test_config_registry_set_overwrite);

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total: " << total << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << (total - passed) << std::endl;

    return (total == passed) ? 0 : 1;
}
