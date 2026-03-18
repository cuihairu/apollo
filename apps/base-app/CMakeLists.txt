cmake_minimum_required(VERSION 3.20)

# BaseApp 可执行文件
add_executable(base-app
    src/main.cpp
)

target_include_directories(base-app
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(base-app
    PRIVATE
        Apollo::base
        Apollo::core
        Apollo::data_orm
        Apollo::data_cache
        Apollo::protocol
)

target_compile_features(base-app PRIVATE cxx_std_20)

# Windows 特定设置
if(WIN32)
    target_link_libraries(base-app PRIVATE ws2_32)
endif()
