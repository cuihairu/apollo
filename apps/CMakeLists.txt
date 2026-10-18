# App entrypoint scaffold.
#
# Concrete executables such as gateway-server, login-server and game-server
# are added incrementally as their required module targets are extracted.

message(STATUS "Apollo app scaffold enabled: apps/")

# Machined——G-1 单机守护（term-contract §1.3 定稿名）：编队目录面
# （收包/喂钟/死亡事件落日志），仅依赖 net_discovery，无门控条件。
add_subdirectory(machined)

# LoggerApp——日志收集进程壳（ADR-013 立项）：只读汇聚 CLI（scan/follow），
# 依赖 apollo::core + apollo::runtime，无门控条件。
add_subdirectory(logger)

# AdminApp——指标 exporter 进程壳（metrics admin 通道，logging.md §5.1
# 单一 exporter）：serve 子命令 = link passive "metrics" + HTTP /metrics，
# 与 logger 同为运维侧无门控进程。
add_subdirectory(admin)

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
    add_subdirectory(base-server)
    add_subdirectory(database)
    add_subdirectory(login-app)
    add_subdirectory(fleetctl)
    message(STATUS "Legacy protocol apps enabled (apollo_protocol + game_session found)")
else()
    message(STATUS "Legacy protocol apps disabled (requires legacy apollo_protocol AND APOLLO_BUILD_GAME_MODULE=ON)")
endif()

# WorldApp 以新模块为判据（P0-5，audit §1.3 app 门控链收口）：world-app 不链
# apollo_protocol 库目标，协议编解码面直接编入 app 目标（直编习语沿袭）。
if(TARGET apollo::game_core AND TARGET apollo::game_world)
    add_subdirectory(world-app)
    message(STATUS "WorldApp enabled (game modules found)")
else()
    message(STATUS "WorldApp disabled (requires APOLLO_BUILD_GAME_MODULE=ON)")
endif()

# Game 模块
if(TARGET apollo::game_core)
    add_subdirectory(game-server)
    message(STATUS "Game-server app enabled (apollo::game_core found)")
else()
    message(STATUS "Game-server app disabled (apollo::game_core not found - set APOLLO_BUILD_GAME_MODULE=ON)")
endif()
