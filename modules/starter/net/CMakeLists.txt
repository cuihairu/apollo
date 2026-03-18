# Starter - Network module
add_library(apollo_starter_net INTERFACE)
add_library(apollo::starter_net ALIAS apollo_starter_net)

target_include_directories(apollo_starter_net
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_starter_net
    INTERFACE
        apollo::starter_core
        apollo::net_core
        apollo::net_tcp
        apollo::net_http
        apollo::net_websocket
)

target_compile_features(apollo_starter_net INTERFACE cxx_std_20)

if(MSVC)
    target_compile_options(apollo_starter_net INTERFACE /utf-8)
endif()
