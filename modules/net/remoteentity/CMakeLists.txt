# RemoteEntityCall 四件套（remote-entity-call 余件批：RemoteEntityRef /
# RemoteMethodSchema / RouteResolver / RpcTrace）。
# 设计见 docs/architecture/remote-entity-call-design.md。
add_library(apollo_net_remoteentity STATIC
    src/route_resolver.cpp
    src/rpc_trace.cpp
)
add_library(apollo::net_remoteentity ALIAS apollo_net_remoteentity)

target_include_directories(apollo_net_remoteentity
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(apollo_net_remoteentity PUBLIC cxx_std_20)

target_link_libraries(apollo_net_remoteentity
    PUBLIC
        apollo::net_linktrace
        apollo::core
)

if(MSVC)
    target_compile_options(apollo_net_remoteentity PUBLIC /utf-8)
    target_compile_definitions(apollo_net_remoteentity PUBLIC _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
