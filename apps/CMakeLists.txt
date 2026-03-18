# App entrypoint scaffold.
#
# Concrete executables such as gateway-server, login-server and game-server
# are added incrementally as their required module targets are extracted.

message(STATUS "Apollo app scaffold enabled: apps/")

# 检查依赖是否可用
# Protocol 模块 (需要 NNG)
if(TARGET apollo::protocol)
    add_subdirectory(gateway-app)
    add_subdirectory(login-app)
    add_subdirectory(base-app)
    add_subdirectory(cell-app)
    message(STATUS "Protocol apps enabled (apollo::protocol found)")
else()
    message(STATUS "Protocol apps disabled (apollo::protocol not found - NNG required)")
endif()

# Game 模块
if(TARGET apollo::game_core)
    add_subdirectory(game-server)
    message(STATUS "Game-server app enabled (apollo::game_core found)")
else()
    message(STATUS "Game-server app disabled (apollo::game_core not found - set APOLLO_BUILD_GAME_MODULE=ON)")
endif()
