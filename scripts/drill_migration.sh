#!/usr/bin/env bash
# P3-5 跨 World 迁移演练 re-home 版（docs/zh/design/backup-revive.md §9.2 五步
# ①②③④ 编排 + §9.8「单玩家切换耗时」实测件）：真 login（目录 owner +
# 942 编排受理面）+ 两真 world（源 zone 2 / 目标 zone 5，真 PlayerStore
# M1 面，933/935/936/937 迁移 wire 经 StoreDial 真拨），fleetctl migrate
# 驱动（942→943，与测试走同一 dispatchRequest 受理面），逐段耗时取自 943
# 回执（不墙钟反推）。
#
# 用法：scripts/drill_migration.sh
#   APOLLO_BUILD_DIR   构建树（默认 <repo>/build，须 APOLLO_BUILD_GAME_MODULE=ON 已构建）
#   APOLLO_DRILL_DIR   运行目录（默认 /tmp/apollo-drill-migration-<pid>）
#   APOLLO_MIG_PLAYER  迁移玩家号（默认 42）
#
# 演练拓扑（独立端口段，不与 drill_update 19610-19614/19013-19014/19105、
# drill_kill9 19600-19602/19003-19005/19015、dev_fleet 9001-9005/9101-9107
# 冲突）：
#   login = login --port 19623 --directory-port 19625（fleetctl 编排命令面）
#                   --recovery-port 19622 --recovery-expected 2（两报告方收敛）
#                   --mirror-port 19626 --mirror-snapshot-ms 30000
#                   （30s 周期 = 演练窗内 ④ 后恰有一次周期快照，作回卷断言面）
#   old   = world --world-id 2 --component-id 30 --zone-id 2 --world-journal <…>
#   new   = world --world-id 5 --component-id 31 --zone-id 5 --world-journal <…>
# 如实边界：world 无 --host/--data-dir/--player-store-m1-port/--report-* 解析面
# （今日无档案目录与迁移报告接线），这些旧 flag 曾静默忽略、现已拒启；
# 目标端档案落盘断言依赖该档案目录面，未接线前该段断言会 fail。
# 种档（会话面无种子工具——旧 base-app --demo-anchors 随进程退场）：种子档
# player_<N>.json 落源端 dataDir，PlayerStore 启动 restore 离线建锚（档案在、
# 会话无）→ 全量重报 intake 落目录。会话绑定列为空是该种子形态的如实边界
#（迁移 ②③ 快照带空会话；会话/网关双锚恢复随迁移链测试 test_migration_chain）。
#
# 断言面（wire 优先，日志为落证）：镜像观察端 = 独立 UDP 收包进程解析 APD2
# 快照（§9.2 ④ 改指立即推 + 30s 周期快照都看 wire 条目 zone/epoch，日志不算
# 证据）；存档一致 = 新侧档案 sha256 与种子档恒等（flush 回写 roundtrip）。
#
# 口径注记（如实）：五步 ⑤（gateway 上行缓冲放行 + 老侧摘锚）未接线
#（§9.9-2），实测 ①停写→④改指编排段（943 total_ms）；源侧停留 Transferring
#（无 unfreeze 契约面——§9.2「人工介入」形态），且冻结锚点离源端在册集——
# 源端周期重报不再携带（防 intake 回卷 ④ 改指），目标域独占报告权。

set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${APOLLO_BUILD_DIR:-$REPO_ROOT/build}"
DRILL_DIR="${APOLLO_DRILL_DIR:-/tmp/apollo-drill-migration-$$}"
PLAYER="${APOLLO_MIG_PLAYER:-42}"

RUN_DIR="$DRILL_DIR/run"
OLD_DIR="$DRILL_DIR/old"
NEW_DIR="$DRILL_DIR/new"
OLD_DATA="$OLD_DIR/players"
NEW_DATA="$NEW_DIR/players"
mkdir -p "$RUN_DIR" "$OLD_DATA" "$NEW_DATA"

