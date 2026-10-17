# Application-level services shared by World and Login; no extra process roles.
add_library(apollo_app_services STATIC
    src/player_store.cpp
    src/database_service.cpp
    src/player_store_m1.cpp
    src/online_directory.cpp
    src/directory_m1.cpp
)
target_include_directories(apollo_app_services PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(apollo_app_services PUBLIC
    apollo::base apollo::core apollo::runtime
    apollo::data_orm apollo::data_cache apollo::game_session
    apollo_net_link apollo_net_linkrpc apollo_protocol
)
target_compile_features(apollo_app_services PUBLIC cxx_std_20)
