add_library(apollo_base STATIC
    src/id_pool.cpp
)
add_library(Apollo::base ALIAS apollo_base)

target_include_directories(apollo_base
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(apollo_base PUBLIC cxx_std_20)

if(BUILD_TESTING)
    add_executable(apollo_base_tests
        tests/base_tests.cpp
    )

    target_link_libraries(apollo_base_tests
        PRIVATE
            apollo_base
    )

    target_compile_features(apollo_base_tests PRIVATE cxx_std_20)

    add_test(NAME apollo_base_tests COMMAND apollo_base_tests)
endif()
