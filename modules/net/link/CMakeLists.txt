# net M1 内核（net-abstraction §5.6：帧定界 + contract_route 分发 + 背压水位 +
# 单播拓扑 + InterServerLink；正确性优先不设性能指标）。
# 纯 std 组件：零 apollo 内部依赖、零内部线程（泵由宿主驱动），测试用
# loopback 传输缝，TCP 底座独立成件。
add_library(apollo_net_link STATIC
    src/frame.cpp
    src/route_table.cpp
    src/route_json.cpp
    src/dispatcher.cpp
    src/watermark.cpp
    src/interserver_link.cpp
    src/tcp_transport.cpp
)
add_library(apollo::net_link ALIAS apollo_net_link)

target_include_directories(apollo_net_link
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(apollo_net_link PUBLIC cxx_std_20)

find_package(nlohmann_json CONFIG QUIET)
if(nlohmann_json_FOUND)
    target_link_libraries(apollo_net_link PUBLIC nlohmann_json::nlohmann_json)
    target_compile_definitions(apollo_net_link PUBLIC APOLLO_NET_LINK_HAS_JSON=1)
endif()

if(MSVC)
    target_compile_options(apollo_net_link PUBLIC /utf-8)
    target_compile_definitions(apollo_net_link PUBLIC _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

if(WIN32)
    target_link_libraries(apollo_net_link PUBLIC ws2_32)
endif()
