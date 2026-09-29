#!/usr/bin/env bash
# schema_hash_report（sdk-contract §12.2）：对 base/head 两 ref 的**已提交 golden**
#   生成契约变更报告（markdown → stdout）——评审可见性：这个 PR 动没动客户端契约
#   一眼可见。不重建：闸二（apollo_gen_golden_check）已保证 golden 与契约源一致，
#   golden 即事实。
#
#   报告三块：① 三 hash 对照（client_hash 不变 = 客户端包不重发，internal_hash
#   不变 = 服务端私有面无变更）；② 变更域判定；③ 消息/属性集合 diff（id/name/
#   域/绑定级；字段级细节留给 git diff）。base 侧 golden 缺失/缺字段（早于相应
#   批）按「新增/无值」处理，不报错。
#
# 用法: schema_hash_report.sh <base-ref> <head-ref>
# 依赖: git、jq
set -euo pipefail

base=${1:?用法: schema_hash_report.sh <base-ref> <head-ref>}
head=${2:?用法: schema_hash_report.sh <base-ref> <head-ref>}
route=sdks/cpp/generated/contract_route.json
full=sdks/cpp/generated/apollo_contract.json

show() { git show "$1:$2" 2>/dev/null || true; }
route_b=$(show "$base" "$route"); route_h=$(show "$head" "$route")
full_b=$(show "$base" "$full");  full_h=$(show "$head" "$full")

hash_of() { # <json内容> <jq路径> → 值或（无）
    v=$(printf '%s' "$1" | jq -r "$2 // empty" 2>/dev/null || true)
    [ -n "$v" ] || v="（无）"
    printf '%s' "$v"
}
mark() { [ "$1" = "$2" ] && printf '不变' || printf '**变更**'; }

sh_b=$(hash_of "$route_b" .schema_hash);   sh_h=$(hash_of "$route_h" .schema_hash)
ch_b=$(hash_of "$route_b" .client_hash);   ch_h=$(hash_of "$route_h" .client_hash)
ih_b=$(hash_of "$route_b" .internal_hash); ih_h=$(hash_of "$route_h" .internal_hash)

msgs()  { printf '%s' "$1"  | jq -r '.routes[]? | "\(.id)\t\(.name)\t\(.domain)\t\(.binding)\t\(.dir)"' 2>/dev/null || true; }
attrs() { printf '%s' "$1"  | jq -r '.attrs[]?  | "\(.id)\t\(.name)\t\(.type)"' 2>/dev/null || true; }
added_msgs=$(comm -13 <(msgs "$route_b" | LC_ALL=C sort) <(msgs "$route_h" | LC_ALL=C sort) || true)
removed_msgs=$(comm -23 <(msgs "$route_b" | LC_ALL=C sort) <(msgs "$route_h" | LC_ALL=C sort) || true)
added_attrs=$(comm -13 <(attrs "$full_b" | LC_ALL=C sort) <(attrs "$full_h" | LC_ALL=C sort) || true)
removed_attrs=$(comm -23 <(attrs "$full_b" | LC_ALL=C sort) <(attrs "$full_h" | LC_ALL=C sort) || true)

fmt_add() { sed 's/\t/ · /g; s/^/＋ /'; }
fmt_rm()  { sed 's/\t/ · /g; s/^/－ /'; }

msg_block=$(printf '%s' "$added_msgs" | fmt_add; printf '%s' "$removed_msgs" | fmt_rm)
[ -n "$msg_block" ] || msg_block="（无增删）"
attr_block=$(printf '%s' "$added_attrs" | fmt_add; printf '%s' "$removed_attrs" | fmt_rm)
[ -n "$attr_block" ] || attr_block="（无增删）"

client_changed=false; internal_changed=false
[ "$ch_b" != "$ch_h" ] && client_changed=true || true
[ "$ih_b" != "$ih_h" ] && internal_changed=true || true
if $client_changed && $internal_changed; then
    domains="client + internal（握手 hash 变——客户端需拉新包）"
elif $client_changed; then
    domains="client（握手 hash 变——客户端需拉新包）"
elif $internal_changed; then
    domains="internal（客户端包不受影响——握手稳定，§11.3 ②）"
else
    domains="（无——三 hash 全不变；本次未改契约语义）"
fi

cat <<EOF
## 契约变更报告（apollo-gen golden diff）

| hash | base | head | 变化 |
|---|---|---|---|
| schema_hash（全量身份） | \`${sh_b}\` | \`${sh_h}\` | $(mark "$sh_b" "$sh_h") |
| client_hash（握手对象） | \`${ch_b}\` | \`${ch_h}\` | $(mark "$ch_b" "$ch_h") |
| internal_hash（服务端私有面） | \`${ih_b}\` | \`${ih_h}\` | $(mark "$ih_b" "$ih_h") |

**变更域**：$domains

**消息**（字段级 diff 见 \`git diff sdks/cpp/generated/\`）

$msg_block

**属性**（数值/默认值级 diff 见 \`git diff sdks/cpp/generated/\`）

$attr_block

> client_hash 不变 = 客户端包逐文件不重发；internal_hash 变更只需服务端同批重编部署。报告由已提交 golden 生成（闸二保证其与契约源一致）。
EOF
