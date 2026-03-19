# Modular layout entrypoint.
#
# This subtree provides the new modular architecture for Apollo.
# Each module has clear dependencies and can be used independently.

message(STATUS "Apollo modular layout scaffold enabled: modules/")

# Level 1: Base (no dependencies)
add_subdirectory(base)

# Level 2: Core (depends on base)
add_subdirectory(core)

# Level 3: Runtime (depends on core)
add_subdirectory(runtime)

# Level 4: Data (depends on core)
add_subdirectory(data)

# Level 5: Network (depends on runtime)
add_subdirectory(net)

# Level 5.5: Protocol (old NNG wrapper - deprecated, use net/protocol instead)
# Note: Old protocol module has issues, disabled in favor of modules/net/protocol
# add_subdirectory(protocol)

# Level 6: Actor - REMOVED
# Actor module has been removed in favor of CAF (C++ Actor Framework)
# See: https://github.com/actor-framework/actor-framework

# Level 7: Game (depends on core)
# Note: Game module has compilation issues, temporarily disabled
option(APOLLO_BUILD_GAME_MODULE "Build game framework module" OFF)
if(APOLLO_BUILD_GAME_MODULE)
    add_subdirectory(game)
endif()

# Level 8: BigWorld compatibility (depends on game, runtime)
# Note: BigWorld module depends on game, also disabled
option(APOLLO_BUILD_BIGWORLD_MODULE "Build BigWorld compatibility module" OFF)
if(APOLLO_BUILD_BIGWORLD_MODULE)
    add_subdirectory(bigworld)
endif()

# Level 9: Starter (assembly layer)
add_subdirectory(starter)

# Apollo aggregated library (for backward compatibility)
#
# This library aggregates all modular components into a single 'apollo' target
# to maintain compatibility with existing tests and applications.

if(NOT TARGET apollo)
    add_library(apollo INTERFACE)
    add_library(apollo::lib ALIAS apollo)

    # Link all modular components
    target_link_libraries(apollo INTERFACE
        apollo::base
        apollo::core
        apollo::runtime
        apollo::data_cache
        apollo::data_orm
        apollo::net_tcp
        apollo::net_http
        apollo::net_websocket
        apollo::net_rpc
        apollo::net_protocol
    )

    # Include directories
    target_include_directories(apollo INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<INSTALL_INTERFACE:include>
    )

    # Additional components (optional)
    if(TARGET apollo::data_redis)
        target_link_libraries(apollo INTERFACE apollo::data_redis)
    endif()

    if(TARGET apollo::game_core)
        target_link_libraries(apollo INTERFACE
            apollo::game_core
            apollo::game_world
            apollo::game_attributes
            apollo::game_battle
        )
    endif()

    if(TARGET apollo::bigworld)
        target_link_libraries(apollo INTERFACE apollo::bigworld)
    endif()

    message(STATUS "Apollo aggregated library created (alias: apollo::lib)")
endif()
