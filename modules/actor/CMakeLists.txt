# Actor module
add_library(apollo_actor STATIC
    src/actor_cell.cpp
    src/actor_system.cpp
    src/actor_manager.cpp
    src/actor_ref.cpp
    src/actor_utils.cpp
    src/dispatcher.cpp
    src/message_bus.cpp
    src/message_serialization.cpp
    src/monitoring.cpp
    src/timer_service.cpp
)
add_library(Apollo::actor ALIAS apollo_actor)

target_include_directories(apollo_actor
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_actor
    PUBLIC
        Apollo::runtime
        Apollo::net_tcp
)

target_compile_features(apollo_actor PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_actor PUBLIC /utf-8)
    target_compile_definitions(apollo_actor PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()

if(BUILD_TESTING)
    # Comprehensive tests with 80%+ coverage
    add_executable(apollo_actor_comprehensive_tests
        tests/actor_comprehensive_tests.cpp
    )

    target_link_libraries(apollo_actor_comprehensive_tests
        PRIVATE
            apollo_actor
            Apollo::base
    )

    target_compile_features(apollo_actor_comprehensive_tests PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(apollo_actor_comprehensive_tests PRIVATE /utf-8)
        target_compile_definitions(apollo_actor_comprehensive_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()

    add_test(NAME apollo_actor_comprehensive_tests COMMAND apollo_actor_comprehensive_tests)
endif()
