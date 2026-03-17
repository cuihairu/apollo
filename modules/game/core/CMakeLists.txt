# Game - Core module
add_library(apollo_game_core STATIC
    src/entity.cpp
)
add_library(Apollo::game_core ALIAS apollo_game_core)

target_include_directories(apollo_game_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_game_core
    PUBLIC
        Apollo::core
)

target_compile_features(apollo_game_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_game_core PUBLIC /utf-8)
    target_compile_definitions(apollo_game_core PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()
