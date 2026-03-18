add_executable(apollo_game_server
    src/main.cpp
)

target_link_libraries(apollo_game_server
    PRIVATE
        apollo_data_orm
        apollo_runtime
)

target_compile_features(apollo_game_server PRIVATE cxx_std_20)
