#!/usr/bin/env bash
# contract_pack（sdk-contract §11.6/§12.2 job ⑤「打包」的执行体）：
#   从已提交的 golden 组契约包——CI 即 manifest 的签发者（同批性的机器物证）。
#   不重建、不重新生成：闸二（apollo_gen_golden_check）已保证 golden 与契约源
#   一致，本脚本只做「定型 + 组包 + 记指纹」。
#
#   客户端包 dist/client/<client_hash>/   ← 目录名即版本指纹（CDN 缓存友好）
#     manifest.json / descriptor.bin（client 域投影）/ semantic.json
#   服务端包 dist/server/<schema_hash>/   ← 同构三件互检的装载单元
#     manifest.json / descriptor_full.bin（全量）/ contract.lua / contract_route.json
#
#   manifest 携带面纪律（§11.3 ②/§11.6）：只带域 hash + 生成器 + 逐文件
#   SHA-256——不带手工 contract_version（internal-only 变更后客户端包必须
#   逐文件不变，含 manifest 自身）；protoc 版本入 manifest（descriptor.bin
#   字节稳定性 = pin 版本 + 记录在案，审计可追）。
#
# 用法: contract_pack.sh <repo根> <dist输出目录>
# 依赖: protoc（CI pin 版本）、jq、sha256sum
set -euo pipefail

root=${1:?用法: contract_pack.sh <repo根> <dist输出目录>}
dist=${2:?用法: contract_pack.sh <repo根> <dist输出目录>}
# dist 定型为绝对路径（CI 传相对路径 dist）：protoc 的 descriptor_set_out
# 在 (cd "$gen" && …) 子 shell 里解析——相对路径会落进 $gen 下不存在的
# 目录，09-29 起 contract_pack 恒红的根因即此
mkdir -p "$dist"
dist=$(cd "$dist" && pwd)
gen="$root/sdks/cpp/generated"
gen=$(cd "$gen" && pwd)

for f in semantic.json apollo_contract_client.proto apollo_contract.proto \
         contract.lua contract_route.json; do
    [ -f "$gen/$f" ] || { echo "contract_pack: 缺少 golden $gen/$f" >&2; exit 1; }
done

client_hash=$(jq -r .client_hash "$gen/semantic.json")
generator=$(jq -r .generator "$gen/semantic.json")
schema_hash=$(jq -r .schema_hash "$gen/contract_route.json")
internal_hash=$(jq -r .internal_hash "$gen/contract_route.json")
for v in "$client_hash" "$schema_hash" "$internal_hash"; do
    [ ${#v} -eq 64 ] || { echo "contract_pack: hash 长度非法：$v" >&2; exit 1; }
done

protoc_version=$(protoc --version)
sha() { sha256sum "$1" | cut -d' ' -f1; }

# ---- 客户端包：client 域投影 bin + 语义小件 ----
cdir="$dist/client/$client_hash"
mkdir -p "$cdir"
(cd "$gen" && protoc -I. --descriptor_set_out="$cdir/descriptor.bin" \
    apollo_contract_client.proto)
cp "$gen/semantic.json" "$cdir/semantic.json"
jq -n --arg gen "$generator" --arg protoc "$protoc_version" --arg ch "$client_hash" \
      --arg d "$(sha "$cdir/descriptor.bin")" --arg s "$(sha "$cdir/semantic.json")" \
    '{kind: "client", generator: $gen, protoc: $protoc, client_hash: $ch,
      files: {"descriptor.bin": $d, "semantic.json": $s}}' > "$cdir/manifest.json"

# ---- 服务端包：全量 bin + Lua 契约表 + 路由清单 ----
sdir="$dist/server/$schema_hash"
mkdir -p "$sdir"
(cd "$gen" && protoc -I. --descriptor_set_out="$sdir/descriptor_full.bin" \
    apollo_contract.proto)
cp "$gen/contract.lua" "$gen/contract_route.json" "$sdir/"
jq -n --arg gen "$generator" --arg protoc "$protoc_version" --arg sh "$schema_hash" \
      --arg ih "$internal_hash" \
      --arg d "$(sha "$sdir/descriptor_full.bin")" \
      --arg l "$(sha "$sdir/contract.lua")" --arg r "$(sha "$sdir/contract_route.json")" \
    '{kind: "server", generator: $gen, protoc: $protoc, schema_hash: $sh,
      internal_hash: $ih,
      files: {"descriptor_full.bin": $d, "contract.lua": $l,
              "contract_route.json": $r}}' > "$sdir/manifest.json"

echo "contract_pack: 组包完成（$protoc_version）"
echo "  client → $cdir"
echo "    $(cd "$cdir" && sha256sum ./* | sed 's/^/    /')"
echo "  server → $sdir"
echo "    $(cd "$sdir" && sha256sum ./* | sed 's/^/    /')"
