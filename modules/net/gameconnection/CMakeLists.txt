# GameConnection 门面（net-abstraction.md §2/§3 L3——游戏逻辑唯一入口）。
# 纯适配层：API 映射 + 泵透传；L2 会话增量（四通道/Resuming/resume 回调/
# 场景线程投递）留后续批。
add_library(apollo_net_gameconnection STATIC
    src/gameconnection.cpp
)
add_library(apollo::net_gameconnection ALIAS apollo_net_gameconnection)

target_include_directories(apollo_net_gameconnection
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_net_gameconnection
    PUBLIC
        apollo::net_link
)

target_compile_features(apollo_net_gameconnection PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_net_gameconnection PUBLIC /utf-8)
    target_compile_definitions(apollo_net_gameconnection PUBLIC _CRT_SECURE_NO_WARNINGS NOMAX)
endif()

if(WIN32)
    target_link_libraries(apollo_net_gameconnection PUBLIC ws2_32)
endif()
