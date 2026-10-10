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

    # ========== RestTemplate Tests (New) ==========

    add_executable(rest_template_tests test_rest_template.cpp)

    target_link_libraries(rest_template_tests PRIVATE apollo)

    if(WIN32)
        target_link_libraries(rest_template_tests PRIVATE ws2_32)
    endif()

    add_test(NAME RestTemplateTests COMMAND rest_template_tests)

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
	        apollo::game_battle
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
	        ../modules/game/world/src/scene.cpp
	        ../modules/game/world/src/scene_aoi.cpp
	        ../modules/game/world/src/viewer_state.cpp
	        ../modules/game/world/src/avatar.cpp
	        ../modules/game/session/src/player_anchor.cpp
	        # Game Attributes (using attribute.hpp, not attribute_value.h)
	        ../modules/game/attributes/src/attribute.cpp
	    )
	    target_include_directories(game_tests PRIVATE
	        ${CMAKE_SOURCE_DIR}/include
	        ${CMAKE_SOURCE_DIR}/modules/game/core/include
	        ${CMAKE_SOURCE_DIR}/modules/game/world/include
	        ${CMAKE_SOURCE_DIR}/modules/game/battle/include
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
    ${CMAKE_SOURCE_DIR}/modules/game/battle/include
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

# ========== Persistence Tests (P1-4) ==========
# Write-behind journal 骨架 + 最小真链路（写盘→重启→读回）。
# 直编源惯例：不依赖 app 目标，三平台 Unit 门禁真实执行。
add_executable(persist_journal_tests
    test_persist_journal.cpp
    ../modules/data/orm/src/persist_journal.cpp
)
target_include_directories(persist_journal_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/modules/data/orm/include
)
if(WIN32)
    target_compile_definitions(persist_journal_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(persist_journal_tests PRIVATE /utf-8)
endif()
add_test(NAME PersistJournalTests COMMAND persist_journal_tests)

add_executable(persistence_chain_tests
    test_persistence_chain.cpp
    ../apps/base-app/src/database_service.cpp
    ../modules/game/session/src/player_anchor.cpp
)
target_include_directories(persistence_chain_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/base-app/include
    ${CMAKE_SOURCE_DIR}/modules/game/session/include
    ${CMAKE_SOURCE_DIR}/modules/core/include
)
target_link_libraries(persistence_chain_tests PRIVATE apollo::base apollo::core)
if(WIN32)
    target_link_libraries(persistence_chain_tests PRIVATE ws2_32)
    target_compile_definitions(persistence_chain_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(persistence_chain_tests PRIVATE /utf-8)
endif()
add_test(NAME PersistenceChainTests COMMAND persistence_chain_tests)

# ========== Recovery Tests (P1-5) ==========
# 三档清单 + restore→admission→ready + journal 消费侧（崩溃回放）。
add_executable(recovery_tests
    test_recovery.cpp
    ../modules/game/session/src/recovery.cpp
    ../modules/game/session/src/anchor_manager.cpp
    ../modules/game/session/src/player_anchor.cpp
    ../modules/data/orm/src/persist_journal.cpp
    ../apps/base-app/src/database_service.cpp
)
target_include_directories(recovery_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/modules/game/session/include
    ${CMAKE_SOURCE_DIR}/modules/data/orm/include
    ${CMAKE_SOURCE_DIR}/modules/core/include
    ${CMAKE_SOURCE_DIR}/apps/base-app/include
)
target_link_libraries(recovery_tests PRIVATE apollo::base apollo::core)
if(WIN32)
    target_link_libraries(recovery_tests PRIVATE ws2_32)
    target_compile_definitions(recovery_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(recovery_tests PRIVATE /utf-8)
endif()
add_test(NAME RecoveryTests COMMAND recovery_tests)

# ========== Reconnect Tests (P1-6) ==========
# 断线挂机窗口 + resume_token 双键校验 + 窗口满 sweep + Avatar 重建
# （P1 出口判据⑤：断线→wait→重连→Avatar 重建）。
add_executable(reconnect_tests
    test_reconnect.cpp
    ../modules/game/core/src/entity.cpp
    ../modules/game/world/src/scene.cpp
    ../modules/game/world/src/scene_aoi.cpp
    ../modules/game/world/src/viewer_state.cpp
    ../modules/game/world/src/avatar.cpp
    ../modules/game/world/src/world_session.cpp
    ../modules/game/world/src/world_session_manager.cpp
    ../modules/game/session/src/player_anchor.cpp
)
target_include_directories(reconnect_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/modules/core/include
    ${CMAKE_SOURCE_DIR}/modules/game/core/include
    ${CMAKE_SOURCE_DIR}/modules/game/world/include
    ${CMAKE_SOURCE_DIR}/modules/game/battle/include
    ${CMAKE_SOURCE_DIR}/modules/game/session/include
)
target_link_libraries(reconnect_tests PRIVATE apollo apollo::base apollo::core)
if(WIN32)
    target_link_libraries(reconnect_tests PRIVATE ws2_32)
    target_compile_definitions(reconnect_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(reconnect_tests PRIVATE /utf-8)
endif()
add_test(NAME ReconnectTests COMMAND reconnect_tests)

# ========== Battle Runtime Tests (P2-2) ==========
# battle-determinism 四约束可测骨架：PCG32 子流派生、生命周期状态机、
# 迭代序归一、四元组复算回放、reward 单向落账、模拟段驱动
# （直编源惯例同 reconnect_tests；battle 依赖 core Entity 实现）
add_executable(battle_runtime_tests
    test_battle_runtime.cpp
    ../modules/game/battle/src/battle_runtime.cpp
    ../modules/game/battle/src/battle_system.cpp
    ../modules/game/core/src/entity.cpp
)
target_include_directories(battle_runtime_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/modules/game/battle/include
    ${CMAKE_SOURCE_DIR}/modules/game/core/include
)
target_link_libraries(battle_runtime_tests PRIVATE apollo apollo::base apollo::core)
if(WIN32)
    target_link_libraries(battle_runtime_tests PRIVATE ws2_32)
    target_compile_definitions(battle_runtime_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(battle_runtime_tests PRIVATE /utf-8)
endif()
add_test(NAME BattleRuntimeTests COMMAND battle_runtime_tests)

# ========== Social Tests (P2-4) ==========
# Guild/Party 最小 CRUD（P2 出口判据④；social 无 core 类型依赖，双 cpp 直编）
add_executable(social_tests
    test_social.cpp
    ../modules/game/social/src/guild.cpp
    ../modules/game/social/src/party.cpp
)
target_include_directories(social_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/modules/game/social/include
)
target_link_libraries(social_tests PRIVATE apollo apollo::base apollo::core)
if(WIN32)
    target_link_libraries(social_tests PRIVATE ws2_32)
    target_compile_definitions(social_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(social_tests PRIVATE /utf-8)
endif()
add_test(NAME SocialTests COMMAND social_tests)

# ========== Metrics Tests (P3-3 批 B) ==========
# MetricRegistry 最小集：counter/gauge 原语 + 标签规范化 + 快照确定性
# （经 apollo::core 传递 apollo_core_metrics，单 cpp）
add_executable(metrics_tests test_metrics.cpp)
target_link_libraries(metrics_tests PRIVATE apollo apollo::base apollo::core)
if(WIN32)
    target_link_libraries(metrics_tests PRIVATE ws2_32)
endif()
if(MSVC)
    target_compile_options(metrics_tests PRIVATE /utf-8)
endif()
add_test(NAME MetricsTests COMMAND metrics_tests)

# ========== Log Structured Tests (P3-3 批 C) ==========
# 结构化行格式（logging.md §5.1 外采契约：稳定键集 + 引号规则 + FileAppender
# structuredOutput 开关 + logfmt 微解析可解析性）
add_executable(log_structured_tests test_log_structured.cpp)
target_link_libraries(log_structured_tests PRIVATE apollo apollo::base apollo::core)
if(WIN32)
    target_link_libraries(log_structured_tests PRIVATE ws2_32)
endif()
if(MSVC)
    target_compile_options(log_structured_tests PRIVATE /utf-8)
endif()
add_test(NAME LogStructuredTests COMMAND log_structured_tests)

# ========== Discovery Tests (P3-1 批 B + 批 C) ==========
# G-1 编队服务发现库层：wire 报文编解码 + Beacon/Registry 行为（内存桩）
# + UDP 传输件 loopback 集成（批 C）
add_executable(discovery_tests
    test_discovery.cpp
    ../modules/net/discovery/src/discovery.cpp
    ../modules/net/discovery/src/udp_transport.cpp
    ../modules/net/discovery/src/directory_locator.cpp
)
target_include_directories(discovery_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/modules/net/discovery/include
)
if(WIN32)
    target_link_libraries(discovery_tests PRIVATE ws2_32)
    target_compile_definitions(discovery_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(discovery_tests PRIVATE /utf-8)
endif()
add_test(NAME DiscoveryTests COMMAND discovery_tests)

# ========== Directory Mirror Tests (P3-1 G-1 收尾批增量②) ==========
# APD2 wire 编解码守卫 + Publisher 发布纪律 + Mirror seq/快照重组 +
# 真目录端到端 + UDP loopback 全链（直编源惯例同 player_directory_tests；
# loopback 复用 net/discovery UDP 传输件源）
add_executable(directory_mirror_tests
    test_directory_mirror.cpp
    ../modules/game/session/src/player_directory.cpp
    ../modules/game/session/src/directory_mirror.cpp
    ../modules/net/discovery/src/udp_transport.cpp
)
target_include_directories(directory_mirror_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/modules/game/session/include
    ${CMAKE_SOURCE_DIR}/modules/net/discovery/include
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(directory_mirror_tests PRIVATE apollo apollo::base apollo::core)
if(WIN32)
    target_link_libraries(directory_mirror_tests PRIVATE ws2_32)
    target_compile_definitions(directory_mirror_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(directory_mirror_tests PRIVATE /utf-8)
endif()
add_test(NAME DirectoryMirrorTests COMMAND directory_mirror_tests)

# ========== Fleet Recovery Tests (P3-1 G-1 收尾批增量③) ==========
# FullReport wire（APD2 kind=4）+ Reporter 分片 + Coordinator 恢复相位收敛 +
# PlayerDirectory intake（restore-not-kick）+ UDP loopback 全链（直编源惯例；
# 准入闸门单测随 baseappmgr_tests）
add_executable(fleet_recovery_tests
    test_fleet_recovery.cpp
    ../modules/game/session/src/player_directory.cpp
    ../modules/game/session/src/directory_mirror.cpp
    ../modules/game/session/src/fleet_recovery.cpp
    ../modules/net/discovery/src/udp_transport.cpp
)
target_include_directories(fleet_recovery_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/modules/game/session/include
    ${CMAKE_SOURCE_DIR}/modules/net/discovery/include
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(fleet_recovery_tests PRIVATE apollo apollo::base apollo::core)
if(WIN32)
    target_link_libraries(fleet_recovery_tests PRIVATE ws2_32)
    target_compile_definitions(fleet_recovery_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(fleet_recovery_tests PRIVATE /utf-8)
endif()
add_test(NAME FleetRecoveryTests COMMAND fleet_recovery_tests)

# ========== Supervisor Roster Tests (P3-1 §6 死亡行窗口处置批) ==========
# machined 监督面：roster zone 列解析（纯数字判据 + 两列/三列旧格式兼容）
# + 监督事件 zone 透传（真 fork 子进程死亡收割）。Windows 下 supervisor.cpp
# 是空转桩（POSIX only 口径），测试端同步空转跳过（直编源惯例）
add_executable(supervisor_tests
    test_supervisor.cpp
    ../apps/machined/src/supervisor.cpp
)
target_include_directories(supervisor_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/machined/src
)
target_link_libraries(supervisor_tests PRIVATE apollo::core_log)
if(WIN32)
    target_compile_definitions(supervisor_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(supervisor_tests PRIVATE /utf-8)
endif()
add_test(NAME SupervisorTests COMMAND supervisor_tests)

# ========== Scene Runtime Tests (P0-3) ==========
# Scene 运行时容器：实体集合 + 玩家进出 + AOI + tick 六阶段
add_executable(scene_tests
    test_scene.cpp
    ../modules/game/core/src/entity.cpp
    ../modules/game/world/src/scene.cpp
    ../modules/game/world/src/scene_aoi.cpp
    ../modules/game/world/src/viewer_state.cpp
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
    ../modules/game/battle/src/battle_runtime.cpp
    ../modules/game/battle/src/battle_system.cpp
    ../modules/game/core/src/entity.cpp
)
target_include_directories(instance_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/modules/core/include
    ${CMAKE_SOURCE_DIR}/modules/game/core/include
    ${CMAKE_SOURCE_DIR}/modules/game/world/include
    ${CMAKE_SOURCE_DIR}/modules/game/battle/include
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

	# ========== RNG Substream Tests (P2-2, battle-determinism §4) ==========
	# RNG 子流组件（modules/base 头件）：同三元组同序列/分域隔离/无游标/任意 tick 重放
	add_executable(rng_substream_tests
		test_rng_substream.cpp
	)
	target_include_directories(rng_substream_tests PRIVATE
		${CMAKE_SOURCE_DIR}/modules/base/include
		${CMAKE_SOURCE_DIR}/include
	)
	target_link_libraries(rng_substream_tests PRIVATE apollo apollo::base)
	if(WIN32)
		target_link_libraries(rng_substream_tests PRIVATE ws2_32)
		target_compile_definitions(rng_substream_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
	endif()
	if(MSVC)
		target_compile_options(rng_substream_tests PRIVATE /utf-8)
	endif()
	add_test(NAME RngSubstreamTests COMMAND rng_substream_tests)

# ========== Timer Wheel Tests (G-4 批) ==========
# 分层时间轮（modules/base 组件，architecture-review §16.8-④ 落点）：
# O(1) 调度/撤销 + 喂钟驱动 + 页界级联降层 + 回调重入 + 补拍语义 +
# 代数防陈旧撤销（直编源惯例同 rng_substream_tests——经 apollo::base
# INTERFACE 源传播编入 timer_wheel.cpp）
add_executable(timer_wheel_tests
    test_timer_wheel.cpp
)
target_include_directories(timer_wheel_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/modules/base/include
)
target_link_libraries(timer_wheel_tests PRIVATE apollo apollo::base)
if(WIN32)
    target_link_libraries(timer_wheel_tests PRIVATE ws2_32)
    target_compile_definitions(timer_wheel_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(timer_wheel_tests PRIVATE /utf-8)
endif()
add_test(NAME TimerWheelTests COMMAND timer_wheel_tests)

# ========== Cell Acceptance Tests (P3-2 批 B, ADR-015(a)/016/017) ==========
# cell 受理面：意图信封队列（FIFO/顶格拒收/两级水位滞回）+ CellServer 受理点
# 接线（维护闸/受理回执/Pong/tick 边界 drain/过载回错误包）。直编协议源惯例
# 同 apps/cell-app（stub 传输，APOLLO_USE_NNG 未定义）。
if(TARGET apollo::game_core AND TARGET apollo::game_world AND TARGET apollo::runtime)
add_executable(cell_acceptance_tests
    test_cell_acceptance.cpp
    ../apps/cell-app/src/cell_server.cpp
    ../modules/protocol/src/nng_wrapper.cpp
    ../modules/protocol/src/messages.cpp
    ../modules/protocol/src/codec.cpp
    ../modules/protocol/src/socket.cpp
)
target_include_directories(cell_acceptance_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/cell-app/include
    ${CMAKE_SOURCE_DIR}/modules/protocol/include
    ${CMAKE_SOURCE_DIR}/modules/runtime/include
)
target_link_libraries(cell_acceptance_tests PRIVATE
    apollo apollo::base apollo::core apollo::game_core apollo::game_world apollo::runtime)
if(WIN32)
    target_link_libraries(cell_acceptance_tests PRIVATE ws2_32)
    target_compile_definitions(cell_acceptance_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(cell_acceptance_tests PRIVATE /utf-8)
endif()
add_test(NAME CellAcceptanceTests COMMAND cell_acceptance_tests)
endif()
	endif()
