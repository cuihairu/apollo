find_package(GTest QUIET)

set(APOLLO_BIGWORLD_TEST_SOURCES
    ../modules/bigworld/src/runtime.cpp
)

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
	
	    add_executable(bigworld_api_tests
            test_bigworld_api.cpp
            ${APOLLO_BIGWORLD_TEST_SOURCES}
        )
	
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

	    if(TARGET apollo::game_session AND TARGET apollo_protocol)
	    add_executable(base_anchor_tests
	        test_base_anchor.cpp
	        ../apps/base-app/src/base_server.cpp
	        ../apps/base-app/src/database_service.cpp
	    )
	    target_include_directories(base_anchor_tests PRIVATE
	        ${CMAKE_SOURCE_DIR}/apps/base-app/include
	    )
	    target_link_libraries(base_anchor_tests PRIVATE
	        apollo
	        apollo::base
	        apollo::core
	        apollo::game_session
	        apollo_protocol
	    )
	    if(WIN32)
	        target_link_libraries(base_anchor_tests PRIVATE ws2_32)
	    endif()
	    add_test(NAME BaseAnchorTests COMMAND base_anchor_tests)

	    # baseappmgr（目录 + 落点裁决）——与 base-app 同档（legacy protocol + game_session）
	    add_executable(baseappmgr_tests
	        test_baseappmgr.cpp
	        ../apps/baseappmgr/src/baseappmgr.cpp
	    )
	    target_include_directories(baseappmgr_tests PRIVATE
	        ${CMAKE_SOURCE_DIR}/apps/baseappmgr/include
	    )
	    target_link_libraries(baseappmgr_tests PRIVATE
	        apollo
	        apollo::base
	        apollo::core
	        apollo::game_session
	        apollo_protocol
	    )
	    if(WIN32)
	        target_link_libraries(baseappmgr_tests PRIVATE ws2_32)
	    endif()
	    add_test(NAME BaseAppMgrTests COMMAND baseappmgr_tests)
	    endif()

	    # 以下两个目标依赖旧 modules/protocol（默认禁用）——按目标存在性守卫
	    # （apps/ 同惯例），模块关闭时跳过而非 configure 即红。
	    if(TARGET apollo_protocol)
	    add_executable(gateway_route_tests
	        test_gateway_route.cpp
	        ../apps/gateway-app/src/gateway_server.cpp
	        ../apps/gateway-app/include/gateway/session_manager.cpp
            ../apps/gateway-app/src/ingress/client_ingress_server.cpp
            ../apps/gateway-app/src/ingress/client_packet_dispatcher.cpp
            ../apps/gateway-app/src/ingress/gateway_connection_registry.cpp
            ../apps/gateway-app/src/ingress/session_admission_service.cpp
	    )
	    target_include_directories(gateway_route_tests PRIVATE
	        ${CMAKE_SOURCE_DIR}/apps/gateway-app/include
	    )
	    target_link_libraries(gateway_route_tests PRIVATE
	        apollo
	        apollo::base
	        apollo::core
	        apollo::runtime
	        apollo::net_tcp
	        apollo::net_protocol
	        apollo_protocol
	    )
	    if(WIN32)
	        target_link_libraries(gateway_route_tests PRIVATE ws2_32)
	    endif()
	    add_test(NAME GatewayRouteTests COMMAND gateway_route_tests)

	    add_executable(protocol_bootstrap_tests test_protocol_bootstrap.cpp)
	    target_link_libraries(protocol_bootstrap_tests PRIVATE
	        apollo
	        apollo_protocol
	    )
	    if(WIN32)
	        target_link_libraries(protocol_bootstrap_tests PRIVATE ws2_32)
	    endif()
	    add_test(NAME ProtocolBootstrapTests COMMAND protocol_bootstrap_tests)
	    endif()

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
	        target_compile_definitions(net_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
	    endif()
	    # /utf-8 是 MSVC 专有旗标——MinGW gcc 当链接输入直接报错（PORT-1）
	    if(MSVC)
	        target_compile_options(net_tests PRIVATE /utf-8)
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
	
	    # Note:暂时禁用
    # add_executable(config_tests_simple test_config.cpp)
	    # Note: OpenSSL header conflicts - disabled
    # add_executable(crypto_tests test_crypto.cpp)
	    add_executable(bigworld_api_tests
            test_bigworld_api.cpp
            ${APOLLO_BIGWORLD_TEST_SOURCES}
        )
	
	    # target_link_libraries(config_tests_simple PRIVATE apollo)
	    # target_link_libraries(crypto_tests PRIVATE apollo)
	    target_link_libraries(bigworld_api_tests PRIVATE apollo)
	
	    # 为Windows添加网络库链接
	    if(WIN32)
	        # target_link_libraries(config_tests_simple PRIVATE ws2_32)
	        # target_link_libraries(crypto_tests PRIVATE ws2_32)
	        target_link_libraries(bigworld_api_tests PRIVATE ws2_32)
	    endif()

	    enable_testing()
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

	    # Session/World skeleton tests（依赖 game 模块目标——模块关闭时跳过，
	    # 与 apps/ 的 if(TARGET …) 守卫同惯例；修复 main 上 CI configure 必红的问题）
	    if(TARGET apollo::game_core)
	    add_executable(session_world_tests test_session_world.cpp)
	    target_link_libraries(session_world_tests PRIVATE
	        apollo
	        apollo::base
	        apollo::core
	        apollo::runtime
	        apollo::game_core
	        apollo::game_session
	        apollo::game_world
	    )
	    if(WIN32)
	        target_link_libraries(session_world_tests PRIVATE ws2_32)
	    endif()
	    add_test(NAME SessionWorldTests COMMAND session_world_tests)
	    endif()

	    if(TARGET apollo::game_session AND TARGET apollo_protocol)
	    add_executable(base_anchor_tests
	        test_base_anchor.cpp
	        ../apps/base-app/src/base_server.cpp
	        ../apps/base-app/src/database_service.cpp
	    )
	    target_include_directories(base_anchor_tests PRIVATE
	        ${CMAKE_SOURCE_DIR}/apps/base-app/include
	    )
	    target_link_libraries(base_anchor_tests PRIVATE
	        apollo
	        apollo::base
	        apollo::core
	        apollo::game_session
	        apollo_protocol
	    )
	    if(WIN32)
	        target_link_libraries(base_anchor_tests PRIVATE ws2_32)
	    endif()
	    add_test(NAME BaseAnchorTests COMMAND base_anchor_tests)

	    # baseappmgr（目录 + 落点裁决）——与 base-app 同档（legacy protocol + game_session）
	    add_executable(baseappmgr_tests
	        test_baseappmgr.cpp
	        ../apps/baseappmgr/src/baseappmgr.cpp
	    )
	    target_include_directories(baseappmgr_tests PRIVATE
	        ${CMAKE_SOURCE_DIR}/apps/baseappmgr/include
	    )
	    target_link_libraries(baseappmgr_tests PRIVATE
	        apollo
	        apollo::base
	        apollo::core
	        apollo::game_session
	        apollo_protocol
	    )
	    if(WIN32)
	        target_link_libraries(baseappmgr_tests PRIVATE ws2_32)
	    endif()
	    add_test(NAME BaseAppMgrTests COMMAND baseappmgr_tests)
	    endif()

	    # 以下两个目标依赖旧 modules/protocol（默认禁用）——按目标存在性守卫
	    # （apps/ 同惯例），模块关闭时跳过而非 configure 即红。
	    if(TARGET apollo_protocol)
	    add_executable(gateway_route_tests
	        test_gateway_route.cpp
	        ../apps/gateway-app/src/gateway_server.cpp
	        ../apps/gateway-app/include/gateway/session_manager.cpp
            ../apps/gateway-app/src/ingress/client_ingress_server.cpp
            ../apps/gateway-app/src/ingress/client_packet_dispatcher.cpp
            ../apps/gateway-app/src/ingress/gateway_connection_registry.cpp
            ../apps/gateway-app/src/ingress/session_admission_service.cpp
	    )
	    target_include_directories(gateway_route_tests PRIVATE
	        ${CMAKE_SOURCE_DIR}/apps/gateway-app/include
	    )
	    target_link_libraries(gateway_route_tests PRIVATE
	        apollo
	        apollo::base
	        apollo::core
	        apollo::runtime
	        apollo::net_tcp
	        apollo::net_protocol
	        apollo_protocol
	    )
	    if(WIN32)
	        target_link_libraries(gateway_route_tests PRIVATE ws2_32)
	    endif()
	    add_test(NAME GatewayRouteTests COMMAND gateway_route_tests)

	    add_executable(protocol_bootstrap_tests test_protocol_bootstrap.cpp)
	    target_link_libraries(protocol_bootstrap_tests PRIVATE
	        apollo
	        apollo_protocol
	    )
	    if(WIN32)
	        target_link_libraries(protocol_bootstrap_tests PRIVATE ws2_32)
	    endif()
	    add_test(NAME ProtocolBootstrapTests COMMAND protocol_bootstrap_tests)
	    endif()

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
	        target_compile_definitions(net_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
	    endif()
	    # /utf-8 是 MSVC 专有旗标——MinGW gcc 当链接输入直接报错（PORT-1）
	    if(MSVC)
	        target_compile_options(net_tests PRIVATE /utf-8)
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
	        ../modules/game/world/src/scene_aoi.cpp
	        ../modules/game/world/src/avatar.cpp
	        ../modules/game/session/src/player_anchor.cpp
	        # Game Attributes (using attribute.hpp, not attribute_value.h)
	        ../modules/game/attributes/src/attribute.cpp
	        # Game Battle ECS
	        ../modules/game/battle/src/ecs.cpp
	    )
	    target_include_directories(game_tests PRIVATE
	        ${CMAKE_SOURCE_DIR}/include
	        ${CMAKE_SOURCE_DIR}/modules/game/core/include
	        ${CMAKE_SOURCE_DIR}/modules/game/world/include
	        ${CMAKE_SOURCE_DIR}/modules/game/session/include
	    )
	    target_link_libraries(game_tests PRIVATE apollo apollo::base apollo::core)
	    if(WIN32)
	        target_link_libraries(game_tests PRIVATE ws2_32)
	        target_compile_definitions(game_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
	    endif()
	    if(MSVC)
	        target_compile_options(game_tests PRIVATE /utf-8)
	    endif()
	    add_test(NAME GameTests COMMAND game_tests)
	# ========== World Host Tests (P0-1) ==========
	# 定帧/追赶/跳拍语义（clock-and-time §3 口径）+ ThreadAffinity 执行上下文标注
	add_executable(world_host_tests
	    test_world_host.cpp
	)
	target_include_directories(world_host_tests PRIVATE
	    ${CMAKE_SOURCE_DIR}/modules/core/include
	    ${CMAKE_SOURCE_DIR}/modules/game/core/include
	)
	target_link_libraries(world_host_tests PRIVATE apollo::runtime)
	if(WIN32)
	    target_link_libraries(world_host_tests PRIVATE ws2_32)
	    target_compile_definitions(world_host_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
	endif()
	if(MSVC)
	    target_compile_options(world_host_tests PRIVATE /utf-8)
	endif()
	add_test(NAME WorldHostTests COMMAND world_host_tests)

# ========== Avatar / Player Object Model Tests (P0-2) ==========
# Avatar 三职责（移动/战斗/AOI 广播）+ Anchor 投影 + PlayerId 强分型
# （直编源惯例同 game_tests；Avatar 依赖 PlayerAnchor 实现，故一并编入）
add_executable(avatar_tests
    test_avatar.cpp
    ../modules/game/world/src/avatar.cpp
    ../modules/game/session/src/player_anchor.cpp
)
target_include_directories(avatar_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/modules/core/include
    ${CMAKE_SOURCE_DIR}/modules/game/core/include
    ${CMAKE_SOURCE_DIR}/modules/game/world/include
    ${CMAKE_SOURCE_DIR}/modules/game/session/include
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(avatar_tests PRIVATE apollo apollo::base apollo::core)
if(WIN32)
    target_link_libraries(avatar_tests PRIVATE ws2_32)
    target_compile_definitions(avatar_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(avatar_tests PRIVATE /utf-8)
endif()
add_test(NAME AvatarTests COMMAND avatar_tests)

# ========== PlayerDirectory Online Directory Behavior Contract Tests (P1-2) ==========
# Direct-source compilation convention (same as avatar_tests): does not depend on the
# GAME_MODULE target—unconditional registration in the CI matrix (GAME_MODULE=OFF) still executes.
add_executable(player_directory_tests
    test_player_directory.cpp
    ../modules/game/session/src/player_directory.cpp
)
target_include_directories(player_directory_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/modules/core/include
    ${CMAKE_SOURCE_DIR}/modules/game/session/include
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(player_directory_tests PRIVATE apollo apollo::base apollo::core)
if(WIN32)
    target_link_libraries(player_directory_tests PRIVATE ws2_32)
    target_compile_definitions(player_directory_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(player_directory_tests PRIVATE /utf-8)
endif()
add_test(NAME PlayerDirectoryTests COMMAND player_directory_tests)

# ========== Scene Runtime Tests (P0-3) ==========
# Scene 运行时容器：实体集合 + 玩家进出 + AOI + tick 六阶段
add_executable(scene_tests
    test_scene.cpp
    ../modules/game/core/src/entity.cpp
    ../modules/game/world/src/scene.cpp
    ../modules/game/world/src/scene_aoi.cpp
    ../modules/game/world/src/avatar.cpp
    ../modules/game/session/src/player_anchor.cpp
)
target_include_directories(scene_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/modules/core/include
    ${CMAKE_SOURCE_DIR}/modules/game/core/include
    ${CMAKE_SOURCE_DIR}/modules/game/world/include
    ${CMAKE_SOURCE_DIR}/modules/game/session/include
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(scene_tests PRIVATE apollo apollo::base apollo::core)
if(WIN32)
    target_link_libraries(scene_tests PRIVATE ws2_32)
    target_compile_definitions(scene_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(scene_tests PRIVATE /utf-8)
endif()
add_test(NAME SceneTests COMMAND scene_tests)

# ========== Instance Eight-State Tests (P0-3) ==========
# Instance 八态状态机 + 转换校验表 + enter 窗口 + 玩法节奏
add_executable(instance_tests
    test_instance.cpp
    ../modules/game/world/src/instance.cpp
)
target_include_directories(instance_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/modules/core/include
    ${CMAKE_SOURCE_DIR}/modules/game/core/include
    ${CMAKE_SOURCE_DIR}/modules/game/world/include
    ${CMAKE_SOURCE_DIR}/modules/game/session/include
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(instance_tests PRIVATE apollo apollo::base apollo::core)
if(WIN32)
    target_link_libraries(instance_tests PRIVATE ws2_32)
    target_compile_definitions(instance_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(instance_tests PRIVATE /utf-8)
endif()
add_test(NAME InstanceTests COMMAND instance_tests)
	endif()
