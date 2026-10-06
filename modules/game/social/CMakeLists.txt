# Game - Social module (Guild/Party minimal CRUD, P2-4)
add_library(apollo_game_social STATIC
    src/guild.cpp
    src/party.cpp
)
add_library(apollo::game_social ALIAS apollo_game_social)

target_include_directories(apollo_game_social
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_game_social
    PUBLIC
        apollo::game_core
)

target_compile_features(apollo_game_social PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_game_social PUBLIC /utf-8)
    target_compile_definitions(apollo_game_social PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()
