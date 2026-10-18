# Game - Cell module (scene-runtime Avatar, player-object-model cell side)
add_library(apollo_game_cell STATIC
    src/avatar.cpp
)
add_library(apollo::game_cell ALIAS apollo_game_cell)

target_include_directories(apollo_game_cell
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_game_cell
    PUBLIC
        apollo::game_core
        apollo::game_base
)

target_compile_features(apollo_game_cell PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_game_cell PUBLIC /utf-8)
    target_compile_definitions(apollo_game_cell PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()
