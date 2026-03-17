# Modular layout entrypoint.
#
# This subtree is intentionally introduced as a non-breaking scaffold while the
# existing monolithic `apollo` target remains the canonical build output.
# New module targets can be added here incrementally and migrated from the root
# target one capability at a time.

message(STATUS "Apollo modular layout scaffold enabled: modules/")

add_subdirectory(base)
add_subdirectory(core)
add_subdirectory(runtime)
add_subdirectory(data)
