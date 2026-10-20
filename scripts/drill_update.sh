#!/usr/bin/env bash
# 路1 端到端更新演练（docs/design/backup-revive.md §8 验收，re-home 版）：
# World 排空（fleetctl drain 956/957，期限强停到 drained 终态）→ 原子换
# 二进制 → SIGTERM 优雅停（journal 排空）→ machined 退避重启 → 恢复相位
# 收敛（离线锚点复断 + 全量重报 + 目录镜像）→ 录取面恢复 → 逐玩家存档
# 哈希比对。
# P3-1/P4 re-home 版拓扑：mgr 面 = login、base 面 = world（旧
# baseappmgr/base-app 版随进程退场退役；SIGUSR1/USR2 排空闸由契约控制面
# 956/957 承接）。
#
# 用法：scripts/drill_update.sh
#   APOLLO_BUILD_DIR    构建树（默认 <repo>/build，须 APOLLO_BUILD_GAME_MODULE=ON 已构建）
#   APOLLO_DRILL_DIR    运行目录（默认 /tmp/apollo-drill-update-<pid>）
#   APOLLO_UPDATE_BIN   可选：换版目标二进制（缺省 = 同版重部署，脚本如实
#                       记录 sha256 相同、inode 不同——换版证据以 inode 为准）
#
# 演练拓扑（独立端口段 19610-19614/19013-19014/19105，不与 drill_kill9
# 19600-19602/19003-19005/19015、drill_migration 19621+/19631+/19641+ 冲突）：
#   machined --port 19610 --interval 500 --backoff 1000（编队监督 + 死亡通知）
#   login = login --port 19013（fleetctl assign 拨入面）
#                  --directory-port 19014 --mirror-port 19611
#                  --recovery-port 19612 --recovery-expected 1
#   world = world --m1-port 19105（fleetctl drain --peer world 拨入面）
# 如实边界：world 无 --host/--data-dir/--player-store-m1-port/--report-* 解析面
# （今日无档案目录与迁移报告接线），这些旧 flag 曾静默忽略、现已拒启。
#
# 种档：player_1..3.json 按 PlayerData::toJson 精确字节序种入 world 数据目录
#（flush 以现档为基线回写，roundtrip 字节恒等——前后 sha256 比对即存档一致
# 性断言）。
#
# 如实边界：排空断言在 World 排空面自身（956 状态机 DRAIN_DRAINED 终态）；
# assign 录取面属 Login 目录（排空不停目录报告，录取窗断言只在更新前后取
# 样）；Gateway 排空（954/955）与副本销毁/回滚证据各有专面测试与
# drill_module_update/drill_proto_update，不在本演练重复。
set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${APOLLO_BUILD_DIR:-$REPO_ROOT/build}"
DRILL_DIR="${APOLLO_DRILL_DIR:-/tmp/apollo-drill-update-$$}"
RUN_DIR="$DRILL_DIR/run"
DATA_DIR="$DRILL_DIR/world-data/players"
mkdir -p "$RUN_DIR" "$DATA_DIR"

MACHINED="$BUILD_DIR/apps/machined/machined"
LOGIN="$BUILD_DIR/apps/login/login"
WORLD="$BUILD_DIR/apps/world/world"
FLEETCTL="$BUILD_DIR/apps/fleetctl/fleetctl"

PORT_DISCO=19610
PORT_RECOVERY=19612
PORT_MIRROR=19611
PORT_LOGIN=19013
PORT_LOGIN_DIR=19014
PORT_LOGIN_M1=19614
PORT_WORLD_STORE=19613
PORT_WORLD=19105
WORLD_ID=2

LOG="$DRILL_DIR/fleet.log"
ROSTER="$DRILL_DIR/drill.roster"
ORIG_BACKUP="$DRILL_DIR/world.orig.drill"
FAIL=0

fail() { echo "FAIL: $*" >&2; FAIL=1; }
ok()   { echo "ok:   $*"; }

for bin in "$MACHINED" "$LOGIN" "$WORLD" "$FLEETCTL"; do
    [[ -x "$bin" ]] || { echo "missing binary: $bin —— 先构建（APOLLO_BUILD_GAME_MODULE=ON）" >&2; exit 1; }
