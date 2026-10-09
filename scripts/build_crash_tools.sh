#!/usr/bin/env bash
#
# Apollo 崩溃处理工具按需构建（crash-capture §2.3 / §3 验收链）。
#
# 刻意口径：本项目客户端面用 Crashpad（out-of-process handler）、处理面用 Breakpad
# 工具（dump_syms + minidump_stackwalk，minidump 同族——Sentry 同款组合），
# 只取 tools、零 Breakpad 客户端。工具是验收/运维一次件，**不入三树门禁**。
#
# 用法：
#   scripts/build_crash_tools.sh [-h]
#   scripts/build_crash_tools.sh [--src <dir>]
#
# 环境变量：
#   BREAKPAD_SRC  源码/构建目录（默认 <repo>/build/crash-tools/breakpad）
#
# 行为：
#   1. shallow clone google/breakpad → BREAKPAD_SRC（已填充则跳过）
#   2. shallow clone chromium linux-syscall-support → src/third_party/lss（linux 构建必需）
#   3. ./configure && make -j$(nproc)
#   4. 产物 dump_syms / minidump_stackwalk → <repo>/tools/crash-bin/（.gitignore）
#
# 失败纪律：断网/构建失败 → 显式报错并非零退出，绝不静默、绝不回退。

set -euo pipefail

BREAKPAD_REPO="https://github.com/google/breakpad"
LSS_REPO="https://chromium.googlesource.com/linux-syscall-support"

usage() {
    grep '^#' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//' | tail -n +2
}

# 平台口径：Linux-only（部署目标）
if [[ "$(uname -s)" != "Linux" ]]; then
    echo "build_crash_tools.sh: 仅支持 Linux（当前 $(uname -s)）。" >&2
    exit 1
fi

SRC_OVERRIDE=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--help) usage; exit 0 ;;
        --src)
            [[ $# -ge 2 ]] || { echo "build_crash_tools.sh: --src 需要目录参数。" >&2; exit 1; }
            SRC_OVERRIDE="$2"
            shift 2
            ;;
        *)
            echo "build_crash_tools.sh: 未知参数：$1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BREAKPAD_SRC="${SRC_OVERRIDE:-${BREAKPAD_SRC:-$REPO_ROOT/build/crash-tools/breakpad}}"
OUT_DIR="$REPO_ROOT/tools/crash-bin"

for tool in git make nproc; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "build_crash_tools.sh: 缺少 $tool（构建前置未满足）。" >&2
        exit 1
    }
done

# 1. clone breakpad（幂等：目录已有 configure 即跳过）
if [[ -f "$BREAKPAD_SRC/configure" ]]; then
    echo "复用已有 breakpad 源码树：$BREAKPAD_SRC"
else
    echo "shallow clone breakpad → $BREAKPAD_SRC"
    mkdir -p "$(dirname "$BREAKPAD_SRC")"
    if ! git clone --depth 1 "$BREAKPAD_REPO" "$BREAKPAD_SRC"; then
        echo "build_crash_tools.sh: 断网或 clone 失败：$BREAKPAD_REPO（无回退，显式报缺）。" >&2
        exit 1
    fi
fi

# 2. lss 第三方（linux syscall 支持，configure 前置）
LSS_DIR="$BREAKPAD_SRC/src/third_party/lss"
if [[ -d "$LSS_DIR/.git" ]]; then
    echo "复用已有 lss：$LSS_DIR"
else
    echo "shallow clone linux-syscall-support → $LSS_DIR"
    if ! git clone --depth 1 "$LSS_REPO" "$LSS_DIR"; then
        echo "build_crash_tools.sh: 断网或 lss clone 失败：$LSS_REPO（无回退，显式报缺）。" >&2
        exit 1
    fi
fi

# 3. configure + make
echo "构建 breakpad tools（configure + make -j$(nproc)）"
if ! ( cd "$BREAKPAD_SRC" && ./configure && make -j"$(nproc)" ); then
    echo "build_crash_tools.sh: breakpad 构建失败（见上）。无回退，验收面显式报缺。" >&2
    exit 1
fi

DUMP_SYMS="$BREAKPAD_SRC/src/tools/linux/dump_syms/dump_syms"
STACKWALK="$BREAKPAD_SRC/src/processor/minidump_stackwalk"

for artifact in "$DUMP_SYMS" "$STACKWALK"; do
    [[ -x "$artifact" ]] || {
        echo "build_crash_tools.sh: 预期产物缺失：$artifact（构建未产出？）。" >&2
        exit 1
    }
done

# 4. 收产物 + .gitignore（仅在 .gitignore 存在时追行；不存在则建单行）
mkdir -p "$OUT_DIR"
cp "$DUMP_SYMS" "$OUT_DIR/dump_syms"
cp "$STACKWALK" "$OUT_DIR/minidump_stackwalk"

GITIGNORE="$REPO_ROOT/.gitignore"
if [[ -f "$GITIGNORE" ]]; then
    if ! grep -qxF 'tools/crash-bin/' "$GITIGNORE"; then
        echo 'tools/crash-bin/' >> "$GITIGNORE"
        echo "已追加 .gitignore 行：tools/crash-bin/"
    fi
else
    echo 'tools/crash-bin/' > "$GITIGNORE"
    echo "已创建 .gitignore（单行：tools/crash-bin/）"
fi

echo "崩溃工具就绪："
echo "  $OUT_DIR/dump_syms"
echo "  $OUT_DIR/minidump_stackwalk"
