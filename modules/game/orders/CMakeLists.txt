# Game - Orders module (durable order intake + delivery + fulfillment, L63/L64)
add_library(apollo_game_orders STATIC
    src/order_store.cpp
    src/order_delivery.cpp
    src/base_authority.cpp
    src/order_fulfillment.cpp
)
add_library(apollo::game_orders ALIAS apollo_game_orders)

target_include_directories(apollo_game_orders
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_game_orders
    PUBLIC
        apollo::game_core
)

target_compile_features(apollo_game_orders PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_game_orders PUBLIC /utf-8)
    target_compile_definitions(apollo_game_orders PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()
