cmake_minimum_required(VERSION 3.20)

# GatewayApp 可执行文件
add_executable(gateway-app
    src/main.cpp
)

target_include_directories(gateway-app
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../gateway-app/include
)

target_link_libraries(gateway-app
    PRIVATE
        Apollo::base
        Apollo::core
        Apollo::runtime
        Apollo::net_tcp
        apollo::protocol
)

target_compile_features(gateway-app PRIVATE cxx_std_20)

# Windows 特定设置
if(WIN32)
    target_link_libraries(gateway-app PRIVATE ws2_32 mswsock)
endif()
