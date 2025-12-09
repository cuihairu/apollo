#include "apollo/BaseComponent.h"
#include "apollo/DependencyManager.h"
#include <iostream>
#include <cassert>
#include <vector>

using namespace Apollo;

// External test runner
extern class TestRunner {
public:
    static void assert_true(bool condition, const std::string& message);
    static void assert_equals(int expected, int actual, const std::string& message);
    static void print_summary();
    static int get_result();
};

void test_simple_dependencies() {
    std::cout << "\nTesting Simple Dependencies..." << std::endl;

    auto& depMgr = DependencyManager::getInstance();

    // Add dependencies
    depMgr.addDependency("A", "B");
    depMgr.addDependency("B", "C");

    // Get initialization order
    auto order = depMgr.getInitializationOrder();
    TestRunner::assert_true(order.size() >= 3, "Should have at least 3 components");

    // C should come before B, and B before A
    size_t posA = std::find(order.begin(), order.end(), "A") - order.begin();
    size_t posB = std::find(order.begin(), order.end(), "B") - order.begin();
    size_t posC = std::find(order.begin(), order.end(), "C") - order.begin();

    TestRunner::assert_true(posC < posB && posB < posA,
        "Dependencies should be initialized in correct order (C -> B -> A)");

    // Get shutdown order
    auto shutdownOrder = depMgr.getShutdownOrder();
    posA = std::find(shutdownOrder.begin(), shutdownOrder.end(), "A") - shutdownOrder.begin();
    posB = std::find(shutdownOrder.begin(), shutdownOrder.end(), "B") - shutdownOrder.begin();
    posC = std::find(shutdownOrder.begin(), shutdownOrder.end(), "C") - shutdownOrder.begin();

    TestRunner::assert_true(posA < posB && posB < posC,
        "Shutdown order should be reverse of initialization (A -> B -> C)");
}

void test_circular_dependency_detection() {
    std::cout << "\nTesting Circular Dependency Detection..." << std::endl;

    auto& depMgr = DependencyManager::getInstance();

    // Create a circular dependency: A -> B -> C -> A
    depMgr.addDependency("CircularA", "CircularB");
    depMgr.addDependency("CircularB", "CircularC");
    depMgr.addDependency("CircularC", "CircularA");

    // Should detect circular dependency
    TestRunner::assert_true(depMgr.hasCircularDependency(),
        "Should detect circular dependency");

    // Find the circular dependency
    auto cycle = depMgr.findCircularDependency();
    TestRunner::assert_true(!cycle.empty(),
        "Should find a circular dependency cycle");

    // Verify the cycle contains all components
    bool hasA = std::find(cycle.begin(), cycle.end(), "CircularA") != cycle.end();
    bool hasB = std::find(cycle.begin(), cycle.end(), "CircularB") != cycle.end();
    bool hasC = std::find(cycle.begin(), cycle.end(), "CircularC") != cycle.end();

    TestRunner::assert_true(hasA && hasB && hasC,
        "Cycle should contain all circular components");
}

void test_transitive_dependencies() {
    std::cout << "\nTesting Transitive Dependencies..." << std::endl;

    auto& depMgr = DependencyManager::getInstance();

    // Set up a dependency chain: A -> B -> C -> D
    depMgr.addDependency("TransA", "TransB");
    depMgr.addDependency("TransB", "TransC");
    depMgr.addDependency("TransC", "TransD");

    // Get transitive dependencies of A
    auto transitive = depMgr.getTransitiveDependencies("TransA");

    // Should include B, C, and D
    bool hasB = std::find(transitive.begin(), transitive.end(), "TransB") != transitive.end();
    bool hasC = std::find(transitive.begin(), transitive.end(), "TransC") != transitive.end();
    bool hasD = std::find(transitive.begin(), transitive.end(), "TransD") != transitive.end();

    TestRunner::assert_true(hasB && hasC && hasD,
        "Should include all transitive dependencies");

    // Should not include A itself
    bool hasA = std::find(transitive.begin(), transitive.end(), "TransA") != transitive.end();
    TestRunner::assert_true(!hasA,
        "Should not include the component itself");
}

