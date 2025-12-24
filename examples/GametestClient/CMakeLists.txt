cmake_minimum_required(VERSION 3.16)

project(GametestClient
    VERSION 1.0.0
    DESCRIPTION "Game Test Client with Shell Interface"
    LANGUAGES CXX
)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖包
find_package(nlohmann_json CONFIG REQUIRED)

# 查找 readline 库
if(UNIX)
    find_package(Readline REQUIRED)
endif()

# 添加可执行文件
add_executable(gametest-client
    src/main.cpp
    src/gametest_client.cpp
    src/shell.cpp
)

# 设置包含目录
target_include_directories(gametest-client
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 链接库
target_link_libraries(gametest-client
    PRIVATE
        nlohmann_json::nlohmann_json
)

# Windows 特定设置
if(WIN32)
    target_link_libraries(gametest-client
        PRIVATE
            ws2_32
    )
endif()

# Unix 特定设置
if(UNIX)
    target_link_libraries(gametest-client
        PRIVATE
            ${Readline_LIBRARIES}
    )
    target_include_directories(gametest-client
        PRIVATE
            ${Readline_INCLUDE_DIR}
    )
    target_compile_definitions(gametest-client
        PRIVATE
            ${Readline_DEFINITIONS}
    )
endif()

# 设置编译选项
target_compile_options(gametest-client
    PRIVATE
        $<$<CONFIG:Debug>:-g -O0 -Wall -Wextra>
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
)

# 设置输出目录
set_target_properties(gametest-client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 安装规则
install(TARGETS gametest-client
    RUNTIME DESTINATION bin
)

# 创建配置目录
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/config)