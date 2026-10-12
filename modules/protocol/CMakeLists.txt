cmake_minimum_required(VERSION 3.20)

# 协议消息定义（P4-4 批⑤：nng 传输面退役，模块 = 编解码 + 编址 helper）
set(PROTOCOL_HEADERS
    include/apollo/protocol/messages.hpp
    include/apollo/protocol/codec.hpp
    include/apollo/protocol/url.hpp
)

set(PROTOCOL_SOURCES
    src/messages.cpp
    src/codec.cpp
)

# 创建协议库
add_library(apollo_protocol STATIC
    ${PROTOCOL_HEADERS}
    ${PROTOCOL_SOURCES}
)

target_include_directories(apollo_protocol
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_protocol
    PUBLIC
        apollo::base
)

# nlohmann/json（codec.cpp 实现依赖），与根 CMakeLists 同口径
find_package(nlohmann_json CONFIG QUIET)
if(nlohmann_json_FOUND)
    target_link_libraries(apollo_protocol PRIVATE nlohmann_json::nlohmann_json)
endif()

target_compile_features(apollo_protocol PUBLIC cxx_std_20)

# alias
add_library(apollo::protocol ALIAS apollo_protocol)

# 测试
if(APOLLO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
