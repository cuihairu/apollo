#!/usr/bin/env bash
# P0-1 路1 端到端更新演练（docs/design/backup-revive.md §8 验收）：更新排空 →
# 原子换二进制 → SIGTERM 优雅停（journal 排空）→ machined 退避重启 →
# 恢复相位收敛 → 排空解除 → 逐玩家存档哈希比对。
#
# 【待迁移（todo P4 更新排空条目）】：本演练仍指向已退场的 baseappmgr/
# base-app 进程（22438e36 re-home 后 Login/World 承接其面）。P4 更新/排空
# 条目开工时按 drill_kill9.sh 的 re-home 版拓扑迁移；迁移完成前本脚本
# 不作为验收证据。
# 用法：scripts/drill_update.sh
#   APOLLO_BUILD_DIR    构建树（默认 <repo>/build，须 GAME_MODULE=ON 已构建）
#   APOLLO_DRILL_DIR    运行目录（默认 /tmp/apollo-drill-update-<pid>）
#   APOLLO_UPDATE_BIN   可选：换版目标二进制（缺省 = 同版重部署，脚本如实
#                       记录 sha256 相同、inode 不同——换版证据以 inode 为准）
#
# 演练拓扑（独立端口段，不与 drill_kill9 19600-19602/19003-19004 冲突）：
#   machined --port 19610 --interval 500 --backoff 1000（cwd=演练运行目录，
#             fork+exec 子进程继承 cwd —— base-app 档案落运行目录下）
#   mgr  = baseappmgr --m1-port 19105（fleetctl 探针拨入面）
#                    --recovery-port 19612 --recovery-expected 1
#                    --mirror-port 19611（目录镜像 owner）
#   base = base-app --report-to-port 19612 --component-id 30 --zone-id 2
#                  --demo-anchors 3 --mirror-port 19611 --mirror-owner-port 19013
# 种档：player_1..3.json 按数据库 toJson 精确字节序种入（flush 以现档为基线
# 回写，roundtrip 字节恒等 —— 前后 sha256 比对即存档一致性断言）。

set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${APOLLO_BUILD_DIR:-$REPO_ROOT/build}"
DRILL_DIR="${APOLLO_DRILL_DIR:-/tmp/apollo-drill-update-$$}"
RUN_DIR="$DRILL_DIR/run"
mkdir -p "$RUN_DIR"

MACHINED="$BUILD_DIR/apps/machined/machined"
MGR="$BUILD_DIR/apps/baseappmgr/baseappmgr"
BASE="$BUILD_DIR/apps/base-app/base-app"
FLEETCTL="$BUILD_DIR/apps/fleetctl/fleetctl"

PORT_DISCO=19610
PORT_RECOVERY=19612
PORT_MIRROR=19611
PORT_MGR=19013
PORT_BASE=19014
PORT_M1=19105

LOG="$DRILL_DIR/fleet.log"
ROSTER="$DRILL_DIR/drill.roster"
DATA_DIR="$RUN_DIR/base-app-data/players"
FAIL=0

fail() { echo "FAIL: $*" >&2; FAIL=1; }
ok()   { echo "ok:   $*"; }

for bin in "$MACHINED" "$MGR" "$BASE" "$FLEETCTL"; do
    [[ -x "$bin" ]] || { echo "missing binary: $bin —— 先构建（GAME_MODULE=ON）" >&2; exit 1; }
done

# 行首 wall-clock 秒戳（HiRes，3 位小数）——分段耗时取自日志本身（kill9 同款）。
ts() { perl -MTime::HiRes=time -pe 'BEGIN { $| = 1 } s/^/sprintf("%.3f ", time)/e'; }

ts_of() { # ts_of <pattern> [occurrence]
    local pat="$1" occ="${2:-1}"
    awk -v pat="$pat" -v occ="$occ" '
        index($0, pat) { c++; if (c == occ) { print $1; exit } }' "$LOG"
}

ts_after() { # ts_after <pattern> <min_ts>
    awk -v pat="$1" -v t="$2" '
        $1+0 > t+0 && index($0, pat) { print $1; exit }' "$LOG"
}

