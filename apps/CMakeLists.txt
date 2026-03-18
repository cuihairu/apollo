# App entrypoint scaffold.
#
# Concrete executables such as gateway-server, login-server and game-server
# are added incrementally as their required module targets are extracted.

message(STATUS "Apollo app scaffold enabled: apps/")

# BigWorld 服务器应用
add_subdirectory(gateway-app)
add_subdirectory(login-app)
add_subdirectory(base-app)
add_subdirectory(cell-app)

# 传统游戏服务器
add_subdirectory(game-server)
