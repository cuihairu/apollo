# Game - World module (AOI, scene, etc.)
add_library(apollo_game_world STATIC
    src/aoi.cpp
    src/scene.cpp
)
add_library(apollo::game_world ALIAS apollo_game_world)

target_include_directories(apollo_game_world
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_game_world
    PUBLIC
        apollo::game_core
)

target_compile_features(apollo_game_world PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_game_world PUBLIC /utf-8)
    target_compile_definitions(apollo_game_world PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()
