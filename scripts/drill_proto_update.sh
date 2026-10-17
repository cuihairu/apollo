#!/usr/bin/env bash
# drill_proto_update（sdk-contract §6 发布/回滚验收 · 真进程）：真 login-app
# 目录 M1 面 + fleetctl 双代次拨入——协议更新的发布/回滚三形各在真 TCP +
# 真 hello 握手上落证：
#   A 破坏性变更（窗关）：旧代 fleetctl 拨本代服务面 → UNREACHABLE(exit 3)
#   B 兼容变更发布（窗开）：两端各带 APOLLO_ROUTE_COMPAT（滚动窗双向声明），
#                          旧代照常 round-trip(exit 2)，本代同代不受影响
#   C 回滚收敛（撤窗）  ：旧代再拒(exit 3)，本代健在(exit 2)
#
# 「旧代」= 契约路由表副本换 schema_hash（路由 id/域不变——消息仍可路由，
# 只有握手代次不同）：fleetctl 经 APOLLO_FLEETCTL_ROUTE_JSON 注入；服务面
# 经 APOLLO_ROUTE_COMPAT 开窗受理。每相先以同代探针证「服务面在」再证
# 「版本拒」，把 exit 3 归因到版本而非进程未起。
#
# 用法: scripts/drill_proto_update.sh
# 依赖: APOLLO_BUILD_DIR（默认 build，须已构建）、python3
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${APOLLO_BUILD_DIR:-$REPO_ROOT/build}"
BUILD_DIR="$(cd "$BUILD_DIR" && pwd)"  # 绝对化：子 shell cd 后相对路径失效
DRILL_DIR="${APOLLO_DRILL_DIR:-/tmp/apollo-drill-proto-$$}"
mkdir -p "$DRILL_DIR"

LOGIN="$BUILD_DIR/apps/login-app/login-app"
FLEETCTL="$BUILD_DIR/apps/fleetctl/fleetctl"
GOLDEN="$REPO_ROOT/sdks/cpp/generated/contract_route.json"

PORT_LOGIN=19623
M1_PORT=19624      # admission 面（避默认 9103 与 dev fleet 相撞）
DIR_PORT=19625     # 目录 M1 控制面（fleetctl 编排命令面）
PLAYER=999         # 不存在的玩家：RESOLVE_FAILED(exit 2) = wire 已 round-trip

LOG="$DRILL_DIR/login.log"
FAIL=0
LOGIN_PID=""

fail() { echo "FAIL: $*" >&2; FAIL=1; }
ok()   { echo "ok:   $*"; }

[[ -x "$LOGIN" && -x "$FLEETCTL" ]] || { echo "missing binary —— 先构建" >&2; exit 1; }
[[ -f "$GOLDEN" ]] || { echo "missing golden route json: $GOLDEN" >&2; exit 1; }

CUR_HASH=$(python3 -c "import json;print(json.load(open('$GOLDEN'))['schema_hash'])")
OLD_HASH="1f2e3d4c5b6a7988fedcba98765432100000000000000000abcdef0000001234"
OLD_JSON="$DRILL_DIR/contract_route_old.json"
python3 - "$GOLDEN" "$OLD_JSON" "$OLD_HASH" <<'PYEOF'
import json, sys
src, dst, old_hash = sys.argv[1], sys.argv[2], sys.argv[3]
doc = json.load(open(src))
doc["schema_hash"] = old_hash  # 仅换代次指纹：routes 原样（消息仍可路由）
json.dump(doc, open(dst, "w"))
PYEOF
ok "旧代路由表：$OLD_JSON（schema_hash=$OLD_HASH，routes 同源）"

stop_login() {
    if [[ -n "$LOGIN_PID" ]]; then
        kill -TERM "$LOGIN_PID" 2>/dev/null || true
        sleep 0.5
        kill -KILL "$LOGIN_PID" 2>/dev/null || true
        LOGIN_PID=""
    fi
    pkill -KILL -x login-app 2>/dev/null || true
    sleep 0.3
}
cleanup() {
    echo "[drill] teardown" >&2
    stop_login
}
trap cleanup EXIT
stop_login

