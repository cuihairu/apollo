# Protocol module - NNG wrapper with auto-negotiation
add_library(apollo_net_protocol STATIC
    src/channel.cpp
    src/endpoint.cpp
)
add_library(apollo::net_protocol ALIAS apollo_net_protocol)

target_include_directories(apollo_net_protocol
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
)

# Find NNG
find_package(nng CONFIG QUIET)

if(TARGET nng::nng)
    target_link_libraries(apollo_net_protocol
        PUBLIC
            nng::nng
            apollo::net_core
    )
    target_compile_definitions(apollo_net_protocol PRIVATE APOLLO_USE_NNG=1)
    message(STATUS "apollo_net_protocol: using NNG")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../../vcpkg/installed/x64-windows/lib/nng.lib")
    # Fallback: direct vcpkg path
    target_include_directories(apollo_net_protocol SYSTEM PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../vcpkg/installed/x64-windows/include")
    target_link_libraries(apollo_net_protocol PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../vcpkg/installed/x64-windows/lib/nng.lib")
    target_compile_definitions(apollo_net_protocol PRIVATE APOLLO_USE_NNG=1)
    message(STATUS "apollo_net_protocol: using NNG (direct path)")
else()
    message(STATUS "NNG not found, apollo_net_protocol will use stub implementation")
endif()

# Optional: Redis for service discovery
find_package(redis++ CONFIG QUIET)
if(TARGET redis++::redis++)
    target_link_libraries(apollo_net_protocol PRIVATE redis++::redis++)
    target_compile_definitions(apollo_net_protocol PRIVATE APOLLO_USE_REDIS_PP=1)
    message(STATUS "apollo_net_protocol: Redis service discovery enabled")
endif()

target_compile_features(apollo_net_protocol PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_net_protocol PUBLIC /utf-8)
    target_compile_definitions(apollo_net_protocol PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()
