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
	        ../apps/base-app/src/m1_base_server.cpp
	    )
	    target_include_directories(base_anchor_tests PRIVATE
	        ${CMAKE_SOURCE_DIR}/apps/base-app/include
	        ${CMAKE_SOURCE_DIR}/sdks/cpp/generated
	    )
	    target_compile_definitions(base_anchor_tests PRIVATE
	        APOLLO_BASEAPP_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
	    )
	    target_link_libraries(base_anchor_tests PRIVATE
	        apollo
	        apollo::base
	        apollo::core
	        apollo::game_session
	        apollo_protocol
	        apollo_net_link
	        apollo_net_linkrpc
	    )
	    if(WIN32)
	        target_link_libraries(base_anchor_tests PRIVATE ws2_32)
	    endif()
	    add_test(NAME BaseAnchorTests COMMAND base_anchor_tests)

	    # baseappmgr（目录 + 落点裁决）——与 base-app 同档（legacy protocol + game_session）
	    add_executable(baseappmgr_tests
	        test_baseappmgr.cpp
	        ../apps/baseappmgr/src/baseappmgr.cpp
	        ../apps/baseappmgr/src/m1_control_server.cpp
	        ../apps/base-app/src/base_server.cpp
	        ../apps/base-app/src/database_service.cpp
	        ../apps/base-app/src/m1_base_server.cpp
	    )
	    target_include_directories(baseappmgr_tests PRIVATE
	        ${CMAKE_SOURCE_DIR}/apps/baseappmgr/include
	        ${CMAKE_SOURCE_DIR}/apps/base-app/include
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
	        ../apps/gateway-app/src/m1_egress.cpp
	        ../apps/gateway-app/src/m1_route_client.cpp
	        ../apps/gateway-app/include/gateway/session_manager.cpp
            ../apps/gateway-app/src/ingress/client_ingress_server.cpp
            ../apps/gateway-app/src/ingress/tcp_client_ingress_server.cpp
            ../apps/gateway-app/src/ingress/client_packet_dispatcher.cpp
            ../apps/gateway-app/src/ingress/gateway_connection_registry.cpp
            ../apps/gateway-app/src/ingress/session_admission_service.cpp
	    )
	    target_include_directories(gateway_route_tests PRIVATE
	        ${CMAKE_SOURCE_DIR}/apps/gateway-app/include
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
	        ../apps/gateway-app/src/gateway_server.cpp
	        ../apps/gateway-app/src/m1_egress.cpp
	        ../apps/gateway-app/src/m1_route_client.cpp
	        ../apps/gateway-app/include/gateway/session_manager.cpp
            ../apps/gateway-app/src/ingress/client_ingress_server.cpp
            ../apps/gateway-app/src/ingress/tcp_client_ingress_server.cpp
            ../apps/gateway-app/src/ingress/client_packet_dispatcher.cpp
            ../apps/gateway-app/src/ingress/gateway_connection_registry.cpp
            ../apps/gateway-app/src/ingress/session_admission_service.cpp
	    )
	    target_include_directories(gateway_ingress_tests PRIVATE
	        ${CMAKE_SOURCE_DIR}/apps/gateway-app/include
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
	        ../apps/base-app/src/m1_base_server.cpp
	    )
	    target_include_directories(base_anchor_tests PRIVATE
	        ${CMAKE_SOURCE_DIR}/apps/base-app/include
	        ${CMAKE_SOURCE_DIR}/sdks/cpp/generated
	    )
	    target_compile_definitions(base_anchor_tests PRIVATE
	        APOLLO_BASEAPP_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
	    )
	    target_link_libraries(base_anchor_tests PRIVATE
	        apollo
	        apollo::base
	        apollo::core
	        apollo::game_session
	        apollo_protocol
	        apollo_net_link
	        apollo_net_linkrpc
	    )
	    if(WIN32)
	        target_link_libraries(base_anchor_tests PRIVATE ws2_32)
	    endif()
	    add_test(NAME BaseAnchorTests COMMAND base_anchor_tests)

	    # baseappmgr（目录 + 落点裁决）——与 base-app 同档（legacy protocol + game_session）
	    add_executable(baseappmgr_tests
	        test_baseappmgr.cpp
	        ../apps/baseappmgr/src/baseappmgr.cpp
	        ../apps/baseappmgr/src/m1_control_server.cpp
	        ../apps/base-app/src/base_server.cpp
	        ../apps/base-app/src/database_service.cpp
	        ../apps/base-app/src/m1_base_server.cpp
	    )
	    target_include_directories(baseappmgr_tests PRIVATE
	        ${CMAKE_SOURCE_DIR}/apps/baseappmgr/include
	        ${CMAKE_SOURCE_DIR}/apps/base-app/include
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
	        ../apps/gateway-app/src/m1_egress.cpp
	        ../apps/gateway-app/src/m1_route_client.cpp
	        ../apps/gateway-app/include/gateway/session_manager.cpp
            ../apps/gateway-app/src/ingress/client_ingress_server.cpp
            ../apps/gateway-app/src/ingress/tcp_client_ingress_server.cpp
            ../apps/gateway-app/src/ingress/client_packet_dispatcher.cpp
            ../apps/gateway-app/src/ingress/gateway_connection_registry.cpp
            ../apps/gateway-app/src/ingress/session_admission_service.cpp
	    )
	    target_include_directories(gateway_route_tests PRIVATE
	        ${CMAKE_SOURCE_DIR}/apps/gateway-app/include
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
	        ../apps/gateway-app/src/gateway_server.cpp
	        ../apps/gateway-app/src/m1_egress.cpp
	        ../apps/gateway-app/src/m1_route_client.cpp
	        ../apps/gateway-app/include/gateway/session_manager.cpp
            ../apps/gateway-app/src/ingress/client_ingress_server.cpp
            ../apps/gateway-app/src/ingress/tcp_client_ingress_server.cpp
            ../apps/gateway-app/src/ingress/client_packet_dispatcher.cpp
            ../apps/gateway-app/src/ingress/gateway_connection_registry.cpp
            ../apps/gateway-app/src/ingress/session_admission_service.cpp
	    )
	    target_include_directories(gateway_ingress_tests PRIVATE
	        ${CMAKE_SOURCE_DIR}/apps/gateway-app/include
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
	        # Game AttrSync (scene.cpp attrsync wiring, 2026-10-14 批)
	        ../modules/game/attrsync/src/change_history.cpp
	        ../modules/game/attrsync/src/viewer_attr_state.cpp
	        ../modules/game/attrsync/src/delta_builder.cpp
	        ../modules/game/attrsync/src/entity_attrs.cpp
	        ../modules/game/attrsync/src/attr_sync_manager.cpp
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
	        ${CMAKE_SOURCE_DIR}/modules/game/attrsync/include
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
    # Game AttrSync (scene.cpp attrsync wiring, 2026-10-14 批)
    ../modules/game/attrsync/src/change_history.cpp
    ../modules/game/attrsync/src/viewer_attr_state.cpp
    ../modules/game/attrsync/src/delta_builder.cpp
    ../modules/game/attrsync/src/entity_attrs.cpp
    ../modules/game/attrsync/src/attr_sync_manager.cpp
    ../modules/game/session/src/player_anchor.cpp
)
target_include_directories(reconnect_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/modules/core/include
    ${CMAKE_SOURCE_DIR}/modules/game/core/include
    ${CMAKE_SOURCE_DIR}/modules/game/world/include
    ${CMAKE_SOURCE_DIR}/modules/game/battle/include
    ${CMAKE_SOURCE_DIR}/modules/game/session/include
    ${CMAKE_SOURCE_DIR}/modules/game/attrsync/include
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
    # Game AttrSync (scene.cpp attrsync wiring, 2026-10-14 批)
    ../modules/game/attrsync/src/change_history.cpp
    ../modules/game/attrsync/src/viewer_attr_state.cpp
    ../modules/game/attrsync/src/delta_builder.cpp
    ../modules/game/attrsync/src/entity_attrs.cpp
    ../modules/game/attrsync/src/attr_sync_manager.cpp
    ../modules/game/session/src/player_anchor.cpp
)
target_include_directories(scene_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/modules/core/include
    ${CMAKE_SOURCE_DIR}/modules/game/core/include
    ${CMAKE_SOURCE_DIR}/modules/game/world/include
    ${CMAKE_SOURCE_DIR}/modules/game/session/include
    ${CMAKE_SOURCE_DIR}/modules/game/attrsync/include
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
# 同 apps/cell-app（协议源直编习语）。
if(TARGET apollo::game_core AND TARGET apollo::game_world AND TARGET apollo::runtime)
add_executable(cell_acceptance_tests
    test_cell_acceptance.cpp
    ../apps/cell-app/src/cell_server.cpp
    ../apps/cell-app/src/m1_link_server.cpp
    ../modules/protocol/src/messages.cpp
    ../modules/protocol/src/codec.cpp
)
target_include_directories(cell_acceptance_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/cell-app/include
    ${CMAKE_SOURCE_DIR}/modules/protocol/include
    ${CMAKE_SOURCE_DIR}/modules/runtime/include
)
target_compile_definitions(cell_acceptance_tests PRIVATE
    APOLLO_CELL_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)
target_link_libraries(cell_acceptance_tests PRIVATE
    apollo apollo::base apollo::core apollo::game_core apollo::game_world apollo::runtime
    apollo::net_link apollo::net_contract_wire apollo::net_linktrace)
if(WIN32)
    target_link_libraries(cell_acceptance_tests PRIVATE ws2_32)
    target_compile_definitions(cell_acceptance_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(cell_acceptance_tests PRIVATE /utf-8)
endif()
add_test(NAME CellAcceptanceTests COMMAND cell_acceptance_tests)
endif()

# ========== Cell M1 Link Tests (M1 上层接线批 2b-ii) ==========
# cell M1 接收端：契约意图族 924-928 经 InterServerLink 真 TCP 上行 → 契约
# wire 解包 → legacy 帧镜像 → 受理队列 → tick 边界执行 → 延时应答 929
# （request_id 关联，code 0/2101）；字段级非法/域越权/未知 id 拒收面。
# 直编协议源惯例同 cell_acceptance_tests；客户端端 = 裸 LinkManager。
if(TARGET apollo::game_core AND TARGET apollo::game_world AND TARGET apollo::runtime)
add_executable(cell_m1_link_tests
    test_cell_m1_link.cpp
    ../apps/cell-app/src/cell_server.cpp
    ../apps/cell-app/src/m1_link_server.cpp
    ../modules/protocol/src/messages.cpp
    ../modules/protocol/src/codec.cpp
)
target_include_directories(cell_m1_link_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/cell-app/include
    ${CMAKE_SOURCE_DIR}/modules/protocol/include
    ${CMAKE_SOURCE_DIR}/modules/runtime/include
)
target_compile_definitions(cell_m1_link_tests PRIVATE
    APOLLO_CELL_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)
target_link_libraries(cell_m1_link_tests PRIVATE
    apollo apollo::base apollo::core apollo::game_core apollo::game_world
    apollo::runtime apollo::net_link apollo::net_contract_wire apollo::net_linktrace)
if(WIN32)
    target_link_libraries(cell_m1_link_tests PRIVATE ws2_32)
    target_compile_definitions(cell_m1_link_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(cell_m1_link_tests PRIVATE /utf-8)
endif()
add_test(NAME CellM1LinkTests COMMAND cell_m1_link_tests)
endif()

# ========== Gateway M1 Egress Tests (M1 上层接线批 2b-iii) ==========
# gateway M1 出口：legacy 世界帧 → 契约 wire 前递（924-928，request_id 关联）
# → 真 cell M1 接收端 → 延时应答 929 回流 → pending 关联回收/超时清扫；
# 投递箱溢出/不可前递族（断连通知等）落地观察面；真客户端 socket → gateway
# 入口 → cell M1 → 应答回流全链 e2e。gateway 与 cell 两侧直编（缝工厂两侧
# 真实现），Windows 下 link TCP 底座不进 M1 批，用例本体空转。
if(TARGET apollo::game_core AND TARGET apollo::game_world AND TARGET apollo::runtime)
add_executable(gateway_m1_egress_tests
    test_gateway_m1_egress.cpp
    ../apps/gateway-app/src/gateway_server.cpp
    ../apps/gateway-app/src/m1_egress.cpp
    ../apps/gateway-app/src/m1_route_client.cpp
    ../apps/gateway-app/include/gateway/session_manager.cpp
    ../apps/gateway-app/src/ingress/client_ingress_server.cpp
    ../apps/gateway-app/src/ingress/tcp_client_ingress_server.cpp
    ../apps/gateway-app/src/ingress/client_packet_dispatcher.cpp
    ../apps/gateway-app/src/ingress/gateway_connection_registry.cpp
    ../apps/gateway-app/src/ingress/session_admission_service.cpp
    ../apps/cell-app/src/cell_server.cpp
    ../apps/cell-app/src/m1_link_server.cpp
    ../modules/protocol/src/messages.cpp
    ../modules/protocol/src/codec.cpp
)
target_include_directories(gateway_m1_egress_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/gateway-app/include
    ${CMAKE_SOURCE_DIR}/apps/cell-app/include
    ${CMAKE_SOURCE_DIR}/modules/protocol/include
    ${CMAKE_SOURCE_DIR}/modules/runtime/include
)
target_compile_definitions(gateway_m1_egress_tests PRIVATE
    APOLLO_GATEWAY_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
    APOLLO_CELL_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)
target_link_libraries(gateway_m1_egress_tests PRIVATE
    apollo apollo::base apollo::core apollo::game_core apollo::game_world
    apollo::runtime apollo::net_tcp apollo::net_protocol apollo::net_link
    apollo::net_contract_wire apollo::net_linktrace apollo_net_linkrpc apollo_protocol)
if(WIN32)
    target_link_libraries(gateway_m1_egress_tests PRIVATE ws2_32)
    target_compile_definitions(gateway_m1_egress_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(gateway_m1_egress_tests PRIVATE /utf-8)
endif()
add_test(NAME GatewayM1EgressTests COMMAND gateway_m1_egress_tests)
endif()
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
    ../apps/login-app/src/m1_admission_server.cpp
    ../apps/gateway-app/src/ingress/session_admission_service.cpp
    ../apps/gateway-app/include/gateway/session_manager.cpp
    ../modules/protocol/src/messages.cpp
    ../modules/protocol/src/codec.cpp
)
target_include_directories(login_m1_admission_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/login-app/include
    ${CMAKE_SOURCE_DIR}/apps/gateway-app/include
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
    ../apps/base-app/src/m1_base_server.cpp
    ../apps/login-app/src/m1_base_client.cpp
)
target_include_directories(base_m1_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/base-app/include
    ${CMAKE_SOURCE_DIR}/apps/login-app/include
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

# BaseAppMgr M1 控制面 × login/gateway 客户端面 e2e（P4-4 批④）：双拨入方
# fan-out + 真 BaseAppMgr 分派面（AssignWorld/ResolveRoute 全真目录裁决）
# + 对端缺席降级。缝工厂两侧真实现直编；Windows 下 link TCP 底座不进本
# 批，用例本体空转。
if(TARGET apollo::game_session AND TARGET apollo_protocol)
add_executable(baseappmgr_m1_control_tests
    test_baseappmgr_m1_control.cpp
    ../apps/baseappmgr/src/m1_control_server.cpp
    ../apps/baseappmgr/src/baseappmgr.cpp
    ../apps/login-app/src/m1_base_client.cpp
    ../apps/gateway-app/src/m1_route_client.cpp
)
target_include_directories(baseappmgr_m1_control_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/baseappmgr/include
    ${CMAKE_SOURCE_DIR}/apps/base-app/include
    ${CMAKE_SOURCE_DIR}/apps/login-app/include
    ${CMAKE_SOURCE_DIR}/apps/gateway-app/include
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
add_test(NAME BaseAppMgrM1ControlTests COMMAND baseappmgr_m1_control_tests)

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
)
target_link_libraries(attrsync_tests PRIVATE
    apollo::game_attrsync
    apollo::game_core
    apollo::game_world
    GTest::gtest
    GTest::gtest_main
)
if(WIN32)
    target_link_libraries(attrsync_tests PRIVATE ws2_32)
    target_compile_definitions(attrsync_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
if(MSVC)
    target_compile_options(attrsync_tests PRIVATE /utf-8)
endif()
add_test(NAME AttrSyncTests COMMAND attrsync_tests)
endif()
