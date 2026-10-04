# App entrypoint scaffold.
#
# Concrete executables such as gateway-server, login-server and game-server
# are added incrementally as their required module targets are extracted.

message(STATUS "Apollo app scaffold enabled: apps/")

# 检查依赖是否可用
# Gateway app 的 ingress 头仍包含旧 modules/protocol 的 apollo/protocol/*.hpp
# （迁移未完成），故除 net::protocol 外还需旧模块目标存在。
if(TARGET apollo::net_protocol AND TARGET apollo_protocol)
    add_subdirectory(gateway-app)
    message(STATUS "Gateway app enabled (apollo::net_protocol found)")
else()
    message(STATUS "Gateway app disabled (requires apollo::net_protocol and legacy apollo_protocol - enable modules/protocol)")
endif()

# Legacy protocol apps still depend on modules/protocol.
# base-app/baseappmgr additionally link apollo::game_session (game module).
if(TARGET apollo_protocol AND TARGET apollo::game_session)
    add_subdirectory(login-app)
    add_subdirectory(base-app)
    add_subdirectory(baseappmgr)
    message(STATUS "Legacy protocol apps enabled (apollo_protocol + game_session found)")
else()
    message(STATUS "Legacy protocol apps disabled (requires legacy apollo_protocol AND APOLLO_BUILD_GAME_MODULE=ON)")
endif()

# Cell app 以新模块为判据（P0-5，audit §1.3 app 门控链收口）：modules/protocol
# 在 P1 收口前保持禁用（nng 1.11 未适配），cell-app 不再链 apollo_protocol，
# 协议编解码面（无 nng 分支源）直接编入 app 目标（内存桩传输）。
if(TARGET apollo::game_core AND TARGET apollo::game_world)
    add_subdirectory(cell-app)
    message(STATUS "Cell-app enabled (game modules found)")
else()
    message(STATUS "Cell-app disabled (requires APOLLO_BUILD_GAME_MODULE=ON)")
endif()

# Game 模块
if(TARGET apollo::game_core)
    add_subdirectory(game-server)
    message(STATUS "Game-server app enabled (apollo::game_core found)")
else()
    message(STATUS "Game-server app disabled (apollo::game_core not found - set APOLLO_BUILD_GAME_MODULE=ON)")
endif()
