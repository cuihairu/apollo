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

    # ========== Network Tests ==========

    # Buffer tests
    add_executable(buffer_tests test_buffer.cpp)

    target_link_libraries(buffer_tests PRIVATE apollo)

    if(WIN32)
        target_link_libraries(buffer_tests PRIVATE ws2_32)
    endif()

    add_test(NAME BufferTests COMMAND buffer_tests)

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
	    add_executable(base_avatar_tests
	        test_base_avatar.cpp
	        ../apps/common/src/player_store.cpp
	        ../apps/common/src/database_service.cpp
	        ../apps/common/src/player_store_m1.cpp
	    )
	    target_include_directories(base_avatar_tests PRIVATE
	        ${CMAKE_SOURCE_DIR}/apps/common/include
	        ${CMAKE_SOURCE_DIR}/sdks/cpp/generated
	    )
	    target_compile_definitions(base_avatar_tests PRIVATE
	        APOLLO_BASEAPP_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
	    )
	    target_link_libraries(base_avatar_tests PRIVATE
	        apollo
	        apollo::base
	        apollo::core
	        apollo::game_session
	        apollo_protocol
	        apollo_net_link
	        apollo_net_linkrpc
	    )
	    # 直编 database_service.cpp：其 nlohmann 序列化依赖在此口径自取
	    #（可缺失环境守卫，同 modules/protocol 口径）
	    if(TARGET nlohmann_json::nlohmann_json)
	        target_link_libraries(base_avatar_tests PRIVATE nlohmann_json::nlohmann_json)
	    endif()
	    if(WIN32)
	        target_link_libraries(base_avatar_tests PRIVATE ws2_32)
	    endif()
	    add_test(NAME BaseAvatarTests COMMAND base_avatar_tests)

	    # baseappmgr（目录 + 落点裁决）——与 base-app 同档（legacy protocol + game_session）
	    add_executable(baseappmgr_tests
	        test_baseappmgr.cpp
	        ../apps/common/src/online_directory.cpp
	        ../apps/common/src/directory_m1.cpp
	        ../apps/common/src/player_store.cpp
	        ../apps/common/src/database_service.cpp
	        ../apps/common/src/player_store_m1.cpp
	    )
	    target_include_directories(baseappmgr_tests PRIVATE
	        ${CMAKE_SOURCE_DIR}/apps/common/include
	        ${CMAKE_SOURCE_DIR}/apps/common/include
	        ${CMAKE_SOURCE_DIR}/sdks/cpp/generated
	    )
	    target_compile_definitions(baseappmgr_tests PRIVATE
	        APOLLO_BASEAPP_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
	    )
	    target_link_libraries(baseappmgr_tests PRIVATE
	        apollo
	        apollo::base
	        apollo::core
	        apollo::game_session
	        apollo_protocol
	        apollo_net_link
	        apollo_net_linkrpc
	    )
	    if(TARGET nlohmann_json::nlohmann_json)
	        target_link_libraries(baseappmgr_tests PRIVATE nlohmann_json::nlohmann_json)
	    endif()
	    if(WIN32)
	        target_link_libraries(baseappmgr_tests PRIVATE ws2_32)
	    endif()
	    add_test(NAME OnlineDirectoryTests COMMAND baseappmgr_tests)
	    endif()

	    # 以下两个目标依赖旧 modules/protocol（默认禁用）——按目标存在性守卫
	    # （apps/ 同惯例），模块关闭时跳过而非 configure 即红。
	    if(TARGET apollo_protocol)
	    add_executable(gateway_route_tests
	        test_gateway_route.cpp
	        ../apps/gateway/src/gateway_server.cpp
	        ../apps/gateway/src/gateway_control_server.cpp
	        ../apps/gateway/src/m1_egress.cpp
	        ../apps/gateway/src/m1_route_client.cpp
        ../apps/gateway/src/m1_base_client.cpp
	        ../apps/gateway/include/gateway/session_manager.cpp
            ../apps/gateway/src/ingress/client_ingress_server.cpp
            ../apps/gateway/src/ingress/tcp_client_ingress_server.cpp
            ../apps/gateway/src/ingress/ws_client_ingress_server.cpp
            ../apps/gateway/src/ingress/ingress_fan_in.cpp
            ../apps/gateway/src/ingress/client_packet_dispatcher.cpp
            ../apps/gateway/src/ingress/gateway_connection_registry.cpp
            ../apps/gateway/src/ingress/session_admission_service.cpp
	    )
	    target_include_directories(gateway_route_tests PRIVATE
	        ${CMAKE_SOURCE_DIR}/apps/gateway/include
	    )
	    target_compile_definitions(gateway_route_tests PRIVATE
	        APOLLO_GATEWAY_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
	    )
	    target_link_libraries(gateway_route_tests PRIVATE
	        apollo
	        apollo::base
	        apollo::core
	        apollo::runtime
	        apollo::net_tcp
	        apollo::net_protocol
	        apollo::net_link
	        apollo_net_tls
	        apollo_net_ws
	        apollo::net_contract_wire
	        apollo::net_linktrace
	        apollo_net_linkrpc
	        apollo_protocol
	    )
	    if(WIN32)
	        target_link_libraries(gateway_route_tests PRIVATE ws2_32)
	    endif()
	    add_test(NAME GatewayRouteTests COMMAND gateway_route_tests)

	    # gateway 入口真接线测试（link 帧定界 + TCP 底座，localhost 实 socket；
	    # Windows 下 link TCP 底座不进 M1 批，TCP 用例本体空转）
	    add_executable(gateway_ingress_tests
	        test_gateway_ingress.cpp
	        ../apps/gateway/src/gateway_server.cpp
	        ../apps/gateway/src/gateway_control_server.cpp
	        ../apps/gateway/src/m1_egress.cpp
	        ../apps/gateway/src/m1_route_client.cpp
        ../apps/gateway/src/m1_base_client.cpp
	        ../apps/gateway/include/gateway/session_manager.cpp
            ../apps/gateway/src/ingress/client_ingress_server.cpp
            ../apps/gateway/src/ingress/tcp_client_ingress_server.cpp
            ../apps/gateway/src/ingress/ws_client_ingress_server.cpp
            ../apps/gateway/src/ingress/ingress_fan_in.cpp
            ../apps/gateway/src/ingress/client_packet_dispatcher.cpp
            ../apps/gateway/src/ingress/gateway_connection_registry.cpp
            ../apps/gateway/src/ingress/session_admission_service.cpp
	    )
	    target_include_directories(gateway_ingress_tests PRIVATE
	        ${CMAKE_SOURCE_DIR}/apps/gateway/include
	    )
	    target_compile_definitions(gateway_ingress_tests PRIVATE
	        APOLLO_GATEWAY_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
	    )
	    target_link_libraries(gateway_ingress_tests PRIVATE
	        apollo
	        apollo::base
	        apollo::core
	        apollo::runtime
	        apollo::net_tcp
	        apollo::net_protocol
	        apollo::net_link
	        apollo_net_tls
	        apollo_net_ws
	        apollo::net_contract_wire
	        apollo::net_linktrace
	        apollo_net_linkrpc
	        apollo_protocol
	    )
	    if(WIN32)
	        target_link_libraries(gateway_ingress_tests PRIVATE ws2_32)
	        target_compile_definitions(gateway_ingress_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
	    endif()
	    if(MSVC)
	        target_compile_options(gateway_ingress_tests PRIVATE /utf-8)
	    endif()
	    add_test(NAME GatewayIngressTests COMMAND gateway_ingress_tests)

	    # gateway 排空真接线测试（P4 排空批；契约 954/955 + drain 状态机，
	    # localhost 实 socket；Windows 下 link TCP 底座不进 M1 批，TCP 用例
	    # 本体空转）
	    add_executable(gateway_drain_tests
	        test_gateway_drain.cpp
	        ../apps/gateway/src/gateway_server.cpp
	        ../apps/gateway/src/gateway_control_server.cpp
	        ../apps/gateway/src/m1_egress.cpp
	        ../apps/gateway/src/m1_route_client.cpp
        ../apps/gateway/src/m1_base_client.cpp
	        ../apps/gateway/include/gateway/session_manager.cpp
            ../apps/gateway/src/ingress/client_ingress_server.cpp
            ../apps/gateway/src/ingress/tcp_client_ingress_server.cpp
            ../apps/gateway/src/ingress/ws_client_ingress_server.cpp
            ../apps/gateway/src/ingress/ingress_fan_in.cpp
            ../apps/gateway/src/ingress/client_packet_dispatcher.cpp
            ../apps/gateway/src/ingress/gateway_connection_registry.cpp
            ../apps/gateway/src/ingress/session_admission_service.cpp
	    )
	    target_include_directories(gateway_drain_tests PRIVATE
	        ${CMAKE_SOURCE_DIR}/apps/gateway/include
	    )
	    target_compile_definitions(gateway_drain_tests PRIVATE
	        APOLLO_GATEWAY_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
	    )
	    target_link_libraries(gateway_drain_tests PRIVATE
	        apollo
	        apollo::base
	        apollo::core
	        apollo::runtime
	        apollo::net_tcp
	        apollo::net_protocol
	        apollo::net_link
	        apollo_net_tls
	        apollo_net_ws
	        apollo::net_contract_wire
	        apollo::net_linktrace
	        apollo_net_linkrpc
	        apollo_protocol
	    )
	    if(WIN32)
	        target_link_libraries(gateway_drain_tests PRIVATE ws2_32)
	        target_compile_definitions(gateway_drain_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
	    endif()
	    if(MSVC)
	        target_compile_options(gateway_drain_tests PRIVATE /utf-8)
	    endif()
	    add_test(NAME GatewayDrainTests COMMAND gateway_drain_tests)

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

	    if(TARGET apollo::game_session AND TARGET apollo_protocol)
	    add_executable(base_avatar_tests
	        test_base_avatar.cpp
	        ../apps/common/src/player_store.cpp
	        ../apps/common/src/database_service.cpp
	        ../apps/common/src/player_store_m1.cpp
	    )
	    target_include_directories(base_avatar_tests PRIVATE
	        ${CMAKE_SOURCE_DIR}/apps/common/include
	        ${CMAKE_SOURCE_DIR}/sdks/cpp/generated
	    )
	    target_compile_definitions(base_avatar_tests PRIVATE
	        APOLLO_BASEAPP_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
	    )
	    target_link_libraries(base_avatar_tests PRIVATE
	        apollo
	        apollo::base
	        apollo::core
	        apollo::game_session
	        apollo_protocol
	        apollo_net_link
	        apollo_net_linkrpc
	    )
	    # 直编 database_service.cpp：其 nlohmann 序列化依赖在此口径自取
	    #（可缺失环境守卫，同 modules/protocol 口径）
	    if(TARGET nlohmann_json::nlohmann_json)
	        target_link_libraries(base_avatar_tests PRIVATE nlohmann_json::nlohmann_json)
	    endif()
	    if(WIN32)
	        target_link_libraries(base_avatar_tests PRIVATE ws2_32)
	    endif()
	    add_test(NAME BaseAvatarTests COMMAND base_avatar_tests)

	    # baseappmgr（目录 + 落点裁决）——与 base-app 同档（legacy protocol + game_session）
	    add_executable(baseappmgr_tests
	        test_baseappmgr.cpp
	        ../apps/common/src/online_directory.cpp
	        ../apps/common/src/directory_m1.cpp
	        ../apps/common/src/player_store.cpp
	        ../apps/common/src/database_service.cpp
	        ../apps/common/src/player_store_m1.cpp
	    )
	    target_include_directories(baseappmgr_tests PRIVATE
	        ${CMAKE_SOURCE_DIR}/apps/common/include
	        ${CMAKE_SOURCE_DIR}/apps/common/include
	        ${CMAKE_SOURCE_DIR}/sdks/cpp/generated
	    )
	    target_compile_definitions(baseappmgr_tests PRIVATE
	        APOLLO_BASEAPP_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
	    )
	    target_link_libraries(baseappmgr_tests PRIVATE
	        apollo
	        apollo::base
	        apollo::core
	        apollo::game_session
	        apollo_protocol
	        apollo_net_link
	        apollo_net_linkrpc
	    )
	    if(TARGET nlohmann_json::nlohmann_json)
	        target_link_libraries(baseappmgr_tests PRIVATE nlohmann_json::nlohmann_json)
	    endif()
	    if(WIN32)
	        target_link_libraries(baseappmgr_tests PRIVATE ws2_32)
	    endif()
	    add_test(NAME OnlineDirectoryTests COMMAND baseappmgr_tests)
	    endif()

	    # 以下两个目标依赖旧 modules/protocol（默认禁用）——按目标存在性守卫
	    # （apps/ 同惯例），模块关闭时跳过而非 configure 即红。
	    if(TARGET apollo_protocol)
	    add_executable(gateway_route_tests
	        test_gateway_route.cpp
	        ../apps/gateway/src/gateway_server.cpp
	        ../apps/gateway/src/gateway_control_server.cpp
	        ../apps/gateway/src/m1_egress.cpp
	        ../apps/gateway/src/m1_route_client.cpp
        ../apps/gateway/src/m1_base_client.cpp
	        ../apps/gateway/include/gateway/session_manager.cpp
            ../apps/gateway/src/ingress/client_ingress_server.cpp
            ../apps/gateway/src/ingress/tcp_client_ingress_server.cpp
            ../apps/gateway/src/ingress/ws_client_ingress_server.cpp
            ../apps/gateway/src/ingress/ingress_fan_in.cpp
            ../apps/gateway/src/ingress/client_packet_dispatcher.cpp
            ../apps/gateway/src/ingress/gateway_connection_registry.cpp
            ../apps/gateway/src/ingress/session_admission_service.cpp
	    )
	    target_include_directories(gateway_route_tests PRIVATE
	        ${CMAKE_SOURCE_DIR}/apps/gateway/include
	    )
	    target_compile_definitions(gateway_route_tests PRIVATE
	        APOLLO_GATEWAY_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
	    )
	    target_link_libraries(gateway_route_tests PRIVATE
	        apollo
	        apollo::base
	        apollo::core
	        apollo::runtime
	        apollo::net_tcp
	        apollo::net_protocol
	        apollo::net_link
	        apollo_net_tls
	        apollo_net_ws
	        apollo::net_contract_wire
	        apollo::net_linktrace
	        apollo_net_linkrpc
	        apollo_protocol
	    )
	    if(WIN32)
	        target_link_libraries(gateway_route_tests PRIVATE ws2_32)
	    endif()
	    add_test(NAME GatewayRouteTests COMMAND gateway_route_tests)

	    # gateway 入口真接线测试（link 帧定界 + TCP 底座，localhost 实 socket；
	    # Windows 下 link TCP 底座不进 M1 批，TCP 用例本体空转）
	    add_executable(gateway_ingress_tests
	        test_gateway_ingress.cpp
	        ../apps/gateway/src/gateway_server.cpp
	        ../apps/gateway/src/gateway_control_server.cpp
	        ../apps/gateway/src/m1_egress.cpp
	        ../apps/gateway/src/m1_route_client.cpp
        ../apps/gateway/src/m1_base_client.cpp
	        ../apps/gateway/include/gateway/session_manager.cpp
            ../apps/gateway/src/ingress/client_ingress_server.cpp
            ../apps/gateway/src/ingress/tcp_client_ingress_server.cpp
            ../apps/gateway/src/ingress/ws_client_ingress_server.cpp
            ../apps/gateway/src/ingress/ingress_fan_in.cpp
            ../apps/gateway/src/ingress/client_packet_dispatcher.cpp
            ../apps/gateway/src/ingress/gateway_connection_registry.cpp
            ../apps/gateway/src/ingress/session_admission_service.cpp
	    )
	    target_include_directories(gateway_ingress_tests PRIVATE
	        ${CMAKE_SOURCE_DIR}/apps/gateway/include
	    )
	    target_compile_definitions(gateway_ingress_tests PRIVATE
	        APOLLO_GATEWAY_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
	    )
	    target_link_libraries(gateway_ingress_tests PRIVATE
	        apollo
	        apollo::base
	        apollo::core
	        apollo::runtime
	        apollo::net_tcp
	        apollo::net_protocol
	        apollo::net_link
	        apollo_net_tls
	        apollo_net_ws
	        apollo::net_contract_wire
	        apollo::net_linktrace
	        apollo_net_linkrpc
	        apollo_protocol
	    )
	    if(WIN32)
	        target_link_libraries(gateway_ingress_tests PRIVATE ws2_32)
	        target_compile_definitions(gateway_ingress_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
	    endif()
	    if(MSVC)
	        target_compile_options(gateway_ingress_tests PRIVATE /utf-8)
	    endif()
	    add_test(NAME GatewayIngressTests COMMAND gateway_ingress_tests)

	    # gateway 排空真接线测试（P4 排空批；契约 954/955 + drain 状态机，
	    # localhost 实 socket；Windows 下 link TCP 底座不进 M1 批，TCP 用例
	    # 本体空转）
	    add_executable(gateway_drain_tests
	        test_gateway_drain.cpp
	        ../apps/gateway/src/gateway_server.cpp
	        ../apps/gateway/src/gateway_control_server.cpp
	        ../apps/gateway/src/m1_egress.cpp
	        ../apps/gateway/src/m1_route_client.cpp
        ../apps/gateway/src/m1_base_client.cpp
	        ../apps/gateway/include/gateway/session_manager.cpp
            ../apps/gateway/src/ingress/client_ingress_server.cpp
            ../apps/gateway/src/ingress/tcp_client_ingress_server.cpp
            ../apps/gateway/src/ingress/ws_client_ingress_server.cpp
            ../apps/gateway/src/ingress/ingress_fan_in.cpp
            ../apps/gateway/src/ingress/client_packet_dispatcher.cpp
            ../apps/gateway/src/ingress/gateway_connection_registry.cpp
            ../apps/gateway/src/ingress/session_admission_service.cpp
	    )
	    target_include_directories(gateway_drain_tests PRIVATE
	        ${CMAKE_SOURCE_DIR}/apps/gateway/include
	    )
	    target_compile_definitions(gateway_drain_tests PRIVATE
	        APOLLO_GATEWAY_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
	    )
	    target_link_libraries(gateway_drain_tests PRIVATE
	        apollo
	        apollo::base
	        apollo::core
	        apollo::runtime
	        apollo::net_tcp
	        apollo::net_protocol
	        apollo::net_link
	        apollo_net_tls
	        apollo_net_ws
	        apollo::net_contract_wire
	        apollo::net_linktrace
	        apollo_net_linkrpc
	        apollo_protocol
	    )
	    if(WIN32)
	        target_link_libraries(gateway_drain_tests PRIVATE ws2_32)
	        target_compile_definitions(gateway_drain_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
	    endif()
	    if(MSVC)
	        target_compile_options(gateway_drain_tests PRIVATE /utf-8)
	    endif()
	    add_test(NAME GatewayDrainTests COMMAND gateway_drain_tests)

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

	endif()

# ========== TEST_ASSERT Suite Registration (moved from else() fallback, 2026-10-16) ==========
# These 24 suites use the TEST_ASSERT harness (not GTest). They were misplaced in the
# GTest-false else() branch, silently absent from the local gate when GTest is available.
# Moved to top-level so they register regardless of GTest availability.

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
        ../modules/game/cell/src/avatar.cpp
        # Game AttrSync (scene.cpp attrsync wiring, 2026-10-14 批)
        ../modules/game/attrsync/src/change_history.cpp
        ../modules/game/attrsync/src/viewer_attr_state.cpp
        ../modules/game/attrsync/src/delta_builder.cpp
        ../modules/game/attrsync/src/entity_attrs.cpp
        ../modules/game/attrsync/src/attr_sync_manager.cpp
        ../modules/game/base/src/avatar.cpp
        # Game Attributes (using attribute.hpp, not attribute_value.h)
        ../modules/game/attributes/src/attribute.cpp
    )
    target_include_directories(game_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/modules/game/core/include
        ${CMAKE_SOURCE_DIR}/modules/game/cell/include
        ${CMAKE_SOURCE_DIR}/modules/game/world/include
        ${CMAKE_SOURCE_DIR}/modules/game/battle/include
        ${CMAKE_SOURCE_DIR}/modules/game/base/include
        ${CMAKE_SOURCE_DIR}/modules/game/session/include
        ${CMAKE_SOURCE_DIR}/modules/game/attrsync/include
        # scene.cpp 直编含 Scene 脚本桥头（Scene 桥批起）
        ${CMAKE_SOURCE_DIR}/modules/script/include
    )
    target_link_libraries(game_tests PRIVATE apollo apollo::base apollo::core apollo::script)
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
# （直编源惯例同 game_tests；cell::Avatar 依赖 base::Avatar 实现，故一并编入）
add_executable(avatar_tests
    test_avatar.cpp
    ../modules/game/cell/src/avatar.cpp
    ../modules/game/base/src/avatar.cpp
)
target_include_directories(avatar_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/modules/core/include
    ${CMAKE_SOURCE_DIR}/modules/game/core/include
    ${CMAKE_SOURCE_DIR}/modules/game/cell/include
    ${CMAKE_SOURCE_DIR}/modules/game/world/include
    ${CMAKE_SOURCE_DIR}/modules/game/battle/include
    ${CMAKE_SOURCE_DIR}/modules/game/base/include
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
    ${CMAKE_SOURCE_DIR}/modules/game/base/include
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
    ../apps/common/src/database_service.cpp
    ../modules/game/base/src/avatar.cpp
)
target_include_directories(persistence_chain_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/common/include
    ${CMAKE_SOURCE_DIR}/modules/game/base/include
    ${CMAKE_SOURCE_DIR}/modules/game/session/include
    ${CMAKE_SOURCE_DIR}/modules/core/include
)
target_link_libraries(persistence_chain_tests PRIVATE apollo::base apollo::core)
# 直编 database_service.cpp：其 nlohmann 序列化依赖在此口径自取
#（可缺失环境守卫，同 modules/protocol 口径）
find_package(nlohmann_json CONFIG QUIET)
if(nlohmann_json_FOUND)
    target_link_libraries(persistence_chain_tests PRIVATE nlohmann_json::nlohmann_json)
endif()
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
    ../modules/game/base/src/recovery.cpp
    ../modules/game/base/src/avatar_manager.cpp
    ../modules/game/base/src/avatar.cpp
    ../modules/data/orm/src/persist_journal.cpp
    ../apps/common/src/database_service.cpp
)
target_include_directories(recovery_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/modules/game/base/include
    ${CMAKE_SOURCE_DIR}/modules/game/session/include
    ${CMAKE_SOURCE_DIR}/modules/data/orm/include
    ${CMAKE_SOURCE_DIR}/modules/core/include
    ${CMAKE_SOURCE_DIR}/apps/common/include
)
target_link_libraries(recovery_tests PRIVATE apollo::base apollo::core)
# 直编 database_service.cpp：同 persistence_chain_tests 的 nlohmann 口径
if(nlohmann_json_FOUND)
    target_link_libraries(recovery_tests PRIVATE nlohmann_json::nlohmann_json)
endif()
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
    ../modules/game/cell/src/avatar.cpp
    ../modules/game/world/src/world_session.cpp
    ../modules/game/world/src/world_session_manager.cpp
    # Game AttrSync (scene.cpp attrsync wiring, 2026-10-14 批)
    ../modules/game/attrsync/src/change_history.cpp
    ../modules/game/attrsync/src/viewer_attr_state.cpp
    ../modules/game/attrsync/src/delta_builder.cpp
    ../modules/game/attrsync/src/entity_attrs.cpp
    ../modules/game/attrsync/src/attr_sync_manager.cpp
    ../modules/game/base/src/avatar.cpp
)
target_include_directories(reconnect_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/modules/core/include
    ${CMAKE_SOURCE_DIR}/modules/game/core/include
    ${CMAKE_SOURCE_DIR}/modules/game/cell/include
    ${CMAKE_SOURCE_DIR}/modules/game/world/include
    ${CMAKE_SOURCE_DIR}/modules/game/battle/include
    ${CMAKE_SOURCE_DIR}/modules/game/base/include
    ${CMAKE_SOURCE_DIR}/modules/game/session/include
    ${CMAKE_SOURCE_DIR}/modules/game/attrsync/include
    # scene.cpp 直编含 Scene 脚本桥头（Scene 桥批起）
    ${CMAKE_SOURCE_DIR}/modules/script/include
)
target_link_libraries(reconnect_tests PRIVATE apollo apollo::base apollo::core apollo::script)
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

# ========== Battle Offload Tests (P3-1 批 2) ==========
# instance offload 骨架（内存桩过渡）：wire 信封编解码（契约 919-923 字段序
# 逐字段小端）+ spawn 生命周期全链（ack-on-accept → 目录就绪钩子 → Running）
# + 落回单向（Result Dispatcher → IRewardSink）+ 投影水位 + 崩溃即作废 +
# 跨边界确定性（同 seed 同输入同 hash 链）
# （直编源惯例同 battle_runtime_tests；信封身份取生成器 MsgId——
# sdks/cpp/generated 头文件常量，无链接面）
add_executable(battle_offload_tests
    test_battle_offload.cpp
    ../modules/game/battle/src/battle_runtime.cpp
    ../modules/game/battle/src/battle_system.cpp
    ../modules/game/battle/src/offload_wire.cpp
    ../modules/game/battle/src/offload_host.cpp
    ../modules/game/battle/src/offload_node.cpp
    ../modules/game/core/src/entity.cpp
)
target_include_directories(battle_offload_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/modules/game/battle/include
    ${CMAKE_SOURCE_DIR}/modules/game/core/include
    ${CMAKE_SOURCE_DIR}/sdks/cpp/generated
)
target_link_libraries(battle_offload_tests PRIVATE apollo apollo::base apollo::core)
if(WIN32)
    target_link_libraries(battle_offload_tests PRIVATE ws2_32)
    target_compile_definitions(battle_offload_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(battle_offload_tests PRIVATE /utf-8)
endif()
add_test(NAME BattleOffloadTests COMMAND battle_offload_tests)

# ========== Link Tests (net M1 内核) ==========
# 帧定界 + 契约路由分发 + 背压水位 + 单播拓扑 + InterServerLink
# （net-abstraction §5.6 M1；正确性优先。内核组件零 apollo 依赖——
# 链 apollo_net_link 取 PUBLIC 面[nlohmann/HAS_JSON/头树]；TCP 用例走
# localhost 实 socket）
add_executable(link_tests
    test_net_link.cpp
)
target_link_libraries(link_tests PRIVATE apollo_net_link)

# 契约 wire codec 测试（golden：随仓契约全量消息 round-trip + 截断/布局负例）
add_executable(net_wire_tests
    test_net_wire.cpp
)
target_link_libraries(net_wire_tests PRIVATE apollo::net_contract_wire)
if(MSVC)
    target_compile_options(net_wire_tests PRIVATE /utf-8)
endif()
add_test(NAME NetWireTests COMMAND net_wire_tests)

target_compile_definitions(link_tests PRIVATE
    APOLLO_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)
if(WIN32)
    target_link_libraries(link_tests PRIVATE ws2_32)
    target_compile_definitions(link_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(link_tests PRIVATE /utf-8)
endif()
add_test(NAME LinkTests COMMAND link_tests)

# ========== TLS 底座测试 ==========
# TLS 是 link 内核的底座替换件（ILinkTransport 缝纪律同源），OpenSSL 只
# 进 apollo_net_tls 与本目标——link_tests 保持纯 std 面。证书夹具由测试
# 经 OpenSSL API 现场自签（tests/tls_test_support.hpp），仓库不存凭据。
add_executable(tls_link_tests
    test_net_link_tls.cpp
)
target_link_libraries(tls_link_tests PRIVATE apollo_net_tls)
if(MSVC)
    target_compile_options(tls_link_tests PRIVATE /utf-8)
endif()
add_test(NAME TlsLinkTests COMMAND tls_link_tests)

# ========== WS 编解码测试 ==========
# 纯编解码件（零 socket 零线程）：升级握手 + RFC 6455 帧。传输与生命
# 周期归宿主泵（gateway 游戏入口 / interfaces HTTP 升级路由各自驱动）。
add_executable(ws_tests
    test_net_ws.cpp
)
target_link_libraries(ws_tests PRIVATE apollo_net_ws)
if(MSVC)
    target_compile_options(ws_tests PRIVATE /utf-8)
endif()
add_test(NAME WsTests COMMAND ws_tests)

# ========== Gateway WS/WSS 入口真接线测试 (L45 批③) ==========
# 多监听扇入 + ws/wss 游戏入口（localhost 实 socket；ws binary 载荷 =
# link Frame 同 TCP 入口线形）。WSS 证书夹具现场自签（tls_test_support），
# 仓库不存凭据。Windows 下 link TCP/TLS 底座不进 M1 批，用例本体空转。
if(TARGET apollo_protocol)
add_executable(gateway_ws_ingress_tests
    test_gateway_ws_ingress.cpp
    ../apps/gateway/src/gateway_server.cpp
    ../apps/gateway/src/gateway_control_server.cpp
    ../apps/gateway/src/m1_egress.cpp
    ../apps/gateway/src/m1_route_client.cpp
    ../apps/gateway/src/m1_base_client.cpp
    ../apps/gateway/include/gateway/session_manager.cpp
    ../apps/gateway/src/ingress/client_ingress_server.cpp
    ../apps/gateway/src/ingress/tcp_client_ingress_server.cpp
    ../apps/gateway/src/ingress/ws_client_ingress_server.cpp
    ../apps/gateway/src/ingress/ingress_fan_in.cpp
    ../apps/gateway/src/ingress/client_packet_dispatcher.cpp
    ../apps/gateway/src/ingress/gateway_connection_registry.cpp
    ../apps/gateway/src/ingress/session_admission_service.cpp
)
target_include_directories(gateway_ws_ingress_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/gateway/include
)
target_compile_definitions(gateway_ws_ingress_tests PRIVATE
    APOLLO_GATEWAY_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)
target_link_libraries(gateway_ws_ingress_tests PRIVATE
    apollo
    apollo::base
    apollo::core
    apollo::runtime
    apollo::net_tcp
    apollo::net_protocol
    apollo::net_link
    apollo_net_tls
    apollo_net_ws
    apollo::net_contract_wire
    apollo::net_linktrace
    apollo_net_linkrpc
    apollo_protocol
)
if(WIN32)
    target_link_libraries(gateway_ws_ingress_tests PRIVATE ws2_32)
    target_compile_definitions(gateway_ws_ingress_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(gateway_ws_ingress_tests PRIVATE /utf-8)
endif()
add_test(NAME GatewayWsIngressTests COMMAND gateway_ws_ingress_tests)
endif()

# ========== Gateway 会话连续测试 (L53 断线重连：保留窗内跨连接/跨传输恢复) ==========
# TCP 与 WS 分别验收（TCP 掉线 → WS 恢复 / WS 掉线 → TCP 恢复双向），恢复
# 判据走线缆可见面（PONG 回执头 sessionId 同号）。Windows 下 link TCP/TLS
# 底座不进 M1 批，用例本体空转。
if(TARGET apollo_protocol)
add_executable(gateway_session_resume_tests
    test_gateway_session_resume.cpp
    ../apps/gateway/src/gateway_server.cpp
    ../apps/gateway/src/gateway_control_server.cpp
    ../apps/gateway/src/m1_egress.cpp
    ../apps/gateway/src/m1_route_client.cpp
    ../apps/gateway/src/m1_base_client.cpp
    ../apps/gateway/include/gateway/session_manager.cpp
    ../apps/gateway/src/ingress/client_ingress_server.cpp
    ../apps/gateway/src/ingress/tcp_client_ingress_server.cpp
    ../apps/gateway/src/ingress/ws_client_ingress_server.cpp
    ../apps/gateway/src/ingress/ingress_fan_in.cpp
    ../apps/gateway/src/ingress/client_packet_dispatcher.cpp
    ../apps/gateway/src/ingress/gateway_connection_registry.cpp
    ../apps/gateway/src/ingress/session_admission_service.cpp
)
target_include_directories(gateway_session_resume_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/gateway/include
)
target_compile_definitions(gateway_session_resume_tests PRIVATE
    APOLLO_GATEWAY_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)
target_link_libraries(gateway_session_resume_tests PRIVATE
    apollo
    apollo::base
    apollo::core
    apollo::runtime
    apollo::net_tcp
    apollo::net_protocol
    apollo::net_link
    apollo_net_tls
    apollo_net_ws
    apollo::net_contract_wire
    apollo::net_linktrace
    apollo_net_linkrpc
    apollo_protocol
)
if(WIN32)
    target_link_libraries(gateway_session_resume_tests PRIVATE ws2_32)
    target_compile_definitions(gateway_session_resume_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(gateway_session_resume_tests PRIVATE /utf-8)
endif()
add_test(NAME GatewaySessionResumeTests COMMAND gateway_session_resume_tests)
endif()

# ========== Gateway Base 激活面测试 (L53 登录串联第三环：Base 经 database 激活) ==========
# 真 Base PlayerStore M1 受理面宿主（StoreHost 纯 store 位）+ 网关拨号：
# 认证 → 激活 + 绑定两跳（锚点 Online + SessionBinding 同号同网关身份）、
# 无档案拒绝（面在场 = 错误显形）、面关退容忍（旧语义）、死端口 fail-closed。
if(TARGET apollo_app_services)
add_executable(gateway_base_activation_tests
    test_gateway_base_activation.cpp
    ../apps/gateway/src/gateway_server.cpp
    ../apps/gateway/src/gateway_control_server.cpp
    ../apps/gateway/src/m1_egress.cpp
    ../apps/gateway/src/m1_route_client.cpp
    ../apps/gateway/src/m1_base_client.cpp
    ../apps/gateway/src/m1_base_client.cpp
    ../apps/gateway/include/gateway/session_manager.cpp
    ../apps/gateway/src/ingress/client_ingress_server.cpp
    ../apps/gateway/src/ingress/tcp_client_ingress_server.cpp
    ../apps/gateway/src/ingress/ws_client_ingress_server.cpp
    ../apps/gateway/src/ingress/ingress_fan_in.cpp
    ../apps/gateway/src/ingress/client_packet_dispatcher.cpp
    ../apps/gateway/src/ingress/gateway_connection_registry.cpp
    ../apps/gateway/src/ingress/session_admission_service.cpp
)
target_include_directories(gateway_base_activation_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/gateway/include
)
target_compile_definitions(gateway_base_activation_tests PRIVATE
    APOLLO_GATEWAY_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
    APOLLO_TEST_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)
target_link_libraries(gateway_base_activation_tests PRIVATE
    apollo
    apollo::base
    apollo::core
    apollo::runtime
    apollo_app_services
    apollo::net_tcp
    apollo::net_protocol
    apollo::net_link
    apollo_net_tls
    apollo_net_ws
    apollo::net_contract_wire
    apollo::net_linktrace
    apollo_net_linkrpc
    apollo::net_discovery
    apollo_protocol
)
if(WIN32)
    target_link_libraries(gateway_base_activation_tests PRIVATE ws2_32)
    target_compile_definitions(gateway_base_activation_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(gateway_base_activation_tests PRIVATE /utf-8)
endif()
add_test(NAME GatewayBaseActivationTests COMMAND gateway_base_activation_tests)
endif()

# ========== Interfaces HTTP/HTTPS 入口真接线测试 (L45 批④) ==========
# 平台/渠道回调入口（localhost 实 socket）：HTTP 路由 + 专用 ws 升级路由
# 路由隔离（升级路由不降级、http 路由不受理升级）+ 预算面（8KiB 头 →
# 431、1MiB 体 → 413）+ HTTPS 底座（证书夹具现场自签）+ 并行监听互斥
# 号段 + 端口冲突显形。只依赖 net M1 三件（link/tcp+tls/ws），无协议
# 门控。Windows 下 link TCP/TLS 底座不进 M1 批，用例本体空转。
add_executable(interfaces_http_tests
    test_interfaces_http.cpp
    ../apps/interfaces/src/http_ingress.cpp
)
target_include_directories(interfaces_http_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/interfaces/include
)
target_link_libraries(interfaces_http_tests PRIVATE
    apollo::core
    apollo::net_link
    apollo_net_tls
    apollo_net_ws
)
if(WIN32)
    target_link_libraries(interfaces_http_tests PRIVATE ws2_32)
    target_compile_definitions(interfaces_http_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(interfaces_http_tests PRIVATE /utf-8)
endif()
add_test(NAME InterfacesHttpTests COMMAND interfaces_http_tests)

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

# ========== Orders Tests (L63 批⑯) ==========
# 订单/通知可靠受理与持久待办（经 apollo::game_orders 链接真实模块目标）
if(TARGET apollo::game_orders)
add_executable(orders_tests test_orders.cpp)
target_link_libraries(orders_tests PRIVATE
    apollo
    apollo::base
    apollo::core
    apollo::game_orders
)
if(WIN32)
    target_link_libraries(orders_tests PRIVATE ws2_32)
    target_compile_definitions(orders_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(orders_tests PRIVATE /utf-8)
endif()
add_test(NAME OrdersTests COMMAND orders_tests)
endif()

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
    ${CMAKE_SOURCE_DIR}/modules/game/base/include
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
    ${CMAKE_SOURCE_DIR}/modules/game/base/include
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
    ../modules/game/cell/src/avatar.cpp
    # Game AttrSync (scene.cpp attrsync wiring, 2026-10-14 批)
    ../modules/game/attrsync/src/change_history.cpp
    ../modules/game/attrsync/src/viewer_attr_state.cpp
    ../modules/game/attrsync/src/delta_builder.cpp
    ../modules/game/attrsync/src/entity_attrs.cpp
    ../modules/game/attrsync/src/attr_sync_manager.cpp
    ../modules/game/base/src/avatar.cpp
)
target_include_directories(scene_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/modules/core/include
    ${CMAKE_SOURCE_DIR}/modules/game/core/include
    ${CMAKE_SOURCE_DIR}/modules/game/cell/include
    ${CMAKE_SOURCE_DIR}/modules/game/world/include
    ${CMAKE_SOURCE_DIR}/modules/game/base/include
    ${CMAKE_SOURCE_DIR}/modules/game/session/include
    ${CMAKE_SOURCE_DIR}/modules/game/attrsync/include
    ${CMAKE_SOURCE_DIR}/include
    # scene.cpp 直编含 Scene 脚本桥头（Scene 桥批起）
    ${CMAKE_SOURCE_DIR}/modules/script/include
)
target_link_libraries(scene_tests PRIVATE apollo apollo::base apollo::core apollo::script)
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
    ${CMAKE_SOURCE_DIR}/modules/game/cell/include
    ${CMAKE_SOURCE_DIR}/modules/game/world/include
    ${CMAKE_SOURCE_DIR}/modules/game/battle/include
    ${CMAKE_SOURCE_DIR}/modules/game/base/include
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


# ========== World Role Tests (L27 共用二进制分角色) ==========
# --role 解析：缺省 manager（管理端）/ --role=cell 执行端受理 / 未知与缺值
# 显式拒绝 / 重复后到覆盖（header-only 纯解析直测，无网络依赖）
add_executable(world_role_tests
    test_world_role.cpp
)
target_include_directories(world_role_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/world/include
)
if(MSVC)
    target_compile_options(world_role_tests PRIVATE /utf-8)
endif()
add_test(NAME WorldRoleTests COMMAND world_role_tests)


# ========== Apollo CLI Tests (P0.2 L36 项目初始化唯一入口) ==========
# init 子命令：help 三形态/无参用法码、未知子命令与缺参拒绝、空目录骨架
# 生成（目录占位 + README + .gitignore + 项目标记）、非空目录/已存在文件/
# 空 path/目录穿越显式拒绝且不动原有内容、空已存在目录受理、二次 init 拒收、
# 相对 path 按调用者 cwd 解析（header-only 分派直测，无网络依赖）
add_executable(apollo_cli_tests
    test_apollo_cli.cpp
)
target_include_directories(apollo_cli_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/apolloc/include
)
if(MSVC)
    target_compile_options(apollo_cli_tests PRIVATE /utf-8)
endif()
add_test(NAME ApolloCliTests COMMAND apollo_cli_tests)


# ========== Cell Acceptance Tests (P3-2 批 B, ADR-015(a)/016/017) ==========
# WorldApp 受理面：意图信封队列（FIFO/顶格拒收/两级水位滞回）+ WorldApp 受理点
# 接线（维护闸/受理回执/Pong/tick 边界 drain/过载回错误包）。直编协议源惯例
# 同 apps/world（协议源直编习语）。
if(TARGET apollo::game_core AND TARGET apollo::game_world AND TARGET apollo::runtime)
add_executable(world_app_acceptance_tests
    test_world_app_acceptance.cpp
    ../apps/world/src/world_app.cpp
    ../apps/world/src/cell_app.cpp
    ../apps/world/src/m1_link_server.cpp
    ../modules/protocol/src/messages.cpp
    ../modules/protocol/src/codec.cpp
)
target_include_directories(world_app_acceptance_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/world/include
    ${CMAKE_SOURCE_DIR}/modules/protocol/include
    ${CMAKE_SOURCE_DIR}/modules/runtime/include
    ${CMAKE_SOURCE_DIR}/modules/script/include
)
target_compile_definitions(world_app_acceptance_tests PRIVATE
    APOLLO_WORLD_APP_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)
target_link_libraries(world_app_acceptance_tests PRIVATE
    apollo apollo::base apollo::core apollo::game_core apollo::game_world apollo::script apollo::runtime
    apollo::net_link apollo::net_contract_wire apollo::net_linktrace)
if(WIN32)
    target_link_libraries(world_app_acceptance_tests PRIVATE ws2_32)
    target_compile_definitions(world_app_acceptance_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(world_app_acceptance_tests PRIVATE /utf-8)
endif()
add_test(NAME WorldAppAcceptanceTests COMMAND world_app_acceptance_tests)
endif()

# ========== Cell Kernel Tests (L98 首切片：执行/管理分离) ==========
# 执行内核 CellApp 独立宿主（tests/test_cell_app.cpp）：接缝（IM1Egress +
# pre/post tick 钩子）即断言面——只编 cell_app.cpp + 协议源，不编 WorldApp /
# M1LinkServer（内核对传输零依赖的证明面）。覆盖：维护闸 / tick 边界执行 /
# 延时应答 / 顶格拒收 / 停机尾扫 / 双内核隔离。route json 宏有 #ifndef 空串
# 兜底（内核不拨号）。
if(TARGET apollo::game_core AND TARGET apollo::game_world AND TARGET apollo::runtime)
add_executable(cell_app_tests
    test_cell_app.cpp
    ../apps/world/src/cell_app.cpp
    ../modules/protocol/src/messages.cpp
    ../modules/protocol/src/codec.cpp
)
target_include_directories(cell_app_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/world/include
    ${CMAKE_SOURCE_DIR}/modules/protocol/include
    ${CMAKE_SOURCE_DIR}/modules/runtime/include
    ${CMAKE_SOURCE_DIR}/modules/script/include
)
target_link_libraries(cell_app_tests PRIVATE
    apollo apollo::base apollo::core apollo::game_core apollo::game_world apollo::script apollo::runtime
    apollo::net_link apollo::net_linktrace)
if(WIN32)
    target_link_libraries(cell_app_tests PRIVATE ws2_32)
    target_compile_definitions(cell_app_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(cell_app_tests PRIVATE /utf-8)
endif()
add_test(NAME CellAppTests COMMAND cell_app_tests)
endif()

# ========== WorldApp M1 Link Tests (M1 上层接线批 2b-ii) ==========
# WorldApp M1 接收端：契约意图族 924-928 经 InterServerLink 真 TCP 上行 → 契约
# wire 解包 → legacy 帧镜像 → 受理队列 → tick 边界执行 → 延时应答 929
# （request_id 关联，code 0/2101）；字段级非法/域越权/未知 id 拒收面。
# 直编协议源惯例同 world_app_acceptance_tests；客户端端 = 裸 LinkManager。
if(TARGET apollo::game_core AND TARGET apollo::game_world AND TARGET apollo::runtime)
add_executable(world_app_m1_link_tests
    test_world_app_m1_link.cpp
    ../apps/world/src/world_app.cpp
    ../apps/world/src/cell_app.cpp
    ../apps/world/src/m1_link_server.cpp
    ../modules/protocol/src/messages.cpp
    ../modules/protocol/src/codec.cpp
)
target_include_directories(world_app_m1_link_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/world/include
    ${CMAKE_SOURCE_DIR}/modules/protocol/include
    ${CMAKE_SOURCE_DIR}/modules/runtime/include
    ${CMAKE_SOURCE_DIR}/modules/script/include
)
target_compile_definitions(world_app_m1_link_tests PRIVATE
    APOLLO_WORLD_APP_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)
target_link_libraries(world_app_m1_link_tests PRIVATE
    apollo apollo::base apollo::core apollo::game_core apollo::game_world apollo::script
    apollo::runtime apollo::net_link apollo::net_contract_wire apollo::net_linktrace)
if(WIN32)
    target_link_libraries(world_app_m1_link_tests PRIVATE ws2_32)
    target_compile_definitions(world_app_m1_link_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(world_app_m1_link_tests PRIVATE /utf-8)
endif()
add_test(NAME WorldAppM1LinkTests COMMAND world_app_m1_link_tests)
endif()

# ========== Gateway M1 Egress Tests (M1 上层接线批 2b-iii) ==========
# gateway M1 出口：legacy 世界帧 → 契约 wire 前递（924-928，request_id 关联）
# → 真 WorldApp M1 接收端 → 延时应答 929 回流 → pending 关联回收/超时清扫；
# 投递箱溢出/不可前递族（断连通知等）落地观察面；真客户端 socket → gateway
# 入口 → WorldApp M1 → 应答回流全链 e2e。gateway 与 cell 两侧直编（缝工厂两侧
# 真实现），Windows 下 link TCP 底座不进 M1 批，用例本体空转。
if(TARGET apollo::game_core AND TARGET apollo::game_world AND TARGET apollo::runtime)
add_executable(gateway_m1_egress_tests
    test_gateway_m1_egress.cpp
    ../apps/gateway/src/gateway_server.cpp
    ../apps/gateway/src/gateway_control_server.cpp
    ../apps/gateway/src/m1_egress.cpp
    ../apps/gateway/src/m1_route_client.cpp
    ../apps/gateway/src/m1_base_client.cpp
    ../apps/gateway/include/gateway/session_manager.cpp
    ../apps/gateway/src/ingress/client_ingress_server.cpp
    ../apps/gateway/src/ingress/tcp_client_ingress_server.cpp
    ../apps/gateway/src/ingress/ws_client_ingress_server.cpp
    ../apps/gateway/src/ingress/ingress_fan_in.cpp
    ../apps/gateway/src/ingress/client_packet_dispatcher.cpp
    ../apps/gateway/src/ingress/gateway_connection_registry.cpp
    ../apps/gateway/src/ingress/session_admission_service.cpp
    ../apps/world/src/world_app.cpp
    ../apps/world/src/cell_app.cpp
    ../apps/world/src/m1_link_server.cpp
    ../modules/protocol/src/messages.cpp
    ../modules/protocol/src/codec.cpp
)
target_include_directories(gateway_m1_egress_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/gateway/include
    ${CMAKE_SOURCE_DIR}/apps/world/include
    ${CMAKE_SOURCE_DIR}/modules/protocol/include
    ${CMAKE_SOURCE_DIR}/modules/runtime/include
    ${CMAKE_SOURCE_DIR}/modules/script/include
)
target_compile_definitions(gateway_m1_egress_tests PRIVATE
    APOLLO_GATEWAY_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
    APOLLO_WORLD_APP_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)
target_link_libraries(gateway_m1_egress_tests PRIVATE
    apollo apollo::base apollo::core apollo::game_core apollo::game_world apollo::script
    apollo::runtime apollo::net_tcp apollo::net_protocol apollo::net_link
    apollo_net_tls apollo_net_ws apollo::net_contract_wire apollo::net_linktrace apollo_net_linkrpc apollo_protocol)
if(WIN32)
    target_link_libraries(gateway_m1_egress_tests PRIVATE ws2_32)
    target_compile_definitions(gateway_m1_egress_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(gateway_m1_egress_tests PRIVATE /utf-8)
endif()
add_test(NAME GatewayM1EgressTests COMMAND gateway_m1_egress_tests)
endif()

# ========== Squad Wire E2E Tests (P2 真实上行/下行) ==========
# 小队副本全链真进程 e2e：客户端帧 → 真 gateway M1Egress（契约翻译 932）
# → 真 WorldApp M1 接收端（gameLoop 受理）→ SquadInstanceService 执行面
# → 结果/状态帧 → 契约 939 client_deliver → egress 客户端投递口。
# 断言事实源 = 投递口收到的协议帧（日志不算下行证据）。Windows 下 link
# TCP 底座不进 M1 批，用例本体空转。
if(TARGET apollo::game_core AND TARGET apollo::game_world AND TARGET apollo::runtime)
add_executable(squad_wire_e2e_tests
    test_squad_wire_e2e.cpp
    ../apps/gateway/src/m1_egress.cpp
    ../apps/gateway/src/m1_route_client.cpp
    ../apps/gateway/src/m1_base_client.cpp
    ../apps/world/src/world_app.cpp
    ../apps/world/src/cell_app.cpp
    ../apps/world/src/m1_link_server.cpp
    ../modules/protocol/src/messages.cpp
    ../modules/protocol/src/codec.cpp
)
target_include_directories(squad_wire_e2e_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/gateway/include
    ${CMAKE_SOURCE_DIR}/apps/world/include
    ${CMAKE_SOURCE_DIR}/modules/protocol/include
    ${CMAKE_SOURCE_DIR}/modules/runtime/include
    ${CMAKE_SOURCE_DIR}/modules/script/include
)
target_compile_definitions(squad_wire_e2e_tests PRIVATE
    APOLLO_TEST_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)
target_link_libraries(squad_wire_e2e_tests PRIVATE
    apollo apollo::base apollo::core apollo::game_core apollo::game_world apollo::script
    apollo::runtime apollo::net_tcp apollo::net_protocol apollo::net_link
    apollo_net_tls apollo_net_ws apollo::net_contract_wire apollo::net_linktrace apollo_net_linkrpc apollo_protocol)
if(WIN32)
    target_link_libraries(squad_wire_e2e_tests PRIVATE ws2_32)
    target_compile_definitions(squad_wire_e2e_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(squad_wire_e2e_tests PRIVATE /utf-8)
endif()
add_test(NAME SquadWireE2eTests COMMAND squad_wire_e2e_tests)

add_executable(world_drain_tests
    test_world_drain.cpp
    ../apps/gateway/src/m1_egress.cpp
    ../apps/gateway/src/m1_route_client.cpp
    ../apps/gateway/src/m1_base_client.cpp
    ../apps/world/src/world_app.cpp
    ../apps/world/src/cell_app.cpp
    ../apps/world/src/m1_link_server.cpp
    ../modules/protocol/src/messages.cpp
    ../modules/protocol/src/codec.cpp
)
target_include_directories(world_drain_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/gateway/include
    ${CMAKE_SOURCE_DIR}/apps/world/include
    ${CMAKE_SOURCE_DIR}/modules/protocol/include
    ${CMAKE_SOURCE_DIR}/modules/runtime/include
    ${CMAKE_SOURCE_DIR}/modules/script/include
)
target_compile_definitions(world_drain_tests PRIVATE
    APOLLO_TEST_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)
target_link_libraries(world_drain_tests PRIVATE
    apollo apollo::base apollo::core apollo::game_core apollo::game_world apollo::script
    apollo::runtime apollo::net_tcp apollo::net_protocol apollo::net_link
    apollo_net_tls apollo_net_ws apollo::net_contract_wire apollo::net_linktrace apollo_net_linkrpc apollo_protocol)
if(WIN32)
    target_link_libraries(world_drain_tests PRIVATE ws2_32)
    target_compile_definitions(world_drain_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(world_drain_tests PRIVATE /utf-8)
endif()
add_test(NAME WorldDrainTests COMMAND world_drain_tests)

# cell 注册受理面（L101 注册首切片；契约 960/961）：裸 960 上行 → WorldApp
# 注册表（代次裁决）→ 961 回执。不代替 cell 侧主动拨号生产接线（StoreDial
# 先例，后续切片）。
add_executable(cell_register_tests
    test_cell_register.cpp
    ../apps/world/src/world_app.cpp
    ../apps/world/src/cell_app.cpp
    ../apps/world/src/m1_link_server.cpp
    ../apps/world/src/cell_register_client.cpp
    ../modules/protocol/src/messages.cpp
    ../modules/protocol/src/codec.cpp
)
target_include_directories(cell_register_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/world/include
    ${CMAKE_SOURCE_DIR}/modules/protocol/include
    ${CMAKE_SOURCE_DIR}/modules/runtime/include
    ${CMAKE_SOURCE_DIR}/modules/script/include
)
target_compile_definitions(cell_register_tests PRIVATE
    APOLLO_TEST_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)
target_link_libraries(cell_register_tests PRIVATE
    apollo apollo::base apollo::core apollo::game_core apollo::game_world apollo::script
    apollo::runtime apollo::net_tcp apollo::net_protocol apollo::net_link
    apollo_net_tls apollo_net_ws apollo::net_contract_wire apollo::net_linktrace apollo_net_linkrpc apollo_protocol)
if(WIN32)
    target_link_libraries(cell_register_tests PRIVATE ws2_32)
    target_compile_definitions(cell_register_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(cell_register_tests PRIVATE /utf-8)
endif()
add_test(NAME CellRegisterTests COMMAND cell_register_tests)

# 更新连续性：真实客户端 TCP → Gateway → World，Lua 模块替换与 World
# 排空替换分别验收。不代替业务双版本解码、生产登录或跨 Gateway 恢复。
add_executable(update_continuity_tests
    test_update_continuity.cpp
    ../apps/gateway/src/gateway_server.cpp
    ../apps/gateway/src/gateway_control_server.cpp
    ../apps/gateway/include/gateway/session_manager.cpp
    ../apps/gateway/src/ingress/client_ingress_server.cpp
    ../apps/gateway/src/ingress/tcp_client_ingress_server.cpp
    ../apps/gateway/src/ingress/ws_client_ingress_server.cpp
    ../apps/gateway/src/ingress/ingress_fan_in.cpp
    ../apps/gateway/src/ingress/client_packet_dispatcher.cpp
    ../apps/gateway/src/ingress/gateway_connection_registry.cpp
    ../apps/gateway/src/ingress/session_admission_service.cpp
    ../apps/gateway/src/m1_egress.cpp
    ../apps/gateway/src/m1_route_client.cpp
    ../apps/gateway/src/m1_base_client.cpp
    ../apps/world/src/world_app.cpp
    ../apps/world/src/cell_app.cpp
    ../apps/world/src/m1_link_server.cpp
    ../modules/protocol/src/messages.cpp
    ../modules/protocol/src/codec.cpp
)
target_include_directories(update_continuity_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/gateway/include
    ${CMAKE_SOURCE_DIR}/apps/world/include
    ${CMAKE_SOURCE_DIR}/modules/protocol/include
    ${CMAKE_SOURCE_DIR}/modules/runtime/include
    ${CMAKE_SOURCE_DIR}/modules/script/include
)
target_compile_definitions(update_continuity_tests PRIVATE
    APOLLO_TEST_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
    APOLLO_GATEWAY_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)
target_link_libraries(update_continuity_tests PRIVATE
    apollo apollo::base apollo::core apollo::game_core apollo::game_world apollo::script
    apollo::runtime apollo::net_tcp apollo::net_protocol apollo::net_link
    apollo_net_tls apollo_net_ws apollo::net_contract_wire apollo::net_linktrace apollo_net_linkrpc apollo_protocol)
if(WIN32)
    target_link_libraries(update_continuity_tests PRIVATE ws2_32)
    target_compile_definitions(update_continuity_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(update_continuity_tests PRIVATE /utf-8)
endif()
add_test(NAME UpdateContinuityTests COMMAND update_continuity_tests)

# 网关服务端主动下行（L51 游戏 WS/WSS → Lua/场景接线与下行第一锤）：
# 真实 TCP/WS 客户端 → Gateway → World 建队 → World 939 回推 PARTY_RESULT
# → Gateway sendToClient → 真 socket。World drain/update_continuity 只用
# ClientFace 汇流点证下行到 egress，本批证网关自身到真实客户端 socket 的
# 全程下行（TCP + WS + 多会话寻址隔离）。与 update_continuity 同源列表。
add_executable(gateway_push_downlink_tests
    test_gateway_push_downlink.cpp
    ../apps/gateway/src/gateway_server.cpp
    ../apps/gateway/src/gateway_control_server.cpp
    ../apps/gateway/include/gateway/session_manager.cpp
    ../apps/gateway/src/ingress/client_ingress_server.cpp
    ../apps/gateway/src/ingress/tcp_client_ingress_server.cpp
    ../apps/gateway/src/ingress/ws_client_ingress_server.cpp
    ../apps/gateway/src/ingress/ingress_fan_in.cpp
    ../apps/gateway/src/ingress/client_packet_dispatcher.cpp
    ../apps/gateway/src/ingress/gateway_connection_registry.cpp
    ../apps/gateway/src/ingress/session_admission_service.cpp
    ../apps/gateway/src/m1_egress.cpp
    ../apps/gateway/src/m1_route_client.cpp
    ../apps/gateway/src/m1_base_client.cpp
    ../apps/world/src/world_app.cpp
    ../apps/world/src/cell_app.cpp
    ../apps/world/src/m1_link_server.cpp
    ../modules/protocol/src/messages.cpp
    ../modules/protocol/src/codec.cpp
)
target_include_directories(gateway_push_downlink_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/gateway/include
    ${CMAKE_SOURCE_DIR}/apps/world/include
    ${CMAKE_SOURCE_DIR}/modules/protocol/include
    ${CMAKE_SOURCE_DIR}/modules/runtime/include
    ${CMAKE_SOURCE_DIR}/modules/script/include
)
target_compile_definitions(gateway_push_downlink_tests PRIVATE
    APOLLO_TEST_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
    APOLLO_GATEWAY_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)
target_link_libraries(gateway_push_downlink_tests PRIVATE
    apollo apollo::base apollo::core apollo::game_core apollo::game_world apollo::script
    apollo::runtime apollo::net_tcp apollo::net_protocol apollo::net_link
    apollo_net_tls apollo_net_ws apollo::net_contract_wire apollo::net_linktrace apollo_net_linkrpc apollo_protocol)
if(WIN32)
    target_link_libraries(gateway_push_downlink_tests PRIVATE ws2_32)
    target_compile_definitions(gateway_push_downlink_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(gateway_push_downlink_tests PRIVATE /utf-8)
endif()
add_test(NAME GatewayPushDownlinkTests COMMAND gateway_push_downlink_tests)
endif()

# ========== Logger Retrieval Tests (ADR-013 批 L3) ==========
# LoggerApp 检索面 v1：结构化行读侧（写侧契约镜像）、proc/level/ts/cat
# 过滤、目录摄取全链（FileAppender structuredOutput → 落盘 → ingest）、
# push 接缝空推（ADR-013 留接口不实现）。logger 目标无门控条件
# （apollo::core + apollo::runtime 恒在，同 machined/supervisor_tests）。
add_executable(logger_tests
    test_logger_retrieval.cpp
    ../apps/logger/src/archive_writer.cpp
    ../apps/logger/src/bi_tap.cpp
    ../apps/logger/src/logger_app.cpp
    ../apps/logger/src/logger_serve.cpp
    ../apps/logger/src/structured_line.cpp
)
target_include_directories(logger_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/logger/include
)
# 契约路由表 JSON（serve 受理面装配事实源；生成物随仓）
target_compile_definitions(logger_tests PRIVATE
    APOLLO_LOGGER_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)
target_link_libraries(logger_tests PRIVATE apollo::core apollo_net_link apollo::net_contract_wire)
if(WIN32)
    target_link_libraries(logger_tests PRIVATE ws2_32)
    target_compile_definitions(logger_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(logger_tests PRIVATE /utf-8)
endif()
add_test(NAME LoggerRetrievalTests COMMAND logger_tests)

# ========== Archive Writer Tests (collector 归档滚动批，P3-3 移交余件③) ==========
# ArchiveWriter 按日滚动 + 保留份数清理（logging.md §5 collector 职责
# 「磁盘滚动」落地）：滚动语义沿用 FileAppender 既有（关旧开新当日本名，
# 不落 rename），日期缝注假日期强制跨日。无门控条件（logger 目标同口径）。
add_executable(archive_writer_tests
    test_archive_writer.cpp
    ../apps/logger/src/archive_writer.cpp
)
target_include_directories(archive_writer_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/logger/include
)
if(WIN32)
    target_compile_definitions(archive_writer_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(archive_writer_tests PRIVATE /utf-8)
endif()
add_test(NAME ArchiveWriterTests COMMAND archive_writer_tests)

# ========== Log Push Tests (L4 push 通道) ==========
# LogPushClient 发送面（有界队列断连降级 + 重连补投）× collector 受集面
# round-trip；PushAppender/LogManagerConfig.pushSink 接线面。Windows 只跑
# appender 面（link TCP 底座 POSIX only，同 link_tests 口径）。
add_executable(logpush_tests
    test_log_push.cpp
)
target_link_libraries(logpush_tests PRIVATE
    apollo_net_logpush
    apollo_net_link
    apollo::net_contract_wire
    apollo::core
)
target_compile_definitions(logpush_tests PRIVATE
    APOLLO_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)
if(WIN32)
    target_link_libraries(logpush_tests PRIVATE ws2_32)
    target_compile_definitions(logpush_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(logpush_tests PRIVATE /utf-8)
endif()
add_test(NAME LogPushTests COMMAND logpush_tests)

# Metrics Push Tests (P3-3 批：metrics admin 通道)
# MetricsPushClient 发送面（kDroppable 无队列周期推，最新者胜）× hub 受集
# 面 round-trip + render_exposition 文本面 + apps/admin 聚合/HTTP 出口面
# （MetricsHub last-wins/陈旧墓碑/体长上限；MetricsHttp 真端口 GET
# /metrics 往返）。Windows 只跑 render 面（link TCP 底座 POSIX only，
# 同 logpush_tests 口径）。
add_executable(metricspush_tests
    test_metrics_push.cpp
    ../apps/admin/src/metrics_hub.cpp
    ../apps/admin/src/metrics_http.cpp
)
target_include_directories(metricspush_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/admin/include
)
target_link_libraries(metricspush_tests PRIVATE
    apollo_net_metricspush
    apollo_net_link
    apollo::net_contract_wire
    apollo::core
)
target_compile_definitions(metricspush_tests PRIVATE
    APOLLO_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)
if(WIN32)
    target_link_libraries(metricspush_tests PRIVATE ws2_32)
    target_compile_definitions(metricspush_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(metricspush_tests PRIVATE /utf-8)
endif()
add_test(NAME MetricsPushTests COMMAND metricspush_tests)

# Alert Notifier Tests (P3-3 余项批：告警 notifier 最小面)
# 判定/静默/限速/出站纯逻辑面（§5.2 层 B 内核；webhook 随 curl 出站批）。
# 纯逻辑无平台依赖，Windows 同跑。
add_executable(alert_notifier_tests
    test_alert_notifier.cpp
    ../apps/admin/src/alert_notifier.cpp
)
target_include_directories(alert_notifier_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/admin/include
)
target_link_libraries(alert_notifier_tests PRIVATE apollo::core)
if(WIN32)
    target_link_libraries(alert_notifier_tests PRIVATE ws2_32)
endif()
if(MSVC)
    target_compile_options(alert_notifier_tests PRIVATE /utf-8)
endif()
add_test(NAME AlertNotifierTests COMMAND alert_notifier_tests)

# BI 分流 tap 测试（P3-3 余项批：cat 白名单/确定性采样/原文透传/
# 失败语义/解析面）——纯逻辑面，Windows 同跑
add_executable(bi_tap_tests
    test_bi_tap.cpp
    ../apps/logger/src/bi_tap.cpp
)
target_include_directories(bi_tap_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/logger/include
)
target_link_libraries(bi_tap_tests PRIVATE apollo::core)
if(WIN32)
    target_link_libraries(bi_tap_tests PRIVATE ws2_32)
endif()
if(MSVC)
    target_compile_options(bi_tap_tests PRIVATE /utf-8)
endif()
add_test(NAME BiTapTests COMMAND bi_tap_tests)

# Link RPC 关联原语测试（P4-4 批⓪：关联信封 round-trip / 并发不串号 /
# 超时摘表迟到弃 / 断连即败 / 坏信封拒收计数）
add_executable(linkrpc_tests
    test_link_rpc.cpp
)
target_link_libraries(linkrpc_tests PRIVATE
    apollo::net_linkrpc
    apollo_net_link
    apollo::net_contract_wire
)
target_compile_definitions(linkrpc_tests PRIVATE
    APOLLO_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)
if(WIN32)
    target_link_libraries(linkrpc_tests PRIVATE ws2_32)
    target_compile_definitions(linkrpc_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(linkrpc_tests PRIVATE /utf-8)
endif()
add_test(NAME LinkRpcTests COMMAND linkrpc_tests)

# Rest curl outbound tests (net-abstraction §5.10 出站裁决批)
# rest_client 真实现（curl 进 vcpkg、桩路径删除）× 出站目标白名单 SSRF
# 面（缺省全拒/精确/前导点后缀/IPv4 CIDR）× 重定向守卫 × 环回真请求。
# Windows 只跑纯白名单逻辑用例（link TCP 底座 POSIX only，同
# metricspush_tests 口径）。
find_package(CURL CONFIG REQUIRED)
add_executable(rest_curl_tests
    test_rest_curl.cpp
    ../modules/net/http/src/rest_client.cpp
)
target_link_libraries(rest_curl_tests PRIVATE
    CURL::libcurl
    apollo_net_link
    apollo::core
)
if(WIN32)
    target_link_libraries(rest_curl_tests PRIVATE ws2_32)
endif()
if(MSVC)
    target_compile_options(rest_curl_tests PRIVATE /utf-8)
    target_compile_definitions(rest_curl_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
add_test(NAME RestCurlTests COMMAND rest_curl_tests)

# Alert Webhook Tests (logging.md §5.2 批：告警 webhook 出站面)
# 四家模板渲染/加签 URL/队列面（纯逻辑，Windows 同跑）× 环回真 POST
# e2e × 重试退避 + fallback 回写 × stop 可中断。Windows 只跑纯逻辑
# 用例（link TCP 底座 POSIX only，同 metricspush_tests 口径）。
find_package(OpenSSL REQUIRED)
add_executable(alert_webhook_tests
    test_alert_webhook.cpp
    ../apps/admin/src/alert_webhook.cpp
    ../apps/admin/src/alert_notifier.cpp
)
target_include_directories(alert_webhook_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/admin/include
)
target_link_libraries(alert_webhook_tests PRIVATE
    apollo::net_http
    apollo_net_link
    OpenSSL::Crypto
    apollo::core
)
if(WIN32)
    target_link_libraries(alert_webhook_tests PRIVATE ws2_32)
endif()
if(MSVC)
    target_compile_options(alert_webhook_tests PRIVATE /utf-8)
    target_compile_definitions(alert_webhook_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
add_test(NAME AlertWebhookTests COMMAND alert_webhook_tests)

# trace 子集原语测试（P3-3 批 · 语义层：traceparent 往返/采样确定性/
# 932 信封往返与 wire 层互证/span 缝发射面/并发 span id 唯一）
add_executable(linktrace_tests
    test_link_trace.cpp
)
target_link_libraries(linktrace_tests PRIVATE
    apollo::net_linktrace
    apollo::net_contract_wire
    apollo::core
)
if(WIN32)
    target_link_libraries(linktrace_tests PRIVATE ws2_32)
    target_compile_definitions(linktrace_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(linktrace_tests PRIVATE /utf-8)
endif()
add_test(NAME LinkTraceTests COMMAND linktrace_tests)

# ========== RemoteEntityCall 四件套测试 ==========
# RemoteEntityRef / RemoteMethodSchema / RouteResolver / RpcTrace
add_executable(remoteentity_tests
    test_remote_entity.cpp
)
target_link_libraries(remoteentity_tests PRIVATE
    apollo::net_remoteentity
    apollo::core
    GTest::gtest
    GTest::gtest_main
)
if(WIN32)
    target_link_libraries(remoteentity_tests PRIVATE ws2_32)
    target_compile_definitions(remoteentity_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(remoteentity_tests PRIVATE /utf-8)
endif()
add_test(NAME RemoteEntityTests COMMAND remoteentity_tests)

# ========== Login M1 Admission Tests (P4-4 批①) ==========
# login 准入受理面 ↔ gateway 链路准入服务全链：真 codec 假会话规则收/拒
# 两面 + 对端缺席降级容忍。缝工厂两侧真实现直编（gateway_m1_egress_tests
# 惯例）；Windows 下 link TCP 底座不进本批，用例本体空转。
add_executable(login_m1_admission_tests
    test_login_m1_admission.cpp
    ../apps/login/src/m1_admission_server.cpp
    ../apps/gateway/src/ingress/session_admission_service.cpp
    ../apps/gateway/include/gateway/session_manager.cpp
    ../modules/protocol/src/messages.cpp
    ../modules/protocol/src/codec.cpp
)
target_include_directories(login_m1_admission_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/login/include
    ${CMAKE_SOURCE_DIR}/apps/gateway/include
    ${CMAKE_SOURCE_DIR}/modules/protocol/include
)
target_compile_definitions(login_m1_admission_tests PRIVATE
    APOLLO_LOGIN_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
    APOLLO_GATEWAY_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)
target_link_libraries(login_m1_admission_tests PRIVATE
    apollo::base apollo::core apollo::runtime
    apollo::net_link apollo::net_contract_wire apollo_net_linkrpc apollo_protocol)
if(WIN32)
    target_link_libraries(login_m1_admission_tests PRIVATE ws2_32)
    target_compile_definitions(login_m1_admission_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(login_m1_admission_tests PRIVATE /utf-8)
endif()
add_test(NAME LoginM1AdmissionTests COMMAND login_m1_admission_tests)

# BaseApp M1 base 面 × login base 客户端 e2e（P4-4 批③）：受理/应答/错误包
# 三路径 + 对端缺席降级。缝工厂两侧真实现直编（login_m1_admission_tests
# 惯例）；Windows 下 link TCP 底座不进本批，用例本体空转。
add_executable(base_m1_tests
    test_base_m1.cpp
    ../apps/common/src/player_store_m1.cpp
    ../apps/login/src/m1_base_client.cpp
)
target_include_directories(base_m1_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/common/include
    ${CMAKE_SOURCE_DIR}/apps/login/include
    ${CMAKE_SOURCE_DIR}/modules/protocol/include
)
target_compile_definitions(base_m1_tests PRIVATE
    APOLLO_BASEAPP_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
    APOLLO_LOGIN_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)
target_link_libraries(base_m1_tests PRIVATE
    apollo::base apollo::core apollo::runtime
    apollo::net_link apollo::net_contract_wire apollo_net_linkrpc apollo_protocol)
if(WIN32)
    target_link_libraries(base_m1_tests PRIVATE ws2_32)
    target_compile_definitions(base_m1_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(base_m1_tests PRIVATE /utf-8)
endif()
add_test(NAME BaseM1Tests COMMAND base_m1_tests)

# ========== GameConnection Facade Tests (net-abstraction.md §2/§3 L3) ==========
# 纯 net 层测试（只依赖 apollo::net_gameconnection + apollo::net_link），
# 无门控注册——OFF 树（GAME_MODULE=OFF）同样执行。
add_executable(gameconnection_tests test_game_connection.cpp)
target_link_libraries(gameconnection_tests PRIVATE
    apollo::net_gameconnection
    apollo::net_link
    GTest::gtest
    GTest::gtest_main
)
if(WIN32)
    target_link_libraries(gameconnection_tests PRIVATE ws2_32)
    target_compile_definitions(gameconnection_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(gameconnection_tests PRIVATE /utf-8)
endif()
add_test(NAME GameConnectionTests COMMAND gameconnection_tests)

# ========== Script Runtime Tests (scripting-lua.md §2/§4/§6) ==========
# Lua 沙盒宿主测试；无门控注册（script 模块零 apollo 依赖，OFF 树同样执行）。
add_executable(script_tests test_script_runtime.cpp)
# 生成器产物（contract.lua）装载面集成用例——随仓 golden 的绝对路径（四树
# 同源：CMAKE_SOURCE_DIR 指仓库根，产物已提交不依赖构建期生成）。
target_compile_definitions(script_tests PRIVATE
    APOLLO_GENERATED_CONTRACT_LUA="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract.lua"
)
target_link_libraries(script_tests PRIVATE
    apollo::script
    GTest::gtest
    GTest::gtest_main
)
if(WIN32)
    target_link_libraries(script_tests PRIVATE ws2_32)
    target_compile_definitions(script_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(script_tests PRIVATE /utf-8)
endif()
add_test(NAME ScriptTests COMMAND script_tests)

# ========== Script Async Tests (scripting-lua.md §8 + §7.2) ==========
# 异步底座测试（L1 交接 + L2 协程面 + 协程清单）；无门控注册（同
# script_tests——script 模块零 apollo 依赖，OFF 树同样执行）。
add_executable(async_tests test_script_async.cpp)
target_link_libraries(async_tests PRIVATE
    apollo::script
    GTest::gtest
    GTest::gtest_main
)
if(WIN32)
    target_link_libraries(async_tests PRIVATE ws2_32)
    target_compile_definitions(async_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(async_tests PRIVATE /utf-8)
endif()
add_test(NAME AsyncTests COMMAND async_tests)

# ========== Script Phase A Hot Swap Tests (scripting-lua.md §3.2) ==========
# 两段式热替换测试（prepare 工作线程预加载 + commit owning 面 O(1) 换表）；
# 无门控注册（同 script_tests——script 模块零 apollo 依赖，OFF 树同样执行）。
add_executable(phase_a_tests test_script_phase_a.cpp)
target_link_libraries(phase_a_tests PRIVATE
    apollo::script
    GTest::gtest
    GTest::gtest_main
)
if(WIN32)
    target_link_libraries(phase_a_tests PRIVATE ws2_32)
    target_compile_definitions(phase_a_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(phase_a_tests PRIVATE /utf-8)
endif()
add_test(NAME PhaseATests COMMAND phase_a_tests)

# current 版本指针持久化测试（§3.2 防坏版本崩溃循环）；无门控注册（同
# phase_a_tests——script 模块零 apollo 依赖，OFF 树同样执行）。
add_executable(version_store_tests test_script_version_store.cpp)
target_link_libraries(version_store_tests PRIVATE
    apollo::script
    GTest::gtest
    GTest::gtest_main
)
if(WIN32)
    target_link_libraries(version_store_tests PRIVATE ws2_32)
    target_compile_definitions(version_store_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(version_store_tests PRIVATE /utf-8)
endif()
add_test(NAME VersionStoreTests COMMAND version_store_tests)

# patch 目录读与 hash 校验测试（§3.2 Phase A 工程面）；无门控注册（同
# version_store_tests——script 模块零 apollo 依赖，OFF 树同样执行）。
add_executable(patch_loader_tests test_script_patch_loader.cpp)
target_link_libraries(patch_loader_tests PRIVATE
    apollo::script
    GTest::gtest
    GTest::gtest_main
)
if(WIN32)
    target_link_libraries(patch_loader_tests PRIVATE ws2_32)
    target_compile_definitions(patch_loader_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(patch_loader_tests PRIVATE /utf-8)
endif()
add_test(NAME PatchLoaderTests COMMAND patch_loader_tests)

# Phase C 灰度观察期 canary 测试（§3.2 防坏版本灰度回路）；无门控注册（同
# patch_loader_tests——script 模块零 apollo 依赖，OFF 树同样执行）。
add_executable(canary_tests test_script_canary.cpp)
target_link_libraries(canary_tests PRIVATE
    apollo::script
    GTest::gtest
    GTest::gtest_main
)
if(WIN32)
    target_link_libraries(canary_tests PRIVATE ws2_32)
    target_compile_definitions(canary_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(canary_tests PRIVATE /utf-8)
endif()
add_test(NAME CanaryTests COMMAND canary_tests)

# 灰度投递编排测试（§3.2 Phase A 工程面：多 runtime 分步投递 + Phase C 判定）；
# 无门控注册（同 canary_tests——script 模块零 apollo 依赖，OFF 树同样执行）。
add_executable(rollout_tests test_script_rollout.cpp)
target_link_libraries(rollout_tests PRIVATE
    apollo::script
    GTest::gtest
    GTest::gtest_main
)
if(WIN32)
    target_link_libraries(rollout_tests PRIVATE ws2_32)
    target_compile_definitions(rollout_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(rollout_tests PRIVATE /utf-8)
endif()
add_test(NAME RolloutTests COMMAND rollout_tests)

# 宿主启动装载·handler 面同步闸（§3.4 玩法 Lua 批第一片）：shipped handler
# 占位模块必须满足 golden 契约装载闸——apollo_gen 新增 reflect 消息而
# handler 面没跟上时此测红。无门控注册（apollo::script 零 apollo 依赖，
# OFF 树同样执行）。
add_executable(contract_handler_tests test_contract_handlers.cpp)
target_compile_definitions(contract_handler_tests PRIVATE
    APOLLO_WORLD_APP_CONTRACT_HANDLERS_LUA="${CMAKE_SOURCE_DIR}/apps/world/scripts/contract_handlers.lua"
    APOLLO_GENERATED_CONTRACT_LUA="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract.lua"
)
target_link_libraries(contract_handler_tests PRIVATE
    apollo::script
    GTest::gtest
    GTest::gtest_main
)
if(WIN32)
    target_link_libraries(contract_handler_tests PRIVATE ws2_32)
    target_compile_definitions(contract_handler_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(contract_handler_tests PRIVATE /utf-8)
endif()
add_test(NAME ContractHandlerTests COMMAND contract_handler_tests)

# ========== Lua Bytecode Cache Tests (scripting-lua.md §3.5) ==========
# 缓存面测试（manifest 三元组校验 + 回退源码编译）；无门控注册（同
# script_tests——script 模块零 apollo 依赖，OFF 树同样执行）。
add_executable(bytecode_cache_tests test_bytecode_cache.cpp)
target_link_libraries(bytecode_cache_tests PRIVATE
    apollo::script
    GTest::gtest
    GTest::gtest_main
)
if(WIN32)
    target_link_libraries(bytecode_cache_tests PRIVATE ws2_32)
    target_compile_definitions(bytecode_cache_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(bytecode_cache_tests PRIVATE /utf-8)
endif()
add_test(NAME BytecodeCacheTests COMMAND bytecode_cache_tests)

# lua_precompile 工具冒烟：跑通 + manifest 行格式自洽（工具↔运行期契约最小闸）。
add_test(NAME BytecodeCacheToolSmoke
    COMMAND ${CMAKE_COMMAND}
        -Dexe=$<TARGET_FILE:lua_precompile>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/lua_precompile_smoke.cmake)

# ========== Lua State Introspection Tests (scripting-lua.md §7.2) ==========
# 状态面只读内省（GC 面 + 热替换审计环 + env 快照）；无门控注册（同
# script_tests——script 模块零 apollo 依赖，OFF 树同样执行）。
add_executable(script_state_tests test_script_state.cpp)
target_link_libraries(script_state_tests PRIVATE
    apollo::script
    GTest::gtest
    GTest::gtest_main
)
if(WIN32)
    target_link_libraries(script_state_tests PRIVATE ws2_32)
    target_compile_definitions(script_state_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(script_state_tests PRIVATE /utf-8)
endif()
add_test(NAME ScriptStateTests COMMAND script_state_tests)

# OnlineDirectory M1 控制面 × login/gateway 客户端面 e2e（P4-4 批④）：双拨入方
# fan-out + 真 OnlineDirectory 分派面（AssignWorld/ResolveRoute 全真目录裁决）
# + 对端缺席降级。缝工厂两侧真实现直编；Windows 下 link TCP 底座不进本
# 批，用例本体空转。
if(TARGET apollo::game_session AND TARGET apollo_protocol)
add_executable(baseappmgr_m1_control_tests
    test_baseappmgr_m1_control.cpp
    ../apps/common/src/directory_m1.cpp
    ../apps/common/src/online_directory.cpp
    ../apps/login/src/m1_base_client.cpp
    ../apps/gateway/src/m1_route_client.cpp
    ../apps/gateway/src/m1_base_client.cpp
)
target_include_directories(baseappmgr_m1_control_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/common/include
    ${CMAKE_SOURCE_DIR}/apps/common/include
    ${CMAKE_SOURCE_DIR}/apps/login/include
    ${CMAKE_SOURCE_DIR}/apps/gateway/include
    ${CMAKE_SOURCE_DIR}/modules/protocol/include
)
target_compile_definitions(baseappmgr_m1_control_tests PRIVATE
    APOLLO_BASEAPPMGR_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
    APOLLO_LOGIN_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
    APOLLO_GATEWAY_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)
target_link_libraries(baseappmgr_m1_control_tests PRIVATE
    apollo apollo::base apollo::core apollo::game_session apollo::runtime
    apollo::net_link apollo::net_contract_wire apollo_net_linkrpc apollo_protocol)
if(WIN32)
    target_link_libraries(baseappmgr_m1_control_tests PRIVATE ws2_32)
    target_compile_definitions(baseappmgr_m1_control_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(baseappmgr_m1_control_tests PRIVATE /utf-8)
endif()
add_test(NAME OnlineDirectoryM1ControlTests COMMAND baseappmgr_m1_control_tests)

# ========== Attribute Sync Delta Pipeline Tests ==========
# ChangeHistory / ViewerAttrState / AttrDelta encoding / DeltaBuilder
# + Scene × AttrSyncManager AOI 事件接线（§4.1，2026-10-14 批）
add_executable(attrsync_tests
    test_attr_sync.cpp
    test_attr_sync_manager.cpp
    test_attr_frame_codec.cpp
    test_attr_ack_degrade.cpp
    test_scene_attrsync_wiring.cpp
    test_attr_sync_scheduler.cpp
    test_attr_sync_change_sink.cpp
    test_attr_sync_attr_hook.cpp
    test_attr_domain_mapping.cpp
)
target_link_libraries(attrsync_tests PRIVATE
    apollo::game_attrsync
    apollo::game_core
    apollo::game_world
    GTest::gtest
    GTest::gtest_main
)
# 契约 sync 掩码 → 域映射测试读生成物 canonical 位定义（attrsync 镜像常量的漂移钉）
target_include_directories(attrsync_tests PRIVATE ${CMAKE_SOURCE_DIR}/sdks/cpp/generated)
if(WIN32)
    target_link_libraries(attrsync_tests PRIVATE ws2_32)
    target_compile_definitions(attrsync_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(attrsync_tests PRIVATE /utf-8)
endif()
add_test(NAME AttrSyncTests COMMAND attrsync_tests)

# ========== Scene × Script Runtime Host Wiring Tests ==========
# §5.1 属性钩子窗口适配桥 + §7.1 admin attach 执行队列（scripting-lua 宿主接线批）
add_executable(scene_script_tests
    test_scene_attr_hook_bridge.cpp
    test_scene_debug_eval_queue.cpp
)
target_link_libraries(scene_script_tests PRIVATE
    apollo::game_world
    apollo::game_attrsync
    apollo::game_core
    apollo::script
    GTest::gtest
    GTest::gtest_main
)
if(WIN32)
    target_link_libraries(scene_script_tests PRIVATE ws2_32)
    target_compile_definitions(scene_script_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(scene_script_tests PRIVATE /utf-8)
endif()
add_test(NAME SceneScriptTests COMMAND scene_script_tests)
endif()

# ========== Cell Script Host Tests (admin attach transport 批 A) ==========
# cell 脚本宿主接线（scripting-lua §2 一状态一逻辑线程 + §5.1 Scene 桥）：
# boot 源加载（成功执行 / 缺失不停机）+ 未挂 scene 幂等补挂（新 scene 下一拍
# 生效）+ debug eval 队列在宿主 tick 窗口的回传。直编协议源习语同
# world_app_acceptance_tests；OFF 树无 game_core/game_world/runtime 故带目标门控。
if(TARGET apollo::game_core AND TARGET apollo::game_world AND TARGET apollo::runtime
   AND TARGET apollo::script)
add_executable(world_app_script_host_tests
    test_world_app_script_host.cpp
    ../apps/world/src/world_app.cpp
    ../apps/world/src/cell_app.cpp
    ../apps/world/src/m1_link_server.cpp
    ../modules/protocol/src/messages.cpp
    ../modules/protocol/src/codec.cpp
)
target_include_directories(world_app_script_host_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/world/include
    ${CMAKE_SOURCE_DIR}/modules/protocol/include
    ${CMAKE_SOURCE_DIR}/modules/runtime/include
    ${CMAKE_SOURCE_DIR}/modules/script/include
)
target_compile_definitions(world_app_script_host_tests PRIVATE
    APOLLO_WORLD_APP_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)
target_link_libraries(world_app_script_host_tests PRIVATE
    apollo apollo::base apollo::core apollo::game_core apollo::game_world apollo::script
    apollo::runtime apollo::net_link apollo::net_contract_wire apollo::net_linktrace)
if(WIN32)
    target_link_libraries(world_app_script_host_tests PRIVATE ws2_32)
    target_compile_definitions(world_app_script_host_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(world_app_script_host_tests PRIVATE /utf-8)
endif()
add_test(NAME WorldAppScriptHostTests COMMAND world_app_script_host_tests)
endif()

# Runtime sources host shared services even when executable apps are disabled.
foreach(apollo_hosted_target world_app_acceptance_tests world_app_m1_link_tests world_app_script_host_tests cell_app_tests gateway_m1_egress_tests squad_wire_e2e_tests world_drain_tests cell_register_tests update_continuity_tests gateway_push_downlink_tests fleet_rehome_chain_tests login_m1_admission_tests base_m1_tests baseappmgr_m1_control_tests)
    if(TARGET ${apollo_hosted_target})
        # apollo_app_services 仅在 game 模块开启（⇒ apps/common 加入）时存在：
        # 其守卫是 `TARGET apollo_protocol AND apollo::game_session`，而
        # APOLLO_BUILD_GAME_MODULE 默认 OFF（CI 的 ci.yml 只开 TESTS、关
        # EXAMPLES，未开 game）。此时 game/session 未建 ⇒ 守卫假 ⇒
        # apps/common 不加入 ⇒ apollo_app_services 目标不存在，裸链会退化成
        # `-lapollo_app_services`（非目标全路径）在 macOS 报「library not
        # found」。base_m1_tests / login_m1_admission_tests 无条件
        # add_executable 且直编所需源、不依赖该库符号（206bbfb4 前本就绿），
        # 故 game-OFF 时守卫跳过；game-ON 的宿主壳照常链接。
        if(TARGET apollo_app_services)
            target_link_libraries(${apollo_hosted_target} PRIVATE apollo_app_services)
        endif()
        # hosted 壳编进 world_app.cpp/login_server.cpp——其头面直含 discovery
        # （编队面 re-home），include 路径与库经此传递
        target_link_libraries(${apollo_hosted_target} PRIVATE apollo::net_discovery)
    endif()
    # world_app.cpp 编入的目标需小队受理面符号（P2）
    if(TARGET ${apollo_hosted_target} AND TARGET apollo_squad_instance)
        target_link_libraries(${apollo_hosted_target} PRIVATE apollo_squad_instance)
    endif()
endforeach()

if(TARGET apollo::game_world AND TARGET apollo_app_services)
add_executable(hosted_services_tests
    test_hosted_services.cpp
    ../apps/world/src/world_app.cpp
    ../apps/world/src/cell_app.cpp
    ../apps/world/src/m1_link_server.cpp
    ../apps/login/src/login_server.cpp
    ../apps/login/src/m1_admission_server.cpp
    ../apps/login/src/m1_base_client.cpp
    ../apps/gateway/src/m1_route_client.cpp
    ../apps/gateway/src/m1_base_client.cpp)
target_include_directories(hosted_services_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/world/include
    ${CMAKE_SOURCE_DIR}/apps/login/include
    ${CMAKE_SOURCE_DIR}/apps/gateway/include)
target_compile_definitions(hosted_services_tests PRIVATE
    APOLLO_TEST_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json")
target_link_libraries(hosted_services_tests PRIVATE apollo_app_services
    apollo::game_world apollo::script apollo::net_linktrace apollo::net_discovery)
if(TARGET apollo_squad_instance)
    target_link_libraries(hosted_services_tests PRIVATE apollo_squad_instance)
endif()
add_test(NAME HostedServicesTests COMMAND hosted_services_tests)
endif()

# ========== Fleet Re-home Chain Tests (P3-1 真实进程链验收) ==========
# 编队面 re-home：真 LoginServer（目录 owner 三面——镜像/恢复 intake/死亡
# 订阅）+ 真 WorldApp（FleetReporter 全量重报）+ 测试侧 machined 替身，
# 全 UDP 回环。断言事实源 = 门闸判定 / 目录条目态 / 镜像投影（日志不算
# 证据）。Windows 下 UDP 面用例本体空转。
if(TARGET apollo::game_world AND TARGET apollo_app_services)
add_executable(fleet_rehome_chain_tests
    test_fleet_rehome_chain.cpp
    ../apps/world/src/world_app.cpp
    ../apps/world/src/cell_app.cpp
    ../apps/world/src/m1_link_server.cpp
    ../apps/login/src/login_server.cpp
    ../apps/login/src/m1_admission_server.cpp
    ../apps/login/src/m1_base_client.cpp
    ../apps/gateway/src/m1_route_client.cpp
    ../apps/gateway/src/m1_base_client.cpp)
target_include_directories(fleet_rehome_chain_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/world/include
    ${CMAKE_SOURCE_DIR}/apps/login/include
    ${CMAKE_SOURCE_DIR}/apps/gateway/include)
target_compile_definitions(fleet_rehome_chain_tests PRIVATE
    APOLLO_TEST_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json")
target_link_libraries(fleet_rehome_chain_tests PRIVATE apollo_app_services
    apollo::game_world apollo::script apollo::net_linktrace apollo::net_discovery)
if(TARGET apollo_squad_instance)
    target_link_libraries(fleet_rehome_chain_tests PRIVATE apollo_squad_instance)
endif()
add_test(NAME FleetRehomeChainTests COMMAND fleet_rehome_chain_tests)
endif()

# ========== Migration Chain Tests (P3-5 跨 World 迁移真实进程链) ==========
# 真 LoginServer（目录 owner + 942 编排受理面）+ 两真 WorldApp（源/目标
# PlayerStore M1 面 + 933/935/936/937 迁移 wire 真拨）。断言：源端冻结形态、
# 导出/目标确认（锚点 + 档案一致）、路由切换（改指 + 代次 + 立即推镜像）、
# 周期重报不回卷、失败恢复两形态（源不可达干净拒绝 / 目标不可达如实停留）。
if(TARGET apollo::game_world AND TARGET apollo_app_services)
add_executable(migration_chain_tests
    test_migration_chain.cpp
    ../apps/world/src/world_app.cpp
    ../apps/world/src/cell_app.cpp
    ../apps/world/src/m1_link_server.cpp
    ../apps/login/src/login_server.cpp
    ../apps/login/src/m1_admission_server.cpp
    ../apps/login/src/m1_base_client.cpp
    ../apps/gateway/src/m1_route_client.cpp
    ../apps/gateway/src/m1_base_client.cpp)
target_include_directories(migration_chain_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/world/include
    ${CMAKE_SOURCE_DIR}/apps/login/include
    ${CMAKE_SOURCE_DIR}/apps/gateway/include)
target_compile_definitions(migration_chain_tests PRIVATE
    APOLLO_TEST_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json")
target_link_libraries(migration_chain_tests PRIVATE apollo_app_services
    apollo::game_world apollo::script apollo::net_linktrace apollo::net_discovery)
if(TARGET apollo_squad_instance)
    target_link_libraries(migration_chain_tests PRIVATE apollo_squad_instance)
endif()
add_test(NAME MigrationChainTests COMMAND migration_chain_tests)
endif()

# ========== Base Business Wire Tests (P1 基础业务与结算迁入 958/959) ==========
# 独立 PlayerStore 宿主（StoreHost 纯 store 位）+ StoreDial 真拨：常规落账
# （锚点 exp 读数）、无锚点/冻结拒绝、SettlementLedger 业务受理装配
# （Ok + 幂等 + 停面挂账）。
if(TARGET apollo_app_services)
add_executable(base_business_wire_tests
    test_base_business_wire.cpp)
target_compile_definitions(base_business_wire_tests PRIVATE
    APOLLO_TEST_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json")
target_link_libraries(base_business_wire_tests PRIVATE apollo_app_services
    apollo::game_world apollo::script apollo::net_linktrace apollo::net_discovery)
if(TARGET apollo_squad_instance)
    target_link_libraries(base_business_wire_tests PRIVATE apollo_squad_instance)
endif()
add_test(NAME BaseBusinessWireTests COMMAND base_business_wire_tests)
endif()

# Squad instance admission (P2)：小队创建请求受理——队伍/成员/人数/准入
# 校验、World 容量选择、独立 Instance 与临时 Scene 创建。
if(TARGET apollo_squad_instance)
add_executable(squad_instance_tests
    test_squad_instance.cpp
)
target_link_libraries(squad_instance_tests PRIVATE apollo_squad_instance apollo::base apollo::core)
add_test(NAME SquadInstanceTests COMMAND squad_instance_tests)
endif()

# Independent Base/storage boundary over real M1 TCP (current transport is POSIX).
if(TARGET apollo_database_rpc AND NOT WIN32)
    add_executable(base_database_tests test_base_database.cpp)
    target_link_libraries(base_database_tests PRIVATE apollo_database_rpc)
    target_compile_definitions(base_database_tests PRIVATE
        APOLLO_TEST_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json")
    add_test(NAME BaseDatabaseTests COMMAND base_database_tests)
    set_tests_properties(BaseDatabaseTests PROPERTIES TIMEOUT 30)
endif()
if(TARGET base_database_tests AND APOLLO_BUILD_APPS)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_dependencies(base_database_tests base database)
        add_test(NAME BaseDatabaseProcessSmoke COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/base_database_process_smoke.py
            $<TARGET_FILE:base> $<TARGET_FILE:database>
            $<TARGET_FILE:base_database_tests>
            ${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json)
        set_tests_properties(BaseDatabaseProcessSmoke PROPERTIES TIMEOUT 40)
    endif()
endif()
