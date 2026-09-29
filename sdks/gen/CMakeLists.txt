cmake_minimum_required(VERSION 3.20)

# apollo-gen：契约生成器（独立二进制，docs/36 决策 #4——KBE kbe/tools/xlsx2py 在 src
# 之外的先例；生成器只被本目录目标与 CI 引用，不进任何运行时服务的链接图）。
#
# CI 双闸（xml-generation §4）：
#   闸一 xmllint --schema（CI 侧）；闸二 = 下方 apollo_gen_golden_check——
#   改契约忘重新生成 → 生成物与 sdks/cpp/generated 提交版不一致 → 测试红。

find_package(nlohmann_json CONFIG REQUIRED)

add_executable(apollo_gen
    src/main.cpp
)

target_compile_features(apollo_gen PRIVATE cxx_std_20)
target_link_libraries(apollo_gen
    PRIVATE
        apollo::contract
        nlohmann_json::nlohmann_json
)

if(MSVC)
    target_compile_options(apollo_gen PRIVATE /utf-8)
endif()

add_test(NAME apollo_gen_golden_check
    COMMAND apollo_gen
        --contract ${CMAKE_SOURCE_DIR}/sdks/contract
        --check ${CMAKE_SOURCE_DIR}/sdks/cpp/generated
)
