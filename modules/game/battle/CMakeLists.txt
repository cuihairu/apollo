# Game - Battle module
add_library(apollo_game_battle STATIC
    src/battle_system.cpp
    src/battle_runtime.cpp
)
add_library(apollo::game_battle ALIAS apollo_game_battle)

target_include_directories(apollo_game_battle
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_game_battle
    PUBLIC
        apollo::game_core
)

target_compile_features(apollo_game_battle PUBLIC cxx_std_20)

# 浮点纪律（battle-determinism §2-②）：IEEE754 + 关浮点收缩（禁 FMA 合并），
# 保证判定跨平台位精确。MSVC 默认 /fp:precise，无收缩开关。
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(apollo_game_battle PUBLIC -ffp-contract=off)
endif()

if(MSVC)
    target_compile_options(apollo_game_battle PUBLIC /utf-8)
    target_compile_definitions(apollo_game_battle PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()
