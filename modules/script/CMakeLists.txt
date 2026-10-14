# Script 模块（scripting-lua.md §2/§4/§6 第一批——Lua 沙盒宿主 ScriptRuntime）。
# 依赖面最小：仅 vcpkg lua（5.5 线，设计稿 §2 版本锁定纪律）+ 标准库。
find_package(unofficial-lua CONFIG REQUIRED)

add_library(apollo_script STATIC
    src/script_runtime.cpp
    src/lua_bytecode_cache.cpp
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

# §3.5 构建期预编译工具（与运行期同链 lua——bytecode 格式同源；manifest
# 独占写面）。内容管线按需调用；构建默认不产出任何缓存目录。
add_executable(lua_precompile tools/lua_precompile.cpp)
target_link_libraries(lua_precompile PRIVATE apollo::script)
target_compile_features(lua_precompile PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(lua_precompile PRIVATE /utf-8)
endif()
