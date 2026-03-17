add_library(apollo_core STATIC
    src/config/config_registry.cpp
    src/di/application_context.cpp
    src/log/log_manager.cpp
    src/module_manifest.cpp
)
add_library(Apollo::core ALIAS apollo_core)

target_include_directories(apollo_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_core
    PUBLIC
        apollo_base
)

target_compile_features(apollo_core PUBLIC cxx_std_20)

if(BUILD_TESTING)
    add_executable(apollo_core_tests
        tests/core_tests.cpp
    )

    target_link_libraries(apollo_core_tests
        PRIVATE
            apollo_core
    )

    target_compile_features(apollo_core_tests PRIVATE cxx_std_20)

    add_test(NAME apollo_core_tests COMMAND apollo_core_tests)
endif()
