cmake_minimum_required(VERSION 3.20)

# 查找 GoogleTest
find_package(GTest REQUIRED)

# 测试可执行文件
add_executable(protocol_tests
    protocol_comprehensive_tests.cpp
)

target_link_libraries(protocol_tests
    PRIVATE
        apollo::protocol
        GTest::gtest
        GTest::gtest_main
)

# 发现测试
include(GoogleTest)
gtest_discover_tests(protocol_tests)
