# Network - Discovery submodule（G-1 编队服务发现库层：wire 报文 + Beacon/
# Registry 两侧行为语义；不含 socket——UDP 传输随进程壳批，BeaconTransport
# 注入。术语中立：machined 名不入户（term-contract §0-2））
add_library(apollo_net_discovery STATIC
    src/discovery.cpp
)
add_library(apollo::net_discovery ALIAS apollo_net_discovery)

target_include_directories(apollo_net_discovery
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(apollo_net_discovery PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_net_discovery PUBLIC /utf-8)
    target_compile_definitions(apollo_net_discovery PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()
