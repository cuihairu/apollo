# CodeCoverage.cmake - 代码覆盖率支持

# 查找必要的程序
find_program(GCOV_PATH gcov)
find_program(LCOV_PATH lcov)
find_program(GENHTML_PATH genhtml)

if(NOT GCOV_PATH)
    message(FATAL_ERROR "gcov not found! Please install gcc with gcov support.")
endif()

# 设置编译选项以启用代码覆盖率
set(CMAKE_CXX_FLAGS_COVERAGE
    "-g -O0 --coverage -fprofile-arcs -ftest-coverage"
    CACHE STRING "Flags used by the C++ compiler during coverage builds."
    FORCE)

set(CMAKE_C_FLAGS_COVERAGE
    "-g -O0 --coverage -fprofile-arcs -ftest-coverage"
    CACHE STRING "Flags used by the C compiler during coverage builds."
    FORCE)

set(CMAKE_EXE_LINKER_FLAGS_COVERAGE
    "--coverage"
    CACHE STRING "Flags used for linking binaries during coverage builds."
    FORCE)

set(CMAKE_SHARED_LINKER_FLAGS_COVERAGE
    "--coverage"
    CACHE STRING "Flags used for linking shared libraries during coverage builds."
    FORCE)

# 标记覆盖率构建类型
mark_as_advanced(
    CMAKE_CXX_FLAGS_COVERAGE
    CMAKE_C_FLAGS_COVERAGE
    CMAKE_EXE_LINKER_FLAGS_COVERAGE
    CMAKE_SHARED_LINKER_FLAGS_COVERAGE)

# 设置 Coverage 构建类型
set(CMAKE_CXX_FLAGS_COVERAGE
    "${CMAKE_CXX_FLAGS_DEBUG} ${CMAKE_CXX_FLAGS_COVERAGE}"
    CACHE STRING "Flags used by the C++ compiler during Coverage builds."
    FORCE)
set(CMAKE_C_FLAGS_COVERAGE
    "${CMAKE_C_FLAGS_DEBUG} ${CMAKE_C_FLAGS_COVERAGE}"
    CACHE STRING "Flags used by the C compiler during Coverage builds."
    FORCE)
set(CMAKE_EXE_LINKER_FLAGS_COVERAGE
    "${CMAKE_EXE_LINKER_FLAGS_DEBUG} ${CMAKE_EXE_LINKER_FLAGS_COVERAGE}"
    CACHE STRING "Flags used for linking binaries during Coverage builds."
    FORCE)

# 添加覆盖率构建类型
set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES} Coverage" CACHE STRING "Available build types" FORCE)

# 定义函数：为目标添加覆盖率支持
function(SETUP_TARGET_FOR_COVERAGE target)
    if(NOT LCOV_PATH)
        message(WARNING "lcov not found! Cannot generate coverage report.")
        return()
    endif()

    # 清理之前的覆盖率数据
    add_custom_target(coverage-clean
        COMMAND ${LCOV_PATH} --directory . --zerocounters
        COMMAND find . -name "*.gcda" -exec rm {} \;
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Cleaning up coverage data"
    )

    # 生成覆盖率报告
    add_custom_target(coverage-report
        COMMAND ${LCOV_PATH} --capture --directory . --output-file coverage.info
        COMMAND ${LCOV_PATH} --remove coverage.info '/usr/*' --output-file coverage.info
        COMMAND ${LCOV_PATH} --remove coverage.info '${CMAKE_SOURCE_DIR}/tests/*' --output-file coverage.info
        COMMAND ${LCOV_PATH} --remove coverage.info '${CMAKE_SOURCE_DIR}/examples/*' --output-file coverage.info
        COMMAND ${LCOV_PATH} --list coverage.info
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating coverage report"
    )

    # 生成 HTML 报告
    if(GENHTML_PATH)
        add_custom_target(coverage-html
            ${LCOV_PATH} --capture --directory . --output-file coverage.info
            COMMAND ${LCOV_PATH} --remove coverage.info '/usr/*' --output-file coverage.info
            COMMAND ${LCOV_PATH} --remove coverage.info '${CMAKE_SOURCE_DIR}/tests/*' --output-file coverage.info
            COMMAND ${LCOV_PATH} --remove coverage.info '${CMAKE_SOURCE_DIR}/examples/*' --output-file coverage.info
            COMMAND ${GENHTML_PATH} -o coverage-html coverage.info
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating HTML coverage report"
        )
    endif()

    # 添加依赖关系
    add_dependencies(coverage-report ${target})
    if(GENHTML_PATH)
        add_dependencies(coverage-html ${target})
    endif()

    message(STATUS "Coverage support enabled for target: ${target}")
endfunction()

# 定义函数：为测试添加覆盖率支持
function(ENABLE_COVERAGE_FOR_TESTS test_target)
    # 确保测试目标使用覆盖率编译标志
    get_target_property(test_type ${test_target} TYPE)
    if(test_type STREQUAL "EXECUTABLE")
        target_compile_options(${test_target} PRIVATE
            $<$<CONFIG:Coverage>:${CMAKE_CXX_FLAGS_COVERAGE}>
            $<$<CONFIG:Coverage>:${CMAKE_C_FLAGS_COVERAGE}>
        )
        target_link_libraries(${test_target} PRIVATE
            $<$<CONFIG:Coverage>:--coverage>
        )

        if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
            target_compile_options(${test_target} PRIVATE ${CMAKE_CXX_FLAGS_COVERAGE})
            target_link_libraries(${test_target} PRIVATE --coverage)
        endif()

        # 设置覆盖率报告目标
        setup_target_for_coverage(${test_target})
    endif()
endfunction()

# 打印覆盖率配置信息
message(STATUS "Code Coverage configuration:")
message(STATUS "  GCOV: ${GCOV_PATH}")
message(STATUS "  LCOV: ${LCOV_PATH}")
message(STATUS "  GENHTML: ${GENHTML_PATH}")