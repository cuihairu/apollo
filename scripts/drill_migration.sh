#!/usr/bin/env bash
# P0-2 路 2 端到端迁移演练（docs/design/backup-revive.md §9.2 五步 ①②③④ 编排
# + §9.8 唯一待定档「单玩家切换耗时」实测件）：两 zone 真进程 + manager 编排面，
# fleetctl migrate 驱动（942→943），逐段耗时取自进程自身（不墙钟反推）。
#
# 【待迁移（todo P3 跨 World 迁移条目）】：本演练仍指向已退场的 baseappmgr/
# base-app 进程（22438e36 re-home 后 Login/World 承接其面）。跨 World 迁移
# 条目开工时按 drill_kill9.sh 的 re-home 版拓扑迁移（login-app = 编排面、
# world-app = 两 zone 承载面）；迁移完成前本脚本不作为验收证据。
# 用法：scripts/drill_migration.sh
#   APOLLO_BUILD_DIR   构建树（默认 <repo>/build，须 GAME_MODULE=ON 已构建）
#   APOLLO_DRILL_DIR   运行目录（默认 /tmp/apollo-drill-migration-<pid>）
#   APOLLO_MIG_PLAYER  迁移玩家号（默认 42）
#
# 演练拓扑（独立端口段，不与 drill_update 19610-19614/19013-19014/19105、
# drill_kill9 19600-19602/19003-19004、dev_fleet 9001-9005/9101-9107 冲突）：
#   mgr  = baseappmgr --port 19621 --m1-port 19625（fleetctl 编排命令面）
#                    --recovery-port 19622 --recovery-expected 1（准入 + intake）
#                    --mirror-port 19626 --mirror-snapshot-ms 30000
#                    （30s 周期 = 演练窗内不会自动触发；出现「snapshot published」
#                     即 §9.2 ④「改指立即推」的证据，不可能是周期面）
#   old  = base-app --port 19631 --m1-port 19632 --zone-id 2 --component-id 30
#                  --report-to-port 19622 --report-interval-ms 1000 --demo-anchors N
#   new  = base-app --port 19641 --m1-port 19642 --zone-id 5（空载承接）
# 种档：player_<N>.json 按 PlayerData::toJson 精确字节序种入（drill_update 同款）
# —— 迁移前后 sha256 比对即「存档一致」断言（flush 回写 roundtrip 字节恒等）。
#
# 口径注记（如实）：五步 ⑤（gateway 上行缓冲放行 + 老侧摘锚）未接线
# （§9.9-2），本演练实测的是 ①停写→④改指的编排段耗时（943 total_ms）；
# 停写窗口 = ①→⑤，缺 ⑤ 时 total_ms 是下界而非全窗口。老进程在演练后停留
# Transferring（无 unfreeze 契约面——§9.2「本轮跳过 + 人工介入」的形态）。

set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${APOLLO_BUILD_DIR:-$REPO_ROOT/build}"
DRILL_DIR="${APOLLO_DRILL_DIR:-/tmp/apollo-drill-migration-$$}"
PLAYER="${APOLLO_MIG_PLAYER:-42}"

RUN_DIR="$DRILL_DIR/run"
OLD_DIR="$DRILL_DIR/old"
NEW_DIR="$DRILL_DIR/new"
OLD_DATA="$OLD_DIR/base-app-data/players"
NEW_DATA="$NEW_DIR/base-app-data/players"
mkdir -p "$RUN_DIR" "$OLD_DATA" "$NEW_DATA"

MGR="$BUILD_DIR/apps/baseappmgr/baseappmgr"
BASE="$BUILD_DIR/apps/base-app/base-app"
FLEETCTL="$BUILD_DIR/apps/fleetctl/fleetctl"

PORT_MGR=19621
PORT_M1_MGR=19625        # manager M1 控制面（fleetctl 编排命令面）
PORT_RECOVERY=19622      # manager 全量重报 intake（老 zone 报告方）
PORT_MIRROR=19626        # manager 目录镜像发布（无消费者，UDP fire-and-forget）
PORT_OLD=19631
M1_OLD=19632             # 老 zone M1 面（① 933 受理）
PORT_NEW=19641
M1_NEW=19642             # 新 zone M1 面（② 935/936 拉取 + ③ 937 接权）