wait_log() { # wait_log <pattern> <min_count> <timeout_s>
    local pat="$1" need="$2" tmo="${3:-20}" n=0 waited=0
    while (( waited < tmo * 20 )); do
        n=$(grep -cF -- "$pat" "$LOG" 2>/dev/null || true)
        (( n >= need )) && return 0
        sleep 0.05; waited=$((waited + 1))
    done
    return 1
}

wait_log_after() { # wait_log_after <pattern> <min_ts> [timeout_s]
    local pat="$1" t="$2" tmo="${3:-20}" waited=0
    while (( waited < tmo * 20 )); do
        [[ -n "$(ts_after "$pat" "$t")" ]] && return 0
        sleep 0.05; waited=$((waited + 1))
    done
    return 1
}

fmt() { awk -v a="$1" -v b="$2" 'BEGIN { printf "%.3fs", b - a }'; }

probe() { # probe <player_id> <want_rc> <label> —— fleetctl 三态录取断言面
    local out rc want_rc="$2" label="$3"
    out=$("$FLEETCTL" assign --host 127.0.0.1 --port "$PORT_M1" --player "$1" 2>&1)
    rc=$?
    if (( rc == want_rc )); then
        ok "$label: $out (exit=$rc)"
    else
        fail "$label: got exit=$rc want=$want_rc — $out"
    fi
}

ORIG_BACKUP="$BASE.drill-update-orig"
RESTORE_BIN=0
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
    if (( RESTORE_BIN )) && [[ -f "$ORIG_BACKUP" ]]; then
        mv -f "$ORIG_BACKUP" "$BASE"
        echo "[drill] build-tree binary restored" >&2
    fi
    echo "[drill] teardown end" >&2
}
trap cleanup EXIT

# 残留进程护栏（-x 精确匹配进程名）
pkill -KILL -x baseappmgr 2>/dev/null || true
pkill -KILL -x base-app 2>/dev/null || true
pkill -KILL -x machined 2>/dev/null || true
sleep 0.5

# —— 种档：PlayerData::toJson 精确字节序（database_service.cpp:20 字段序、
# 无空格无换行；值取 flush 回写 roundtrip 恒等的简单形态）——
mkdir -p "$DATA_DIR"
seed_profile() { # seed_profile <id> <username> <level> <exp> <hp> <x>
    printf '{"playerId":%s,"username":"%s","level":%s,"exp":%s,"hp":%s,"maxHp":100,"mp":50,"maxMp":50,"x":%s.5,"y":2.5,"z":3.5}' \
        "$1" "$2" "$3" "$4" "$5" "$6" > "$DATA_DIR/player_$1.json"
}
seed_profile 1 drill-alpha 7 1234 88 1
seed_profile 2 drill-beta  8 2345 77 2
seed_profile 3 drill-gamma 9 3456 66 3

# 种档留证（更新前基准哈希——比对在种档处一次成形，不在断言处重算内容）
: > "$DRILL_DIR/before.sha"
sha256sum "$DATA_DIR"/player_{1,2,3}.json > "$DRILL_DIR/before.sha"

cat > "$ROSTER" <<EOF
mgr  | 0 | 0 | $MGR --port $PORT_MGR --m1-port $PORT_M1 --machined-port $PORT_DISCO --recovery-port $PORT_RECOVERY --recovery-expected 1 --mirror-port $PORT_MIRROR --mirror-snapshot-ms 500 --suspend-window-ticks 30
base | 30 | 2 | $BASE --port $PORT_BASE --report-to-port $PORT_RECOVERY --component-id 30 --zone-id 2 --report-interval-ms 5000 --demo-anchors 3 --mirror-port $PORT_MIRROR --mirror-owner-port $PORT_MGR
EOF

: > "$LOG"
( cd "$RUN_DIR" && stdbuf -oL "$MACHINED" --port "$PORT_DISCO" --interval 500 \
    --backoff 1000 --roster "$ROSTER" 2>&1 ) | ts >> "$LOG" &

echo "== 演练启动（日志 $LOG，数据 $DATA_DIR）=="
if ! wait_log "收敛开放" 1 20; then
    tail -30 "$LOG"; fail "initial recovery convergence not observed"; exit 1
fi
if ! wait_log "recovery ready" 1 20; then
    tail -30 "$LOG"; fail "base-app recovery ready not observed"; exit 1
