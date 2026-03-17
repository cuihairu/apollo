# Starter - Core module
add_library(apollo_starter_core INTERFACE)
add_library(Apollo::starter_core ALIAS apollo_starter_core)

target_include_directories(apollo_starter_core
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_starter_core
    INTERFACE
        Apollo::base
        Apollo::core
        Apollo::runtime
)

target_compile_features(apollo_starter_core INTERFACE cxx_std_20)

if(MSVC)
    target_compile_options(apollo_starter_core INTERFACE /utf-8)
endif()
