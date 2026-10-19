cmake_minimum_required(VERSION 3.20)

# apolloc CLI——项目初始化的唯一入口（P0.2 L36）：init 子命令生成最小项目
# 骨架（模板内容随 L37 落地）。分派逻辑 header-only 于 include/apollo_cli/，
# tests/test_apollo_cli.cpp 直编同源单测；纯标准库无链接依赖（不接崩溃
# 采集：一次性工具不污染 stdout、不在调用者 cwd 落 crashdumps/）。
add_executable(apolloc
    src/main.cpp
)

target_include_directories(apolloc
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(apolloc PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(apolloc PRIVATE /utf-8)
endif()
