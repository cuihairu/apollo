cmake_minimum_required(VERSION 3.16)

# When using vcpkg, avoid treating imported include dirs as SYSTEM (-isystem).
# Otherwise, globally-installed headers (e.g. /usr/local/include/google/protobuf)
# can be picked up instead of the vcpkg headers, causing ABI mismatches and UB.
if(DEFINED VCPKG_INSTALLED_DIR)
    set(CMAKE_NO_SYSTEM_FROM_IMPORTED ON)
endif()

project(ApolloMMORPG VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
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

    # Starter (Spring-like auto-configuration)
    src/starter/ApolloApplication.cpp

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
    src/apollo/net/adapters/native_adapter.cpp
    src/apollo/net/session_manager.cpp
    src/apollo/net/message_codec.cpp
    src/apollo/net/rpc.cpp
    src/apollo/net/http.cpp
    src/apollo/net/websocket.cpp
    src/apollo/net/event_loop.cpp
    src/apollo/net/http/rest_client.cpp

    # Redis
    src/apollo/storage/redis/redis.cpp
    src/apollo/storage/redis/redis_client_impl.cpp
    src/apollo/redis/redis_template.cpp

    # Storage
    src/storage/cache/redis.cpp
    src/storage/database/connection_pool.cpp

    # Utils
    src/utils/config/config_loader.cpp
    src/utils/io/FileWatcher.cpp
    src/utils/logging/logger.cpp
    src/utils/threading/thread_pool.cpp
    src/apollo/utils/id_pool.cpp
    src/apollo/utils/crypto.cpp

    # Core - Log
    src/apollo/core/log/logger.cpp
    src/apollo/core/log/file_appender.cpp
    src/apollo/core/log/console_appender.cpp
    src/apollo/core/log/log_manager.cpp

    # Core - Timer
    src/apollo/core/timer/timer_manager.cpp

    # Core - Config
    src/apollo/core/config/config_value.cpp
    src/apollo/core/config/config_manager.cpp

    # Core - Service Discovery
    src/apollo/core/service_discovery.cpp

    # Core - Distributed Lock
    src/apollo/core/distributed_lock.cpp

    # Server
    src/apollo/server/game_server.cpp

    # Storage - Redis
    src/apollo/storage/redis/redis.cpp
    src/apollo/storage/redis/redis_client_impl.cpp

    # Storage - Database
    src/apollo/storage/database/db.cpp
    src/apollo/storage/database/db_common.cpp
    src/apollo/storage/database/db_mysql.cpp
    src/apollo/storage/database/db_mock.cpp

    # Database - SqlTemplate
    src/apollo/database/sql_template.cpp
    src/apollo/database/datasource.cpp

    # IPC - Shared Memory Channel
    src/apollo/ipc/shared_memory_channel.cpp
    src/apollo/ipc/channel.cpp
    src/apollo/ipc/qos.cpp

    # IPC - Service Discovery
    src/apollo/ipc/service_discovery.cpp
    src/apollo/ipc/sqlite_service_discovery.cpp
    src/apollo/ipc/redis_service_discovery.cpp
    src/apollo/ipc/service_endpoint_ex.cpp

    # Actor Framework
    src/apollo/actor/actor_cell.cpp
    src/apollo/actor/actor_system.cpp
    src/apollo/actor/message_bus.cpp
    src/apollo/actor/dispatcher.cpp
    src/apollo/actor/actor_manager.cpp
    src/apollo/actor/monitoring.cpp
    src/apollo/actor/timer_service.cpp
    src/apollo/actor/actor_ref.cpp
    src/apollo/actor/actor_utils.cpp
    src/apollo/actor/message_serialization.cpp

    # BigWorld 兼容层（增量实现）
    src/apollo/bw/runtime.cpp
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

# yaml-cpp (optional for YAML serialization support)
option(APOLLO_ENABLE_YAML "Enable YAML support using yaml-cpp" ON)
if(APOLLO_ENABLE_YAML)
    find_package(yaml-cpp CONFIG QUIET)
    if(NOT yaml-cpp_FOUND)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(yaml-cpp yaml-cpp)
        endif()
    endif()
    if(yaml-cpp_FOUND)
        if(TARGET yaml-cpp::yaml-cpp)
            target_link_libraries(apollo PUBLIC yaml-cpp::yaml-cpp)
        else()
            target_link_libraries(apollo PUBLIC ${yaml-cpp_LIBRARIES})
            target_include_directories(apollo SYSTEM PUBLIC ${yaml-cpp_INCLUDE_DIRS})
        endif()
        target_compile_definitions(apollo PRIVATE APOLLO_HAS_YAML=1)
        message(STATUS "YAML support enabled (yaml-cpp)")
    else()
        message(STATUS "yaml-cpp not found, YAML support disabled (simplified fallback only)")
    endif()
endif()

# OpenSSL (required for WebSocket SHA-1)
find_package(OpenSSL REQUIRED)
if(OpenSSL_FOUND)
    if(TARGET OpenSSL::Crypto)
        target_link_libraries(apollo PUBLIC OpenSSL::Crypto)
    else()
        target_link_libraries(apollo PUBLIC ${OPENSSL_CRYPTO_LIBRARY})
        target_include_directories(apollo SYSTEM PUBLIC ${OPENSSL_INCLUDE_DIR})
    endif()
    message(STATUS "OpenSSL found: ${OPENSSL_VERSION}")
endif()

# libcurl (optional for RestTemplate)
option(APOLLO_ENABLE_CURL "Enable libcurl support for RestTemplate" ON)
if(APOLLO_ENABLE_CURL)
    find_package(CURL CONFIG QUIET)
    if(NOT CURL_FOUND)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(CURL libcurl)
        endif()
    endif()
    if(CURL_FOUND)
        if(TARGET CURL::libcurl)
            target_link_libraries(apollo PUBLIC CURL::libcurl)
        else()
            target_link_libraries(apollo PUBLIC ${CURL_LIBRARIES})
            target_include_directories(apollo SYSTEM PUBLIC ${CURL_INCLUDE_DIRS})
        endif()
        target_compile_definitions(apollo PRIVATE APOLLO_HAS_CURL=1)
        message(STATUS "libcurl found, RestTemplate enabled")
    else()
        message(STATUS "libcurl not found, RestTemplate will be stub only")
    endif()
endif()

# hiredis (optional for RedisTemplate)
option(APOLLO_ENABLE_HIREDIS "Enable hiredis support for RedisTemplate" ON)
if(APOLLO_ENABLE_HIREDIS)
    find_package(hiredis CONFIG QUIET)
    if(NOT hiredis_FOUND)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(hiredis hiredis)
        endif()
    endif()
    if(hiredis_FOUND OR hiredis_FOUND)
        if(TARGET hiredis::hiredis)
            target_link_libraries(apollo PUBLIC hiredis::hiredis)
        elseif(hiredis_FOUND)
            target_link_libraries(apollo PUBLIC ${hiredis_LIBRARIES})
            target_include_directories(apollo SYSTEM PUBLIC ${hiredis_INCLUDE_DIRS})
        else()
            target_link_libraries(apollo PUBLIC hiredis::hiredis)
        endif()
        target_compile_definitions(apollo PRIVATE APOLLO_HAS_HIREDIS=1)
        message(STATUS "hiredis found, RedisTemplate enabled")
    else()
        message(STATUS "hiredis not found, RedisTemplate will be stub only")
    endif()
endif()

# Fruit DI (optional for Spring-like auto-configuration)
# Note: Fruit from vcpkg community triplet doesn't provide CMake config
# We manually add the include paths and libraries
set(FRUIT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/packages/fruit_x64-osx")
if(EXISTS "${FRUIT_ROOT}")
    target_include_directories(apollo SYSTEM PRIVATE "${FRUIT_ROOT}/include")
    target_link_libraries(apollo PRIVATE "${FRUIT_ROOT}/lib/libfruit.a")
    target_compile_definitions(apollo PRIVATE HAVE_FRUIT=1)
    message(STATUS "Fruit DI enabled (vcpkg community triplet)")
else()
    message(STATUS "Fruit not found, using built-in IoC framework")
    target_compile_definitions(apollo PRIVATE APOLLO_USE_BUILTIN_DI=1)
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

target_compile_features(apollo PUBLIC cxx_std_20)

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
