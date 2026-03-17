# Core - Logging submodule
add_library(apollo_core_log STATIC
    src/logger.cpp
    src/console_appender.cpp
    src/file_appender.cpp
    src/log_manager.cpp
)
add_library(Apollo::core_log ALIAS apollo_core_log)

target_include_directories(apollo_core_log
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_core_log
    PUBLIC
        Apollo::base
)

target_compile_features(apollo_core_log PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_core_log PUBLIC /utf-8)
    target_compile_definitions(apollo_core_log PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()
