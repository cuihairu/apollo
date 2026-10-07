# ========== RPC Demo ==========
add_executable(rpc_demo rpc_demo.cpp)
target_link_libraries(rpc_demo PRIVATE apollo)

# ========== Session Demo ==========
add_executable(session_demo session_demo.cpp)
target_link_libraries(session_demo PRIVATE apollo)

# ========== Game Server Demo ==========
add_executable(game_server_demo game_server_demo.cpp)
target_link_libraries(game_server_demo PRIVATE apollo)

# ========== Full Loop Demo (P4-2) ==========
# 任务书 §29 全链路进程内驱动（login→lobby→instance→enter→spawn→AOI→battle
# →reward→leave）：直编 world/battle/session 真件，每步断言门=组合回归验证器。
add_executable(full_loop_demo full_loop_demo.cpp
    ${CMAKE_SOURCE_DIR}/modules/game/core/src/entity.cpp
    ${CMAKE_SOURCE_DIR}/modules/game/world/src/scene.cpp
    ${CMAKE_SOURCE_DIR}/modules/game/world/src/scene_aoi.cpp
    ${CMAKE_SOURCE_DIR}/modules/game/world/src/viewer_state.cpp
    ${CMAKE_SOURCE_DIR}/modules/game/world/src/avatar.cpp
    ${CMAKE_SOURCE_DIR}/modules/game/world/src/instance.cpp
    ${CMAKE_SOURCE_DIR}/modules/game/battle/src/battle_runtime.cpp
    ${CMAKE_SOURCE_DIR}/modules/game/battle/src/battle_system.cpp
    ${CMAKE_SOURCE_DIR}/modules/game/session/src/anchor_manager.cpp
    ${CMAKE_SOURCE_DIR}/modules/game/session/src/player_anchor.cpp
)
target_include_directories(full_loop_demo PRIVATE
    ${CMAKE_SOURCE_DIR}/modules/core/include
    ${CMAKE_SOURCE_DIR}/modules/game/core/include
    ${CMAKE_SOURCE_DIR}/modules/game/battle/include
    ${CMAKE_SOURCE_DIR}/modules/game/world/include
    ${CMAKE_SOURCE_DIR}/modules/game/session/include
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(full_loop_demo PRIVATE apollo apollo::base apollo::core)

# ========== Benchmark ==========
# 三模型场景基准（P3-2 批 A）：直编 game world 六源（scene_tests 同款惯例，
# 不依赖 GAME_MODULE target——OFF 树照常构建），负载链 Scene/AOI/Avatar 真实路径。
add_executable(benchmark benchmark.cpp
    ${CMAKE_SOURCE_DIR}/modules/game/core/src/entity.cpp
    ${CMAKE_SOURCE_DIR}/modules/game/world/src/scene.cpp
    ${CMAKE_SOURCE_DIR}/modules/game/world/src/scene_aoi.cpp
    ${CMAKE_SOURCE_DIR}/modules/game/world/src/viewer_state.cpp
    ${CMAKE_SOURCE_DIR}/modules/game/world/src/avatar.cpp
    ${CMAKE_SOURCE_DIR}/modules/game/session/src/player_anchor.cpp
)
target_include_directories(benchmark PRIVATE
    ${CMAKE_SOURCE_DIR}/modules/core/include
    ${CMAKE_SOURCE_DIR}/modules/game/core/include
    ${CMAKE_SOURCE_DIR}/modules/game/world/include
    ${CMAKE_SOURCE_DIR}/modules/game/session/include
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(benchmark PRIVATE apollo apollo::base apollo::core)

# ========== Server Template ==========
add_executable(server_template server_template.cpp)
target_link_libraries(server_template PRIVATE apollo)

# ========== Serializer Demo ==========
add_executable(serializer_demo serializer_demo.cpp)
target_link_libraries(serializer_demo PRIVATE apollo)

# ========== Network Utility Demo ==========
add_executable(netutil_demo netutil_demo.cpp)
target_link_libraries(netutil_demo PRIVATE apollo)

# ========== HTTP Demo ==========
add_executable(http_demo http_demo.cpp)
target_link_libraries(http_demo PRIVATE apollo)

# ========== WebSocket Demo ==========
add_executable(websocket_demo websocket_demo.cpp)
target_link_libraries(websocket_demo PRIVATE apollo)

# ========== Event Loop Demo ==========
add_executable(event_loop_demo event_loop_demo.cpp)
target_link_libraries(event_loop_demo PRIVATE apollo)

# ========== Skill System Demo ==========
add_executable(skill_system_demo skill_system_demo.cpp)
target_link_libraries(skill_system_demo PRIVATE apollo)
