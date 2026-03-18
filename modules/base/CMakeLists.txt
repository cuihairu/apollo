add_library(apollo_base INTERFACE)
add_library(Apollo::base ALIAS apollo_base)

target_include_directories(apollo_base
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(apollo_base INTERFACE cxx_std_20)

# Header-only library, but we have one source file
target_sources(apollo_base INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/id_pool.cpp>
)

if(BUILD_TESTING)
    # Original simple tests
    add_executable(apollo_base_tests
        tests/base_tests.cpp
    )

    target_link_libraries(apollo_base_tests
        PRIVATE
            apollo_base
    )

    target_compile_features(apollo_base_tests PRIVATE cxx_std_20)

    add_test(NAME apollo_base_tests COMMAND apollo_base_tests)

    # Comprehensive tests with 80%+ coverage
    # GTest should be available via vcpkg toolchain
    add_executable(apollo_base_comprehensive_tests
        tests/base_comprehensive_tests.cpp
    )

    target_link_libraries(apollo_base_comprehensive_tests
        PRIVATE
            apollo_base
            GTest::gtest
            GTest::gtest_main
    )

    target_compile_features(apollo_base_comprehensive_tests PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(apollo_base_comprehensive_tests PRIVATE /utf-8)
        target_compile_definitions(apollo_base_comprehensive_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()

    add_test(NAME apollo_base_comprehensive_tests COMMAND apollo_base_comprehensive_tests)
endif()
