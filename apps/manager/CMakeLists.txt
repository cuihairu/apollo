cmake_minimum_required(VERSION 3.20)

# ManagerApp 可执行文件——目录 + 落点裁决（无 DB 依赖，纯调度面）
add_executable(manager
    src/main.cpp
    src/manager_app.cpp
)

target_include_directories(manager
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(manager
    PRIVATE
        apollo::base
        apollo::core
        apollo::runtime
        apollo::game_session
        apollo::net_discovery
        apollo_protocol
)

target_compile_features(manager PRIVATE cxx_std_20)
apollo_package_crash_handler(manager)

# Windows 特定设置
if(WIN32)
    target_link_libraries(manager PRIVATE ws2_32)
endif()
