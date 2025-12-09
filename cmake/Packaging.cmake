# Packaging.cmake - CPack 配置

# 设置包信息
set(CPACK_PACKAGE_NAME "ApolloMMORPG")
set(CPACK_PACKAGE_VENDOR "Apollo Project")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A high-performance, modular MMORPG server development framework")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Apollo")

# 根据平台设置包生成器
if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_DISPLAY_NAME "Apollo MMORPG Server Framework")
    set(CPACK_NSIS_PACKAGE_NAME "Apollo")
    set(CPACK_NSIS_CONTACT "cuihairu@example.com")
    set(CPACK_NSIS_MODIFY_PATH ON)
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop;TGZ")
    set(CPACK_DMG_VOLUME_NAME "Apollo MMORPG Framework")
    set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/docs/images/dmg-background.png")
else()
    set(CPACK_GENERATOR "TGZ;DEB;RPM")

    # DEB 配置
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Apollo Project")
    set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libjson-dev, libprotobuf-dev, libmysqlclient-dev, libhiredis-dev")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

    # RPM 配置
    set(CPACK_RPM_PACKAGE_RELEASE 1)
    set(CPACK_RPM_PACKAGE_GROUP "Development/Libraries")
    set(CPACK_RPM_PACKAGE_VENDOR "Apollo Project")
    set(CPACK_RPM_PACKAGE_REQUIRES "json-devel protobuf-devel mysql-devel hiredis-devel")
endif()

# 设置包组件
set(CPACK_COMPONENTS_ALL
    runtime
    development
    examples
    documentation
)

# Runtime 组件（运行时库）
set(CPACK_COMPONENT_RUNTIME_DISPLAY_NAME "Runtime Libraries")
set(CPACK_COMPONENT_RUNTIME_DESCRIPTION "Apollo MMORPG Server Framework runtime libraries")
set(CPACK_COMPONENT_RUNTIME_REQUIRED ON)

# Development 组件（开发文件）
set(CPACK_COMPONENT_DEVELOPMENT_DISPLAY_NAME "Development Files")
set(CPACK_COMPONENT_DEVELOPMENT_DESCRIPTION "Apollo MMORPG Server Framework headers and CMake files")

# Examples 组件（示例程序）
set(CPACK_COMPONENT_EXAMPLES_DISPLAY_NAME "Example Applications")
set(CPACK_COMPONENT_EXAMPLES_DESCRIPTION "Example applications demonstrating Apollo usage")
set(CPACK_COMPONENT_EXAMPLES_DEPENDS runtime)

# Documentation 组件（文档）
set(CPACK_COMPONENT_DOCUMENTATION_DISPLAY_NAME "Documentation")
set(CPACK_COMPONENT_DOCUMENTATION_DESCRIPTION "API documentation and user guides")

# 安装组件定义
install(TARGETS common
    COMPONENT runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/apollo
    COMPONENT development
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ApolloIoCConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ApolloIoCConfigVersion.cmake"
    COMPONENT development
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ApolloIoC
)

install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
    COMPONENT documentation
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

# 包含 CPack
include(CPack)