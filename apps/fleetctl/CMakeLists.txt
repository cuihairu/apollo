cmake_minimum_required(VERSION 3.20)

# FleetCtl 编队探针（P0-1 路1 演练量测面：assign/resolve → baseappmgr
# M1 控制面；drill 脚本断言面——三种退出码 = 闸开/闸拒/链不通）。
add_executable(fleetctl
    src/main.cpp
)

# migrate 命令的 wire 面（base/migration_wire.hpp，header-only）
target_include_directories(fleetctl PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/base-app/include
)

target_link_libraries(fleetctl
    PRIVATE
        apollo::base
        apollo::core
        apollo_protocol
        apollo_net_link
        apollo_net_linkrpc
)

target_compile_definitions(fleetctl PRIVATE
    APOLLO_FLEETCTL_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)

target_compile_features(fleetctl PRIVATE cxx_std_20)

apollo_package_crash_handler(fleetctl)
