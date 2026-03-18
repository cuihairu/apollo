# Game module
add_subdirectory(core)
add_subdirectory(world)
add_subdirectory(battle)
add_subdirectory(attributes)

if(BUILD_TESTING)
    # Find GTest
    find_package(GTest QUIET)

    # Comprehensive tests with 80%+ coverage (requires GTest)
    if(GTest_FOUND)
        add_executable(apollo_game_comprehensive_tests
            tests/game_comprehensive_tests.cpp
        )

        target_link_libraries(apollo_game_comprehensive_tests
            PRIVATE
                apollo_game_core
                apollo_game_world
                GTest::gtest
                GTest::gtest_main
        )

        target_compile_features(apollo_game_comprehensive_tests PRIVATE cxx_std_20)

        if(MSVC)
            target_compile_options(apollo_game_comprehensive_tests PRIVATE /utf-8)
            target_compile_definitions(apollo_game_comprehensive_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()

        add_test(NAME apollo_game_comprehensive_tests COMMAND apollo_game_comprehensive_tests)
    endif()
endif()
