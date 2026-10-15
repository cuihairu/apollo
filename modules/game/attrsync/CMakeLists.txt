# Game - Attribute sync module (per-viewer delta pipeline)
add_library(apollo_game_attrsync STATIC
    src/change_history.cpp
    src/viewer_attr_state.cpp
    src/attr_delta.cpp
    src/delta_builder.cpp
    src/entity_attrs.cpp
    src/attr_sync_manager.cpp
    src/attr_sync_scheduler.cpp
    src/frame_codec.cpp
    src/attr_domain_mapping.cpp
)
add_library(apollo::game_attrsync ALIAS apollo_game_attrsync)

target_include_directories(apollo_game_attrsync
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_game_attrsync
    PUBLIC
        apollo::game_core
)

target_compile_features(apollo_game_attrsync PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_game_attrsync PUBLIC /utf-8)
    target_compile_definitions(apollo_game_attrsync PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()
