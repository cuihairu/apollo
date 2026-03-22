cmake_minimum_required(VERSION 3.20)

# CellApp 可执行文件
add_executable(cell-app
    src/main.cpp
    src/cell_server.cpp
)

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
        apollo_protocol
)

target_compile_features(cell-app PRIVATE cxx_std_20)

# Windows 特定设置
if(WIN32)
    target_link_libraries(cell-app PRIVATE ws2_32)
endif()