start_login() { # start_login <APOLLO_ROUTE_COMPAT 值，空 = 不设>
    : > "$LOG"
    local compat="$1"
    ( cd "$DRILL_DIR" && env ${compat:+APOLLO_ROUTE_COMPAT="$compat"} \
        stdbuf -oL "$LOGIN" --host 127.0.0.1 --port "$PORT_LOGIN" \
        --m1-port "$M1_PORT" --directory-port "$DIR_PORT" \
        --directory-epochs "$DRILL_DIR/epochs.json" 2>&1 ) >> "$LOG" &
    LOGIN_PID=$!
    sleep 0.5
}

probe() { # probe <route_json 覆盖，空 = 编译期契约> <APOLLO_ROUTE_COMPAT 值>
          #          → 打印 fleetctl exit code
    local route="${1:-}" compat="${2:-}"
    env ${route:+APOLLO_FLEETCTL_ROUTE_JSON="$route"} \
        ${compat:+APOLLO_ROUTE_COMPAT="$compat"} \
        "$FLEETCTL" resolve --host 127.0.0.1 --port "$DIR_PORT" \
        --player "$PLAYER" >> "$LOG" 2>&1
}

# —— A 破坏性变更（窗关）：同代可达证服务面在，旧代被版本拒 ——
echo "== A 破坏性变更（窗关）=="
start_login ""
probe "" "" ; RC_CUR=$?
(( RC_CUR == 2 )) || { tail -20 "$LOG"; fail "A: same-gen probe expected 2, got $RC_CUR"; }
ok "A 同代探针：resolve round-trip（exit 2 = 服务面在、代次一致）"
probe "$OLD_JSON" "" ; RC_OLD=$?
(( RC_OLD == 3 )) || fail "A: old-gen probe expected 3, got $RC_OLD"
ok "A 旧代拨入被拒：UNREACHABLE（exit 3 = hello 版本失配，非进程缺席）"

# —— B 兼容变更发布（窗开）：两端各带窗口，旧代 round-trip、本代共存 ——
echo "== B 兼容变更发布（窗开）=="
stop_login
start_login "$OLD_HASH"
probe "$OLD_JSON" "$CUR_HASH" ; RC_OLD_W=$?
(( RC_OLD_W == 2 )) || { tail -20 "$LOG"; fail "B: windowed old-gen probe expected 2, got $RC_OLD_W"; }
ok "B 窗内旧代 round-trip：RESOLVE_FAILED(exit 2) = 旧代业务过窗（玩家不在=未命中）"
probe "" "$CUR_HASH" ; RC_CUR_W=$?
(( RC_CUR_W == 2 )) || fail "B: same-gen under window expected 2, got $RC_CUR_W"
ok "B 本代与窗共存：resolve round-trip（exit 2）"

# —— C 回滚收敛（撤窗）：本代健在，旧代再拒 ——
echo "== C 回滚收敛（撤窗）=="
stop_login
start_login ""
probe "" "" ; RC_CUR_R=$?
(( RC_CUR_R == 2 )) || { tail -20 "$LOG"; fail "C: same-gen probe after rollback expected 2, got $RC_CUR_R"; }
ok "C 撤窗后本代健在：resolve round-trip（exit 2）"
probe "$OLD_JSON" "" ; RC_OLD_R=$?
(( RC_OLD_R == 3 )) || fail "C: old-gen probe after rollback expected 3, got $RC_OLD_R"
ok "C 撤窗后旧代再拒：UNREACHABLE（exit 3）"

echo
if (( FAIL == 0 )); then
    echo "DRILL OK: 协议更新三形全过（A 窗关拒旧代 / B 窗开纳旧代 / C 撤窗回滚收敛）"
    exit 0
fi
echo "DRILL FAILED"
exit 1
