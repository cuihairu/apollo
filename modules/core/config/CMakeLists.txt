# Core - Config submodule
add_library(apollo_core_config STATIC
    src/config_value.cpp
    src/config_manager.cpp
)
add_library(apollo::core_config ALIAS apollo_core_config)

target_include_directories(apollo_core_config
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_core_config
    PUBLIC
        apollo::base
)

# Optional: nlohmann/json for JSON config support
find_package(nlohmann_json CONFIG QUIET)
if(nlohmann_json_FOUND)
    target_link_libraries(apollo_core_config PRIVATE nlohmann_json::nlohmann_json)
    target_compile_definitions(apollo_core_config PRIVATE APOLLO_HAS_NLOHMANN_JSON=1)
endif()

# Optional: yaml-cpp for YAML config support
option(APOLLO_ENABLE_YAML "Enable YAML support using yaml-cpp" ON)
if(APOLLO_ENABLE_YAML)
    find_package(yaml-cpp CONFIG QUIET)
    if(NOT yaml-cpp_FOUND)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(yaml-cpp yaml-cpp)
        endif()
    endif()
    if(yaml-cpp_FOUND)
        if(TARGET yaml-cpp::yaml-cpp)
            target_link_libraries(apollo_core_config PRIVATE yaml-cpp::yaml-cpp)
        else()
            target_link_libraries(apollo_core_config PRIVATE ${yaml-cpp_LIBRARIES})
            target_include_directories(apollo_core_config SYSTEM PRIVATE ${yaml-cpp_INCLUDE_DIRS})
        endif()
        target_compile_definitions(apollo_core_config PRIVATE APOLLO_HAS_YAML=1)
    endif()
endif()

target_compile_features(apollo_core_config PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_core_config PUBLIC /utf-8)
    target_compile_definitions(apollo_core_config PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()
