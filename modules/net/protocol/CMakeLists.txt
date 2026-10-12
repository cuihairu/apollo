# Protocol module - service endpoint abstraction（P4-4 批⑤：nng Channel 退役）
add_library(apollo_net_protocol STATIC
    src/endpoint.cpp
)
add_library(apollo::net_protocol ALIAS apollo_net_protocol)

target_include_directories(apollo_net_protocol
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_net_protocol
    PUBLIC
        apollo::net_core
)

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
