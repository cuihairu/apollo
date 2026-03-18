find_package(GTest QUIET)

if(GTest_FOUND AND APOLLO_BUILD_GTESTS)
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

    if(APOLLO_ENABLE_IPC)
        # ========== Channel Tests (New) - Shared Memory + Backpressure ==========

        add_executable(channel_tests test_channel.cpp)

        target_link_libraries(channel_tests PRIVATE apollo)

        if(WIN32)
            target_link_libraries(channel_tests PRIVATE ws2_32)
        endif()

        add_test(NAME ChannelTests COMMAND channel_tests)
    endif()

    # ========== Actor Framework Tests ==========
    # REMOVED: Actor module removed in favor of CAF (C++ Actor Framework)
    # See: https://github.com/actor-framework/actor-framework

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

	    # ========== Base Module Tests (New) ==========

	    # Memory utilities tests
	    add_executable(memory_tests test_memory.cpp)

	    target_link_libraries(memory_tests PRIVATE apollo apollo::base)

	    if(WIN32)
	        target_link_libraries(memory_tests PRIVATE ws2_32)
	    endif()

	    add_test(NAME MemoryTests COMMAND memory_tests)

	    # String utilities tests
	    add_executable(string_tests test_string.cpp)

	    target_link_libraries(string_tests PRIVATE apollo apollo::base)

	    if(WIN32)
	        target_link_libraries(string_tests PRIVATE ws2_32)
	    endif()

	    add_test(NAME StringTests COMMAND string_tests)

	    # Terminal utilities tests
	    add_executable(terminal_tests test_terminal.cpp)

	    target_link_libraries(terminal_tests PRIVATE apollo apollo::base)

	    if(WIN32)
	        target_link_libraries(terminal_tests PRIVATE ws2_32)
	    endif()

	    add_test(NAME TerminalTests COMMAND terminal_tests)

	    # ThreadPool tests
	    add_executable(thread_pool_tests test_thread_pool.cpp)

	    target_link_libraries(thread_pool_tests PRIVATE apollo apollo::base)

	    if(WIN32)
	        target_link_libraries(thread_pool_tests PRIVATE ws2_32)
	    endif()

	    add_test(NAME ThreadPoolTests COMMAND thread_pool_tests)

	    # Time utilities tests
	    add_executable(time_tests test_time.cpp)

	    target_link_libraries(time_tests PRIVATE apollo apollo::base)

	    if(WIN32)
	        target_link_libraries(time_tests PRIVATE ws2_32)
	    endif()

	    add_test(NAME TimeTests COMMAND time_tests)

	    # Core lifecycle tests
	    add_executable(core_lifecycle_tests test_core_lifecycle.cpp)

	    target_link_libraries(core_lifecycle_tests PRIVATE apollo apollo::core)

	    if(WIN32)
	        target_link_libraries(core_lifecycle_tests PRIVATE ws2_32)
	    endif()

	    add_test(NAME CoreLifecycleTests COMMAND core_lifecycle_tests)

	    # Core log tests
	    add_executable(core_log_tests test_core_log.cpp)

	    target_link_libraries(core_log_tests PRIVATE apollo apollo::base apollo::core)

	    if(WIN32)
	        target_link_libraries(core_log_tests PRIVATE ws2_32)
	    endif()

	    add_test(NAME CoreLogTests COMMAND core_log_tests)

	    # Core config tests
	    add_executable(core_config_tests test_core_config.cpp)

	    target_link_libraries(core_config_tests PRIVATE apollo apollo::base apollo::core)

	    if(WIN32)
	        target_link_libraries(core_config_tests PRIVATE ws2_32)
	    endif()

	    add_test(NAME CoreConfigTests COMMAND core_config_tests)

	    # Runtime tests
	    add_executable(runtime_tests test_runtime.cpp)

	    target_link_libraries(runtime_tests PRIVATE apollo apollo::base apollo::core apollo::runtime)

	    if(WIN32)
	        target_link_libraries(runtime_tests PRIVATE ws2_32)
	    endif()

	    add_test(NAME RuntimeTests COMMAND runtime_tests)

	    # Data module tests
	    add_executable(data_tests test_data.cpp)
	    target_link_libraries(data_tests PRIVATE apollo apollo::base apollo::core apollo::runtime apollo::data_cache apollo::data_orm)
	    if(WIN32)
	        target_link_libraries(data_tests PRIVATE ws2_32)
	    endif()
	    add_test(NAME DataTests COMMAND data_tests)

	    # ========== Net Module Tests (New) ==========
	    add_executable(net_tests test_net.cpp)
	    target_link_libraries(net_tests PRIVATE apollo apollo::base)
	    if(WIN32)
	        target_link_libraries(net_tests PRIVATE ws2_32)
	        target_compile_options(net_tests PRIVATE /utf-8)
	        target_compile_definitions(net_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
	    endif()
	    add_test(NAME NetTests COMMAND net_tests)

	    # ========== Crypto Tests ==========
	    # Note: OpenSSL header conflicts on Windows, disabled pending fix
	    # # Note: OpenSSL header conflicts - disabled
    # add_executable(crypto_tests test_crypto.cpp)
	    # # target_link_libraries(crypto_tests PRIVATE apollo)
	    # if(WIN32)
	    #     # target_link_libraries(crypto_tests PRIVATE ws2_32)
	    # endif()
	    # # add_test(NAME CryptoTests COMMAND crypto_tests)

	else()
	    message(STATUS "GTest not found or APOLLO_BUILD_GTESTS=OFF, building simple tests")
	
	    # Simple test without GTest
	    add_executable(ioc_tests_simple test_simple.cpp)
	    # Note:暂时禁用
    # add_executable(config_tests_simple test_config.cpp)
	    # Note: OpenSSL header conflicts - disabled
    # add_executable(crypto_tests test_crypto.cpp)
	    add_executable(bigworld_api_tests test_bigworld_api.cpp)
	
	    target_link_libraries(ioc_tests_simple PRIVATE apollo)
	    # target_link_libraries(config_tests_simple PRIVATE apollo)
	    # target_link_libraries(crypto_tests PRIVATE apollo)
	    target_link_libraries(bigworld_api_tests PRIVATE apollo)
	
	    # 为Windows添加网络库链接
	    if(WIN32)
	        target_link_libraries(ioc_tests_simple PRIVATE ws2_32)
	        # target_link_libraries(config_tests_simple PRIVATE ws2_32)
	        # target_link_libraries(crypto_tests PRIVATE ws2_32)
	        target_link_libraries(bigworld_api_tests PRIVATE ws2_32)
	    endif()

	    enable_testing()
	    add_test(NAME SimpleTests COMMAND ioc_tests_simple)
	    # add_test(NAME ConfigTestsSimple COMMAND config_tests_simple)
	    add_test(NAME BigWorldApiTests COMMAND bigworld_api_tests)

	    # ========== Base Module Tests (New) ==========
	    add_executable(memory_tests test_memory.cpp)
	    target_link_libraries(memory_tests PRIVATE apollo apollo::base)
	    if(WIN32)
	        target_link_libraries(memory_tests PRIVATE ws2_32)
	    endif()
	    add_test(NAME MemoryTests COMMAND memory_tests)

	    add_executable(string_tests test_string.cpp)
	    target_link_libraries(string_tests PRIVATE apollo apollo::base)
	    if(WIN32)
	        target_link_libraries(string_tests PRIVATE ws2_32)
	    endif()
	    add_test(NAME StringTests COMMAND string_tests)

	    add_executable(terminal_tests test_terminal.cpp)
	    target_link_libraries(terminal_tests PRIVATE apollo apollo::base)
	    if(WIN32)
	        target_link_libraries(terminal_tests PRIVATE ws2_32)
	    endif()
	    add_test(NAME TerminalTests COMMAND terminal_tests)

	    add_executable(thread_pool_tests test_thread_pool.cpp)
	    target_link_libraries(thread_pool_tests PRIVATE apollo apollo::base)
	    if(WIN32)
	        target_link_libraries(thread_pool_tests PRIVATE ws2_32)
	    endif()
	    add_test(NAME ThreadPoolTests COMMAND thread_pool_tests)

	    add_executable(time_tests test_time.cpp)
	    target_link_libraries(time_tests PRIVATE apollo apollo::base)
	    if(WIN32)
	        target_link_libraries(time_tests PRIVATE ws2_32)
	    endif()
	    add_test(NAME TimeTests COMMAND time_tests)

	    add_executable(core_lifecycle_tests test_core_lifecycle.cpp)
	    target_link_libraries(core_lifecycle_tests PRIVATE apollo apollo::base apollo::core)
	    if(WIN32)
	        target_link_libraries(core_lifecycle_tests PRIVATE ws2_32)
	    endif()
	    add_test(NAME CoreLifecycleTests COMMAND core_lifecycle_tests)

	    # Core log tests
	    add_executable(core_log_tests test_core_log.cpp)
	    target_link_libraries(core_log_tests PRIVATE apollo apollo::base apollo::core)
	    if(WIN32)
	        target_link_libraries(core_log_tests PRIVATE ws2_32)
	    endif()
	    add_test(NAME CoreLogTests COMMAND core_log_tests)

	    # Core config tests
	    add_executable(core_config_tests test_core_config.cpp)
	    target_link_libraries(core_config_tests PRIVATE apollo apollo::base apollo::core)
	    if(WIN32)
	        target_link_libraries(core_config_tests PRIVATE ws2_32)
	    endif()
	    add_test(NAME CoreConfigTests COMMAND core_config_tests)

	    # Runtime tests
	    add_executable(runtime_tests test_runtime.cpp)
	    target_link_libraries(runtime_tests PRIVATE apollo apollo::base apollo::core apollo::runtime)
	    if(WIN32)
	        target_link_libraries(runtime_tests PRIVATE ws2_32)
	    endif()
	    add_test(NAME RuntimeTests COMMAND runtime_tests)

	    # Data module tests
	    add_executable(data_tests test_data.cpp)
	    target_link_libraries(data_tests PRIVATE apollo apollo::base apollo::core apollo::runtime apollo::data_cache apollo::data_orm)
	    if(WIN32)
	        target_link_libraries(data_tests PRIVATE ws2_32)
	    endif()
	    add_test(NAME DataTests COMMAND data_tests)

	    # ========== Net Module Tests (New) ==========
	    add_executable(net_tests test_net.cpp)
	    target_link_libraries(net_tests PRIVATE apollo apollo::base)
	    if(WIN32)
	        target_link_libraries(net_tests PRIVATE ws2_32)
	        target_compile_options(net_tests PRIVATE /utf-8)
	        target_compile_definitions(net_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
	    endif()
	    add_test(NAME NetTests COMMAND net_tests)

	    # add_test(NAME CryptoTests COMMAND crypto_tests)

	    # ========== Game Module Tests (New) ==========
	    # Game module tests include source files directly
	    add_executable(game_tests
	        test_game.cpp
	        # Game Core
	        ../modules/game/core/src/entity.cpp
	        # Game World
	        ../modules/game/world/src/aoi.cpp
	        ../modules/game/world/src/scene.cpp
	        # Game Attributes (using attribute.hpp, not attribute_value.h)
	        ../modules/game/attributes/src/attribute.cpp
	        # Game Battle ECS
	        ../modules/game/battle/src/ecs.cpp
	    )
	    target_include_directories(game_tests PRIVATE
	        ${CMAKE_SOURCE_DIR}/include
	        ${CMAKE_SOURCE_DIR}/modules/game/core/include
	        ${CMAKE_SOURCE_DIR}/modules/game/world/include
	    )
	    target_link_libraries(game_tests PRIVATE apollo apollo::base apollo::core)
	    if(WIN32)
	        target_link_libraries(game_tests PRIVATE ws2_32)
	        target_compile_options(game_tests PRIVATE /utf-8)
	        target_compile_definitions(game_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
	    endif()
	    add_test(NAME GameTests COMMAND game_tests)
	endif()
