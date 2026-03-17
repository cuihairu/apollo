add_library(apollo_runtime STATIC
    src/application_host.cpp
    src/runtime_manifest.cpp
)
add_library(Apollo::runtime ALIAS apollo_runtime)

target_include_directories(apollo_runtime
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_runtime
    PUBLIC
        apollo_core
)

target_compile_features(apollo_runtime PUBLIC cxx_std_20)

if(BUILD_TESTING)
    add_executable(apollo_runtime_tests
        tests/runtime_tests.cpp
    )

    target_link_libraries(apollo_runtime_tests
        PRIVATE
            apollo_runtime
    )

    target_compile_features(apollo_runtime_tests PRIVATE cxx_std_20)

    add_test(NAME apollo_runtime_tests COMMAND apollo_runtime_tests)
endif()
