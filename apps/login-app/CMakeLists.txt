cmake_minimum_required(VERSION 3.20)

# LoginApp 可执行文件
add_executable(login-app
    src/main.cpp
    src/login_server.cpp
)

target_include_directories(login-app
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(login-app
    PRIVATE
        apollo::base
        apollo::core
        apollo::runtime
        apollo_protocol
)

target_compile_features(login-app PRIVATE cxx_std_20)

# Windows 特定设置
if(WIN32)
    target_link_libraries(login-app PRIVATE ws2_32)
endif()
