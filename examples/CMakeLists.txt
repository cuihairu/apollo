# ========== RPC Demo ==========
add_executable(rpc_demo rpc_demo.cpp)
target_link_libraries(rpc_demo PRIVATE apollo)

# ========== Session Demo ==========
add_executable(session_demo session_demo.cpp)
target_link_libraries(session_demo PRIVATE apollo)

# ========== Game Server Demo ==========
add_executable(game_server_demo game_server_demo.cpp)
target_link_libraries(game_server_demo PRIVATE apollo)

# ========== Benchmark ==========
add_executable(benchmark benchmark.cpp)

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
