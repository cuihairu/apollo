# Script 模块（scripting-lua.md §2/§4/§6 第一批——Lua 沙盒宿主 ScriptRuntime）。
# 依赖面最小：仅 vcpkg lua（5.5 线，设计稿 §2 版本锁定纪律）+ 标准库。
find_package(unofficial-lua CONFIG REQUIRED)

add_library(apollo_script STATIC
    src/script_runtime.cpp
)
add_library(apollo::script ALIAS apollo_script)

target_include_directories(apollo_script
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_script
    PUBLIC
        unofficial::lua::lua
)

target_compile_features(apollo_script PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(apollo_script PUBLIC /utf-8)
endif()
