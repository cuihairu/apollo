cmake_minimum_required(VERSION 3.20)

# GatewayApp 可执行文件
add_executable(gateway-app
    src/main.cpp
    src/gateway_server.cpp
    src/gateway_control_server.cpp
    src/m1_egress.cpp
    src/m1_route_client.cpp
    include/gateway/session_manager.cpp
    src/ingress/client_ingress_server.cpp
    src/ingress/tcp_client_ingress_server.cpp
    src/ingress/client_packet_dispatcher.cpp
    src/ingress/gateway_connection_registry.cpp
    src/ingress/session_admission_service.cpp
)

target_include_directories(gateway-app
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../gateway-app/include
)

# 契约路由表 JSON（M1 出口装配事实源；生成物随仓）
target_compile_definitions(gateway-app PRIVATE
    APOLLO_GATEWAY_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)

# span 头部采样率生产装配面（logging.md §5.1「配置常量，入口进程决定」）：
# configure 期 -DAPOLLO_TRACE_SAMPLE_RATE=<N> 即定死编译期常量（压过 dev
# env 覆写——生产口径由装配方给定，运行期不可改）；未装配 = dev 面
# （APOLLO_TRACE_SAMPLE_RATE env / 缺省 1/10⁴ 常量）
if(DEFINED APOLLO_TRACE_SAMPLE_RATE AND NOT APOLLO_TRACE_SAMPLE_RATE STREQUAL "")
    if(NOT APOLLO_TRACE_SAMPLE_RATE MATCHES "^[1-9][0-9]*$")
        message(FATAL_ERROR
            "APOLLO_TRACE_SAMPLE_RATE must be a positive integer (1/N denominator), got: ${APOLLO_TRACE_SAMPLE_RATE}")
    endif()
    target_compile_definitions(gateway-app PRIVATE
        APOLLO_TRACE_SAMPLE_RATE_CONST=${APOLLO_TRACE_SAMPLE_RATE})
endif()

target_link_libraries(gateway-app
    PRIVATE
        apollo::base
        apollo::core
        apollo::runtime
        apollo::net_tcp
        apollo::net_protocol
        apollo::net_link
        apollo::net_contract_wire
        apollo::net_linktrace
        apollo::net_discovery
        apollo_net_linkrpc
        apollo_net_logpush
        apollo_net_metricspush
        apollo_protocol
)

target_compile_features(gateway-app PRIVATE cxx_std_20)

# Windows 特定设置
if(WIN32)
    target_link_libraries(gateway-app PRIVATE ws2_32 mswsock)
endif()

apollo_package_crash_handler(gateway-app)