LOGIN="$BUILD_DIR/apps/login/login"
WORLD="$BUILD_DIR/apps/world/world"
FLEETCTL="$BUILD_DIR/apps/fleetctl/fleetctl"

PORT_LOGIN=19623
PORT_LOGIN_DIR=19625     # 目录 M1 控制面（fleetctl 编排命令面）
PORT_RECOVERY=19622      # 全量重报 intake（两 zone 报告方）
PORT_MIRROR=19626        # 目录镜像发布（观察端收 APD2 快照）
M1_OLD=19631
STORE_OLD=19632          # 源 store M1 面（① 933 受理 + ② 935/936 应答）
M1_NEW=19641
STORE_NEW=19642          # 目标 store M1 面（③ 937 接权 + 现场拉取）

LOG="$DRILL_DIR/fleet.log"
OLD_LOG="$DRILL_DIR/old.log"
NEW_LOG="$DRILL_DIR/new.log"
MIRROR_LOG="$DRILL_DIR/mirror.log"
FAIL=0

fail() { echo "FAIL: $*" >&2; FAIL=1; }
ok()   { echo "ok:   $*"; }

for bin in "$LOGIN" "$WORLD" "$FLEETCTL"; do
    [[ -x "$bin" ]] || { echo "missing binary: $bin —— 先构建（APOLLO_BUILD_GAME_MODULE=ON）" >&2; exit 1; }
done

ts() { perl -MTime::HiRes=time -pe 'BEGIN { $| = 1 } s/^/sprintf("%.3f ", time)/e'; }
fmt() { awk -v a="$1" -v b="$2" 'BEGIN { printf "%.3fs", b - a }'; }

wait_log() { # wait_log <file> <pattern> <min_count> <timeout_s>
    local f="$1" pat="$2" need="$3" tmo="${4:-20}" n=0 waited=0
    while (( waited < tmo * 20 )); do
        n=$(grep -cF -- "$pat" "$f" 2>/dev/null || true)
        (( n >= need )) && return 0
        sleep 0.05; waited=$((waited + 1))
    done
    return 1
}

wait_log_match() { # wait_log_match <file> <regex> <min_count> <timeout_s>
    local f="$1" pat="$2" need="$3" tmo="${4:-20}" n=0 waited=0
    while (( waited < tmo * 20 )); do
        n=$(grep -cE -- "$pat" "$f" 2>/dev/null || true)
        (( n >= need )) && return 0
        sleep 0.05; waited=$((waited + 1))
    done
    return 1
}

wait_log_after() { # wait_log_after <file> <pattern> <min_ts> [timeout_s]
    local f="$1" pat="$2" t="$3" tmo="${4:-20}" waited=0
    while (( waited < tmo * 20 )); do
        awk -v pat="$pat" -v t="$t" '
            $1+0 > t+0 && index($0, pat) { found = 1; exit }
            END { exit found ? 0 : 1 }' "$f" >/dev/null 2>&1 && return 0
        sleep 0.05; waited=$((waited + 1))
    done
    return 1
}

ts_of() { # ts_of <file> <pattern> [occurrence]
    local f="$1" pat="$2" occ="${3:-1}"
    awk -v pat="$pat" -v occ="$occ" '
        index($0, pat) { c++; if (c == occ) { print $1; exit } }' "$f"
}

ts_after() { # ts_after <file> <pattern> <min_ts>
    local f="$1" pat="$2" t="$3"
    awk -v pat="$pat" -v t="$t" '
        $1+0 > t+0 && index($0, pat) { print $1; exit }' "$f"
}

log_line() { # log_line <file> <pattern> [occurrence]
    local f="$1" pat="$2" occ="${3:-1}"
    awk -v pat="$pat" -v occ="$occ" '
        index($0, pat) { c++; if (c == occ) { print; exit } }' "$f"
}

