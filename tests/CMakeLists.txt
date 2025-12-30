find_package(GTest QUIET)

if(GTest_FOUND)
    # vcpkg often exposes headers as SYSTEM (-isystem). On macOS, /usr/local/include
    # is searched before system include dirs, which can accidentally pick up a
    # different protobuf installation and cause ABI mismatches. Prefer normal -I.
    foreach(_gt_target IN ITEMS GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)
        if(TARGET ${_gt_target})
            set_property(TARGET ${_gt_target} PROPERTY INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "")
        endif()
    endforeach()

    enable_testing()

    add_executable(ioc_tests
        main_test.cpp
    )

    target_link_libraries(ioc_tests
        PRIVATE
        apollo
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

    # Redis tests
    add_executable(redis_tests
        test_redis.cpp
    )

    target_link_libraries(redis_tests
        PRIVATE
        apollo
        GTest::gtest
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
        apollo
        GTest::gtest
    )

    # 为Protobuf测试添加网络库链接
    if(WIN32)
        target_link_libraries(protobuf_tests PRIVATE ws2_32)
    endif()

    add_test(NAME ProtobufTests COMMAND protobuf_tests)

    # ========== Network Tests ==========

    # Buffer tests
    add_executable(buffer_tests test_buffer.cpp)

    target_link_libraries(buffer_tests PRIVATE apollo)

    if(WIN32)
        target_link_libraries(buffer_tests PRIVATE ws2_32)
    endif()

    add_test(NAME BufferTests COMMAND buffer_tests)

    # Network integration tests
    add_executable(network_tests
        test_network.cpp
        test_network_main.cpp
    )

    target_link_libraries(network_tests PRIVATE apollo)

    if(WIN32)
        target_link_libraries(network_tests PRIVATE ws2_32)
    endif()

    add_test(NAME NetworkTests COMMAND network_tests)

    # ========== Log Tests ==========

    add_executable(log_tests test_log.cpp)

    target_link_libraries(log_tests PRIVATE apollo)

    if(WIN32)
        target_link_libraries(log_tests PRIVATE ws2_32)
    endif()

    add_test(NAME LogTests COMMAND log_tests)

    # ========== Timer Tests ==========

    add_executable(timer_tests test_timer.cpp)

    target_link_libraries(timer_tests PRIVATE apollo)

    if(WIN32)
        target_link_libraries(timer_tests PRIVATE ws2_32)
    endif()

    add_test(NAME TimerTests COMMAND timer_tests)

    # ========== Config Tests ==========

    add_executable(config_tests test_config.cpp)

    target_link_libraries(config_tests PRIVATE apollo)

    if(WIN32)
        target_link_libraries(config_tests PRIVATE ws2_32)
    endif()

    add_test(NAME ConfigTests COMMAND config_tests)

    # ========== Redis Tests ==========

    add_executable(redis_tests test_redis_new.cpp)

    target_link_libraries(redis_tests PRIVATE apollo)

    if(WIN32)
        target_link_libraries(redis_tests PRIVATE ws2_32)
    endif()

    add_test(NAME RedisTests COMMAND redis_tests)

    # ========== Database Tests ==========

    add_executable(database_tests test_database.cpp)

    target_link_libraries(database_tests PRIVATE apollo)

    if(WIN32)
        target_link_libraries(database_tests PRIVATE ws2_32)
    endif()

    add_test(NAME DatabaseTests COMMAND database_tests)

    # ========== IdPool Tests ==========

    add_executable(id_pool_tests test_id_pool.cpp)

    target_link_libraries(id_pool_tests PRIVATE apollo)

    if(WIN32)
        target_link_libraries(id_pool_tests PRIVATE ws2_32)
    endif()

    add_test(NAME IdPoolTests COMMAND id_pool_tests)

    # ========== Utils Tests ==========

    add_executable(utils_tests test_utils.cpp)

    target_link_libraries(utils_tests PRIVATE apollo)

    if(WIN32)
        target_link_libraries(utils_tests PRIVATE ws2_32)
    endif()

    add_test(NAME UtilsTests COMMAND utils_tests)

    # ========== SqlTemplate Tests (New) ==========

    add_executable(sql_template_tests test_sql_template.cpp)

    target_link_libraries(sql_template_tests PRIVATE apollo)

    if(WIN32)
        target_link_libraries(sql_template_tests PRIVATE ws2_32)
    endif()

    add_test(NAME SqlTemplateTests COMMAND sql_template_tests)

    # ========== RestTemplate Tests (New) ==========

    add_executable(rest_template_tests test_rest_template.cpp)

    target_link_libraries(rest_template_tests PRIVATE apollo)

    if(WIN32)
        target_link_libraries(rest_template_tests PRIVATE ws2_32)
    endif()

    add_test(NAME RestTemplateTests COMMAND rest_template_tests)

    # ========== Integration Tests (New) ==========

    add_executable(integration_tests test_integration.cpp)

    target_link_libraries(integration_tests PRIVATE apollo)

    if(WIN32)
        target_link_libraries(integration_tests PRIVATE ws2_32)
    endif()

    add_test(NAME IntegrationTests COMMAND integration_tests)

    # ========== Channel Tests (New) - Shared Memory + Backpressure ==========

    add_executable(channel_tests test_channel.cpp)

    target_link_libraries(channel_tests PRIVATE apollo)

    if(WIN32)
        target_link_libraries(channel_tests PRIVATE ws2_32)
    endif()

    add_test(NAME ChannelTests COMMAND channel_tests)

    # ========== Actor Framework Tests (New) ==========

    # Basic Actor tests
    add_executable(actor_framework_tests test_actor_framework.cpp)

    target_link_libraries(actor_framework_tests PRIVATE apollo)

    if(WIN32)
        target_link_libraries(actor_framework_tests PRIVATE ws2_32)
    endif()

    add_test(NAME ActorFrameworkTests COMMAND actor_framework_tests)

    # Concurrency and stress tests
    add_executable(actor_concurrency_tests test_actor_concurrency.cpp)

    target_link_libraries(actor_concurrency_tests PRIVATE apollo)

    if(WIN32)
        target_link_libraries(actor_concurrency_tests PRIVATE ws2_32)
    endif()

    add_test(NAME ActorConcurrencyTests COMMAND actor_concurrency_tests)

    # ========== Data Structures Tests (New) ==========

    add_executable(data_structures_tests test_data_structures.cpp)

    target_link_libraries(data_structures_tests PRIVATE apollo)

    if(WIN32)
        target_link_libraries(data_structures_tests PRIVATE ws2_32)
    endif()

    add_test(NAME DataStructuresTests COMMAND data_structures_tests)

    # ========== Serialization Tests (New) ==========

    add_executable(serialization_tests test_serialization.cpp)

    target_link_libraries(serialization_tests PRIVATE apollo)

    if(WIN32)
        target_link_libraries(serialization_tests PRIVATE ws2_32)
    endif()

    add_test(NAME SerializationTests COMMAND serialization_tests)

    # ========== BigWorld Compatibility Tests (New) ==========

    add_executable(bigworld_api_tests test_bigworld_api.cpp)

    target_link_libraries(bigworld_api_tests PRIVATE apollo)

    if(WIN32)
        target_link_libraries(bigworld_api_tests PRIVATE ws2_32)
    endif()

    add_test(NAME BigWorldApiTests COMMAND bigworld_api_tests)

else()
    message(STATUS "GTest not found, building simple test")

    # Simple test without GTest
    add_executable(ioc_tests_simple test_simple.cpp)
    add_executable(all_features_test test_all_features.cpp)
    add_executable(config_tests_simple test_config.cpp)
    add_executable(bigworld_api_tests test_bigworld_api.cpp)

    target_link_libraries(ioc_tests_simple PRIVATE apollo)
    target_link_libraries(all_features_test PRIVATE apollo)
    target_link_libraries(config_tests_simple PRIVATE apollo)
    target_link_libraries(bigworld_api_tests PRIVATE apollo)

    # 为Windows添加网络库链接
    if(WIN32)
        target_link_libraries(ioc_tests_simple PRIVATE ws2_32)
        target_link_libraries(all_features_test PRIVATE ws2_32)
        target_link_libraries(config_tests_simple PRIVATE ws2_32)
        target_link_libraries(bigworld_api_tests PRIVATE ws2_32)
    endif()

    enable_testing()
    add_test(NAME SimpleTests COMMAND ioc_tests_simple)
    add_test(NAME ConfigTestsSimple COMMAND config_tests_simple)
    add_test(NAME BigWorldApiTests COMMAND bigworld_api_tests)
endif()
