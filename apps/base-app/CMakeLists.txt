cmake_minimum_required(VERSION 3.20)

# BaseApp 可执行文件
add_executable(base-app
    src/main.cpp
    src/base_server.cpp
    src/database_service.cpp
)

target_include_directories(base-app
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(base-app
    PRIVATE
        apollo::base
        apollo::core
        apollo::data_orm
        apollo::data_cache
        apollo::game_session
        apollo_protocol
)

target_compile_features(base-app PRIVATE cxx_std_20)

# Windows 特定设置
if(WIN32)
    target_link_libraries(base-app PRIVATE ws2_32)
endif()
