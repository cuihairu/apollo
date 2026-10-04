cmake_minimum_required(VERSION 3.20)

# 协议编解码面（P0-5）：modules/protocol 保持禁用（nng 1.11 未适配），此处把
# 其无 nng 依赖的源直接编入 app 目标；未定义 APOLLO_USE_NNG → socket/nng_wrapper
# 走 stub 分支（内存桩传输，真传输随 P1 收口接回）。
set(CELLAPP_PROTOCOL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/protocol/src/nng_wrapper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/protocol/src/messages.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/protocol/src/codec.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/protocol/src/socket.cpp
)

# CellApp 可执行文件
add_executable(cell-app
    src/main.cpp
    src/cell_server.cpp
    ${CELLAPP_PROTOCOL_SOURCES}
)

find_package(nlohmann_json CONFIG QUIET)

target_include_directories(cell-app
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/protocol/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/runtime/include
)

target_link_libraries(cell-app
    PRIVATE
        apollo::base
        apollo::core
        apollo::game_core
        apollo::game_world
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
