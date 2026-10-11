# 日志 push 客户端（L4：logging.md §5 双出口发送面——link active 端点 +
# 契约 930 编码 + 有界队列断连降级；ADR-013「push 留接口」的落地面）。
# 内部泵线程单写者（链路收发/重连归泵拍，push 只入队）。
add_library(apollo_net_logpush STATIC
    src/log_push_client.cpp
)
add_library(apollo::net_logpush ALIAS apollo_net_logpush)

target_include_directories(apollo_net_logpush
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(apollo_net_logpush PUBLIC cxx_std_20)

target_link_libraries(apollo_net_logpush
    PUBLIC
        apollo_net_link
        apollo::net_contract_wire
)

if(MSVC)
    target_compile_options(apollo_net_logpush PUBLIC /utf-8)
    target_compile_definitions(apollo_net_logpush PUBLIC _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