LOG="$DRILL_DIR/fleet.log"
OLD_LOG="$DRILL_DIR/old.log"
NEW_LOG="$DRILL_DIR/new.log"
FAIL=0

fail() { echo "FAIL: $*" >&2; FAIL=1; }
ok()   { echo "ok:   $*"; }

for bin in "$MGR" "$BASE" "$FLEETCTL"; do
    [[ -x "$bin" ]] || { echo "missing binary: $bin —— 先构建（GAME_MODULE=ON）" >&2; exit 1; }
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

log_line() { # log_line <file> <pattern> [occurrence] —— 命中整行（含 ANSI 色码）
    local f="$1" pat="$2" occ="${3:-1}"
    awk -v pat="$pat" -v occ="$occ" '
        index($0, pat) { c++; if (c == occ) { print; exit } }' "$f"
}

MGR_PID=""
OLD_PID=""
NEW_PID=""
cleanup() {
    echo "[drill] teardown begin" >&2
    for p in "$MGR_PID" "$OLD_PID" "$NEW_PID"; do
        [[ -n "$p" ]] && kill -TERM "$p" 2>/dev/null || true
    done
    sleep 1
    for p in "$MGR_PID" "$OLD_PID" "$NEW_PID"; do
        [[ -n "$p" ]] && kill -KILL "$p" 2>/dev/null || true
    done
    pkill -KILL -x baseappmgr 2>/dev/null || true
    pkill -KILL -x base-app 2>/dev/null || true
    echo "[drill] teardown end" >&2
}
trap cleanup EXIT

# 残留进程护栏（-x 精确匹配进程名）
pkill -KILL -x baseappmgr 2>/dev/null || true
pkill -KILL -x base-app 2>/dev/null || true
sleep 0.5

# —— 种档：PlayerData::toJson 精确字节序（database_service 字段序、
# 无空格无换行；值取 flush 回写 roundtrip 恒等的简单形态）——
printf '{"playerId":%s,"username":"mig-alpha","level":6,"exp":1000,"hp":88,"maxHp":100,"mp":50,"maxMp":50,"x":42.5,"y":2.5,"z":3.5}' \
    "$PLAYER" > "$OLD_DATA/player_${PLAYER}.json"
BEFORE_SHA=$(sha256sum "$OLD_DATA/player_${PLAYER}.json" | awk '{print $1}')
: > "$LOG"

# —— manager 先行：恢复相位排他（准入关闭）+ intake + 镜像发布面 ——
( cd "$RUN_DIR" && stdbuf -oL "$MGR" --port "$PORT_MGR" \
    --m1-port "$PORT_M1_MGR" \
    --recovery-port "$PORT_RECOVERY" --recovery-expected 1 \
    --mirror-port "$PORT_MIRROR" --mirror-snapshot-ms 30000 \
    2>&1 ) | ts >> "$LOG" &

# —— 老 zone：种档离线恢复 + demo 锚点（会话绑定 + home zone）+ 全量重报 ——
( cd "$OLD_DIR" && stdbuf -oL "$BASE" --port "$PORT_OLD" --m1-port "$M1_OLD" \
    --report-to-host 127.0.0.1 --report-to-port "$PORT_RECOVERY" \
    --component-id 30 --zone-id 2 --report-interval-ms 1000 \
    --demo-anchors "$PLAYER" \
    2>&1 ) | ts >> "$OLD_LOG" &

# —— 新 zone：空载承接（无会话，不需报告）——
( cd "$NEW_DIR" && stdbuf -oL "$BASE" --port "$PORT_NEW" --m1-port "$M1_NEW" \
    --component-id 31 --zone-id 5 \
    2>&1 ) | ts >> "$NEW_LOG" &

MGR_PID=$(pgrep -x baseappmgr | head -1)
OLD_PID=$(pgrep -x base-app | head -1)
NEW_PID=$(pgrep -x base-app | tail -1)

echo "== 演练启动（日志 $LOG/$OLD_LOG/$NEW_LOG，数据 $OLD_DATA → $NEW_DATA）=="
if ! wait_log "$LOG" "full report from component=30" 1 20; then
    tail -30 "$LOG"; tail -30 "$OLD_LOG"
    fail "old zone full report not received"; exit 1
