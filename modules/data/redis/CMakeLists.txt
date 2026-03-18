# Data - Redis module
add_library(apollo_data_redis STATIC
    src/redis_client_impl.cpp
    src/redis_template.cpp
    src/redis_connection_pool.cpp
    src/redis_client_wrapper.cpp
    src/redis_template_wrapper.cpp
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

# 设置 vcpkg 路径用于 find_package
set(CMAKE_PREFIX_PATH "C:/Users/cui/Workspaces/vcpkg/installed/x64-windows" ${CMAKE_PREFIX_PATH})

# Find redis-plus-plus (includes hiredis dependency)
find_package(redis++ CONFIG QUIET)

if(NOT redis++_FOUND)
    # 备用：直接链接
    if(EXISTS "C:/Users/cui/Workspaces/vcpkg/installed/x64-windows/lib/redis++.lib")
        message(STATUS "Found redis-plus-plus via direct path")
        target_include_directories(apollo_data_redis SYSTEM PUBLIC
            "C:/Users/cui/Workspaces/vcpkg/installed/x64-windows/include")
        target_link_libraries(apollo_data_redis PUBLIC
            "C:/Users/cui/Workspaces/vcpkg/installed/x64-windows/lib/redis++.lib")
        target_compile_definitions(apollo_data_redis PRIVATE APOLLO_USE_REDIS_PLUS_PLUS=1)
    else()
        message(STATUS "redis-plus-plus not found, apollo_data_redis will use stub implementation")
    endif()
elseif(TARGET redis++::redis++)
    target_link_libraries(apollo_data_redis PUBLIC redis++::redis++)
    target_compile_definitions(apollo_data_redis PRIVATE APOLLO_USE_REDIS_PLUS_PLUS=1)
    message(STATUS "redis-plus-plus found for apollo_data_redis")
endif()

target_compile_features(apollo_data_redis PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_data_redis PUBLIC /utf-8)
    target_compile_definitions(apollo_data_redis PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()
