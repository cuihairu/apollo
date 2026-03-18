# Runtime module
find_package(Drogon CONFIG QUIET)

add_library(apollo_runtime STATIC
    src/application_host.cpp
    src/runtime_manifest.cpp
)
add_library(apollo::runtime ALIAS apollo_runtime)

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
