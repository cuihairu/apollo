# Network - Discovery submodule（G-1 编队服务发现库层：wire 报文 + Beacon/
# Registry 两侧行为语义 + UDP 传输库件；守护进程壳 = Machined（term-contract
# v1.0 §1.3 定稿名——2026-10-07 勘误：批 B 曾记「machined 不入户」系误读）
add_library(apollo_net_discovery STATIC
    src/discovery.cpp
    src/udp_transport.cpp
    src/directory_locator.cpp
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

if(WIN32)
    target_link_libraries(apollo_net_discovery PUBLIC ws2_32)
endif()
