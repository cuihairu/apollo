cmake_minimum_required(VERSION 3.20)

# 协议编解码面（P0-5）：编解码源直接编入 app 目标，不链 apollo_protocol 库
# 目标（WorldApp 受理入口唯一 = M1 链路）。
set(WORLD_APP_PROTOCOL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/protocol/src/messages.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/protocol/src/codec.cpp
)

# WorldApp 可执行文件
add_executable(world
    src/main.cpp
    src/world_app.cpp
    src/cell_main.cpp
    src/cell_app.cpp
    src/m1_link_server.cpp
    src/cell_register_client.cpp
    ../login/src/m1_service_register_client.cpp
    ${WORLD_APP_PROTOCOL_SOURCES}
)

find_package(nlohmann_json CONFIG QUIET)

target_include_directories(world
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../login/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/protocol/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/runtime/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/script/include
)

# 契约路由表 JSON（M1 接收端装配事实源；生成物随仓）
target_compile_definitions(world PRIVATE
    APOLLO_WORLD_APP_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)

target_link_libraries(world
    PRIVATE
        apollo_app_services
        apollo_squad_instance
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
    target_link_libraries(world PRIVATE nlohmann_json::nlohmann_json)
endif()

target_compile_features(world PRIVATE cxx_std_20)

# Windows 特定设置
if(WIN32)
    target_link_libraries(world PRIVATE ws2_32)
endif()

apollo_package_crash_handler(world)

# ---- cellServer：cell 执行端独立进程边界（L104 首片收口） ----
# 只编执行端源（cell_main 入口 + CellApp 内核 + M1 受理端 + cell 注册 +
# 协议编解码），不编 main.cpp/world_app.cpp——进程内不含 WorldApp 管理壳。
# 模拟循环、WorldHost/Scene/Avatar、AOI、战斗与 Lua 均由 CellApp 装配。
add_executable(cellServer
    src/cell_server_main.cpp
    src/cell_main.cpp
    src/cell_app.cpp
    src/m1_link_server.cpp
    src/cell_register_client.cpp
    ${WORLD_APP_PROTOCOL_SOURCES}
)

target_include_directories(cellServer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../login/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/protocol/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/runtime/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/script/include
)

target_compile_definitions(cellServer PRIVATE
    APOLLO_WORLD_APP_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)

target_link_libraries(cellServer
    PRIVATE
        apollo_app_services
        apollo_squad_instance
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
    target_link_libraries(cellServer PRIVATE nlohmann_json::nlohmann_json)
endif()

target_compile_features(cellServer PRIVATE cxx_std_20)

if(WIN32)
    target_link_libraries(cellServer PRIVATE ws2_32)
endif()

apollo_package_crash_handler(cellServer)

# 入口冒烟：--help 走完整配置解析面后退出 0（不启 M1 监听/不拨号）。
add_test(NAME CellServerEntrySmoke COMMAND cellServer --help)
