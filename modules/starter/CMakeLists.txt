# Starter module - assembly layer
add_subdirectory(core)
add_subdirectory(net)

if(BUILD_TESTING)
    # Comprehensive tests with 80%+ coverage
    add_executable(apollo_starter_comprehensive_tests
        tests/starter_comprehensive_tests.cpp
    )

    target_link_libraries(apollo_starter_comprehensive_tests
        PRIVATE
            apollo_starter_core
            apollo_starter_net
            Apollo::base
            Apollo::core
    )

    target_compile_features(apollo_starter_comprehensive_tests PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(apollo_starter_comprehensive_tests PRIVATE /utf-8)
        target_compile_definitions(apollo_starter_comprehensive_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()

    add_test(NAME apollo_starter_comprehensive_tests COMMAND apollo_starter_comprehensive_tests)
endif()
