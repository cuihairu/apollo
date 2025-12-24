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

# ========== Network Utility Demo ==========
add_executable(netutil_demo netutil_demo.cpp)
target_link_libraries(netutil_demo PRIVATE apollo)

# ========== Simple Server ==========
add_executable(simple_server simple_server.cpp)
target_link_libraries(simple_server PRIVATE apollo)