LOGIN_PID=""
OLD_PID=""
NEW_PID=""
WATCH_PID=""
cleanup() {
    echo "[drill] teardown begin" >&2
    for p in "$LOGIN_PID" "$OLD_PID" "$NEW_PID" "$WATCH_PID"; do
        [[ -n "$p" ]] && kill -TERM "$p" 2>/dev/null || true
    done
    sleep 1
    for p in "$LOGIN_PID" "$OLD_PID" "$NEW_PID" "$WATCH_PID"; do
        [[ -n "$p" ]] && kill -KILL "$p" 2>/dev/null || true
    done
    pkill -KILL -x login 2>/dev/null || true
    pkill -KILL -x world 2>/dev/null || true
    echo "[drill] teardown end" >&2
}
trap cleanup EXIT

# 残留进程护栏（-x 精确匹配进程名）
pkill -KILL -x login 2>/dev/null || true
pkill -KILL -x world 2>/dev/null || true
sleep 0.5

# —— 种档：PlayerData::toJson 精确字节序（database_service 字段序、
# 无空格无换行；值取 flush 回写 roundtrip 恒等的简单形态）——
printf '{"playerId":%s,"username":"mig-alpha","level":6,"exp":1000,"hp":88,"maxHp":100,"mp":50,"maxMp":50,"x":42.5,"y":2.5,"z":3.5}' \
    "$PLAYER" > "$OLD_DATA/player_${PLAYER}.json"
BEFORE_SHA=$(sha256sum "$OLD_DATA/player_${PLAYER}.json" | awk '{print $1}')
: > "$LOG"; : > "$MIRROR_LOG"

# —— 镜像观察端（wire 断言面）：APD2 kind3 快照逐条目解出 zone/epoch ——
cat > "$RUN_DIR/mirror_watch.py" <<PYEOF
import socket, struct, sys

wire = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
wire.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1 << 20)
wire.bind(("127.0.0.1", $PORT_MIRROR))
entry = struct.Struct("<QQQII B I QQQQ")  # 69B 小端（world_id 后无对齐）
while True:
    data, _ = wire.recvfrom(65535)
    if len(data) < 8 or struct.unpack_from("<I", data, 0)[0] != 0x41504432:
        continue
    version, kind = struct.unpack_from("<HB", data, 4)
    if version != 1 or kind != 3:
        continue
    snap_id, total_parts, part_index = struct.unpack_from("<IHH", data, 8)
    # 布局：8B 头 + snapshot_id u32@8 | total_parts u16@12 | part_index
    # u16@14 | baseline_seq u32@16 | entry_count u16@20 → 条目区 @22
    entry_count = struct.unpack_from("<H", data, 20)[0]
    off = 22
    for _ in range(entry_count):
        if off + entry.size > len(data):
            break
        (pid, epoch, _sid, _gw, zone, _state, _wid, _map,
         _inst, _space, _route) = entry.unpack_from(data, off)
        off += entry.size
        if pid != 0:
            print("MIRROR p=%d zone=%d epoch=%d snap=%d part=%d/%d"
                  % (pid, zone, epoch, snap_id, part_index, total_parts),
                  flush=True)
PYEOF
python3 -u "$RUN_DIR/mirror_watch.py" >> "$MIRROR_LOG" 2>&1 &
WATCH_PID=$!

# —— 目录 owner：恢复相位（expected 2）+ intake + 镜像发布面 ——
( cd "$RUN_DIR" && stdbuf -oL "$LOGIN" --host 127.0.0.1 --port "$PORT_LOGIN" \
    --m1-port 19624 \
    --directory-port "$PORT_LOGIN_DIR" --directory-epochs "$DRILL_DIR/epochs.json" \
    --mirror-port "$PORT_MIRROR" --mirror-snapshot-ms 30000 \
    --recovery-port "$PORT_RECOVERY" --recovery-expected 2 \
    --world-store-m1-port "$STORE_OLD" --component-id 2 \
    2>&1 ) | ts >> "$LOG" &

# —— 源 zone 2：种档离线建锚 + 全量重报 ——
( cd "$OLD_DIR" && stdbuf -oL "$WORLD" --world-id 2 \
    --m1-port "$M1_OLD" --world-journal "$OLD_DIR/journal/persist.log" \
    --machined-port 0 --component-id 30 --zone-id 2 \
    2>&1 ) | ts >> "$OLD_LOG" &

