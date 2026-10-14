cmake_minimum_required(VERSION 3.20)

# 协议编解码面（P0-5）：编解码源直接编入 app 目标，不链 apollo_protocol 库
# 目标（cell 受理入口唯一 = M1 链路）。
set(CELLAPP_PROTOCOL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/protocol/src/messages.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/protocol/src/codec.cpp
)

# CellApp 可执行文件
add_executable(cell-app
    src/main.cpp
    src/cell_server.cpp
    src/m1_link_server.cpp
    ${CELLAPP_PROTOCOL_SOURCES}
)

find_package(nlohmann_json CONFIG QUIET)

target_include_directories(cell-app
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/protocol/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/runtime/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/script/include
)

# 契约路由表 JSON（M1 接收端装配事实源；生成物随仓）
target_compile_definitions(cell-app PRIVATE
    APOLLO_CELL_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)

target_link_libraries(cell-app
    PRIVATE
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
    target_link_libraries(cell-app PRIVATE nlohmann_json::nlohmann_json)
endif()

target_compile_features(cell-app PRIVATE cxx_std_20)

# Windows 特定设置
if(WIN32)
    target_link_libraries(cell-app PRIVATE ws2_32)
endif()

apollo_package_crash_handler(cell-app)
