cmake_minimum_required(VERSION 3.20)

# BaseApp 可执行文件
add_executable(base-app
    src/main.cpp
    src/base_server.cpp
    src/database_service.cpp
    src/m1_base_server.cpp
)

target_include_directories(base-app
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/sdks/cpp/generated  # 迁移族 MsgId（migration_wire 钉生成器）
)

target_link_libraries(base-app
    PRIVATE
        apollo::base
        apollo::core
        apollo::runtime
        apollo::data_orm
        apollo::data_cache
        apollo::game_session
        apollo::net_discovery
        apollo_net_logpush
        apollo_net_metricspush
        apollo_net_link
        apollo_net_linkrpc
        apollo_protocol
)

target_compile_definitions(base-app PRIVATE
    APOLLO_BASEAPP_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)

target_compile_features(base-app PRIVATE cxx_std_20)
apollo_package_crash_handler(base-app)

# Windows 特定设置
if(WIN32)
    target_link_libraries(base-app PRIVATE ws2_32)
endif()
