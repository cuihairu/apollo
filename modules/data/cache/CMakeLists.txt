# Data - Cache module
add_library(apollo_data_cache STATIC
    src/cache_manager.cpp
    src/primitive_cache.cpp
)
add_library(apollo::data_cache ALIAS apollo_data_cache)

target_include_directories(apollo_data_cache
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_data_cache
    PUBLIC
        apollo::core
        apollo::data_redis
)

target_compile_features(apollo_data_cache PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_data_cache PUBLIC /utf-8)
    target_compile_definitions(apollo_data_cache PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()