done

# 行首 wall-clock 秒戳（HiRes，3 位小数）——分段耗时取自日志本身。
# $|=1 行缓冲：日志是 grep 轮询的断言面，块缓冲会把已写行压在 stdio 里。
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

wait_log_after() { # 等到 min_ts 后出现 pattern
    local pat="$1" t="$2" tmo="${3:-20}" waited=0
    while (( waited < tmo * 20 )); do
        [[ -n "$(ts_after "$pat" "$t")" ]] && return 0
        sleep 0.05; waited=$((waited + 1))
    done
    return 1
}

fmt() { awk -v a="$1" -v b="$2" 'BEGIN { printf "%.3fs", b - a }'; }

probe() { # probe <label> —— World M1 受理面探针（944 eval：受理→tick 执行→
           # 945 回执全链活 = serving 证据；fleetctl assign 属 baseappmgr 期
           # 目录面，re-home 后无服务端，不作录取探针）
    local out rc
    out=$("$FLEETCTL" eval --host 127.0.0.1 --port "$PORT_WORLD" --peer world \
        --source 'return 1' 2>&1)
    rc=$?
    if [[ "$rc" != 0 ]]; then
        fail "$1 (exit=$rc): $out"
    else
        ok "$1（$out）"
    fi
}

cleanup() {
    echo "[drill] teardown begin" >&2
    local mpid; mpid=$(pgrep -x machined | head -1)
    if [[ -n "$mpid" ]]; then
        kill -TERM "$mpid" 2>/dev/null || true
        sleep 2
        pgrep -x machined >/dev/null 2>&1 && kill -KILL "$mpid" 2>/dev/null || true
    fi
    pkill -KILL -x login 2>/dev/null || true
    pkill -KILL -x world 2>/dev/null || true
    echo "[drill] teardown end" >&2
}
trap cleanup EXIT

# 残留进程护栏（同端口编队不互踩；-x 精确匹配进程名，不误伤外层 shell）
pkill -KILL -x login 2>/dev/null || true
pkill -KILL -x world 2>/dev/null || true
pkill -KILL -x machined 2>/dev/null || true
sleep 0.5

# —— 种档：PlayerData::toJson 精确字节序（database_service.cpp:20 字段序、
# 无空格无换行；值取 flush 回写 roundtrip 恒等的简单形态）——
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
login | 2 | 0 | $LOGIN --host 127.0.0.1 --port $PORT_LOGIN --m1-port $PORT_LOGIN_M1 --directory-port $PORT_LOGIN_DIR --directory-epochs $DRILL_DIR/epochs.json --machined-port $PORT_DISCO --component-id 2 --recovery-port $PORT_RECOVERY --recovery-expected 1 --mirror-port $PORT_MIRROR --mirror-snapshot-ms 500 --suspend-window-ticks 30 --world-store-m1-port $PORT_WORLD_STORE
world | 30 | 2 | $WORLD --world-id $WORLD_ID --m1-port $PORT_WORLD --world-journal $DRILL_DIR/world-data/journal/persist.log --machined-port $PORT_DISCO --component-id 30 --zone-id $WORLD_ID
EOF

: > "$LOG"
( cd "$RUN_DIR" && stdbuf -oL "$MACHINED" --port "$PORT_DISCO" --interval 500 \
    --backoff 1000 --roster "$ROSTER" 2>&1 ) | ts >> "$LOG" &

echo "== 演练启动（日志 $LOG，数据 $DATA_DIR）=="
if ! wait_log "admission open" 1 20; then
    tail -30 "$LOG"; fail "initial recovery convergence not observed"; exit 1
fi
if ! wait_log "recovery ready" 1 20; then
    tail -30 "$LOG"; fail "world recovery ready not observed"; exit 1
fi
ok "初始收敛：恢复相位开放 + world recovery ready"
# 种档在启动前落盘 → 首次启动即走离线恢复扫描（档案在、会话无）
wait_log "Anchor restore: 3 anchors offline-resumed" 1 10 || \
    fail "startup anchor restore from seeded profiles not observed"
