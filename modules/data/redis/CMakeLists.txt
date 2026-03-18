# Data - Redis module
add_library(apollo_data_redis STATIC
    src/redis_client_impl.cpp
    src/redis_template.cpp
    src/redis_connection_pool.cpp
)
add_library(apollo::data_redis ALIAS apollo_data_redis)

target_include_directories(apollo_data_redis
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_data_redis
    PUBLIC
        apollo::core
)

# Find hiredis
find_package(hiredis CONFIG QUIET)
if(NOT hiredis_FOUND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(hiredis hiredis)
    endif()
endif()

if(hiredis_FOUND OR TARGET hiredis::hiredis)
    if(TARGET hiredis::hiredis)
        target_link_libraries(apollo_data_redis PRIVATE hiredis::hiredis)
    elseif(hiredis_FOUND)
        target_link_libraries(apollo_data_redis PRIVATE ${hiredis_LIBRARIES})
        target_include_directories(apollo_data_redis SYSTEM PRIVATE ${hiredis_INCLUDE_DIRS})
    endif()
    target_compile_definitions(apollo_data_redis PRIVATE APOLLO_HAS_HIREDIS=1)
    message(STATUS "hiredis found for apollo_data_redis")
else()
    message(STATUS "hiredis not found, apollo_data_redis will use stub implementation")
endif()

target_compile_features(apollo_data_redis PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_data_redis PUBLIC /utf-8)
    target_compile_definitions(apollo_data_redis PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()