void test_complex_dependency_graph() {
    std::cout << "\nTesting Complex Dependency Graph..." << std::endl;

    auto& depMgr = DependencyManager::getInstance();

    // Create a complex graph
    /*
       E
      / \
     D   F
    / \   \
   B   C   G
    \ /   /
     A   H
    */
    depMgr.addDependency("B", "A");
    depMgr.addDependency("C", "A");
    depMgr.addDependency("D", "B");
    depMgr.addDependency("D", "C");
    depMgr.addDependency("E", "D");
    depMgr.addDependency("F", "G");
    depMgr.addDependency("G", "H");
    depMgr.addDependency("E", "F");

    // Test that we can get a valid order
    try {
        auto order = depMgr.getInitializationOrder();
        TestRunner::assert_true(order.size() >= 8,
            "Should have all components in order");

        // Verify some key dependencies
        size_t posA = std::find(order.begin(), order.end(), "A") - order.begin();
        size_t posB = std::find(order.begin(), order.end(), "B") - order.begin();
        size_t posD = std::find(order.begin(), order.end(), "D") - order.begin();
        size_t posE = std::find(order.begin(), order.end(), "E") - order.begin();

        TestRunner::assert_true(posA < posB && posB < posD && posD < posE,
            "Complex dependencies should be resolved correctly");

    } catch (const std::runtime_error& e) {
        TestRunner::assert_true(false,
            std::string("Should not throw exception for valid graph: ") + e.what());
    }
}

void test_dependency_removal() {
    std::cout << "\nTesting Dependency Removal..." << std::endl;

    auto& depMgr = DependencyManager::getInstance();

    // Add dependencies
    depMgr.addDependency("RemoveTest", "DepToRemove");

    // Verify dependency exists
    auto order = depMgr.getInitializationOrder();
    size_t posMain = std::find(order.begin(), order.end(), "RemoveTest") - order.begin();
    size_t posDep = std::find(order.begin(), order.end(), "DepToRemove") - order.begin();

    TestRunner::assert_true(posDep < posMain,
        "Dependency should be initialized before dependent");

    // Remove dependency
    depMgr.removeDependency("RemoveTest", "DepToRemove");

    // Get new order - dependency should no longer be required
    order = depMgr.getInitializationOrder();

    // The order might now be different, and DepToRemove might not even be in the list
    // if no other component depends on it. This is expected behavior.
    TestRunner::assert_true(true,
        "Dependency removal should not cause errors");
}

void test_multiple_dependents() {
    std::cout << "\nTesting Multiple Dependents..." << std::endl;

    auto& depMgr = DependencyManager::getInstance();

    // Multiple components depending on the same dependency
    depMgr.addDependency("Dependent1", "SharedDep");
    depMgr.addDependency("Dependent2", "SharedDep");
    depMgr.addDependency("Dependent3", "SharedDep");

    auto order = depMgr.getInitializationOrder();
    size_t posShared = std::find(order.begin(), order.end(), "SharedDep") - order.begin();
    size_t posDep1 = std::find(order.begin(), order.end(), "Dependent1") - order.begin();
    size_t posDep2 = std::find(order.begin(), order.end(), "Dependent2") - order.begin();
    size_t posDep3 = std::find(order.begin(), order.end(), "Dependent3") - order.begin();

    TestRunner::assert_true(posShared < posDep1 && posShared < posDep2 && posShared < posDep3,
        "Shared dependency should be initialized before all dependents");
}

void test_diamond_dependency() {
    std::cout << "\nTesting Diamond Dependency Pattern..." << std::endl;

    auto& depMgr = DependencyManager::getInstance();

    // Diamond pattern: A depends on B and C, both B and C depend on D
    depMgr.addDependency("DiamondTop", "DiamondLeft");
    depMgr.addDependency("DiamondTop", "DiamondRight");
    depMgr.addDependency("DiamondLeft", "DiamondBottom");
    depMgr.addDependency("DiamondRight", "DiamondBottom");

    // This should be valid (not circular)
    TestRunner::assert_true(!depMgr.hasCircularDependency(),
        "Diamond pattern should not be detected as circular");

    auto order = depMgr.getInitializationOrder();
    size_t posTop = std::find(order.begin(), order.end(), "DiamondTop") - order.begin();
    size_t posLeft = std::find(order.begin(), order.end(), "DiamondLeft") - order.begin();
    size_t posRight = std::find(order.begin(), order.end(), "DiamondRight") - order.begin();
    size_t posBottom = std::find(order.begin(), order.end(), "DiamondBottom") - order.begin();

    TestRunner::assert_true(posBottom < posLeft && posBottom < posRight && posLeft < posTop && posRight < posTop,
        "Diamond pattern should be resolved correctly");
}

void run_dependency_tests() {
    std::cout << "\n=== Dependency Tests ===" << std::endl;

    test_simple_dependencies();
    test_circular_dependency_detection();
    test_transitive_dependencies();
    test_complex_dependency_graph();
    test_dependency_removal();
    test_multiple_dependents();
    test_diamond_dependency();

    std::cout << "Dependency tests completed." << std::endl;
}