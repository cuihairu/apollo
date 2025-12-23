cmake_minimum_required(VERSION 3.16)

# When using vcpkg, avoid treating imported include dirs as SYSTEM (-isystem).
# Otherwise, globally-installed headers (e.g. /usr/local/include/google/protobuf)
# can be picked up instead of the vcpkg headers, causing ABI mismatches and UB.
if(DEFINED VCPKG_INSTALLED_DIR)
    set(CMAKE_NO_SYSTEM_FROM_IMPORTED ON)
endif()

project(ApolloMMORPG VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Apollo 专用选项
option(APOLLO_BUILD_EXAMPLES "Build example applications" ON)
option(APOLLO_BUILD_TESTS "Build unit tests" ON)
option(APOLLO_ENABLE_COVERAGE "Enable code coverage (requires gcov/lcov)" OFF)
option(APOLLO_ENABLE_DOXYGEN "Generate Doxygen documentation" OFF)
option(APOLLO_ENABLE_CLANG_TIDY "Enable clang-tidy checks" OFF)
option(ENABLE_FILEWATCHER "Enable file watcher support" ON)

# 兼容旧选项
if(BUILD_EXAMPLES)
    set(APOLLO_BUILD_EXAMPLES ON CACHE BOOL "Build example applications" FORCE)
endif()
if(BUILD_TESTS)
    set(APOLLO_BUILD_TESTS ON CACHE BOOL "Build unit tests" FORCE)
endif()

# Apollo 核心库
add_library(apollo STATIC
    # Framework
    src/framework/ioc/ConfigManager.cpp

    # Game
    src/game/aoi/aoi.cpp
    src/game/attributes/attribute.cpp
    src/game/battle/ecs/ecs.cpp

    # Network
    src/network/net_common.cpp
    src/network/socket.cpp
    src/network/protobuf_message.cpp
    src/network/rpc.cpp
    src/network/reactor.cpp

    # Storage
    src/storage/cache/redis.cpp
    src/storage/database/connection_pool.cpp

    # Utils
    src/utils/config/config_loader.cpp
    src/utils/io/FileWatcher.cpp
    src/utils/logging/logger.cpp
    src/utils/threading/thread_pool.cpp
)

# 设置目标名称为 apollo
set_target_properties(apollo PROPERTIES OUTPUT_NAME apollo)

target_include_directories(apollo PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# 添加 CMake 模块路径
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# 依赖项
#
# 说明：
# - Protobuf 出现在 public headers 里（如 include/apollo/network/messaging/protobuf_message.hpp），
#   因此必须以 PUBLIC 方式链接，确保下游目标获得 include/link 需求。
# - nlohmann_json 仅影响实现（ConfigManager.cpp），找不到时回退到 bundled header。

# Protobuf (required)
set(_apollo_protobuf_target "")
find_package(protobuf CONFIG QUIET)
if(TARGET protobuf::libprotobuf)
    set(_apollo_protobuf_target protobuf::libprotobuf)
endif()

if(NOT _apollo_protobuf_target)
    find_package(Protobuf CONFIG QUIET)
    if(TARGET protobuf::libprotobuf)
        set(_apollo_protobuf_target protobuf::libprotobuf)
    elseif(TARGET Protobuf::libprotobuf)
        set(_apollo_protobuf_target Protobuf::libprotobuf)
    endif()
endif()

if(NOT _apollo_protobuf_target)
    message(FATAL_ERROR "Protobuf not found. If you are using vcpkg, configure with -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake")
endif()

# vcpkg may mark dependency includes as SYSTEM (-isystem), which can cause Clang to
# prefer /usr/local/include over the vcpkg headers (same header names, different ABI).
if(TARGET ${_apollo_protobuf_target})
    set_property(TARGET ${_apollo_protobuf_target} PROPERTY INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "")
endif()

target_link_libraries(apollo PUBLIC ${_apollo_protobuf_target})

# nlohmann/json (optional)
find_package(nlohmann_json CONFIG QUIET)
if(nlohmann_json_FOUND)
    target_link_libraries(apollo PRIVATE nlohmann_json::nlohmann_json)
    target_compile_definitions(apollo PRIVATE HAVE_NLOHMANN_JSON=1)
else()
    message(STATUS "nlohmann/json not found, using bundled header")
    target_include_directories(apollo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
endif()

# Fruit DI (required for Spring-like auto-configuration)
find_package(fruit CONFIG QUIET)
if(TARGET fruit)
    target_link_libraries(apollo PUBLIC fruit)
    target_compile_definitions(apollo PRIVATE HAVE_FRUIT=1)
else()
    message(FATAL_ERROR "Fruit not found. If you are using vcpkg, configure with -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake")
endif()

if(ENABLE_FILEWATCHER)
    if(WIN32)
        target_compile_definitions(apollo PRIVATE ENABLE_FILEWATCHER)
    else()
        target_compile_definitions(apollo PRIVATE ENABLE_FILEWATCHER)
    endif()
endif()

# 代码覆盖率支持
if(APOLLO_ENABLE_COVERAGE)
    include(CodeCoverage)
    target_compile_options(apollo PRIVATE ${CMAKE_CXX_FLAGS_COVERAGE})
    target_link_libraries(apollo PRIVATE --coverage)
    message(STATUS "Code coverage enabled")
endif()

# Clang-tidy 支持
if(APOLLO_ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(CLANG_TIDY_EXE)
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
        message(STATUS "Clang-tidy enabled: ${CLANG_TIDY_EXE}")
    else()
        message(WARNING "Clang-tidy requested but not found")
    endif()
endif()

target_compile_features(apollo PUBLIC cxx_std_17)

# 编译选项
if(MSVC)
    target_compile_options(apollo PRIVATE /W4 /WX)
    target_compile_definitions(apollo PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(apollo PRIVATE -Wall -Wextra -Wpedantic -Werror)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # Protobuf/abseil headers may trigger this warning with some Clang setups.
        target_compile_options(apollo PRIVATE -Wno-nullability-extension)
        target_compile_options(apollo PRIVATE -Wno-variadic-macro-arguments-omitted)
        target_compile_options(apollo PRIVATE -Wno-gcc-compat)
    endif()

    # 调试模式下的额外检查
    target_compile_options(apollo PRIVATE
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
    )
endif()

# 导出 compile_commands.json for clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

install(TARGETS apollo
    EXPORT ApolloTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT ApolloTargets
    FILE ApolloTargets.cmake
    NAMESPACE Apollo::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Apollo
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ApolloConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ApolloConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ApolloConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Apollo
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ApolloConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ApolloConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Apollo
)

# Doxygen 文档生成
if(APOLLO_ENABLE_DOXYGEN)
    find_package(Doxygen QUIET)
    if(DOXYGEN_FOUND)
        set(DOXYGEN_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/docs)
        set(DOXYGEN_CONFIG_FILE ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile)

        # 检查 Doxyfile 是否存在
        if(EXISTS ${DOXYGEN_CONFIG_FILE})
            configure_file(${DOXYGEN_CONFIG_FILE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)

            add_custom_target(doc
                COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                COMMENT "Generating API documentation with Doxygen" VERBATIM
            )

            # 添加清理文档的目标
            add_custom_target(doc-clean
                COMMAND ${CMAKE_COMMAND} -E remove_directory ${DOXYGEN_OUTPUT_DIR}
                COMMENT "Cleaning documentation directory"
            )

            message(STATUS "Doxygen documentation enabled")
        else()
            message(WARNING "Doxyfile not found at ${DOXYGEN_CONFIG_FILE}")
        endif()
    else()
        message(WARNING "Doxygen requested but not found")
    endif()
endif()

if(APOLLO_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(APOLLO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
