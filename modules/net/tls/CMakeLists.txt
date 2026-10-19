# TLS 底座（net M1 内核的底座替换件；link 内核保持纯 std——OpenSSL 只进
# 本目标及其消费者。transport.hpp 缝纪律同源：底座替换 = 换 ILinkTransport
# 实现。§5.9：凭据与安全传输是显式部署集成，机制只装载部署供给的证书，
# 不生成不内置任何凭据。POSIX only，TCP 底座同款纪律。）
find_package(OpenSSL REQUIRED)
if(OPENSSL_FOUND AND NOT TARGET OpenSSL::SSL)
    add_library(OpenSSL::SSL UNKNOWN IMPORTED)
    set_target_properties(OpenSSL::SSL PROPERTIES
        IMPORTED_LOCATION "${OPENSSL_SSL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
    )
endif()

add_library(apollo_net_tls STATIC
    src/tls_transport.cpp
)
add_library(apollo::net_tls ALIAS apollo_net_tls)

target_include_directories(apollo_net_tls
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_net_tls
    PUBLIC
        apollo::net_link
        OpenSSL::SSL
        OpenSSL::Crypto
)

target_compile_features(apollo_net_tls PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_net_tls PUBLIC /utf-8)
    target_compile_definitions(apollo_net_tls PUBLIC _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
