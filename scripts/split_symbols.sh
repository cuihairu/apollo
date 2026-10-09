#!/usr/bin/env bash
#
# Apollo 崩溃符号分离（crash-capture §2.3 部署面）。
#
# 用法：
#   scripts/split_symbols.sh <binary> [<binary> ...]
#
# 每个 binary 三步（build-id 同源关联，.debug 收进符号仓、发行件不含符号）：
#   objcopy --only-keep-debug <bin> <bin>.debug
#   strip --strip-debug <bin>
#   objcopy --add-gnu-debuglink=<bin>.debug <bin>
#
# 还原管线（工具 made by scripts/build_crash_tools.sh）：
#   dump_syms <bin> > symbols/<name>/<build-id>/<name>.sym
#   minidump_stackwalk --symbol-path symbols/ <dump>
#
# 边界：部署目标 Linux（net-abstraction §5.8 同口径）；binutils 缺失即显式报缺。

set -euo pipefail

# 平台口径：Linux-only（部署目标），非 Linux 显式退出
if [[ "$(uname -s)" != "Linux" ]]; then
    echo "split_symbols.sh: 仅支持 Linux（当前 $(uname -s)）。" >&2
    exit 1
fi

if [[ $# -eq 0 ]]; then
    echo "用法：$0 <binary> [<binary> ...]" >&2
    exit 1
fi

# binutils 前置检查（objcopy/strip 缺一即报缺）
for tool in objcopy strip; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "split_symbols.sh: 缺少 $tool（binutils 未安装？）。" >&2
        exit 1
    }
done

for bin in "$@"; do
    if [[ ! -e "$bin" ]]; then
        echo "split_symbols.sh: 路径不存在：$bin" >&2
        exit 1
    fi
    if [[ ! -f "$bin" ]]; then
        echo "split_symbols.sh: 不是普通文件：$bin" >&2
        exit 1
    fi

    debug="$bin.debug"
    objcopy --only-keep-debug "$bin" "$debug"
    strip --strip-debug "$bin"
    objcopy --add-gnu-debuglink="$debug" "$bin"

    echo "符号分离：$bin（发行件，已 strip --strip-debug）"
    echo "          $debug（调试符号，按 build-id 关联）"
done
