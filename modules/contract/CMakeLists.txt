cmake_minimum_required(VERSION 3.20)

# 契约解析/生成库（docs/36 决策 #3/#4/#5）。
# 链接图纪律：本库只被 sdks/gen 与自身测试链接；运行时服务消费
# sdks/cpp/generated 下的生成产物，绝不链接解析器（生成器不进运行时链接图）。

find_package(pugixml CONFIG REQUIRED)

set(CONTRACT_HEADERS
    include/apollo/contract/contract_model.hpp
    include/apollo/contract/contract_parser.hpp
    include/apollo/contract/contract_writer.hpp
    include/apollo/contract/contract_hash.hpp
)

set(CONTRACT_SOURCES
    src/contract_model.cpp
    src/contract_parser.cpp
    src/contract_writer.cpp
    src/contract_hash.cpp
)

add_library(apollo_contract STATIC
    ${CONTRACT_HEADERS}
    ${CONTRACT_SOURCES}
)
add_library(apollo::contract ALIAS apollo_contract)

target_include_directories(apollo_contract
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(apollo_contract PUBLIC cxx_std_20)
target_link_libraries(apollo_contract PRIVATE pugixml::pugixml)

if(MSVC)
    target_compile_options(apollo_contract PRIVATE /utf-8)
endif()

if(BUILD_TESTING OR APOLLO_BUILD_TESTS)
    # 解析器/校验/往返/hash/继承矩阵（仓库测试风格：plain C++ 断言）
    add_executable(apollo_contract_tests
        tests/test_contract.cpp
    )
    target_link_libraries(apollo_contract_tests PRIVATE apollo_contract)
    target_compile_features(apollo_contract_tests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(apollo_contract_tests PRIVATE /utf-8)
        target_compile_definitions(apollo_contract_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    target_compile_definitions(apollo_contract_tests PRIVATE
        APOLLO_CONTRACT_DIR="${CMAKE_SOURCE_DIR}/sdks/contract")
    add_test(NAME apollo_contract_tests COMMAND apollo_contract_tests)

    # 第 ③ 层编译期闸：生成产物必须可编译且关键不变量 static_assert 通过
    add_executable(apollo_contract_gen_compile_test
        tests/gen_compile_test.cpp
    )
    target_compile_features(apollo_contract_gen_compile_test PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(apollo_contract_gen_compile_test PRIVATE /utf-8)
    endif()
    target_include_directories(apollo_contract_gen_compile_test PRIVATE
        ${CMAKE_SOURCE_DIR}/sdks/cpp/generated)
    add_test(NAME apollo_contract_gen_compile_test COMMAND apollo_contract_gen_compile_test)
endif()
