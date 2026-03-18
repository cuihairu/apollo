# Core - Logging submodule (using spdlog)
add_library(apollo_core_log STATIC
    src/log_manager.cpp
)
add_library(apollo::core_log ALIAS apollo_core_log)

target_include_directories(apollo_core_log
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include>
        $<INSTALL_INTERFACE:include>
)

# Find spdlog
find_package(spdlog CONFIG QUIET)

if(TARGET spdlog::spdlog_header_only)
    # Use header-only spdlog (recommended)
    target_link_libraries(apollo_core_log
        PUBLIC
            spdlog::spdlog_header_only
            apollo::base
    )
    target_compile_definitions(apollo_core_log PRIVATE APOLLO_USE_SPDLOG=1)
    message(STATUS "apollo_core_log: using spdlog (header-only)")
elseif(TARGET spdlog::spdlog)
    # Use compiled spdlog
    target_link_libraries(apollo_core_log
        PUBLIC
            spdlog::spdlog
            apollo::base
    )
    target_compile_definitions(apollo_core_log PRIVATE APOLLO_USE_SPDLOG=1)
    message(STATUS "apollo_core_log: using spdlog (compiled)")
else()
    # Fallback to built-in implementation
    target_link_libraries(apollo_core_log
        PUBLIC
            apollo::base
    )
    target_sources(apollo_core_log PRIVATE
        src/logger.cpp
        src/console_appender.cpp
        src/file_appender.cpp
    )
    message(STATUS "apollo_core_log: using built-in logger (spdlog not found)")
endif()

target_compile_features(apollo_core_log PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_core_log PUBLIC /utf-8)
    target_compile_definitions(apollo_core_log PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()
