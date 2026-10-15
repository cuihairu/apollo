#pragma once
//
// attr_domain_mapping — 契约 sync 掩码 → AttrDomain 映射（attribute-sync §2.2）。
//
// 分工：契约 = 数据（sdks/contract/attrs.xml 的 sync 掩码，生成进 kAttrs[].sync），
// 机制 = 本映射，宿主 = 接线（启动期以契约表调 apply_contract_domains）。
// 本模块不引契约依赖边：位定义本地镜像，契约面数据由宿主注入。镜像与生成物
// 的漂移风险由 tests/test_attr_domain_mapping.cpp 的 static_assert 钉住。
//
// 映射规则（最宽可见性胜出；sdk-contract.md §10.2.4 工作样例的执行形态）：
//   WORLD > APPR/PROP > GUILD > TEAM > SELF
//     WORLD      → kWorld      全服广播（保留上游路由意图，机制层等同 public）
//     APPR|PROP  → kPublic     AOI 兴趣域（外观与属性恒兴趣范围内可见）
//     GUILD      → kGuild      公会成员（group gate 判定）
//     TEAM       → kTeam       队伍成员（group gate 判定）
//     SELF       → kSelf       仅 owner
//   - 空掩码 → kPublic（与 EntityAttrs::domain() 的未声明默认一致）。
//   - IMMEDIATE 是时序位（同拍下发），不参与域映射。
//   - DB 位（0x04）属 storage.xml，契约面已禁用，即便出现也不贡献域。
//   - TEAM|GUILD 平局 → kGuild（公会宽于队伍）。
//

#include <cstddef>
#include <cstdint>

#include "apollo/game/attrsync/attr_domain.hpp"
#include "apollo/game/attrsync/entity_attrs.hpp"

namespace apollo::game::attrsync {

// ── 契约 sync 掩码位（本地镜像；canonical 在 apollo_contract.h:21-28）─────
inline constexpr std::uint8_t kSyncMaskAppr = 0x01;
inline constexpr std::uint8_t kSyncMaskProp = 0x02;
inline constexpr std::uint8_t kSyncMaskSelf = 0x08;
inline constexpr std::uint8_t kSyncMaskTeam = 0x10;
inline constexpr std::uint8_t kSyncMaskGuild = 0x20;
inline constexpr std::uint8_t kSyncMaskWorld = 0x40;
inline constexpr std::uint8_t kSyncMaskImmediate = 0x80;

// 掩码 → 域（最宽可见性胜出，规则见头注）
[[nodiscard]] AttrDomain domain_from_sync_mask(std::uint8_t sync_mask);

// 是否为 group-gated 域（kTeam/kGuild；kPublic/kSelf/kWorld 不是——
// 机制层口径：kSelf 走 owner 判定，kPublic/kWorld 恒可见）
[[nodiscard]] bool is_group_scoped(AttrDomain domain);

// 契约表条目：宿主从生成物 kAttrs 抽注（id + sync 位）
struct ContractAttrEntry {
    std::uint32_t attr_id;
    std::uint8_t sync_mask;
};

// 表应用：把契约声明的域标记写入 EntityAttrs。不产生 history 条目
// （声明非变更）；重复应用以末次为准。slice base 的域由 slot 组合 id 继承
// （entity_attrs.hpp domain()），故只需给 base 声明一次。
void apply_contract_domains(EntityAttrs& attrs,
                            const ContractAttrEntry* entries,
                            std::size_t count);

}  // namespace apollo::game::attrsync
