# Runtime module
find_package(Drogon CONFIG QUIET)

# 崩溃采集面（crash-capture 批②，docs/design/crash-capture.md）：find_package
# 失败（非 vcpkg/无 overlay 端口环境）→ no-op 桩（APOLLO_HAS_CRASHPAD=0），
# 三树照常绿——采集面是增强不是依赖（fail-open 同族纪律）。
find_package(crashpad CONFIG QUIET)

add_library(apollo_runtime STATIC
    src/application_host.cpp
    src/world_host.cpp
    src/runtime_manifest.cpp
    src/crash_capture.cpp
)
add_library(apollo::runtime ALIAS apollo_runtime)

if(crashpad_FOUND)
    target_link_libraries(apollo_runtime PRIVATE crashpad::crashpad)
    target_compile_definitions(apollo_runtime PRIVATE APOLLO_HAS_CRASHPAD=1)
    # handler 编译期缓存路径（运行期三级定位的兜底档）：vcpkg tools 产物
    find_file(CRASHPAD_HANDLER_EXE crashpad_handler
        PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/tools/crashpad"
        NO_DEFAULT_PATH)
    if(CRASHPAD_HANDLER_EXE)
        target_compile_definitions(apollo_runtime PRIVATE
            "APOLLO_CRASHPAD_HANDLER_DEFAULT=\"${CRASHPAD_HANDLER_EXE}\"")
    endif()
    message(STATUS "Runtime module: crash capture enabled (crashpad)")
else()
    target_compile_definitions(apollo_runtime PRIVATE APOLLO_HAS_CRASHPAD=0)
    message(STATUS "Runtime module: crash capture stub (crashpad not found)")
endif()

# 产物带 handler（设计 §2.2）：POST_BUILD 复制 crashpad_handler 到 app 可执行
# 文件旁——部署面无额外步骤，handler 与 app 同目录分发。app 侧调用。
# 注意：crashpad_FOUND 与导入 target 均为目录域变量，app 目录不可见——
# 判定走 find_file 的 CACHE 变量（仅 crashpad 接入时被置值）。
function(apollo_package_crash_handler target)
    if(CRASHPAD_HANDLER_EXE)
        # chmod 补执行位（vcpkg tools 产物可能无 x 位；POSIX 面 only）
        if(NOT WIN32)
            set(_crash_chmod COMMAND chmod 755 "$<TARGET_FILE_DIR:${target}>/crashpad_handler")
        else()
            set(_crash_chmod "")
        endif()
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CRASHPAD_HANDLER_EXE}" "$<TARGET_FILE_DIR:${target}>/crashpad_handler"
            ${_crash_chmod}
            COMMENT "Packaging crashpad_handler next to ${target}")
        unset(_crash_chmod)
    endif()
endfunction()

target_include_directories(apollo_runtime
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_runtime
    PUBLIC
        apollo_core
)

# Link with Trantor (from Drogon) for Boost.Asio integration
if(TARGET Drogon::Drogon)
    # Trantor is part of Drogon, provides:
    # - Async I/O (based on Boost.Asio)
    # - Timers
    # - Thread pools
    # - Event loops
    target_link_libraries(apollo_runtime
        PUBLIC
            Trantor::Trantor
    )
    target_compile_definitions(apollo_runtime PUBLIC APOLLO_USE_TRANTOR=1)
    message(STATUS "Runtime module: using Trantor (Boost.Asio via Drogon)")
else()
    message(STATUS "Runtime module: using built-in implementation")
endif()

target_compile_features(apollo_runtime PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_runtime PUBLIC /utf-8)
    target_compile_definitions(apollo_runtime PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()

if(BUILD_TESTING)
    find_package(GTest QUIET)

    # Original simple tests
    add_executable(apollo_runtime_tests
        tests/runtime_tests.cpp
    )

    target_link_libraries(apollo_runtime_tests
        PRIVATE
            apollo_runtime
    )

    target_compile_features(apollo_runtime_tests PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(apollo_runtime_tests PRIVATE /utf-8)
        target_compile_definitions(apollo_runtime_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()

    add_test(NAME apollo_runtime_tests COMMAND apollo_runtime_tests)

    # Comprehensive tests with 80%+ coverage (requires GTest)
    if(GTest_FOUND)
        add_executable(apollo_runtime_comprehensive_tests
            tests/runtime_comprehensive_tests.cpp
        )

        target_link_libraries(apollo_runtime_comprehensive_tests
            PRIVATE
                apollo_runtime
                GTest::gtest
                GTest::gtest_main
        )

        target_compile_features(apollo_runtime_comprehensive_tests PRIVATE cxx_std_20)

        if(MSVC)
            target_compile_options(apollo_runtime_comprehensive_tests PRIVATE /utf-8)
            target_compile_definitions(apollo_runtime_comprehensive_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()

        add_test(NAME apollo_runtime_comprehensive_tests COMMAND apollo_runtime_comprehensive_tests)
    endif()
endif()
