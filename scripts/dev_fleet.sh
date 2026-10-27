#!/usr/bin/env bash
# Apollo 开发编队：Database / BaseServer / Login / Gateway / World（manager
# + 多 cell 执行端），由 machined 拉起和监督。
# 用法：scripts/dev_fleet.sh up|down|status|roster
# APOLLO_BUILD_DIR 指定构建目录，APOLLO_FLEET_DIR 指定运行目录。
# Database：持久档案存储 9200（data-dir/journal 落 $APOLLO_FLEET_DIR）。
# BaseServer：玩家档案与基础业务受理 9201，存储后端为 Database 9200；
#   编队位置在 Database 之后——存储不可达即拒起（由 machined 退避重试）。
# World：玩法链路 9101（玩家档案链路已迁 BaseServer 9201——L19 资料归属切换；
#   结算落账 = 返回业务请求拨 BaseServer 9201，P1 基础业务与结算迁入）。
#   管理角色声明 `--assign-spaces 401,402` 分配池——多 cell 编队样例：注册的
#   就绪 cell 按最小负载独占认领（401/402 归首个就绪 cell，见
#   world-cell-management L101/L104 分配面）；无 cell 注册时池静默不推。
# Cell 执行端（world --role=cell，L98/L101 多 cell 样例，opt-in）：
#   APOLLO_FLEET_CELLS=<n> 启 n 个 cell（缺省 0 = 不启，既有形态不动）——
#   cell-21 @9301 声明 501:arena/502:lobby、cell-22 @9302 声明 503:duel，均
#   拨 World 管理 M1 9101 注册——960 注册/961 裁决、962 分配应用、967 自报
#   对账面全走真链路。组件号：World=11，Gateway=12，Login=13，Cell=21/22。
#   Database/BaseServer 尚未接 machined 成员注册（号=0，同运维工具口径）；
#   注册接线属后续切片。
# Login：认证/准入链路 9103、在线目录链路 9105；档案客户端拨 BaseServer 9201。
# Gateway：客户端 TCP 9002。logger 9102；admin 指标链路 9106、HTTP 9107。

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${APOLLO_BUILD_DIR:-$REPO_ROOT/build}"
FLEET_DIR="${APOLLO_FLEET_DIR:-/tmp/apollo-fleet-$(id -u)}"
ROSTER="$FLEET_DIR/fleet.roster"
PIDFILE="$FLEET_DIR/machined.pid"
LOGFILE="$FLEET_DIR/machined.log"
MACHINED="$BUILD_DIR/apps/machined/machined"

# Cell 执行端（多 cell 编队样例，opt-in）：APOLLO_FLEET_CELLS=<n> 启 n 个
# cell（1 = cell-21，2 = +cell-22；>2 截 2 并告警——样例仅定义两 cell）。
# 缺省 0 = 既有 7 成员形态不动——编队拓扑缺省值（常驻 vs opt-in）属部署
# 口径拍板面，本批取保守 opt-in 交付样例、缺省零变化。cell 均为 world
# --role=cell：cell-21 于 --m1-port 9301 声明 501:arena/502:lobby，
# cell-22 于 9302 声明 503:duel，均拨 World 管理 M1 9101 注册。
cell_entries() {
    local n="${APOLLO_FLEET_CELLS:-0}"
    [[ "$n" -gt 0 ]] || return 0
    if [[ "$n" -gt 2 ]]; then
        echo "# APOLLO_FLEET_CELLS=$n 截为 2（样例仅定义 cell-21/22）" >&2
        n=2
    fi
    if [[ "$n" -ge 1 ]]; then
        echo "cell-21 | 21 | $BUILD_DIR/apps/world/world --role=cell --world-id 1 --m1-port 9301 --register-host 127.0.0.1 --register-port 9101 --cell-id 21 --add-space 501:arena --add-space 502:lobby --machined-port 9600 --component-id 21"
    fi
    if [[ "$n" -ge 2 ]]; then
        echo "cell-22 | 22 | $BUILD_DIR/apps/world/world --role=cell --world-id 1 --m1-port 9302 --register-host 127.0.0.1 --register-port 9101 --cell-id 22 --add-space 503:duel --machined-port 9600 --component-id 22"
    fi
}

