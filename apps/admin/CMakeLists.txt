cmake_minimum_required(VERSION 3.20)

# AdminApp——指标 exporter 进程壳（metrics admin 通道，logging.md §5.1
# 单一 exporter 定案）：serve 受面子命令 = link passive "metrics"(9106)
# 收 931 快照聚合 + HTTP(9107) 吐 /metrics。与 logger（9102 日志面）
# 同为运维侧无门控进程。
add_executable(admin
    src/main.cpp
    src/admin_app.cpp
    src/admin_serve.cpp
    src/metrics_hub.cpp
    src/metrics_http.cpp
)

target_include_directories(admin
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 契约路由表 JSON（受理面装配事实源；生成物随仓）
target_compile_definitions(admin PRIVATE
    APOLLO_ADMIN_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)

target_link_libraries(admin
    PRIVATE
        apollo::core
        apollo::runtime
        apollo_net_link
        apollo::net_contract_wire
)

target_compile_features(admin PRIVATE cxx_std_20)
apollo_package_crash_handler(admin)

if(WIN32)
    target_link_libraries(admin PRIVATE ws2_32)
endif()
if(MSVC)
    target_compile_options(admin PRIVATE /utf-8)
endif()
