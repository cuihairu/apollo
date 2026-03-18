cmake_minimum_required(VERSION 3.20)

# 查找 NNG 依赖 - 直接使用 vcpkg 路径
if(NOT TARGET nng::nng)
    # 尝试使用 vcpkg (CONFIG 模式)
    find_package(nng CONFIG QUIET)

    # 如果 vcpkg 未找到，手动尝试 vcpkg 路径
    if(NOT nng_FOUND)
        if(DEFINED VCPKG_INSTALLED_DIR AND EXISTS "${VCPKG_INSTALLED_DIR}/x64-windows/share/nng/nng-config.cmake")
            message(STATUS "Found nng via VCPKG_INSTALLED_DIR")
            include("${VCPKG_INSTALLED_DIR}/x64-windows/share/nng/nng-config.cmake")
        elseif(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/x64-windows/share/nng/nng-config.cmake")
            message(STATUS "Found nng via build/vcpkg_installed")
            include("${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/x64-windows/share/nng/nng-config.cmake")
        else()
            message(STATUS "nng not found, will need stub implementation")
        endif()
    else()
        message(STATUS "nng found via find_package")
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
        Apollo::base
)

# 链接 NNG
if(TARGET nng::nng)
    target_link_libraries(apollo_protocol PUBLIC nng::nng)
elseif(TARGET nng)
    target_link_libraries(apollo_protocol PUBLIC nng)
endif()

# 如果 nng 已通过 vcpkg 安装，手动设置 include 路径
if(DEFINED VCPKG_INSTALLED_DIR AND EXISTS "${VCPKG_INSTALLED_DIR}/x64-windows/include/nng")
    target_include_directories(apollo_protocol SYSTEM PRIVATE "${VCPKG_INSTALLED_DIR}/x64-windows/include")
    target_link_libraries(apollo_protocol PRIVATE "${VCPKG_INSTALLED_DIR}/x64-windows/lib/nng.lib")
elseif(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/x64-windows/include/nng")
    target_include_directories(apollo_protocol SYSTEM PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/x64-windows/include")
    target_link_libraries(apollo_protocol PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/x64-windows/lib/nng.lib")
else()
    # 尝试直接使用 vcpkg 默认路径
    if(EXISTS "C:/Users/cui/Workspaces/vcpkg/installed/x64-windows/include/nng")
        target_include_directories(apollo_protocol SYSTEM PRIVATE "C:/Users/cui/Workspaces/vcpkg/installed/x64-windows/include")
        target_link_libraries(apollo_protocol PRIVATE "C:/Users/cui/Workspaces/vcpkg/installed/x64-windows/lib/nng.lib")
    endif()
endif()

target_compile_features(apollo_protocol PUBLIC cxx_std_20)

# alias
add_library(Apollo::protocol ALIAS apollo_protocol)

# 测试
if(APOLLO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
