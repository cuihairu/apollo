# Core module - framework kernel with common runtime semantics
add_subdirectory(log)
add_subdirectory(config)

# Core library that aggregates all core functionality
add_library(apollo_core STATIC
    src/di/application_context.cpp
    src/module_manifest.cpp
)
add_library(apollo::core ALIAS apollo_core)

target_include_directories(apollo_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_core
    PUBLIC
        apollo_base
        apollo_core_log
        apollo_core_config
)

target_compile_features(apollo_core PUBLIC cxx_std_20)

# MSVC specific options
if(MSVC)
    target_compile_options(apollo_core PUBLIC /utf-8)
    target_compile_definitions(apollo_core PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()

if(BUILD_TESTING)
    # Find GTest
    find_package(GTest QUIET)

    # Original simple tests
    add_executable(apollo_core_tests
        tests/core_tests.cpp
    )

    target_link_libraries(apollo_core_tests
        PRIVATE
            apollo_core
    )

    target_compile_features(apollo_core_tests PRIVATE cxx_std_20)

    add_test(NAME apollo_core_tests COMMAND apollo_core_tests)

    # Comprehensive tests with 80%+ coverage (requires GTest)
    if(GTest_FOUND)
        add_executable(apollo_core_comprehensive_tests
            tests/core_comprehensive_tests.cpp
        )

        target_link_libraries(apollo_core_comprehensive_tests
            PRIVATE
                apollo_core
                GTest::gtest
                GTest::gtest_main
        )

        target_compile_features(apollo_core_comprehensive_tests PRIVATE cxx_std_20)

        if(MSVC)
            target_compile_options(apollo_core_comprehensive_tests PRIVATE /utf-8)
            target_compile_definitions(apollo_core_comprehensive_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()

        add_test(NAME apollo_core_comprehensive_tests COMMAND apollo_core_comprehensive_tests)
    endif()
endif()
