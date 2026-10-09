#!/usr/bin/env bash
# G-2 批 0 验收链演练（docs/design/backup-revive.md §6）：编队 kill -9 全链
# RTO 实测——死亡上 wire → machined 退避重启 → journal replay →
# RecoveryCoordinator ready → 全量重报收敛 → 目录镜像收敛。
#
# 用法：scripts/drill_kill9.sh
#   APOLLO_BUILD_DIR   构建树（默认 <repo>/build，须 GAME_MODULE=ON 已构建）
#   APOLLO_DRILL_DIR   运行目录（默认 /tmp/apollo-drill-kill9-<pid>，演练日志留档）
#
# 演练拓扑（独立端口段，不与 dev_fleet 9001-9005/9600 冲突）：
#   machined --port 19600 --interval 500 --backoff 1000（编队监督 + 死亡通知）
#   mgr  = baseappmgr --recovery-port 19602 --recovery-expected 1（恢复相位）
#                  --mirror-port 19601（目录镜像 owner）
#   base = base-app --report-to-port 19602 --component-id 30 --zone-id 2
#                  --demo-anchors 3 --mirror-port 19601
# 脚本 kill -9 base 后按行首 wall-clock 戳算各段耗时并断言收敛。
# 如实边界：会话面为 --demo-anchors 冒烟种子，客户端 resume 段在目录级
#（epoch/锚点恢复），真实客户端链路归拍板后批次。

set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${APOLLO_BUILD_DIR:-$REPO_ROOT/build}"
DRILL_DIR="${APOLLO_DRILL_DIR:-/tmp/apollo-drill-kill9-$$}"
mkdir -p "$DRILL_DIR"

MACHINED="$BUILD_DIR/apps/machined/machined"
MGR="$BUILD_DIR/apps/baseappmgr/baseappmgr"
BASE="$BUILD_DIR/apps/base-app/base-app"

PORT_DISCO=19600
PORT_RECOVERY=19602
PORT_MIRROR=19601
PORT_MGR=19003
PORT_BASE=19004

LOG="$DRILL_DIR/fleet.log"
ROSTER="$DRILL_DIR/drill.roster"
FAIL=0

fail() { echo "FAIL: $*" >&2; FAIL=1; }
ok()   { echo "ok:   $*"; }

for bin in "$MACHINED" "$MGR" "$BASE"; do
    [[ -x "$bin" ]] || { echo "missing binary: $bin —— 先构建（GAME_MODULE=ON）" >&2; exit 1; }
done

# 行首 wall-clock 秒戳（HiRes，3 位小数）——分段耗时取自日志本身。
# $|=1 行缓冲：日志是 grep 轮询的断言面，块缓冲会把已写行压在 stdio 里
#（实测 child death 行迟到 >10s 致断言超时）。
ts() { perl -MTime::HiRes=time -pe 'BEGIN { $| = 1 } s/^/sprintf("%.3f ", time)/e'; }

ts_of() { # ts_of <pattern> [occurrence] —— 第 occurrence 次出现的行首时间戳
    local pat="$1" occ="${2:-1}"
    awk -v pat="$pat" -v occ="$occ" '
        index($0, pat) { c++; if (c == occ) { print $1; exit } }' "$LOG"
}

ts_after() { # ts_after <pattern> <min_ts> —— min_ts 之后首个出现的时间戳
    awk -v pat="$1" -v t="$2" '
        $1+0 > t+0 && index($0, pat) { print $1; exit }' "$LOG"
}

wait_log() { # wait_log <pattern> <min_count> <timeout_s>（50ms 轮询日志计数）
    local pat="$1" need="$2" tmo="${3:-20}" n=0 waited=0
    while (( waited < tmo * 20 )); do
        n=$(grep -cF -- "$pat" "$LOG" 2>/dev/null || true)
        (( n >= need )) && return 0
        sleep 0.05; waited=$((waited + 1))
    done
    return 1
}

cleanup() {
    echo "[drill] teardown begin" >&2
    local mpid; mpid=$(pgrep -x machined | head -1)
    if [[ -n "$mpid" ]]; then
        kill -TERM "$mpid" 2>/dev/null || true
        sleep 2
        pgrep -x machined >/dev/null 2>&1 && kill -KILL "$mpid" 2>/dev/null || true
    fi
    pkill -KILL -x baseappmgr 2>/dev/null || true
    pkill -KILL -x base-app 2>/dev/null || true
    echo "[drill] teardown end" >&2
}
trap cleanup EXIT

