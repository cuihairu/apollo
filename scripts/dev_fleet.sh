#!/usr/bin/env bash
#
# Apollo dev 编队一键起停（P3-4；machined 批 F「拉起/重启」半边的包装）。
#
# 用法：
#   scripts/dev_fleet.sh up      # 生成 roster 并后台拉起 machined（含编队子进程）
#   scripts/dev_fleet.sh down    # SIGTERM machined（子进程随批 F 关停面收割）
#   scripts/dev_fleet.sh status  # machined 存活 + 子进程清单
#   scripts/dev_fleet.sh roster  # 打印本次编队 roster 内容
#
# 环境变量：
#   APOLLO_BUILD_DIR  构建树（默认 <repo>/build，须 GAME_MODULE=ON 全 app 已构建）
#   APOLLO_FLEET_DIR  运行目录（默认 /tmp/apollo-fleet-<uid>：roster + pid + 日志）
#
# dev 端口布局（仅本脚本约定的开发面，无配置真相——生产部署不由此起）：
#   login 9001 / gateway 9002 / baseappmgr 9003 / base 9004 / cell 9005
#   （app 缺省端口以各自 config 为准，本脚本显式传参避免歧义）
#
# 边界：编队 = 生命周期半边（拉起/重启/收割）；各 app 尚未接 DiscoveryBeacon
# （G-1 骨架期），machined 目录面看不到成员注册——P3-1 G-1 收尾批接线。

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${APOLLO_BUILD_DIR:-$REPO_ROOT/build}"
FLEET_DIR="${APOLLO_FLEET_DIR:-/tmp/apollo-fleet-$(id -u)}"
ROSTER="$FLEET_DIR/fleet.roster"
PIDFILE="$FLEET_DIR/machined.pid"
LOGFILE="$FLEET_DIR/machined.log"
MACHINED="$BUILD_DIR/apps/machined/machined"

# 编队清单：name | 二进制 相对构建根 | 额外参数（dev 端口见文件头布局）
fleet_entries() {
    cat <<EOF
login     | $BUILD_DIR/apps/login-app/login-app --port 9001
gateway   | $BUILD_DIR/apps/gateway-app/gateway-app --port 9002
baseappmgr| $BUILD_DIR/apps/baseappmgr/baseappmgr --port 9003
base      | $BUILD_DIR/apps/base-app/base-app --port 9004
cell      | $BUILD_DIR/apps/cell-app/cell-app --port 9005
game      | $BUILD_DIR/apps/game-server/apollo_game_server
EOF
}

alive() {
    [[ -f "$1" ]] && kill -0 "$(cat "$1")" 2>/dev/null
}

cmd_roster() {
    local missing=0
    while IFS='|' read -r name cmd; do
        local bin
        bin="$(echo "$cmd" | awk '{print $1}')"
        if [[ -x "$bin" ]]; then
            echo "$name | $cmd"
        else
            echo "# 跳过（未构建）：$name | $cmd" >&2
            missing=1
        fi
    done < <(fleet_entries)
    return 0
}

cmd_up() {
    if alive "$PIDFILE"; then
        echo "编队已在运行（machined pid $(cat "$PIDFILE")）；down 后再 up。" >&2
        exit 1
    fi
    [[ -x "$MACHINED" ]] || { echo "machined 未构建：$MACHINED" >&2; exit 1; }

    mkdir -p "$FLEET_DIR"
    : > "$LOGFILE"
    cmd_roster > "$ROSTER"

    local spawned=0
    while IFS= read -r line; do
        [[ "$line" == \#* || -z "$line" ]] && continue
        spawned=$((spawned + 1))
    done < "$ROSTER"
    if [[ "$spawned" -eq 0 ]]; then
        echo "roster 为空（无已构建 app）——先在 $BUILD_DIR 以 GAME_MODULE=ON 构建编队 app" >&2
        exit 1
    fi

    nohup "$MACHINED" --roster "$ROSTER" >> "$LOGFILE" 2>&1 &
    echo $! > "$PIDFILE"
    sleep 0.3
    if ! alive "$PIDFILE"; then
        echo "machined 启动失败，日志尾部：" >&2
        tail -5 "$LOGFILE" >&2
        rm -f "$PIDFILE"
        exit 1
    fi
    echo "编队已起（machined pid $(cat "$PIDFILE")，$spawned 个编队成员）"
    echo "  目录面 UDP 0.0.0.0:9600；运行目录 $FLEET_DIR"
    echo "  日志：tail -f $LOGFILE"
    echo "  停止：scripts/dev_fleet.sh down"
}

cmd_down() {
    if ! alive "$PIDFILE"; then
        echo "编队未在运行。" >&2
        rm -f "$PIDFILE"
        exit 0
    fi
    local pid
    pid="$(cat "$PIDFILE")"
    echo "SIGTERM machined（pid $pid），子进程随关停面收割……"
    kill -TERM "$pid"
    for _ in $(seq 1 50); do
        kill -0 "$pid" 2>/dev/null || break
        sleep 0.2
    done
    if kill -0 "$pid" 2>/dev/null; then
        echo "10s 未退，强杀 machined（子进程成孤儿，需手动清理）" >&2
        kill -KILL "$pid" 2>/dev/null || true
    fi
    pgrep -P "$pid" 2>/dev/null | while read -r child; do
        kill -KILL "$child" 2>/dev/null || true
    done
    rm -f "$PIDFILE"
    echo "编队已停。"
}

cmd_status() {
    if alive "$PIDFILE"; then
        local pid
        pid="$(cat "$PIDFILE")"
        echo "machined: 运行中（pid $pid）"
        local children
        children="$(pgrep -P "$pid" 2>/dev/null | tr '\n' ' ' || true)"
        if [[ -n "${children// /}" ]]; then
            echo "编队子进程: $children"
        else
            echo "编队子进程: 无（可能全部退出；退避重启面见 $LOGFILE）"
        fi
    else
        echo "machined: 未运行"
        rm -f "$PIDFILE" 2>/dev/null || true
    fi
}

case "${1:-}" in
    up)     cmd_up ;;
    down)   cmd_down ;;
    status) cmd_status ;;
    roster) cmd_roster ;;
    *)
        grep '^#' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
        exit 1
        ;;
esac
