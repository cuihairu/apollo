# WS 编解码（net M1 内核之上的纯件——零 socket 零线程，传输与生命周期
# 归宿主泵驱动。升级握手解析 RFC 6455 §4.2，帧编解码 RFC 6455 §5；
# SHA-1 + base64 借 OpenSSL EVP（与 net_websocket 同一依赖口径，link
# 内核保持纯 std）。本目标不做任何连接模型：gateway 游戏入口与
# interfaces HTTP 升级路由各自按自己的连接与生命周期消费本件。
find_package(OpenSSL REQUIRED)
if(OPENSSL_FOUND AND NOT TARGET OpenSSL::Crypto)
    add_library(OpenSSL::Crypto UNKNOWN IMPORTED)
    set_target_properties(OpenSSL::Crypto PROPERTIES
        IMPORTED_LOCATION "${OPENSSL_CRYPTO_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
    )
endif()

add_library(apollo_net_ws STATIC
    src/handshake.cpp
    src/frame.cpp
)
add_library(apollo::net_ws ALIAS apollo_net_ws)

target_include_directories(apollo_net_ws
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_net_ws
    PUBLIC
        OpenSSL::Crypto
)

target_compile_features(apollo_net_ws PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_net_ws PUBLIC /utf-8)
    target_compile_definitions(apollo_net_ws PUBLIC _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