fi
ok "初始收敛：恢复相位开放 + base-app recovery ready"
wait_log "seeded 3 smoke anchors" 1 10 || fail "demo anchors not seeded"
# 种档在启动前落盘 → 首次启动即走离线恢复扫描（档案在、会话无）
wait_log "Anchor restore: 3 anchors offline-resumed" 1 10 || \
    fail "startup anchor restore from seeded profiles not observed"
ok "启动恢复：3 份种档离线复断（Anchor restore: 3）"

BASE_PID=$(pgrep -x base-app | head -1)
[[ -n "$BASE_PID" ]] || { fail "base-app pid not found"; exit 1; }
MGR_PID=$(pgrep -x baseappmgr | head -1)
[[ -n "$MGR_PID" ]] || { fail "baseappmgr pid not found"; exit 1; }
ok "baseline：mgr pid=$MGR_PID base pid=$BASE_PID"

# —— 排空前录取面：闸开新玩家可进 ——
probe 9001 0 "排空前探针（新玩家 assign 放行）"

# —— 更新窗开启：SIGUSR1 → 1s 主循环拍消费（等待必须 ≥1s）——
T_USR1=$(date +%s.%N)
kill -USR1 "$MGR_PID"
wait_log "update drain ON" 1 5 || { fail "drain ON line not observed"; exit 1; }
T_ON=$(ts_of "update drain ON" 1)
ok "排空闸开（信号→生效 $(fmt "$T_USR1" "$T_ON")，1s 拍内）"

# 排空期录取面：拒新不拒链（fleetctl 三态 exit 2 = ASSIGN_REJECTED）
probe 9002 2 "排空期探针 p1（新落点被拒）"
probe 9003 2 "排空期探针 p2（新落点被拒）"
# 零新落点：排空置位后跨 ≥2 个镜像快照周期（500ms），目录条目数不变
sleep 1.3
LAST_SNAP=$(grep "snapshot published" "$LOG" | tail -1 | sed 's/\x1b\[[0-9;]*m//g')
echo "$LAST_SNAP" | grep -q "entries=3 parts" || \
    fail "zero-new-assertion: directory entries changed during drain — $LAST_SNAP"
ok "零新落点：排空窗内镜像快照条目恒 3（$(echo "$LAST_SNAP" | grep -o 'entries=[0-9]* parts=[0-9]*' | head -1)）"

# —— 原子换二进制：cp 到同目录临时件 + mv 原子替换（同 fs rename）——
NEW_SRC="${APOLLO_UPDATE_BIN:-$BASE}"
cp "$BASE" "$ORIG_BACKUP"
RESTORE_BIN=1
cp "$NEW_SRC" "$BASE.new.drill"
INODE_RUN_A=$(stat -Lc %i "/proc/$BASE_PID/exe")
SHA_NEW=$(sha256sum "$BASE.new.drill" | awk '{print $1}')
SHA_ORIG=$(sha256sum "$ORIG_BACKUP" | awk '{print $1}')
mv -f "$BASE.new.drill" "$BASE"
INODE_FILE_NEW=$(stat -c %i "$BASE")
if [[ "$SHA_NEW" == "$SHA_ORIG" ]]; then
    echo "note: 同版重部署（未设 APOLLO_UPDATE_BIN）——sha256 相同 $SHA_NEW，换版证据以 inode 为准"
else
    ok "换版内容：sha256 $SHA_ORIG → $SHA_NEW"
fi
[[ "$INODE_FILE_NEW" != "$INODE_RUN_A" ]] || fail "swap produced no new inode"
ok "原子换版：运行中 exe inode=$INODE_RUN_A → 磁上新 inode=$INODE_FILE_NEW"

# —— SIGTERM 优雅停：五阶段（M1 收线 → shutdown_flush → journal 排空）——
T_TERM=$(date +%s.%N)
kill -TERM "$BASE_PID"
# base 全程仅一次死亡（首启是 born）；exit=0 = SIGTERM 优雅停走完五阶段
wait_log "child death base" 1 15 || fail "machined child death line missing"
grep -q "child death base exit=0" "$LOG" || fail "graceful stop did not exit 0"
ok "优雅停：base exit=0（machined 收到死亡行）"
wait_log "journal pending: 0" 1 10 || fail "journal not drained at stop"
ok "journal 排空：stopped (journal pending: 0)"

