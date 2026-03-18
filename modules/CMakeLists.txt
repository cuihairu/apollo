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

# Level 6: Actor (depends on runtime, net)
# Note: Actor module requires IPC/NNG integration - currently disabled pending refactoring
option(APOLLO_BUILD_ACTOR_MODULE "Build actor framework module" OFF)
if(APOLLO_BUILD_ACTOR_MODULE)
    add_subdirectory(actor)
endif()

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
