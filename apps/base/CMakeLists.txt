add_executable(base src/main.cpp)
target_link_libraries(base PRIVATE apollo_app_services apollo_database_rpc
    apollo::net_discovery)
target_compile_definitions(base PRIVATE
    APOLLO_SERVICE_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json")
target_compile_features(base PRIVATE cxx_std_20)
apollo_package_crash_handler(base)
