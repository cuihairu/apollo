cmake_minimum_required(VERSION 3.20)

# 查找 NNG 依赖 - 通过 find_package (由 vcpkg toolchain 处理)
find_package(nng CONFIG QUIET)

if(nng_FOUND)
    message(STATUS "NNG found via CONFIG")
elseif(TARGET nng::nng)
    message(STATUS "NNG target already exists")
else()
    message(STATUS "NNG not found, using stub implementation")
endif()

# 协议消息定义
set(PROTOCOL_HEADERS
    include/apollo/protocol/nng_wrapper.hpp
    include/apollo/protocol/messages.hpp
    include/apollo/protocol/codec.hpp
    include/apollo/protocol/socket.hpp
)

set(PROTOCOL_SOURCES
    src/nng_wrapper.cpp
    src/messages.cpp
    src/codec.cpp
    src/socket.cpp
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

# 链接 NNG
if(TARGET nng::nng)
    target_link_libraries(apollo_protocol PUBLIC nng::nng)
    target_compile_definitions(apollo_protocol PRIVATE APOLLO_USE_NNG=1)
elseif(TARGET nng)
    target_link_libraries(apollo_protocol PUBLIC nng)
    target_compile_definitions(apollo_protocol PRIVATE APOLLO_USE_NNG=1)
endif()

target_compile_features(apollo_protocol PUBLIC cxx_std_20)

# alias
add_library(apollo::protocol ALIAS apollo_protocol)

# 测试
if(APOLLO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
