# Core - Metrics submodule (P3-3 批 B：G-5 检测原语载体最小集)
# 原语进模块、聚合归 apps/exporter（net-abstraction §7 两截口径）；
# 依赖面纯 std，不挂 base。
add_library(apollo_core_metrics STATIC
    src/metric_registry.cpp
)
add_library(apollo::core_metrics ALIAS apollo_core_metrics)

target_include_directories(apollo_core_metrics
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(apollo_core_metrics PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_core_metrics PUBLIC /utf-8)
    target_compile_definitions(apollo_core_metrics PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()
