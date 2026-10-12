# Link RPC 关联原语（P4-4 批⓪：nng 迁移底座——契约 940/941 信封 + corr_id
# 关联 + 同步等待桥；载荷不透明，业务语义归各进程 handler）。
add_library(apollo_net_linkrpc STATIC
    src/rpc_channel.cpp
)
add_library(apollo::net_linkrpc ALIAS apollo_net_linkrpc)

target_include_directories(apollo_net_linkrpc
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_net_linkrpc
    PUBLIC
        apollo::net_link
        apollo::net_contract_wire
)

target_compile_features(apollo_net_linkrpc PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_net_linkrpc PUBLIC /utf-8)
endif()
