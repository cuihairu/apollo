find_package(GTest QUIET)

if(GTest_FOUND)
    enable_testing()

    add_executable(ioc_tests
        component_test.cpp
        context_test.cpp
        config_test.cpp
        dependency_test.cpp
        filewatcher_test.cpp
        main_test.cpp
    )

    target_link_libraries(ioc_tests
        PRIVATE
        common
        GTest::gtest
        GTest::gtest_main
    )

    add_test(NAME AllTests COMMAND ioc_tests)

    # Add individual tests for better reporting
    add_test(NAME ComponentTest COMMAND ioc_tests --gtest_filter=ComponentTest.*)
    add_test(NAME ContextTest COMMAND ioc_tests --gtest_filter=ContextTest.*)
    add_test(NAME ConfigTest COMMAND ioc_tests --gtest_filter=ConfigTest.*)
    add_test(NAME DependencyTest COMMAND ioc_tests --gtest_filter=DependencyTest.*)
    add_test(NAME FileWatcherTest COMMAND ioc_tests --gtest_filter=FileWatcherTest.*)
else()
    message(STATUS "GTest not found, building simple test")

    # Simple test without GTest
    add_executable(ioc_tests_simple test_simple.cpp)

    target_link_libraries(ioc_tests_simple PRIVATE common)

    enable_testing()
    add_test(NAME SimpleTests COMMAND ioc_tests_simple)
endif()