# —— machined 退避重启（attempt 1 → 1s backoff）+ 换版证据 ——
wait_log "restart base" 1 15 || fail "machined restart line missing"
T_RESTART=$(ts_of "restart base" 1)
wait_log "recovery ready" 2 20 || fail "restarted base-app recovery ready not observed"
T_READY2=$(ts_of "recovery ready" 2)
wait_log "Anchor restore: 3 anchors offline-resumed" 2 10 || \
    fail "restarted base-app anchor restore not observed"
BASE_PID2=$(pgrep -x base-app | head -1)
[[ -n "$BASE_PID2" && "$BASE_PID2" != "$BASE_PID" ]] || { fail "base-app pid unchanged after restart"; exit 1; }
INODE_RUN_B=$(stat -Lc %i "/proc/$BASE_PID2/exe")
[[ "$INODE_RUN_B" == "$INODE_FILE_NEW" ]] || \
    fail "restarted process did not run swapped binary (inode $INODE_RUN_B != $INODE_FILE_NEW)"
ok "换版重启：新 pid=$BASE_PID2 跑的 exe inode=$INODE_RUN_B = 磁上换版件"

# —— 恢复收敛：全量重报 + 目录镜像 ——
wait_log_after "full report from component=30" "$T_READY2" 20 || \
    fail "restarted base-app full report not received"
LAST_INTAKE=$(grep "full report from component=30" "$LOG" | tail -1)
echo "$LAST_INTAKE" | grep -q "sessions=3 intake=3" || \
    fail "intake count wrong (drop count = 3 - intake): $LAST_INTAKE"
ok "全量重报收敛：$(echo "$LAST_INTAKE" | awk -v FS='full report' '{print "full report"$2}')"
T_INTAKE=$(ts_after "full report from component=30" "$T_READY2")
sleep 1  # 让 intake 后首个镜像快照落日志（周期 500ms）
wait_log_after "entries=3 parts" "$T_INTAKE" 10 || \
    fail "directory mirror not re-converged (entries=3)"
T_MIRROR=$(grep "snapshot published" "$LOG" | awk -v t="$T_INTAKE" '
    $1+0 > t+0 && /entries=3 parts/ { print $1; exit }')
ok "目录镜像收敛：entries=3（无掉线补齐，0 掉线）"

# —— 排空解除：SIGUSR2 → 录取面恢复 ——
T_USR2=$(date +%s.%N)
kill -USR2 "$MGR_PID"
wait_log "update drain OFF" 1 5 || fail "drain OFF line not observed"
T_OFF=$(ts_of "update drain OFF" 1)
ok "排空闸关（信号→生效 $(fmt "$T_USR2" "$T_OFF")）"
probe 9004 0 "排空后探针（新玩家 assign 放行恢复）"

# —— 存档一致性：逐玩家 sha256 前后比对（不是抽查）——
echo
echo "== 存档逐玩家比对（shutdown_flush 回写以现档为基线，roundtrip 应字节恒等）=="
sha256sum -c "$DRILL_DIR/before.sha" || fail "per-player archive hash mismatch"

# —— 分段 RTO ——
T_DEATH=$(ts_of "child death base" 1)
echo
echo "== 路1 更新窗分段 RTO（T0=SIGTERM 时刻；对照 kill -9 全链 2.71s）=="
echo "  优雅停   (SIGTERM → child death exit=0):        $(fmt "$T_TERM" "$T_DEATH")"
echo "  退避重启 (→ machined restart, backoff 1s):       $(fmt "$T_TERM" "$T_RESTART")"
echo "  恢复相位 (→ recovery ready):                    $(fmt "$T_TERM" "$T_READY2")"
echo "  全量重报 (→ full report intake=3):              $(fmt "$T_TERM" "$T_INTAKE")"
echo "  镜像收敛 (→ mirror entries=3, 周期 500ms):      $(fmt "$T_TERM" "$T_MIRROR")"
echo "  排空闸信号→生效 (USR1→ON / USR2→OFF):           $(fmt "$T_USR1" "$T_ON") / $(fmt "$T_USR2" "$T_OFF")"

echo
echo "== 断言清单 =="
[[ $FAIL -eq 0 ]] && ok "全链断言全过" || { echo "== 存在 FAIL 项，见上 ==" >&2; }
exit $FAIL
