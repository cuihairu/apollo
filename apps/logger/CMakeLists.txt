cmake_minimum_required(VERSION 3.20)

# LoggerApp——日志收集进程壳（ADR-013 立项，term-contract §1.3 契约代码
# 标识 LoggerApp；目录/二进制名 logger 同 machined 运维件口径）。
# v1 = 只读汇聚：scan 检索子命令 + follow 跟随（tail 拉，logger-app.md
# §3(a)）；L4 = serve 受面子命令（push 通道接收端，logging.md §5 collector
# 形态：M1 passive 端点收 930 → 原行归档；检索面零链路依赖不变）。
add_executable(logger
    src/main.cpp
    src/logger_app.cpp
    src/logger_serve.cpp
    src/structured_line.cpp
)

target_include_directories(logger
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 契约路由表 JSON（serve 受理面装配事实源；生成物随仓）
target_compile_definitions(logger PRIVATE
    APOLLO_LOGGER_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)

target_link_libraries(logger
    PRIVATE
        apollo::core
        apollo::runtime
        apollo_net_link
        apollo::net_contract_wire
        apollo_net_metricspush
)

target_compile_features(logger PRIVATE cxx_std_20)
apollo_package_crash_handler(logger)

if(WIN32)
    target_link_libraries(logger PRIVATE ws2_32)
endif()
if(MSVC)
    target_compile_options(logger PRIVATE /utf-8)
endif()
