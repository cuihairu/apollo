cmake_minimum_required(VERSION 3.16)
project(ApolloIoC VERSION 1.0.0 LANGUAGES CXX)

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

add_library(common STATIC
    src/common/ConfigManager.cpp
    src/common/FileWatcher.cpp
    src/logger.cpp
    src/thread_pool.cpp
    src/net/net_common.cpp
    src/net/socket.cpp
    src/net/protobuf_message.cpp
    src/net/rpc.cpp
    src/net/reactor.cpp
    src/attribute.cpp
    src/aoi.cpp
    src/battle/ecs.cpp
    src/db/connection_pool.cpp
    src/db/redis.cpp
)

target_include_directories(common PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# 添加 CMake 模块路径
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(nlohmann_json QUIET)

if(nlohmann_json_FOUND)
    target_link_libraries(common PRIVATE nlohmann_json::nlohmann_json)
    target_compile_definitions(common PRIVATE HAVE_NLOHMANN_JSON)
else()
    message(STATUS "nlohmann/json not found, using bundled header")
    target_include_directories(common PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
endif()

if(ENABLE_FILEWATCHER)
    if(WIN32)
        target_compile_definitions(common PRIVATE ENABLE_FILEWATCHER)
    else()
        target_compile_definitions(common PRIVATE ENABLE_FILEWATCHER)
    endif()
endif()

# 代码覆盖率支持
if(APOLLO_ENABLE_COVERAGE)
    include(CodeCoverage)
    target_compile_options(common PRIVATE ${CMAKE_CXX_FLAGS_COVERAGE})
    target_link_libraries(common PRIVATE --coverage)
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

target_compile_features(common PUBLIC cxx_std_17)

# 编译选项
if(MSVC)
    target_compile_options(common PRIVATE /W4 /WX)
    target_compile_definitions(common PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(common PRIVATE -Wall -Wextra -Wpedantic -Werror)

    # 调试模式下的额外检查
    target_compile_options(common PRIVATE
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
    )
endif()

# 导出 compile_commands.json for clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

install(TARGETS common
    EXPORT ApolloIoCTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT ApolloIoCTargets
    FILE ApolloIoCTargets.cmake
    NAMESPACE ApolloIoC::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ApolloIoC
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ApolloIoCConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ApolloIoCConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ApolloIoCConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ApolloIoC
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ApolloIoCConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ApolloIoCConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ApolloIoC
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