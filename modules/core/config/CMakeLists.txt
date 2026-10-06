# Core - Config submodule（P3-1 配置统一收口：legacy ConfigManager/
# ConfigNode/ConfigValue 已删（audit A8——parseXml/parseLua 恒 false、
# 热更断头路、生产零消费），活件唯 ConfigRegistry；json/yaml optional
# 依赖随 legacy 消亡）
add_library(apollo_core_config STATIC
    ../src/config/config_registry.cpp
)
add_library(apollo::core_config ALIAS apollo_core_config)

target_include_directories(apollo_core_config
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_core_config
    PUBLIC
        apollo::base
)

target_compile_features(apollo_core_config PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_core_config PUBLIC /utf-8)
    target_compile_definitions(apollo_core_config PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()
