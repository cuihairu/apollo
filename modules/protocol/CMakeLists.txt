cmake_minimum_required(VERSION 3.20)

# 查找 NNG 依赖
if(NOT TARGET nng::nng)
    # 尝试使用 vcpkg
    find_package(nng QUIET)

    # 如果 vcpkg 未找到，尝试使用 FetchContent
    if(NOT nng_FOUND)
        include(FetchContent)

        FetchContent_Declare(
            nng
            GIT_REPOSITORY https://github.com/nanomsg/nng.git
            GIT_TAG        1.7.3
            GIT_SHALLOW    TRUE
        )

        FetchContent_MakeAvailable(nng)

        # 创建别名
        if(NOT TARGET nng::nng AND TARGET nng)
            add_library(nng::nng ALIAS nng)
        endif()
    endif()
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
elseif(TARGET nng)
    target_link_libraries(apollo_protocol PUBLIC nng)
endif()

target_compile_features(apollo_protocol PUBLIC cxx_std_20)

# alias
add_library(apollo::protocol ALIAS apollo_protocol)

# 测试
if(APOLLO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
