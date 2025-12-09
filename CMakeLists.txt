cmake_minimum_required(VERSION 3.16)
project(ApolloIoC VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_EXAMPLES "Build example applications" ON)
option(BUILD_TESTS "Build unit tests" ON)
option(ENABLE_FILEWATCHER "Enable file watcher support" ON)

add_library(common STATIC
    src/common/ConfigManager.cpp
    src/common/FileWatcher.cpp
)

target_include_directories(common PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

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

target_compile_features(common PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(common PRIVATE /W4)
else()
    target_compile_options(common PRIVATE -Wall -Wextra -Wpedantic)
endif()

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

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()