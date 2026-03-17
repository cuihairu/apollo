add_library(apollo_data_orm STATIC
    src/memory_connection.cpp
    src/sql_template.cpp
)
add_library(Apollo::data_orm ALIAS apollo_data_orm)

target_include_directories(apollo_data_orm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_data_orm
    PUBLIC
        apollo_data_core
)

target_compile_features(apollo_data_orm PUBLIC cxx_std_20)

if(BUILD_TESTING)
    add_executable(apollo_data_orm_tests
        tests/data_orm_tests.cpp
    )

    target_link_libraries(apollo_data_orm_tests
        PRIVATE
            apollo_data_orm
    )

    target_compile_features(apollo_data_orm_tests PRIVATE cxx_std_20)

    add_test(NAME apollo_data_orm_tests COMMAND apollo_data_orm_tests)
endif()