# —— 目标 zone 5：空载承接 + 全量重报（迁移后其报告独占 42）——
( cd "$NEW_DIR" && stdbuf -oL "$WORLD" --world-id 5 \
    --m1-port "$M1_NEW" --world-journal "$NEW_DIR/journal/persist.log" \
    --machined-port 0 --component-id 31 --zone-id 5 \
    2>&1 ) | ts >> "$NEW_LOG" &

sleep 0.5
LOGIN_PID=$(pgrep -x login | head -1)
OLD_PID=$(pgrep -x world | head -1)
NEW_PID=$(pgrep -x world | tail -1)
for pid in "$LOGIN_PID" "$OLD_PID" "$NEW_PID"; do
    [[ -n "$pid" ]] || { fail "process pid missing"; exit 1; }
done

echo "== 演练启动（日志 $LOG / $OLD_LOG / $NEW_LOG / wire $MIRROR_LOG）=="

# —— 收敛：两报告方全报 → 准入开（expected 2）——
if ! wait_log "$LOG" "admission open" 1 20; then
    tail -30 "$LOG"; tail -30 "$OLD_LOG"; tail -30 "$NEW_LOG"
    fail "login recovery convergence not observed"; exit 1
fi
ok "恢复收敛：两报告方全报，准入开放"
if ! wait_log "$LOG" "full report: component=30" 1 10; then
    fail "source zone full report not received"; exit 1
fi
INTAKE_LINE=$(grep "full report: component=30" "$LOG" | tail -1)
echo "$INTAKE_LINE" | grep -q "sessions=1 intake=1" || \
    fail "source intake count wrong: $INTAKE_LINE"
ok "源端在册：种子档离线建锚，重报 intake 落目录（$INTAKE_LINE）"

# —— 迁移前基线（wire）：收敛快照 = 42 在 zone 2 ——
if ! wait_log "$MIRROR_LOG" "p=${PLAYER} zone=2" 1 10; then
    fail "pre-migration mirror baseline (zone=2) not observed"; exit 1
fi
ok "迁移前基线：镜像快照 wire 条目 p=${PLAYER} zone=2 epoch=1"

# —— 五步编排：fleetctl migrate（942 → 943 同步等回执）——
T0=$(date +%s.%N)
MIG_OUT=$("$FLEETCTL" migrate --host 127.0.0.1 --port "$PORT_LOGIN_DIR" \
    --player "$PLAYER" \
    --source-host 127.0.0.1 --source-port "$STORE_OLD" \
    --target-host 127.0.0.1 --target-port "$STORE_NEW" \
    --target-zone 5 2>&1)
RC_MIG=$?
T1=$(date +%s.%N)

if (( RC_MIG != 0 )); then
    tail -20 "$LOG"; tail -20 "$OLD_LOG"; tail -20 "$NEW_LOG"
    fail "migrate command failed: $MIG_OUT (exit=$RC_MIG)"; exit 1
fi
echo "$MIG_OUT" | grep -q "MIGRATE_OK" || fail "migrate not ok: $MIG_OUT"
FREEZE_MS=$(echo "$MIG_OUT" | sed -n 's/.*freeze_ms=\([0-9-]*\).*/\1/p')
HANDOFF_MS=$(echo "$MIG_OUT" | sed -n 's/.*handoff_ms=\([0-9-]*\).*/\1/p')
TOTAL_MS=$(echo "$MIG_OUT" | sed -n 's/.*total_ms=\([0-9-]*\).*/\1/p')
EPOCH=$(echo "$MIG_OUT" | sed -n 's/.*epoch=\([0-9]*\).*/\1/p')
# 目录条目 epoch 由 intake session_up 起算（恒 1），③ 交割值 = 旧+1 = 2
[[ "$EPOCH" == "2" ]] || fail "epoch != 2 (intake=1, handoff=+1): epoch=$EPOCH ($MIG_OUT)"
ok "编排回执：$MIG_OUT（命令行端到端 $(fmt "$T0" "$T1")）"

