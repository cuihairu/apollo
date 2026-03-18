# Network module - core abstractions
add_library(apollo_net_core INTERFACE)
add_library(apollo::net_core ALIAS apollo_net_core)

target_include_directories(apollo_net_core
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(apollo_net_core INTERFACE cxx_std_20)

if(MSVC)
    target_compile_options(apollo_net_core INTERFACE /utf-8)
endif()

target_link_libraries(apollo_net_core
    INTERFACE
        apollo::runtime
)

# Find Drogon (includes Trantor with Boost.Asio)
find_package(Drogon CONFIG QUIET)

if(TARGET Drogon::Drogon)
    message(STATUS "Network module: using Drogon (includes Boost.Asio via Trantor)")
    set(APOLLO_USE_DROGON ON)
else()
    message(STATUS "Drogon not found, using built-in network implementations")
    set(APOLLO_USE_DROGON OFF)
endif()

# Find protobuf for TCP module
set(_net_protobuf_target "")
find_package(protobuf CONFIG QUIET)
if(TARGET protobuf::libprotobuf)
    set(_net_protobuf_target protobuf::libprotobuf)
endif()
if(NOT _net_protobuf_target)
    find_package(Protobuf CONFIG QUIET)
    if(TARGET protobuf::libprotobuf)
        set(_net_protobuf_target protobuf::libprotobuf)
    elseif(TARGET Protobuf::libprotobuf)
        set(_net_protobuf_target Protobuf::libprotobuf)
    endif()
endif()

# TCP implementation (keep built-in for now)
add_library(apollo_net_tcp STATIC
    tcp/src/net_common.cpp
    tcp/src/socket.cpp
    tcp/src/reactor.cpp
    tcp/src/rpc.cpp
    tcp/src/protobuf_message.cpp
)
add_library(apollo::net_tcp ALIAS apollo_net_tcp)

target_include_directories(apollo_net_tcp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/tcp/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_net_tcp
    PUBLIC
        apollo::net_core
        ${_net_protobuf_target}
)

target_compile_features(apollo_net_tcp PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_net_tcp PUBLIC /utf-8)
    target_compile_definitions(apollo_net_tcp PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()

# HTTP implementation - use Drogon if available
if(APOLLO_USE_DROGON)
    # When using Drogon, apollo_net_http is just an alias that links Drogon
    add_library(apollo_net_http INTERFACE)
    add_library(apollo::net_http ALIAS apollo_net_http)

    target_link_libraries(apollo_net_http
        INTERFACE
            apollo::net_core
            Drogon::Drogon
    )

    message(STATUS "apollo_net_http: using Drogon HTTP")
else()
    add_library(apollo_net_http STATIC
        http/src/http.cpp
        http/src/rest_client.cpp
        http/src/event_loop.cpp
    )
    add_library(apollo::net_http ALIAS apollo_net_http)

    target_include_directories(apollo_net_http
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/http/include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
            $<INSTALL_INTERFACE:include>
    )

    target_link_libraries(apollo_net_http
        PUBLIC
            apollo::net_core
    )

    target_compile_features(apollo_net_http PUBLIC cxx_std_20)

    if(MSVC)
        target_compile_options(apollo_net_http PUBLIC /utf-8)
        target_compile_definitions(apollo_net_http PUBLIC _CRT_SECURE_NO_WARNINGS)
    endif()

    message(STATUS "apollo_net_http: using built-in implementation")
endif()

# WebSocket implementation - use Drogon if available
if(APOLLO_USE_DROGON)
    add_library(apollo_net_websocket INTERFACE)
    add_library(apollo::net_websocket ALIAS apollo_net_websocket)

    target_link_libraries(apollo_net_websocket
        INTERFACE
            apollo::net_core
            Drogon::Drogon
    )

    message(STATUS "apollo_net_websocket: using Drogon WebSocket")
else()
    # Find OpenSSL for built-in WebSocket
    find_package(OpenSSL REQUIRED)
    if(OPENSSL_FOUND AND NOT TARGET OpenSSL::Crypto)
        add_library(OpenSSL::Crypto UNKNOWN IMPORTED)
        set_target_properties(OpenSSL::Crypto PROPERTIES
            IMPORTED_LOCATION "${OPENSSL_CRYPTO_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
        )
    endif()

    add_library(apollo_net_websocket STATIC
        websocket/src/websocket.cpp
    )
    add_library(apollo::net_websocket ALIAS apollo_net_websocket)

    target_include_directories(apollo_net_websocket
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/websocket/include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
            $<INSTALL_INTERFACE:include>
    )

    target_link_libraries(apollo_net_websocket
        PUBLIC
            apollo::net_core
            OpenSSL::Crypto
    )

    target_compile_features(apollo_net_websocket PUBLIC cxx_std_20)

    if(MSVC)
        target_compile_options(apollo_net_websocket PUBLIC /utf-8)
        target_compile_definitions(apollo_net_websocket PUBLIC _CRT_SECURE_NO_WARNINGS)
    endif()

    message(STATUS "apollo_net_websocket: using built-in implementation")
endif()

# RPC implementation
add_library(apollo_net_rpc STATIC
    rpc/src/rpc_legacy.cpp
    rpc/src/message_codec.cpp
    rpc/src/session_manager.cpp
    rpc/src/adapters/native_adapter.cpp
)
add_library(apollo::net_rpc ALIAS apollo_net_rpc)

target_include_directories(apollo_net_rpc
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/rpc/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_net_rpc
    PUBLIC
        apollo::net_tcp
)

target_compile_features(apollo_net_rpc PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_net_rpc PUBLIC /utf-8)
    target_compile_definitions(apollo_net_rpc PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()

# Protocol module (NNG wrapper with auto-negotiation)
add_subdirectory(protocol)

if(BUILD_TESTING)
    find_package(GTest QUIET)
    if(GTest_FOUND)
        add_executable(apollo_net_comprehensive_tests
            tests/net_comprehensive_tests.cpp
        )

        target_link_libraries(apollo_net_comprehensive_tests
            PRIVATE
                apollo_net_tcp
                apollo_net_http
                apollo_net_websocket
                apollo_net_rpc
                apollo::base
                GTest::gtest
                GTest::gtest_main
        )

        target_compile_features(apollo_net_comprehensive_tests PRIVATE cxx_std_20)

        if(MSVC)
            target_compile_options(apollo_net_comprehensive_tests PRIVATE /utf-8)
            target_compile_definitions(apollo_net_comprehensive_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()

        add_test(NAME apollo_net_comprehensive_tests COMMAND apollo_net_comprehensive_tests)
    endif()
endif()
