add_executable(database src/main.cpp)
target_link_libraries(database PRIVATE apollo_database_rpc)
target_compile_definitions(database PRIVATE
    APOLLO_SERVICE_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json")
target_compile_features(database PRIVATE cxx_std_20)
apollo_package_crash_handler(database)
