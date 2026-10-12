# trace 子集原语（P3-3 批 · 语义层：logging.md §5.1 借 W3C 模型不引
# SDK——TraceContext/traceparent 串面 + 头部采样器 + span 结构化行发射
# + 932 internal_envelope 字段级编解码缝；载体 = remote-entity-call
# 四件套信封，net-abstraction §7 合流口径）。
add_library(apollo_net_linktrace STATIC
    src/trace_context.cpp
    src/span_emitter.cpp
    src/envelope.cpp
)
add_library(apollo::net_linktrace ALIAS apollo_net_linktrace)

target_include_directories(apollo_net_linktrace
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(apollo_net_linktrace PUBLIC cxx_std_20)

target_link_libraries(apollo_net_linktrace
    PUBLIC
        apollo::net_contract_wire
        apollo::core
)

if(MSVC)
    target_compile_options(apollo_net_linktrace PUBLIC /utf-8)
    target_compile_definitions(apollo_net_linktrace PUBLIC _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