ok "启动恢复：3 份种档离线复断（Anchor restore: 3）"

T_INTAKE1=$(date +%s.%N)
wait_log_after "full report: component=30" "$T_INTAKE1" 20 || \
    fail "world full report not received"
sleep 1  # 让 intake 后首个镜像快照落日志（周期 500ms）
wait_log_after "snapshot published entries=3 parts" "$T_INTAKE1" 10 || \
    fail "directory mirror not converged (entries=3)"
ok "目录镜像收敛：entries=3（3 份种档全量重报入录）"

WORLD_PID=$(pgrep -x world | head -1)
[[ -n "$WORLD_PID" ]] || { fail "world pid not found"; exit 1; }
ok "baseline：world pid=$WORLD_PID"

# —— 排空前录取面：闸开新玩家可进 ——
probe "排空前探针（World M1 eval 受理面活）"

# —— 更新排空：fleetctl drain --peer world（956/957；期限 1s 强停到
# drained 终态——排空态是进程内状态机，随后续替换显形不复开）——
T_DRAIN=$(date +%s.%N)
DRAIN_OUT=$("$FLEETCTL" drain --target 127.0.0.1:"$PORT_WORLD" --peer world \
    --deadline 1 --timeout 15 2>&1)
DRAIN_RC=$?
echo "$DRAIN_OUT" | sed 's/^/  fleetctl: /'
[[ "$DRAIN_RC" == 0 ]] || fail "world drain did not reach drained (exit=$DRAIN_RC)"
echo "$DRAIN_OUT" | grep -q "DRAIN_BEGIN" || fail "drain begin line missing"
echo "$DRAIN_OUT" | grep -q "DRAIN_DRAINED" || fail "drained terminal line missing"
echo "$DRAIN_OUT" | grep -q "detail=drained:" || fail "drained detail line missing"
# 空副本面自然清账（all instances settled）；期限强停字样（force-aborted）
# 只在副本面有活动的排空窗出现——那条断言在 world_drain e2e（非本演练）
ok "排空终态：drained（$(fmt "$T_DRAIN" "$(date +%s.%N)")）"

# —— 原子换二进制：cp 到同目录临时件 + mv 原子替换（同 fs rename）——
NEW_SRC="${APOLLO_UPDATE_BIN:-$WORLD}"
cp "$WORLD" "$ORIG_BACKUP"
cp "$NEW_SRC" "$WORLD.new.drill"
INODE_RUN_A=$(stat -Lc %i "/proc/$WORLD_PID/exe")
SHA_NEW=$(sha256sum "$WORLD.new.drill" | awk '{print $1}')
SHA_ORIG=$(sha256sum "$ORIG_BACKUP" | awk '{print $1}')
mv -f "$WORLD.new.drill" "$WORLD"
INODE_FILE_NEW=$(stat -c %i "$WORLD")
if [[ "$SHA_NEW" == "$SHA_ORIG" ]]; then
    echo "note: 同版重部署（未设 APOLLO_UPDATE_BIN）——sha256 相同 $SHA_NEW，换版证据以 inode 为准"
else
    ok "换版内容：sha256 $SHA_ORIG → $SHA_NEW"
fi
[[ "$INODE_FILE_NEW" != "$INODE_RUN_A" ]] || fail "swap produced no new inode"
ok "原子换版：运行中 exe inode=$INODE_RUN_A → 磁上新 inode=$INODE_FILE_NEW"

# —— SIGTERM 优雅停：五阶段（M1 收线 → shutdown_flush → journal 排空）——
T_TERM=$(date +%s.%N)
kill -TERM "$WORLD_PID"
# world 全程仅一次死亡（首启是 born）；exit=0 = SIGTERM 优雅停走完五阶段
wait_log "child death world" 1 15 || fail "machined child death line missing"
grep -q "child death world exit=0" "$LOG" || fail "graceful stop did not exit 0"
ok "优雅停：world exit=0（machined 收到死亡行）"
wait_log "journal pending: 0" 1 10 || fail "journal not drained at stop"
ok "journal 排空：stopped (journal pending: 0)"

