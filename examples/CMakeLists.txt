add_executable(ioc_example basic_example.cpp)
target_link_libraries(ioc_example PRIVATE apollo)

add_executable(config_example config_example.cpp)
target_link_libraries(config_example PRIVATE apollo)

add_executable(dependency_example dependency_example.cpp)
target_link_libraries(dependency_example PRIVATE apollo)

add_executable(all_features_demo all_features_demo.cpp)
target_link_libraries(all_features_demo PRIVATE apollo)

# ========== ECS Demo ==========
add_executable(ecs_demo ecs_demo.cpp)
target_link_libraries(ecs_demo PRIVATE apollo)

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

# ========== Config Serialization Demo ==========
add_executable(config_serialization_demo config_serialization_demo.cpp)
target_link_libraries(config_serialization_demo PRIVATE apollo)

# ========== Network Utility Demo ==========
add_executable(netutil_demo netutil_demo.cpp)
target_link_libraries(netutil_demo PRIVATE apollo)

# ========== Simple Server ==========
add_executable(simple_server simple_server.cpp)
target_link_libraries(simple_server PRIVATE apollo)

# Starter 自动配置示例
add_executable(starter_example starter_example.cpp)
target_link_libraries(starter_example PRIVATE apollo)

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

# ========== Actor Demo ==========
add_executable(actor_demo actor_demo.cpp)
target_link_libraries(actor_demo PRIVATE apollo)

# ========== Actor Advanced Demo ==========
add_executable(actor_advanced_demo actor_advanced_demo.cpp)
target_link_libraries(actor_advanced_demo PRIVATE apollo)

# ========== Unified Serialization Demo ==========
add_executable(serialization_demo serialization_demo.cpp)
target_link_libraries(serialization_demo PRIVATE apollo)
