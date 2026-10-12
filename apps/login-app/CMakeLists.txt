cmake_minimum_required(VERSION 3.20)

# LoginApp 可执行文件
add_executable(login-app
    src/main.cpp
    src/login_server.cpp
    src/m1_admission_server.cpp
    src/m1_base_client.cpp
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
        apollo_net_discovery
        apollo_net_logpush
        apollo_net_metricspush
        apollo_net_link
        apollo_net_linkrpc
)

target_compile_definitions(login-app PRIVATE
    APOLLO_LOGIN_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)

target_compile_features(login-app PRIVATE cxx_std_20)

# Windows 特定设置
if(WIN32)
    target_link_libraries(login-app PRIVATE ws2_32)
endif()

apollo_package_crash_handler(login-app)
