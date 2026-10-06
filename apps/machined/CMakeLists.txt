cmake_minimum_required(VERSION 3.20)

# Machined——G-1 单机守护（term-contract §1.3 定稿名：每机本地监工）。
# 本批 = 编队目录面最小闭环：UDP 收包喂 Registry + 周期 expire 喂钟 +
# 死亡/成员事件落日志。拉起/重启监督面（「拉起/重启」半边）留后续批。
add_executable(machined
    src/main.cpp
)

target_link_libraries(machined
    PRIVATE
        apollo::net_discovery
)

target_compile_features(machined PRIVATE cxx_std_20)

if(WIN32)
    target_link_libraries(machined PRIVATE ws2_32)
endif()
if(MSVC)
    target_compile_options(machined PRIVATE /utf-8)
endif()
