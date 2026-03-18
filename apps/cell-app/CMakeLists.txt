cmake_minimum_required(VERSION 3.20)

# CellApp 可执行文件
add_executable(cell-app
    src/main.cpp
)

target_include_directories(cell-app
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cell-app
    PRIVATE
        apollo::base
        apollo::core
        apollo::game_core
        apollo::game_world
        apollo::protocol
)

target_compile_features(cell-app PRIVATE cxx_std_20)

# Windows 特定设置
if(WIN32)
    target_link_libraries(cell-app PRIVATE ws2_32)
endif()
