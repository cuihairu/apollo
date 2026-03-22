# Game - Session module (PlayerAnchor, session routing, assignments)
add_library(apollo_game_session STATIC
    src/anchor_manager.cpp
    src/player_anchor.cpp
    src/session_locator.cpp
)
add_library(apollo::game_session ALIAS apollo_game_session)

target_include_directories(apollo_game_session
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_game_session
    PUBLIC
        apollo::game_core
)

target_compile_features(apollo_game_session PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_game_session PUBLIC /utf-8)
    target_compile_definitions(apollo_game_session PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()
