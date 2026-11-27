#!/usr/bin/env bash
# 编译并运行 Lua 方案 C/D 最小 demo（不依赖 apollo 主构建）。
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$HERE"

LUA_INC="${LUA_INC:-/home/cui/vcpkg/installed/x64-linux/include}"
LUA_LIB="${LUA_LIB:-/home/cui/vcpkg/installed/x64-linux/lib}"
BUILD="${TMPDIR:-/tmp}/apollo-lua-demos"
mkdir -p "$BUILD"

build_d() {
  echo "=== 编译 Lua 宿主 runner（Lua 5.5.1）==="
  cat > "$BUILD/runner.c" << 'EOF'
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
int main(int argc, char **argv) {
  lua_State *L = luaL_newstate();
  luaL_openlibs(L);
  if (luaL_dofile(L, argv[1])) {
    printf("LUA ERR: %s\n", lua_tostring(L, -1));
    lua_close(L);
    return 1;
  }
  lua_close(L);
  return 0;
}
EOF
  gcc "$BUILD/runner.c" -I"$LUA_INC" -L"$LUA_LIB" -llua -lm -ldl -o "$BUILD/runner"
}

build_c() {
  echo "=== 编译 C++ owner 冒泡 demo ==="
  g++ -std=c++20 "$HERE/demo-c-owner.cpp" -o "$BUILD/demo_c"
}

case "${1:-all}" in
  d|demo-d|D)
    build_d
    echo "=== 运行 Demo D：写入路径覆盖度 ==="
    "$BUILD/runner" "$HERE/demo-d-coverage.lua"
    ;;
  c|demo-c|C)
    build_c
    echo "=== 运行 Demo C：PropertyOwner 冒泡 ==="
    "$BUILD/demo_c"
    ;;
  all|*)
    build_d
    build_c
    echo "=== 运行 Demo D：写入路径覆盖度 ==="
    "$BUILD/runner" "$HERE/demo-d-coverage.lua"
    echo ""
    echo "=== 运行 Demo C：PropertyOwner 冒泡 ==="
    "$BUILD/demo_c"
    ;;
esac
