#!/usr/bin/env bash
# drill_module_update（todo#52 · P4 仅代码更新批真进程验收）：真双 World M1 面
# + fleetctl module 编排——跨 World 灰度发布的 conductor 相位各在真 TCP 落证：
#   1 首装发布  ：v101 健康模块，canary=w1 先行观察，窗满全绿 → w2 推广
#                （conductor 全相位：prepare×2 → commit w1 → 观察窗 → commit w2）
#   2 迭代发布  ：v102 健康模块再上一轮（首装已成冷冻区，迭代有底可回）
#   3 故障回滚  ：v103 onTick 必炸——canary 窗首 fault 早失败，conductor
#                即止（w2 未触碰），w1 自驱回滚
#   4 抑制半径  ：w2 日志无 v103 提交行（故障半径=灰度集），w1 回滚行在案
#
# 模块源三型同 module_rollout 单测（健康空 onTick / 必炸 onTick）。
#
# 用法: scripts/drill_module_update.sh
# 依赖: APOLLO_BUILD_DIR（默认 build，须已构建）
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${APOLLO_BUILD_DIR:-$REPO_ROOT/build}"
BUILD_DIR="$(cd "$BUILD_DIR" && pwd)"  # 绝对化：子 shell cd 后相对路径失效
DRILL_DIR="${APOLLO_DRILL_DIR:-/tmp/apollo-drill-module-$$}"
mkdir -p "$DRILL_DIR"

WORLD="$BUILD_DIR/apps/world/world"
FLEETCTL="$BUILD_DIR/apps/fleetctl/fleetctl"

W1_PORT=19631
W2_PORT=19632
TICK_MS=20          # 快窗：observe-ticks 5 ≈ 100ms
OBSERVE_TICKS=5
TIMEOUT_S=30

W1_LOG="$DRILL_DIR/w1.log"
W2_LOG="$DRILL_DIR/w2.log"
FAIL=0
W1_PID=""
W2_PID=""

fail() { echo "FAIL: $*" >&2; FAIL=1; }
ok()   { echo "ok:   $*"; }

[[ -x "$WORLD" && -x "$FLEETCTL" ]] || { echo "missing binary —— 先构建" >&2; exit 1; }

# 模块源（module_rollout 单测同型）
cat > "$DRILL_DIR/good.lua" <<'EOF'
return { onTick = function() end, smoke = function() return true end }
EOF
cat > "$DRILL_DIR/bad.lua" <<'EOF'
return { onTick = function() error('boom') end, smoke = function() return true end }
EOF

stop_worlds() {
    for pid in "$W1_PID" "$W2_PID"; do
        if [[ -n "$pid" ]]; then
            kill -TERM "$pid" 2>/dev/null || true
        fi
    done
    sleep 0.5
    for pid in "$W1_PID" "$W2_PID"; do
        if [[ -n "$pid" ]]; then
            kill -KILL "$pid" 2>/dev/null || true
        fi
    done
    pkill -KILL -x world 2>/dev/null || true
    W1_PID=""
    W2_PID=""
    sleep 0.3
}
cleanup() {
    echo "[drill] teardown" >&2
    stop_worlds
}
trap cleanup EXIT
stop_worlds

start_worlds() {
    ( cd "$DRILL_DIR" && stdbuf -oL "$WORLD" --world-id 1 --m1-port "$W1_PORT" \
        --player-store-m1-port 19141 \
        --tick-rate "$TICK_MS" --data-dir "$DRILL_DIR/w1-players" \
        --journal "$DRILL_DIR/w1-journal/persist.log" 2>&1 ) >> "$W1_LOG" &
    W1_PID=$!
    ( cd "$DRILL_DIR" && stdbuf -oL "$WORLD" --world-id 2 --m1-port "$W2_PORT" \
        --player-store-m1-port 19142 \
        --tick-rate "$TICK_MS" --data-dir "$DRILL_DIR/w2-players" \
        --journal "$DRILL_DIR/w2-journal/persist.log" 2>&1 ) >> "$W2_LOG" &
    W2_PID=$!
    sleep 0.8
}

rollout() { # rollout <version> <source.lua> → 打印 fleetctl exit code
    "$FLEETCTL" module \
        --target "127.0.0.1:$W1_PORT" --target "127.0.0.1:$W2_PORT" \
        --name alarm --version "$1" --source-file "$2" \
        --observe-ticks "$OBSERVE_TICKS" --timeout "$TIMEOUT_S" >> "$DRILL_DIR/fleetctl.log" 2>&1
}

start_worlds
ok "双 World 拉起（m1 $W1_PORT / $W2_PORT，tick ${TICK_MS}ms）"

# —— 1 首装发布：canary w1 先行，窗满全绿推广 w2 ——
echo "== 1 首装发布（v101 健康，canary=w1）=="
rollout 101 "$DRILL_DIR/good.lua" ; RC1=$?
(( RC1 == 0 )) || { tail -20 "$W1_LOG" "$W2_LOG" "$DRILL_DIR/fleetctl.log"; fail "1: healthy first install expected 0, got $RC1"; }
ok "1 首装全绿：MODULE_OK（exit 0，canary 观察 → 全量推广）"
grep -q "module rollout committed: alarm v101" "$W1_LOG" \
    && grep -q "module rollout committed: alarm v101" "$W2_LOG" \
    || fail "1: both worlds missing v101 commit log line"
ok "1 双端提交行在案（w1 + w2 日志各含 committed v101）"

# —— 2 迭代发布：v102 再上一轮（v101 成冷冻区）——
echo "== 2 迭代发布（v102 健康）=="
rollout 102 "$DRILL_DIR/good.lua" ; RC2=$?
(( RC2 == 0 )) || fail "2: healthy second rollout expected 0, got $RC2"
ok "2 迭代全绿：MODULE_OK（exit 0）"

# —— 3 故障回滚：v103 必炸——早失败即止，w2 未触碰 ——
echo "== 3 故障回滚（v103 onTick 必炸）=="
: > "$DRILL_DIR/fleetctl.log"
rollout 103 "$DRILL_DIR/bad.lua" ; RC3=$?
(( RC3 == 2 )) || { tail -20 "$DRILL_DIR/fleetctl.log"; fail "3: faulty rollout expected 2, got $RC3"; }
ok "3 故障即止：MODULE_ROLLED_BACK（exit 2，canary 窗首 fault 早失败）"
grep -q "module rollout rolled back: alarm" "$W1_LOG" \
    || fail "3: w1 missing rollback log line"
ok "3 w1 回滚行在案（自驱回滚落日志）"

# —— 4 抑制半径：w2 未被 v103 触碰 ——
echo "== 4 抑制半径（w2 未触碰）=="
if grep -q "committed: alarm v103" "$W2_LOG"; then
    fail "4: w2 must NOT have committed v103 (containment broken)"
else
    ok "4 w2 无 v103 提交行（conductor 灰度即止——故障半径=灰度集）"
fi
rollout 104 "$DRILL_DIR/good.lua" ; RC4=$?
(( RC4 == 0 )) || fail "4: post-rollback rollout expected 0, got $RC4"
ok "4 回滚后发布照常：MODULE_OK（exit 0——模块面仍服务）"

echo
if (( FAIL == 0 )); then
    echo "DRILL OK: 模块更新四相全过（首装灰度 / 迭代 / 故障早失败回滚 / 抑制半径）"
    exit 0
fi
echo "DRILL FAILED"
exit 1
