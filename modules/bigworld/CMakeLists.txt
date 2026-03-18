# BigWorld compatibility layer
# This module wraps the existing apollo::bw implementation

# Copy existing runtime.cpp to src
add_library(apollo_bigworld STATIC
    src/runtime_wrapper.cpp
    src/entity.cpp
    src/callback.cpp
    src/timer.cpp
)
add_library(Apollo::bigworld ALIAS apollo_bigworld)

target_include_directories(apollo_bigworld
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>  # For apollo/bw headers
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_bigworld
    PUBLIC
        Apollo::game_core
        Apollo::runtime
)

target_compile_features(apollo_bigworld PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_bigworld PUBLIC /utf-8)
    target_compile_definitions(apollo_bigworld PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()

if(BUILD_TESTING)
    # Find GTest
    find_package(GTest QUIET)

    # Comprehensive tests with 80%+ coverage (requires GTest)
    if(GTest_FOUND)
        add_executable(apollo_bigworld_comprehensive_tests
            tests/bigworld_comprehensive_tests.cpp
        )

        target_link_libraries(apollo_bigworld_comprehensive_tests
            PRIVATE
                apollo_bigworld
                Apollo::base
                GTest::gtest
                GTest::gtest_main
        )

        target_compile_features(apollo_bigworld_comprehensive_tests PRIVATE cxx_std_20)

        if(MSVC)
            target_compile_options(apollo_bigworld_comprehensive_tests PRIVATE /utf-8)
            target_compile_definitions(apollo_bigworld_comprehensive_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()

        add_test(NAME apollo_bigworld_comprehensive_tests COMMAND apollo_bigworld_comprehensive_tests)
    endif()
endif()

# BigWorld public API compatibility header (include/bigworld/)
# This provides the legacy BigWorld namespace interface