fi
INTAKE_LINE=$(grep "full report from component=30" "$LOG" | tail -1)
echo "$INTAKE_LINE" | grep -q "sessions=${PLAYER} intake=${PLAYER}" || \
    fail "intake count != ${PLAYER}: $INTAKE_LINE"
T_INTAKE=$(ts_of "$LOG" "full report from component=30" 1)
if ! wait_log "$LOG" "收敛开放" 1 20; then
    tail -30 "$LOG"; fail "manager recovery convergence not observed"; exit 1
fi
T_CONVERGE=$(ts_of "$LOG" "收敛开放" 1)
ok "目录收敛：intake=${PLAYER}（恢复相位 → 开放 $(fmt "$T_INTAKE" "$T_CONVERGE")）"

wait_log "$OLD_LOG" "handshake" 1 15 || wait_log "$OLD_LOG" "listening" 1 5 || true
wait_log "$NEW_LOG" "listening" 1 15 || true

# —— 迁移前基线：老侧在线、新侧空载、落点解析就绪 ——
if ! wait_log "$OLD_LOG" "seeded ${PLAYER} smoke anchors" 1 10; then
    fail "old zone smoke anchors not seeded"; exit 1
fi
ok "老侧就绪：${PLAYER} 个冒烟锚点（gateway=1, zone=2）+ 种档离线恢复"
for pid in "$MGR_PID" "$OLD_PID" "$NEW_PID"; do
    [[ -n "$pid" ]] || { fail "process pid missing"; exit 1; }
done
ok "进程基线：mgr pid=$MGR_PID old pid=$OLD_PID new pid=$NEW_PID"

# 落点补齐：intake 只在目录条目建绑定 + home zone，assignment 经命令面显式给
# （resolve 走 SessionLocator 投影 = 登录流落锚面，不经 intake——非本演练口径）
PRE_ASSIGN=$("$FLEETCTL" assign --host 127.0.0.1 --port "$PORT_M1_MGR" \
    --player "$PLAYER" --world 1 2>&1)
RC_PRE=$?
if (( RC_PRE != 0 )); then
    fail "pre-migration assign failed: $PRE_ASSIGN (exit=$RC_PRE)"
fi
ok "迁移前基线：目录条目在册 + 落点指派 $PRE_ASSIGN"

