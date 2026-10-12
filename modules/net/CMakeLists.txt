# Network module - core abstractions
add_library(apollo_net_core INTERFACE)
add_library(apollo::net_core ALIAS apollo_net_core)

target_include_directories(apollo_net_core
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(apollo_net_core INTERFACE cxx_std_20)

if(MSVC)
    target_compile_options(apollo_net_core INTERFACE /utf-8)
endif()

target_link_libraries(apollo_net_core
    INTERFACE
        apollo::runtime
)

# Find protobuf for TCP module
set(_net_protobuf_target "")
find_package(protobuf CONFIG QUIET)
if(TARGET protobuf::libprotobuf)
    set(_net_protobuf_target protobuf::libprotobuf)
endif()
if(NOT _net_protobuf_target)
    find_package(Protobuf CONFIG QUIET)
    if(TARGET protobuf::libprotobuf)
        set(_net_protobuf_target protobuf::libprotobuf)
    elseif(TARGET Protobuf::libprotobuf)
        set(_net_protobuf_target Protobuf::libprotobuf)
    endif()
endif()

# TCP implementation (keep built-in for now)
add_library(apollo_net_tcp STATIC
    tcp/src/net_common.cpp
    tcp/src/socket.cpp
    tcp/src/reactor.cpp
    tcp/src/rpc.cpp
    tcp/src/protobuf_message.cpp
)
add_library(apollo::net_tcp ALIAS apollo_net_tcp)

target_include_directories(apollo_net_tcp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/tcp/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_net_tcp
    PUBLIC
        apollo::net_core
        ${_net_protobuf_target}
)

target_compile_features(apollo_net_tcp PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_net_tcp PUBLIC /utf-8)
    target_compile_definitions(apollo_net_tcp PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()

# HTTP implementation（net-abstraction §5.10 裁决①②：curl 进 vcpkg，
# rest_client 转真实现（APOLLO_HAS_CURL=1）；Drogon 备选分支删除——
# §15.2 禁并存，本目标单形态）
find_package(CURL CONFIG REQUIRED)
add_library(apollo_net_http STATIC
    http/src/http.cpp
    http/src/rest_client.cpp
    http/src/event_loop.cpp
)
add_library(apollo::net_http ALIAS apollo_net_http)

target_include_directories(apollo_net_http
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/http/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_net_http
    PUBLIC
        apollo::net_core
        CURL::libcurl
)

target_compile_definitions(apollo_net_http PRIVATE APOLLO_HAS_CURL=1)

target_compile_features(apollo_net_http PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_net_http PUBLIC /utf-8)
    target_compile_definitions(apollo_net_http PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()

message(STATUS "apollo_net_http: built-in + libcurl (§5.10)")

# WebSocket implementation（§5.10 裁决②：Drogon 备选分支删除，内置单形态）
# Find OpenSSL for built-in WebSocket
find_package(OpenSSL REQUIRED)
if(OPENSSL_FOUND AND NOT TARGET OpenSSL::Crypto)
    add_library(OpenSSL::Crypto UNKNOWN IMPORTED)
    set_target_properties(OpenSSL::Crypto PROPERTIES
        IMPORTED_LOCATION "${OPENSSL_CRYPTO_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
    )
endif()

add_library(apollo_net_websocket STATIC
    websocket/src/websocket.cpp
)
add_library(apollo::net_websocket ALIAS apollo_net_websocket)

target_include_directories(apollo_net_websocket
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/websocket/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_net_websocket
    PUBLIC
        apollo::net_core
        OpenSSL::Crypto
)

target_compile_features(apollo_net_websocket PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_net_websocket PUBLIC /utf-8)
    target_compile_definitions(apollo_net_websocket PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()

message(STATUS "apollo_net_websocket: using built-in implementation")

# RPC implementation
add_library(apollo_net_rpc STATIC
    rpc/src/rpc_legacy.cpp
    rpc/src/message_codec.cpp
    rpc/src/session_manager.cpp
    rpc/src/adapters/native_adapter.cpp
)
add_library(apollo::net_rpc ALIAS apollo_net_rpc)

target_include_directories(apollo_net_rpc
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/rpc/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_net_rpc
    PUBLIC
        apollo::net_tcp
)

target_compile_features(apollo_net_rpc PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_net_rpc PUBLIC /utf-8)
    target_compile_definitions(apollo_net_rpc PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()

# Protocol module (NNG wrapper with auto-negotiation)
add_subdirectory(protocol)

# Discovery module (G-1 编队服务发现库层——wire 报文 + Beacon/Registry，
# socket 随进程壳批)
add_subdirectory(discovery)

# Link module (net M1 内核——帧定界/契约路由分发/背压水位/单播拓扑/
# InterServerLink，net-abstraction §5.6；纯 std，泵由宿主驱动)
add_subdirectory(link)

# Contract wire codec（M1 上层接线批：按生成头 MsgMeta 驱动的契约载荷
# 编解码，业务帧载荷 = u16 id + 载荷信封的按型解释层）
add_subdirectory(contract_wire)

# Log push client（L4：logging.md §5 双出口发送面——link active 端点 +
# 契约 930 + 有界队列断连降级；collector = apps/logger serve）
add_subdirectory(logpush)

# Metrics push client（P3-3 批：metrics admin 通道上行发送面——契约 931
# 快照 kDroppable 周期推，最新者胜不补投；exporter = apps/admin serve）
add_subdirectory(metricspush)

# Link RPC 关联原语（P4-4 批⓪：nng 迁移底座——契约 940/941 关联信封 +
# corr_id 关联 + 同步等待桥；五处 nng 消费面的迁移底座）
add_subdirectory(linkrpc)

# trace 子集原语（P3-3 批：logging.md §5.1 W3C traceparent 上下文 + 头部
# 采样 + span 结构化行发射 + 932 internal_envelope 编解码缝）
add_subdirectory(linktrace)

if(BUILD_TESTING)
    find_package(GTest QUIET)
    if(GTest_FOUND)
        add_executable(apollo_net_comprehensive_tests
            tests/net_comprehensive_tests.cpp
        )

        target_link_libraries(apollo_net_comprehensive_tests
            PRIVATE
                apollo_net_tcp
                apollo_net_http
                apollo_net_websocket
                apollo_net_rpc
                apollo::base
                GTest::gtest
                GTest::gtest_main
        )

        target_compile_features(apollo_net_comprehensive_tests PRIVATE cxx_std_20)

        if(MSVC)
            target_compile_options(apollo_net_comprehensive_tests PRIVATE /utf-8)
            target_compile_definitions(apollo_net_comprehensive_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()

        add_test(NAME apollo_net_comprehensive_tests COMMAND apollo_net_comprehensive_tests)
    endif()
endif()
