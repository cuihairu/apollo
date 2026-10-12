# 指标 push 客户端（P3-3 批：metrics admin 通道上行发送面——logging.md
# §5.1 三禁下的采样流管线；与 logpush 分工 = kDroppable 无队列周期推，
# 最新者胜不补投，见头注）。
# 内部泵线程单写者（链路收发/重连归泵拍，快照拉取也在泵线程）。
add_library(apollo_net_metricspush STATIC
    src/metrics_push_client.cpp
)
add_library(apollo::net_metricspush ALIAS apollo_net_metricspush)

target_include_directories(apollo_net_metricspush
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(apollo_net_metricspush PUBLIC cxx_std_20)

# 契约路由表 JSON（metrics_push_env 接线的路由装配事实源；生成物随仓，
# PUBLIC = 消费 app 直继承，app CMake 零改动）
target_compile_definitions(apollo_net_metricspush PUBLIC
    APOLLO_METRICS_PUSH_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)

target_link_libraries(apollo_net_metricspush
    PUBLIC
        apollo_net_link
        apollo::net_contract_wire
        apollo::core
)

if(MSVC)
    target_compile_options(apollo_net_metricspush PUBLIC /utf-8)
    target_compile_definitions(apollo_net_metricspush PUBLIC _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
