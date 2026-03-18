# Data module
add_subdirectory(core)
add_subdirectory(orm)
add_subdirectory(redis)
add_subdirectory(cache)

if(BUILD_TESTING)
    # Find GTest
    find_package(GTest QUIET)

    # Comprehensive tests with 80%+ coverage (requires GTest)
    if(GTest_FOUND)
        add_executable(apollo_data_comprehensive_tests
            tests/data_comprehensive_tests.cpp
        )

        target_link_libraries(apollo_data_comprehensive_tests
            PRIVATE
                apollo_data_core
                apollo_data_orm
                GTest::gtest
                GTest::gtest_main
        )

        target_compile_features(apollo_data_comprehensive_tests PRIVATE cxx_std_20)

        if(MSVC)
            target_compile_options(apollo_data_comprehensive_tests PRIVATE /utf-8)
            target_compile_definitions(apollo_data_comprehensive_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()

        add_test(NAME apollo_data_comprehensive_tests COMMAND apollo_data_comprehensive_tests)
    endif()
endif()
