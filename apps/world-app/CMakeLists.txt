cmake_minimum_required(VERSION 3.20)

# 协议编解码面（P0-5）：编解码源直接编入 app 目标，不链 apollo_protocol 库
# 目标（WorldApp 受理入口唯一 = M1 链路）。
set(WORLD_APP_PROTOCOL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/protocol/src/messages.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/protocol/src/codec.cpp
)

# WorldApp 可执行文件
add_executable(world-app
    src/main.cpp
    src/world_app.cpp
    src/m1_link_server.cpp
    ${WORLD_APP_PROTOCOL_SOURCES}
)

find_package(nlohmann_json CONFIG QUIET)

target_include_directories(world-app
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/protocol/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/runtime/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/script/include
)

# 契约路由表 JSON（M1 接收端装配事实源；生成物随仓）
target_compile_definitions(world-app PRIVATE
    APOLLO_WORLD_APP_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)

target_link_libraries(world-app
    PRIVATE
        apollo_app_services
        apollo::base
        apollo::core
        apollo::game_core
        apollo::game_world
        apollo::script
        apollo::net_link
        apollo::net_contract_wire
        apollo::net_linktrace
        apollo::net_discovery
        apollo::net_logpush
        apollo_net_metricspush
        apollo::runtime
)

if(nlohmann_json_FOUND)
    target_link_libraries(world-app PRIVATE nlohmann_json::nlohmann_json)
endif()

target_compile_features(world-app PRIVATE cxx_std_20)

# Windows 特定设置
if(WIN32)
    target_link_libraries(world-app PRIVATE ws2_32)
endif()

apollo_package_crash_handler(world-app)
