add_library(apollo_data_core INTERFACE)
add_library(apollo::data_core ALIAS apollo_data_core)

target_include_directories(apollo_data_core
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_data_core
    INTERFACE
        apollo_core
)

target_compile_features(apollo_data_core INTERFACE cxx_std_20)

if(MSVC)
    target_compile_options(apollo_data_core INTERFACE /utf-8)
endif()