fleet_entries() {
    cat <<EOF
logger    | 0 | $BUILD_DIR/apps/logger/logger serve --port 9102 --archive-dir log/pushed
admin     | 0 | $BUILD_DIR/apps/admin/admin serve
database  | 0 | $BUILD_DIR/apps/database/database --port 9200 --data-dir $FLEET_DIR/data/database/players --journal $FLEET_DIR/data/database/persist.log
base | 0 | $BUILD_DIR/apps/base/base --port 9201 --database-port 9200
world     | 11 | $BUILD_DIR/apps/world/world --world-id 1 --m1-port 9101 --base-business-port 9201 --settlement-credential-dir $FLEET_DIR/data/world/settlements --machined-port 9600 --component-id 11 --assign-spaces 401,402 --script-handlers $REPO_ROOT/apps/world/scripts/contract_handlers.lua --script-contract $REPO_ROOT/sdks/cpp/generated/contract.lua
$(cell_entries)
login     | 13 | $BUILD_DIR/apps/login/login --m1-port 9103 --world-store-m1-port 9201 --directory-port 9105 --gateway tcp://127.0.0.1:9002 --machined-port 9600 --component-id 13
gateway   | 12 | $BUILD_DIR/apps/gateway/gateway --port 9002 --world tcp://127.0.0.1:9101 --world-m1-port 9101 --login-m1-port 9103 --directory-m1-port 9105 --machined-port 9600 --component-id 12
EOF
}

alive() {
    [[ -f "$1" ]] && kill -0 "$(cat "$1")" 2>/dev/null
}

cmd_roster() {
    local missing=0
    while IFS='|' read -r name comp cmd; do
        [[ -z "$name" && -z "$cmd" ]] && continue  # 空白条目行（opt-in 面空扩）
        local bin
        bin="$(echo "$cmd" | awk '{print $1}')"
        if [[ -x "$bin" ]]; then
            echo "$name | $comp | $cmd"
        else
            echo "# 跳过（未构建）：$name | $comp | $cmd" >&2
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
        echo "roster 为空（无已构建 app）——先在 $BUILD_DIR 以 APOLLO_BUILD_GAME_MODULE=ON 构建编队 app" >&2
        exit 1
    fi

    # L4 log push（logging.md §5）：七 app 双出口第二出口对 collector 显形；
    # machined fork+execvp 透传环境——roster logger 行居首（collector 先起），
    # 残余就绪竞态由客户端有界队列吸收（断连降级即常态路径）。
    export APOLLO_LOG_PUSH_ADDR="${APOLLO_LOG_PUSH_ADDR:-127.0.0.1:9102}"

    # L4 metrics push（logging.md §5.1 采样流）：各进程 MetricRegistry 快照
    # 对 admin exporter 显形（kDroppable 周期推，exporter 缺席 = 丢拍）。
    export APOLLO_METRICS_PUSH_ADDR="${APOLLO_METRICS_PUSH_ADDR:-127.0.0.1:9106}"

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
    echo "  指标：curl -s localhost:9107/metrics"
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
    # 孤儿兜底：machined 死后子进程重挂 init，pgrep -P 死 pid 恒空——按二进制名
    # 精确清理（pgrep -x 进程名匹配，避 -f 误中外层 shell 命令行）。
    # terminate_all 已发 SIGTERM：先共享 5s 宽限让慢关子进程自行退出，再 KILL 残留。
    local -a names=()
    while IFS='|' read -r name comp cmd; do
        local bin
        bin="$(echo "$cmd" | awk '{print $1}')"
        [[ -z "$bin" ]] && continue
        names+=("$(basename "$bin")")
    done < <(fleet_entries)
    local n i alive
    for ((i = 0; i < 25; i++)); do
        alive=0
        for n in "${names[@]}"; do
            pgrep -x "$n" >/dev/null 2>&1 && alive=1
        done
        (( alive )) || break
        sleep 0.2
    done
    for n in "${names[@]}"; do
        pkill -KILL -x "$n" 2>/dev/null || true
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
