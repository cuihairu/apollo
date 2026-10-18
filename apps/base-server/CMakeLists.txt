add_executable(baseServer src/main.cpp)
target_link_libraries(baseServer PRIVATE apollo_app_services apollo_database_rpc)
target_compile_definitions(baseServer PRIVATE
    APOLLO_SERVICE_ROUTE_JSON="${CMAKE_SOURCE_DIR}/sdks/cpp/generated/contract_route.json")
target_compile_features(baseServer PRIVATE cxx_std_20)
apollo_package_crash_handler(baseServer)
