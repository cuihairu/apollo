# 契约 wire codec（M1 上层接线批：按生成头 MsgMeta 驱动的平铺 LE 编解码，
# 字段布局唯一事实源 = sdks/cpp/generated 生成物；纯 std，零链接依赖——
# 生成头是 constexpr 数据，头文件包含即用）。
add_library(apollo_net_contract_wire STATIC
    src/contract_wire.cpp
)
add_library(apollo::net_contract_wire ALIAS apollo_net_contract_wire)

target_include_directories(apollo_net_contract_wire
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/sdks/cpp/generated>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(apollo_net_contract_wire PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_net_contract_wire PUBLIC /utf-8)
endif()