# —— machined 退避重启（attempt 1 → 1s backoff）+ 换版证据 ——
wait_log "restart world" 1 15 || fail "machined restart line missing"
T_RESTART=$(ts_of "restart world" 1)
wait_log "recovery ready" 2 20 || fail "restarted world recovery ready not observed"
T_READY2=$(ts_of "recovery ready" 2)
wait_log "Anchor restore: 3 anchors offline-resumed" 2 10 || \
    fail "restarted world anchor restore not observed"
WORLD_PID2=$(pgrep -x world | head -1)
[[ -n "$WORLD_PID2" && "$WORLD_PID2" != "$WORLD_PID" ]] || { fail "world pid unchanged after restart"; exit 1; }
INODE_RUN_B=$(stat -Lc %i "/proc/$WORLD_PID2/exe")
[[ "$INODE_RUN_B" == "$INODE_FILE_NEW" ]] || \
    fail "restarted process did not run swapped binary (inode $INODE_RUN_B != $INODE_FILE_NEW)"
ok "换版重启：新 pid=$WORLD_PID2 跑的 exe inode=$INODE_RUN_B = 磁上换版件"

# —— 恢复收敛：全量重报 + 目录镜像（排空态不跨进程：新进程全新受理面）——
wait_log_after "full report: component=30" "$T_READY2" 20 || \
    fail "restarted world full report not received"
LAST_INTAKE=$(grep "full report: component=30" "$LOG" | tail -1)
echo "$LAST_INTAKE" | grep -q "sessions=3 intake=3" || \
    fail "intake count wrong (drop count = 3 - intake): $LAST_INTAKE"
ok "全量重报收敛：$(echo "$LAST_INTAKE" | awk -v FS='full report' '{print "full report"$2}')"
T_INTAKE=$(ts_after "full report: component=30" "$T_READY2")
sleep 1  # 让 intake 后首个镜像快照落日志（周期 500ms）
wait_log_after "entries=3 parts" "$T_INTAKE" 10 || \
    fail "directory mirror not re-converged (entries=3)"
T_MIRROR=$(grep "snapshot published" "$LOG" | awk -v t="$T_INTAKE" '
    $1+0 > t+0 && /entries=3 parts/ { print $1; exit }')
ok "目录镜像收敛：entries=3（无掉线补齐，0 掉线）"

# —— 排空解除：替换后新进程 fresh serving，录取面恢复 ——
probe "排空后探针（替换后新进程 eval 受理面恢复）"

# —— 存档一致性：逐玩家 sha256 前后比对（不是抽查）——
echo
echo "== 存档逐玩家比对（shutdown_flush 回写以现档为基线，roundtrip 应字节恒等）=="
sha256sum -c "$DRILL_DIR/before.sha" || fail "per-player archive hash mismatch"

# —— 分段 RTO ——
T_DEATH=$(ts_of "child death world" 1)
echo
echo "== 路1 更新窗分段 RTO（T0=SIGTERM 时刻；对照 kill -9 全链 drill_kill9）=="
echo "  优雅停   (SIGTERM → child death exit=0):        $(fmt "$T_TERM" "$T_DEATH")"
echo "  退避重启 (→ machined restart, backoff 1s):       $(fmt "$T_TERM" "$T_RESTART")"
echo "  恢复相位 (→ recovery ready):                    $(fmt "$T_TERM" "$T_READY2")"
echo "  全量重报 (→ full report intake=3):              $(fmt "$T_TERM" "$T_INTAKE")"
echo "  镜像收敛 (→ mirror entries=3, 周期 500ms):      $(fmt "$T_TERM" "$T_MIRROR")"
echo "  排空契约面（drain begin→drained 终态）:          $(fmt "$T_DRAIN" "$(date +%s.%N)")"

echo
echo "== 断言清单 =="
[[ $FAIL -eq 0 ]] && ok "全链断言全过" || { echo "== 存在 FAIL 项，见上 ==" >&2; }
exit $FAIL
