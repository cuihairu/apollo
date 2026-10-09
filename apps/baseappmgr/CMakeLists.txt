cmake_minimum_required(VERSION 3.20)

# BaseAppMgr 可执行文件——目录 + 落点裁决（无 DB 依赖，纯调度面）
add_executable(baseappmgr
    src/main.cpp
    src/baseappmgr.cpp
)

target_include_directories(baseappmgr
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(baseappmgr
    PRIVATE
        apollo::base
        apollo::core
        apollo::runtime
        apollo::game_session
        apollo::net_discovery
        apollo_protocol
)

target_compile_features(baseappmgr PRIVATE cxx_std_20)
apollo_package_crash_handler(baseappmgr)

# Windows 特定设置
if(WIN32)
    target_link_libraries(baseappmgr PRIVATE ws2_32)
endif()
