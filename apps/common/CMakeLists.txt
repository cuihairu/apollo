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

# World-hosted squad instance admission (P2): World/Scene/Instance + Party
# orchestration. Separate library so Login-side hosts do not pull game modules.
# battle/session = P2-4 settlement ledger (battle sink → anchor rewards).
if(TARGET apollo::game_world AND TARGET apollo::game_social AND
   TARGET apollo::game_battle AND TARGET apollo::game_session)
add_library(apollo_squad_instance STATIC
    src/squad_instance.cpp
    src/settlement_ledger.cpp
    src/settlement_credential_store.cpp
)
target_include_directories(apollo_squad_instance PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(apollo_squad_instance PUBLIC
    apollo::base apollo::core apollo::game_world apollo::game_social
    apollo::game_battle apollo::game_session
)
target_compile_features(apollo_squad_instance PUBLIC cxx_std_20)
endif()
