cmake_minimum_required(VERSION 3.20)

# BaseApp 可执行文件
add_executable(zone-app
    src/main.cpp
    src/base_server.cpp
    src/database_service.cpp
    src/m1_base_server.cpp
)

target_include_directories(zone-app
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(zone-app
    PRIVATE
        apollo::base
        apollo::core
        apollo::runtime
        apollo::data_orm
        apollo::data_cache
        apollo::game_session
        apollo::net_discovery
        apollo_net_logpush
        apollo_net_link
        apollo_net_linkrpc
        apollo_protocol
)

target_compile_definitions(zone-app PRIVATE
    APOLLO_ZONE_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)

target_compile_features(zone-app PRIVATE cxx_std_20)
apollo_package_crash_handler(zone-app)

# Windows 特定设置
if(WIN32)
    target_link_libraries(zone-app PRIVATE ws2_32)
endif()
