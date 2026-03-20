# App entrypoint scaffold.
#
# Concrete executables such as gateway-server, login-server and game-server
# are added incrementally as their required module targets are extracted.

message(STATUS "Apollo app scaffold enabled: apps/")

# 检查依赖是否可用
# Gateway app has been migrated to the modular net/protocol target.
if(TARGET apollo::net_protocol)
    add_subdirectory(gateway-app)
    message(STATUS "Gateway app enabled (apollo::net_protocol found)")
else()
    message(STATUS "Gateway app disabled (apollo::net_protocol not found - NNG required)")
endif()

# Legacy protocol apps still depend on modules/protocol.
if(TARGET apollo_protocol)
    add_subdirectory(login-app)
    add_subdirectory(base-app)
    message(STATUS "Legacy protocol apps enabled (apollo_protocol found)")
else()
    message(STATUS "Legacy protocol apps disabled (legacy protocol module not enabled)")
endif()

# Cell app 需要 legacy protocol 和 game 模块
if(TARGET apollo_protocol AND TARGET apollo::game_core AND TARGET apollo::game_world)
    add_subdirectory(cell-app)
    message(STATUS "Cell-app enabled (legacy protocol and game modules found)")
else()
    message(STATUS "Cell-app disabled (missing legacy protocol or game modules)")
endif()

# Game 模块
if(TARGET apollo::game_core)
    add_subdirectory(game-server)
    message(STATUS "Game-server app enabled (apollo::game_core found)")
else()
    message(STATUS "Game-server app disabled (apollo::game_core not found - set APOLLO_BUILD_GAME_MODULE=ON)")
endif()
