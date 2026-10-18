# Game - Base module (Avatar long-term object, avatar manager, recovery)
add_library(apollo_game_base STATIC
    src/avatar.cpp
    src/avatar_manager.cpp
    src/recovery.cpp
)
add_library(apollo::game_base ALIAS apollo_game_base)

target_include_directories(apollo_game_base
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_game_base
    PUBLIC
        apollo::game_core
)

target_compile_features(apollo_game_base PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_game_base PUBLIC /utf-8)
    target_compile_definitions(apollo_game_base PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()
