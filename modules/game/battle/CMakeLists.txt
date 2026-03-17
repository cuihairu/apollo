# Game - Battle module (ECS)
add_library(apollo_game_battle STATIC
    src/ecs.cpp
    src/battle_system.cpp
)
add_library(Apollo::game_battle ALIAS apollo_game_battle)

target_include_directories(apollo_game_battle
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_game_battle
    PUBLIC
        Apollo::game_core
)

target_compile_features(apollo_game_battle PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_game_battle PUBLIC /utf-8)
    target_compile_definitions(apollo_game_battle PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()
