cmake_minimum_required(VERSION 3.20)

# crypto 工具拆分单元（LAY-003）：SecureRandom+Hash 门面在 crypto.cpp，
# 其余按算法族分单元（digest/encode/cipher/checksum/password）。
set(APOLLO_CRYPTO_SOURCES
    ${CMAKE_SOURCE_DIR}/src/apollo/utils/crypto.cpp
    ${CMAKE_SOURCE_DIR}/src/apollo/utils/crypto_digest.cpp
    ${CMAKE_SOURCE_DIR}/src/apollo/utils/crypto_encode.cpp
    ${CMAKE_SOURCE_DIR}/src/apollo/utils/crypto_cipher.cpp
    ${CMAKE_SOURCE_DIR}/src/apollo/utils/crypto_checksum.cpp
    ${CMAKE_SOURCE_DIR}/src/apollo/utils/crypto_password.cpp
)


find_package(OpenSSL REQUIRED)

# LoginApp 可执行文件
add_executable(login
    ${APOLLO_CRYPTO_SOURCES}
    src/main.cpp
    src/login_server.cpp
    src/m1_admission_server.cpp
    src/m1_base_client.cpp
    src/m1_service_register_server.cpp
)

target_include_directories(login
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(login
    PRIVATE
        apollo_app_services
        apollo::base
        apollo::core
        apollo::runtime
        apollo_protocol
        apollo_net_discovery
        apollo_net_logpush
        apollo_net_metricspush
        apollo_net_link
        apollo_net_linkrpc
        OpenSSL::Crypto
)

target_compile_definitions(login PRIVATE
    APOLLO_LOGIN_CONTRACT_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json"
)

target_compile_features(login PRIVATE cxx_std_20)

# Windows 特定设置
if(WIN32)
    target_link_libraries(login PRIVATE ws2_32)
endif()

apollo_package_crash_handler(login)