# —— 分段耗时（§9.8；逐段取自 943 回执，不墙钟反推）——
echo
echo "== 五步编排分段耗时（fleetctl migrate → 943 回执）=="
echo "  ① 停写   (933 freeze → 回执):                     ${FREEZE_MS}ms"
echo "  ② 拉取 + ③ 交写权 (937 → 现场拉 935/936 → 接权):   ${HANDOFF_MS}ms"
echo "  ①→④ 编排段 (①停写 → ④改指回执):                   ${TOTAL_MS}ms"
echo "  ⑤ 放行    (gateway 上行缓冲 → 摘锚):               未接线（§9.9-2）"
echo "  命令行端到端 (含 fleetctl 装配 + 链路握手):         $(fmt "$T0" "$T1")"

# —— 分步落证：①③ 两侧日志 + ④ 改指立即推（wire）——
T_FREEZE=$(ts_of "$OLD_LOG" "frozen for migration" 1)
[[ -n "$T_FREEZE" ]] || fail "source freeze line not observed"
ok "① 停写落证：源侧 ${T_FREEZE}「frozen for migration」"
T_HANDOFF=$(ts_of "$NEW_LOG" "handoff accepted" 1)
[[ -n "$T_HANDOFF" ]] || fail "target handoff line not observed"
log_line "$NEW_LOG" "handoff accepted" | grep -q "new epoch=${EPOCH}" || \
    fail "handoff did not take new epoch ${EPOCH}: $(log_line "$NEW_LOG" "handoff accepted")"
ok "③ 交写权落证：目标侧 ${T_HANDOFF}「handoff accepted (new epoch=${EPOCH})」"
log_line "$LOG" "migrate player ${PLAYER}" | grep -q "total_ms=${TOTAL_MS}" || \
    fail "login orchestration log does not match reply: $(log_line "$LOG" "migrate player ${PLAYER}")"
ok "编排落证：login「migrate player ${PLAYER} -> zone 5 ok total_ms=${TOTAL_MS}」"

if ! wait_log "$LOG" "directory rehome: player ${PLAYER} -> zone 5 epoch=${EPOCH}" 1 5; then
    fail "no directory rehome line after migrate"
else
    T_REHOME=$(ts_after "$LOG" "directory rehome" "$T0")
    REHOME=$(log_line "$LOG" "directory rehome" | grep -o 'snapshot_parts=[0-9]*')
    ok "④ 改指落证：${T_REHOME}「directory rehome ... ${REHOME}」（周期 30s 未到，此为改指立即推）"
fi

# —— ④ wire 落证：镜像快照条目改指 zone 5 + epoch 推进 ——
if ! wait_log "$MIRROR_LOG" "p=${PLAYER} zone=5 epoch=${EPOCH}" 1 10; then
    fail "mirror wire did not show re-pointed entry (zone=5 epoch=${EPOCH})"
else
    ok "④ wire 落证：镜像快照 p=${PLAYER} zone=5 epoch=${EPOCH}"
fi
# ④ 之后不得再出现 zone=2（源冻结锚点离在册集——改指不被回卷）
LINE5=$(grep -n "p=${PLAYER} zone=5" "$MIRROR_LOG" | head -1 | cut -d: -f1)
if awk -v n="$LINE5" 'NR > n && index($0, "zone=2")' "$MIRROR_LOG" | grep -q .; then
    fail "mirror regressed to zone=2 after re-point"
fi

# —— 源冻结锚点离源端在册集：源侧重报不再携带 42 ——
T_SNAP=$(ts_after "$LOG" "directory rehome" "$T0")
if ! wait_log_match "$LOG" "full report: component=31.*sessions=1" 1 10; then
    fail "target zone refresh report (sessions=1) not received"
fi
ok "报告权换手：目标周期重报 sessions=1（42 归目标域独占上报）"
if wait_log_after "$LOG" "full report: component=30" "$T_SNAP" 3; then
    POST=$(grep "full report: component=30" "$LOG" | awk -v t="$T_SNAP" '$1+0 > t+0' | tail -1)
    echo "$POST" | grep -q "sessions=0" || \
        fail "source still reporting frozen anchor: $POST"
    ok "冻结离册：源侧改指后重报 sessions=0（Transferring 锚点不在册，防回卷）"