# —— 五步编排：fleetctl migrate（942 → 943 同步等回执）——
T0=$(date +%s.%N)
MIG_OUT=$("$FLEETCTL" migrate --host 127.0.0.1 --port "$PORT_M1_MGR" \
    --player "$PLAYER" \
    --source-host 127.0.0.1 --source-port "$M1_OLD" \
    --target-host 127.0.0.1 --target-port "$M1_NEW" \
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
# 目录条目的初始 epoch 由 intake 顺序决定（非恒 1），此处断言单调递增：
# ③ 交割值 = 旧 epoch+1，故 ≥ 2（intake 建条目即占用 ≥1 个 epoch）
[[ "$EPOCH" =~ ^[0-9]+$ ]] && (( EPOCH >= 2 )) || \
    fail "epoch not bumped monotonically: epoch=$EPOCH ($MIG_OUT)"
ok "编排回执：$MIG_OUT（命令行端到端 $(fmt "$T0" "$T1")）"

# —— 分段耗时（§9.8 唯一待定档；逐段取自 943 回执，不墙钟反推）——
echo
echo "== 五步编排分段耗时（fleetctl migrate → 943 回执；对照 drill_update 全链 2.44-3.81s）=="
echo "  ① 停写   (933 freeze → 回执):                     ${FREEZE_MS}ms"
echo "  ② 拉取 + ③ 交写权 (937 → 现场拉 935/936 → 接权):   ${HANDOFF_MS}ms"
echo "  ①→④ 编排段 (①停写 → ④改指回执):                   ${TOTAL_MS}ms"
echo "  ⑤ 放行    (gateway 上行缓冲 → 摘锚):               未接线（§9.9-2）"
echo "  命令行端到端 (含 fleetctl 装配 + 链路握手):         $(fmt "$T0" "$T1")"

# —— 分步落证：①③ 两侧日志 + ④ 改指立即推 ——
T_FREEZE=$(ts_of "$OLD_LOG" "frozen for migration" 1)
[[ -n "$T_FREEZE" ]] || fail "old side freeze line not observed"
ok "① 停写落证：老侧 ${T_FREEZE}「frozen for migration」"
T_HANDOFF=$(ts_of "$NEW_LOG" "handoff accepted" 1)
[[ -n "$T_HANDOFF" ]] || fail "new side handoff line not observed"
log_line "$NEW_LOG" "handoff accepted" | grep -q "new epoch=${EPOCH}" || \
    fail "handoff did not take new epoch ${EPOCH}: $(log_line "$NEW_LOG" "handoff accepted")"
ok "③ 交写权落证：新侧 ${T_HANDOFF}「handoff accepted (new epoch=${EPOCH})」"
log_line "$LOG" "migrate player ${PLAYER}" | grep -q "total_ms=${TOTAL_MS}" || \
    fail "manager orchestration log does not match reply: $(log_line "$LOG" "migrate player ${PLAYER}")"
ok "编排落证：manager「migrate player ${PLAYER} -> zone 5 ok total_ms=${TOTAL_MS}」"
if wait_log_after "$LOG" "directory rehome" "$T0" 5; then
    T_SNAP=$(ts_after "$LOG" "directory rehome" "$T0")
    REHOME=$(log_line "$LOG" "directory rehome" | grep -o 'snapshot_parts=[0-9]*')
    ok "④ 改指立即推：${T_SNAP}「directory rehome ... ${REHOME}」（$(fmt "$T1" "$T_SNAP") 于回执之后；周期 30s 未触发）"
else
    fail "no immediate directory rehome push after migrate (cycle is 30s, so this must be the ④ push)"
fi

# —— 存档一致：新侧档案字节恒等（roundtrip 语义同 drill_update）——
sleep 0.3
if [[ ! -f "$NEW_DATA/player_${PLAYER}.json" ]]; then
    fail "new side profile not written: $NEW_DATA"
fi
NEW_SHA=$(sha256sum "$NEW_DATA/player_${PLAYER}.json" | awk '{print $1}')
if [[ "$NEW_SHA" == "$BEFORE_SHA" ]]; then
    ok "存档一致：player_${PLAYER}.json sha256 ${NEW_SHA:0:12}… 前后恒等"
else
    fail "profile sha256 changed: old=$BEFORE_SHA new=$NEW_SHA"
fi
grep -q '"level":6' "$NEW_DATA/player_${PLAYER}.json" || \
    fail "new side profile level != 6: $(cat "$NEW_DATA/player_${PLAYER}.json")"

# —— 老侧停留态（⑤ 未接线：无 unfreeze 契约面，Transferring 停留是设计形态）——
if kill -0 "$OLD_PID" 2>/dev/null; then
    ok "老侧仍在运行（pid=$OLD_PID；⑤ 摘锚未接线，Transferring 停留 = §9.2 人工介入形态）"
else
    fail "old zone exited during drill"
fi
[[ -n "$NEW_PID" ]] && kill -0 "$NEW_PID" 2>/dev/null || fail "new zone exited"

# —— 失败路径抽验（三态录取面，exit 2 = MIGRATE_FAILED）——
"$FLEETCTL" migrate --host 127.0.0.1 --port "$PORT_M1_MGR" \
    --player 999 --source-host 127.0.0.1 --source-port "$M1_OLD" \
    --target-host 127.0.0.1 --target-port "$M1_NEW" --target-zone 5 >/dev/null 2>&1
RC_MISS=$?
if (( RC_MISS != 2 )); then
    fail "unknown player migrate did not exit 2 (got $RC_MISS)"
else
    ok "失败路径：未知玩家 migrate exit=2（MIGRATE_FAILED）"
fi
"$FLEETCTL" migrate --host 127.0.0.1 --port "$PORT_M1_MGR" \
    --player "$PLAYER" --source-host 127.0.0.1 --source-port "$M1_OLD" \
    --target-host 127.0.0.1 --target-port "$M1_NEW" --target-zone 0 >/dev/null 2>&1
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
