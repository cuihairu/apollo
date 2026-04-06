# Game - Attributes module
add_library(apollo_game_attributes STATIC
    src/attribute.cpp
    src/attribute_value.cpp
    src/attribute_sync.cpp
)
add_library(apollo::game_attributes ALIAS apollo_game_attributes)

target_include_directories(apollo_game_attributes
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_game_attributes
    PUBLIC
        apollo::game_core
)

target_compile_features(apollo_game_attributes PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_game_attributes PUBLIC /utf-8)
    target_compile_definitions(apollo_game_attributes PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()
