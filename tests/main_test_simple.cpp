#include <iostream>
#include <vector>

// Function declarations for tests
void run_component_tests();
void run_context_tests();
void run_config_tests();
void run_dependency_tests();

// Simple test framework implementation
class TestRunner {
public:
    static void assert_true(bool condition, const std::string& message) {
        tests_total++;
        if (condition) {
            tests_passed++;
        } else {
            std::cerr << "FAIL: " << message << std::endl;
            failed_tests.push_back(message);
        }
    }

    static void assert_equals(int expected, int actual, const std::string& message) {
        assert_true(expected == actual, message + " (expected: " + std::to_string(expected) + ", actual: " + std::to_string(actual) + ")");
    }

    static void print_summary() {
        std::cout << "\n=== Test Summary ===" << std::endl;
        std::cout << "Total: " << tests_total << std::endl;
        std::cout << "Passed: " << tests_passed << std::endl;
        std::cout << "Failed: " << (tests_total - tests_passed) << std::endl;

        if (!failed_tests.empty()) {
            std::cout << "\nFailed tests:" << std::endl;
            for (const auto& test : failed_tests) {
                std::cout << "  - " << test << std::endl;
            }
        }
    }

    static int get_result() {
        return (tests_total == tests_passed) ? 0 : 1;
    }

    static int tests_total;
    static int tests_passed;
    static std::vector<std::string> failed_tests;
};

int TestRunner::tests_total = 0;
int TestRunner::tests_passed = 0;
std::vector<std::string> TestRunner::failed_tests;

int main() {
    std::cout << "=== Apollo IoC Framework Test Suite ===" << std::endl;
    std::cout << "Running simple tests without GTest dependency...\n" << std::endl;

    // Run all test suites
    run_component_tests();
    run_context_tests();
    run_config_tests();
    run_dependency_tests();

    // Print summary
    TestRunner::print_summary();

    return TestRunner::get_result();
}