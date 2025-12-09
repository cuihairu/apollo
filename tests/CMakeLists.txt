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

    # 为测试添加网络库链接
    if(WIN32)
        target_link_libraries(ioc_tests PRIVATE ws2_32)
    endif()

    add_test(NAME AllTests COMMAND ioc_tests)

    # Add individual tests for better reporting
    add_test(NAME ComponentTest COMMAND ioc_tests --gtest_filter=ComponentTest.*)
    add_test(NAME ContextTest COMMAND ioc_tests --gtest_filter=ContextTest.*)
    add_test(NAME ConfigTest COMMAND ioc_tests --gtest_filter=ConfigTest.*)
    add_test(NAME DependencyTest COMMAND ioc_tests --gtest_filter=DependencyTest.*)
    add_test(NAME FileWatcherTest COMMAND ioc_tests --gtest_filter=FileWatcherTest.*)

    # Redis tests
    add_executable(redis_tests
        test_redis.cpp
        test_redis_main.cpp
    )

    target_link_libraries(redis_tests
        PRIVATE
        common
        GTest::gtest
        GTest::gtest_main
    )

    # 为Redis测试添加网络库链接
    if(WIN32)
        target_link_libraries(redis_tests PRIVATE ws2_32)
    endif()

    add_test(NAME RedisTests COMMAND redis_tests)

    # Protobuf tests
    add_executable(protobuf_tests
        test_protobuf.cpp
        test_protobuf_main.cpp
    )

    target_link_libraries(protobuf_tests
        PRIVATE
        common
        GTest::gtest
        GTest::gtest_main
    )

    # 为Protobuf测试添加网络库链接
    if(WIN32)
        target_link_libraries(protobuf_tests PRIVATE ws2_32)
    endif()

    # 查找Protobuf库
    find_package(Protobuf QUIET)
    if(Protobuf_FOUND)
        target_link_libraries(protobuf_tests PRIVATE protobuf::libprotobuf)
    endif()

    add_test(NAME ProtobufTests COMMAND protobuf_tests)
else()
    message(STATUS "GTest not found, building simple test")

    # Simple test without GTest
    add_executable(ioc_tests_simple test_simple.cpp)
    add_executable(all_features_test test_all_features.cpp)

    target_link_libraries(ioc_tests_simple PRIVATE common)
    target_link_libraries(all_features_test PRIVATE common)

    # 为Windows添加网络库链接
    if(WIN32)
        target_link_libraries(ioc_tests_simple PRIVATE ws2_32)
        target_link_libraries(all_features_test PRIVATE ws2_32)
    endif()

    enable_testing()
    add_test(NAME SimpleTests COMMAND ioc_tests_simple)
endif()