# 残留进程护栏（同端口编队不互踩；-x 精确匹配进程名，不误伤外层 shell）
pkill -KILL -x baseappmgr 2>/dev/null || true
pkill -KILL -x base-app 2>/dev/null || true
pkill -KILL -x machined 2>/dev/null || true
sleep 0.5

cat > "$ROSTER" <<EOF
mgr  | 0 | 0 | $MGR --port $PORT_MGR --machined-port $PORT_DISCO --recovery-port $PORT_RECOVERY --recovery-expected 1 --mirror-port $PORT_MIRROR --mirror-snapshot-ms 500 --suspend-window-ticks 30
base | 30 | 2 | $BASE --port $PORT_BASE --report-to-port $PORT_RECOVERY --component-id 30 --zone-id 2 --report-interval-ms 5000 --demo-anchors 3 --mirror-port $PORT_MIRROR --mirror-owner-port $PORT_MGR
EOF

: > "$LOG"
stdbuf -oL "$MACHINED" --port "$PORT_DISCO" --interval 500 --backoff 1000 \
    --roster "$ROSTER" 2>&1 | ts >> "$LOG" &

echo "== 演练启动（日志 $LOG）=="
if ! wait_log "收敛开放" 1 20; then
    tail -30 "$LOG"; fail "initial recovery convergence not observed"; exit 1
fi
if ! wait_log "recovery ready" 1 20; then
    tail -30 "$LOG"; fail "base-app recovery ready not observed"; exit 1
fi
ok "初始收敛：恢复相位开放 + base recovery ready"
wait_log "seeded 3 smoke anchors" 1 10 || fail "demo anchors not seeded"

BASE_PID=$(pgrep -x base-app | head -1)
[[ -n "$BASE_PID" ]] || { fail "base pid not found"; exit 1; }
ok "kill 前 baseline：base pid=$BASE_PID"

T0=$(date +%s.%N)
kill -KILL "$BASE_PID"

# —— 断言与分段取时 ——
wait_log "child death base" 1 10 || fail "machined child death line missing"
grep -q "child death base exit=137" "$LOG" || fail "exit code is not 137 (128+SIGKILL)"
wait_log "restart base" 1 15 || fail "machined restart line missing"
wait_log "recovery ready" 2 20 || fail "restarted base recovery ready not observed"
wait_log "[window] zone death" 1 10 || fail "zone death window disposition missing"

T3=$(ts_of "recovery ready" 2)
wait_log_after() { # 等到 min_ts 后出现 pattern
    local pat="$1" t="$2" tmo="${3:-20}" waited=0
    while (( waited < tmo * 20 )); do
        [[ -n "$(ts_after "$pat" "$t")" ]] && return 0
        sleep 0.05; waited=$((waited + 1))
    done
    return 1
}
wait_log_after "full report from component=30" "$T3" 20 || \
    fail "restarted base full report not received"

T4=$(ts_after "full report from component=30" "$T3")
LAST_INTAKE=$(grep "full report from component=30" "$LOG" | tail -1)
echo "$LAST_INTAKE" | grep -q "sessions=3 intake=3" || \
    fail "intake count wrong: $LAST_INTAKE"
sleep 1  # 让 intake 后首个镜像快照落日志（周期 500ms）
grep -q "entries=3 parts" "$LOG" || fail "directory mirror not re-converged (entries=3)"
wait_log_after "sweep expired" "$T4" 1 && \
    echo "note: window sweep fired at convergence (窗口期内恢复未达)" || true

T1=$(ts_of "child death base")
T2=$(ts_of "restart base")
# 镜像收敛：intake 之后首个 entries=3 快照（挂起窗口内恢复，entries 3→0→3）
T5=$(grep "snapshot published" "$LOG" | awk -v t="$T4" '
    $1+0 > t+0 && /entries=3 parts/ { print $1; exit }')

fmt() { awk -v a="$1" -v b="$2" 'BEGIN { printf "%.3fs", b - a }'; }

echo
echo "== kill -9 全链分段 RTO（T0=kill 时刻）=="
echo "  死亡检测   (kill → machined child death):      $(fmt "$T0" "$T1")"
echo "  退避重启   (→ machined restart 拉起, backoff 1s): $(fmt "$T0" "$T2")"
echo "  journal replay+ready (→ recovery ready):       $(fmt "$T0" "$T3")"
echo "  全量重报收敛 (→ full report intake):           $(fmt "$T0" "$T4")"
echo "  目录镜像收敛 (→ mirror entries=3, 周期 500ms): $(fmt "$T0" "$T5")"

echo
echo "== 断言清单 =="
[[ $FAIL -eq 0 ]] && ok "全链断言全过" || { echo "== 存在 FAIL 项，见上 ==" >&2; }
exit $FAIL
