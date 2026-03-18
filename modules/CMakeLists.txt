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

# Level 5.5: Protocol (depends on base, NNG)
add_subdirectory(protocol)

# Level 6: Actor (depends on runtime, net)
add_subdirectory(actor)

# Level 7: Game (depends on core)
add_subdirectory(game)

# Level 8: BigWorld compatibility (depends on game, runtime)
add_subdirectory(bigworld)

# Level 9: Starter (assembly layer)
add_subdirectory(starter)