fi

# —— 存档一致：新侧档案字节恒等（roundtrip 语义同种子序）——
sleep 0.3
if [[ ! -f "$NEW_DATA/player_${PLAYER}.json" ]]; then
    fail "target profile not written: $NEW_DATA"
fi
NEW_SHA=$(sha256sum "$NEW_DATA/player_${PLAYER}.json" 2>/dev/null | awk '{print $1}')
if [[ "$NEW_SHA" == "$BEFORE_SHA" ]]; then
    ok "存档一致：player_${PLAYER}.json sha256 ${NEW_SHA:0:12}… 前后恒等"
else
    fail "profile sha256 changed: old=$BEFORE_SHA new=$NEW_SHA"
fi
grep -q '"level":6' "$NEW_DATA/player_${PLAYER}.json" 2>/dev/null || \
    fail "target profile level != 6: $(cat "$NEW_DATA/player_${PLAYER}.json" 2>/dev/null)"

# —— 30s 周期快照（回卷断言面）：改指 + 刷新 intake 之后，wire 仍 zone=5 ——
echo "（等待 30s 周期快照作回卷断言面…）"
if ! wait_log "$MIRROR_LOG" "p=${PLAYER} zone=5 epoch=${EPOCH}" 2 40; then
    N=$(grep -c "p=${PLAYER}" "$MIRROR_LOG")
    fail "periodic snapshot missing/wrong (p=${PLAYER} lines=$N, need 2nd with zone=5)"
else
    ok "周期快照回卷断言：改指 + 25+ 轮刷新 intake 后，wire 条目仍 zone=5 epoch=${EPOCH}"
fi
if awk -v n="$LINE5" 'NR > n && index($0, "zone=2")' "$MIRROR_LOG" | grep -q .; then
    fail "mirror regressed to zone=2 after periodic snapshot"
fi

# —— 老侧停留态（⑤ 未接线：无 unfreeze 契约面，Transferring 停留是设计形态）——
if kill -0 "$OLD_PID" 2>/dev/null; then
    ok "源侧仍在运行（pid=$OLD_PID；⑤ 摘锚未接线，Transferring 停留 = §9.2 人工介入形态）"
else
    fail "source zone exited during drill"
fi
kill -0 "$NEW_PID" 2>/dev/null || fail "target zone exited"

# —— 失败路径抽验（三态录取面，exit 2 = MIGRATE_FAILED）——
"$FLEETCTL" migrate --host 127.0.0.1 --port "$PORT_LOGIN_DIR" \
    --player 999 --source-host 127.0.0.1 --source-port "$STORE_OLD" \
    --target-host 127.0.0.1 --target-port "$STORE_NEW" --target-zone 5 >/dev/null 2>&1
RC_MISS=$?
if (( RC_MISS != 2 )); then
    fail "unknown player migrate did not exit 2 (got $RC_MISS)"
else
    ok "失败路径：未知玩家 migrate exit=2（MIGRATE_FAILED）"
fi
"$FLEETCTL" migrate --host 127.0.0.1 --port "$PORT_LOGIN_DIR" \
    --player "$PLAYER" --source-host 127.0.0.1 --source-port "$STORE_OLD" \
    --target-host 127.0.0.1 --target-port "$STORE_NEW" --target-zone 0 >/dev/null 2>&1
RC_ZONE=$?
if (( RC_ZONE != 1 )); then
    fail "target zone 0 migrate did not exit 1 (got $RC_ZONE)"
else
    ok "失败路径：target zone 0 被 CLI 层拒绝（exit=1，未出链）"
fi

echo
echo "== 断言清单 =="
[[ $FAIL -eq 0 ]] && ok "全链断言全过" || { echo "== 存在 FAIL 项，见上 ==" >&2; }
exit $FAIL
