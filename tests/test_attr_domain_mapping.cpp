// Contract sync-mask → AttrDomain mapping tests (attribute-sync §2.2, FW1016D 已代决).
// Pins: 镜像常量 == 生成物 canonical 位定义；最宽可见性胜出的完整真值表；
// 契约表实际 sync 值 → 域（sdk-contract.md §10.2.4 工作样例）；表应用 helper。
#include <cstdint>
#include <cstring>
#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

#include "apollo_contract.h"
#include "apollo/game/attrsync/attr_domain.hpp"
#include "apollo/game/attrsync/attr_domain_mapping.hpp"
#include "apollo/game/attrsync/entity_attrs.hpp"

using namespace apollo::game::attrsync;
namespace gen = apollo::contract::gen;

// ── 镜像常量钉住（canonical = sdks/cpp/generated/apollo_contract.h:21-28）────
//
// attrsync 不引契约依赖边（契约面数据经宿主注入，机制层保持零契约耦合），
// 位定义本地镜像；镜像与生成物漂移在此处编译期失败。

static_assert(kSyncMaskAppr == gen::kSyncAppr);
static_assert(kSyncMaskProp == gen::kSyncProp);
static_assert(kSyncMaskSelf == gen::kSyncSelf);
static_assert(kSyncMaskTeam == gen::kSyncTeam);
static_assert(kSyncMaskGuild == gen::kSyncGuild);
static_assert(kSyncMaskWorld == gen::kSyncWorld);
static_assert(kSyncMaskImmediate == gen::kSyncImmediate);

namespace {

// 按可见性总序取最高优先级位：WORLD > APPR/PROP > GUILD > TEAM > SELF。
// IMMEDIATE/DB 位不参与域映射，仅它们时等价空掩码。
constexpr std::uint8_t top_bit(std::uint8_t mask) {
    if (mask & kSyncMaskWorld) return kSyncMaskWorld;
    if (mask & (kSyncMaskAppr | kSyncMaskProp)) return kSyncMaskProp;
    if (mask & kSyncMaskGuild) return kSyncMaskGuild;
    if (mask & kSyncMaskTeam) return kSyncMaskTeam;
    if (mask & kSyncMaskSelf) return kSyncMaskSelf;
    return 0;
}

}  // namespace

// ── 单比特 ──────────────────────────────────────────────────────

TEST(AttrDomainMapping, SingleFlagMaps) {
    EXPECT_EQ(domain_from_sync_mask(kSyncMaskSelf), AttrDomain::kSelf);
    EXPECT_EQ(domain_from_sync_mask(kSyncMaskTeam), AttrDomain::kTeam);
    EXPECT_EQ(domain_from_sync_mask(kSyncMaskGuild), AttrDomain::kGuild);
    EXPECT_EQ(domain_from_sync_mask(kSyncMaskProp), AttrDomain::kPublic);
    EXPECT_EQ(domain_from_sync_mask(kSyncMaskAppr), AttrDomain::kPublic);
    EXPECT_EQ(domain_from_sync_mask(kSyncMaskWorld), AttrDomain::kWorld);
}

TEST(AttrDomainMapping, EmptyMaskDefaultsPublic) {
    EXPECT_EQ(domain_from_sync_mask(0), AttrDomain::kPublic);
}

TEST(AttrDomainMapping, TimingAndStorageBitsIgnored) {
    // IMMEDIATE 是时序位（同拍下发），不进域映射
    EXPECT_EQ(domain_from_sync_mask(kSyncMaskImmediate), AttrDomain::kPublic);
    EXPECT_EQ(domain_from_sync_mask(kSyncMaskAppr | kSyncMaskImmediate), AttrDomain::kPublic);
    // DB 位属 storage.xml（契约禁用位），即便出现也不贡献域
    EXPECT_EQ(domain_from_sync_mask(gen::kSyncDbBanned), AttrDomain::kPublic);
    EXPECT_EQ(domain_from_sync_mask(kSyncMaskSelf | gen::kSyncDbBanned), AttrDomain::kSelf);
}

// ── 多比特：最宽可见性胜出 ──────────────────────────────────────

TEST(AttrDomainMapping, WidestVisibilityWins) {
    // level = PROP TEAM WORLD（attrs.xml id=300）→ kWorld（保留全服路由意图）
    EXPECT_EQ(domain_from_sync_mask(kSyncMaskProp | kSyncMaskTeam | kSyncMaskWorld),
              AttrDomain::kWorld);
    // SELF|TEAM → kTeam（队伍宽于自身）
    EXPECT_EQ(domain_from_sync_mask(kSyncMaskSelf | kSyncMaskTeam), AttrDomain::kTeam);
    // TEAM|GUILD → kGuild（公会宽于队伍）
    EXPECT_EQ(domain_from_sync_mask(kSyncMaskTeam | kSyncMaskGuild), AttrDomain::kGuild);
    // PROP|SELF / APPR|SELF → kPublic（公开位宽于自身位）
    EXPECT_EQ(domain_from_sync_mask(kSyncMaskProp | kSyncMaskSelf), AttrDomain::kPublic);
    EXPECT_EQ(domain_from_sync_mask(kSyncMaskAppr | kSyncMaskSelf), AttrDomain::kPublic);
    // GUILD|WORLD → kWorld
    EXPECT_EQ(domain_from_sync_mask(kSyncMaskGuild | kSyncMaskWorld), AttrDomain::kWorld);
}

TEST(AttrDomainMapping, DomainIsDeterminedByHighestPriorityBit) {
    // 结构性性质（非规则复述）：多比特掩码的域 == 其中最高优先级比特的域。
    // 若规则退化为「按位计数」或「末位胜出」，此穷举会红。
    for (int m = 0; m < 256; ++m) {
        const std::uint8_t mask = static_cast<std::uint8_t>(m);
        EXPECT_EQ(domain_from_sync_mask(mask), domain_from_sync_mask(top_bit(mask)))
            << "mask=" << m;
    }
}

TEST(AttrDomainMapping, GroupScopedPredicate) {
    EXPECT_TRUE(is_group_scoped(AttrDomain::kTeam));
    EXPECT_TRUE(is_group_scoped(AttrDomain::kGuild));
    EXPECT_FALSE(is_group_scoped(AttrDomain::kPublic));
    EXPECT_FALSE(is_group_scoped(AttrDomain::kSelf));
    EXPECT_FALSE(is_group_scoped(AttrDomain::kWorld));
}

// ── 契约表实际 sync 值 → 域（sdk-contract.md §10.2.4 工作样例）──────────

TEST(AttrDomainMapping, ContractTableMatchesDocWorkedExample) {
    // hp=PROP, level=PROP TEAM WORLD, gold=SELF
    // → viewer=self {hp,level,gold}, 队友 {hp,level}, 路人 {hp,level}
    auto sync_of = [](const char* name) -> std::uint8_t {
        for (const auto& meta : gen::kAttrs) {
            if (std::strcmp(meta.name, name) == 0) return meta.sync;
        }
        return 0xFF;
    };

    ASSERT_NE(sync_of("hp"), 0xFF);
    EXPECT_EQ(domain_from_sync_mask(sync_of("hp")), AttrDomain::kPublic);

    ASSERT_NE(sync_of("level"), 0xFF);
    EXPECT_EQ(domain_from_sync_mask(sync_of("level")), AttrDomain::kWorld);

    ASSERT_NE(sync_of("gold"), 0xFF);
    EXPECT_EQ(domain_from_sync_mask(sync_of("gold")), AttrDomain::kSelf);

    // weapon_appearance = APPR IMMEDIATE → 外观恒 AOI 可见
    ASSERT_NE(sync_of("weapon_appearance"), 0xFF);
    EXPECT_EQ(domain_from_sync_mask(sync_of("weapon_appearance")), AttrDomain::kPublic);

    // online = WORLD → 全服
    ASSERT_NE(sync_of("online"), 0xFF);
    EXPECT_EQ(domain_from_sync_mask(sync_of("online")), AttrDomain::kWorld);

    // 表内不出现禁用的 DB 位
    for (const auto& meta : gen::kAttrs) {
        EXPECT_EQ(meta.sync & gen::kSyncDbBanned, 0) << meta.name;
    }
}

// ── 表应用 helper ──────────────────────────────────────────────

TEST(AttrDomainMapping, ApplyContractDomains) {
    EntityAttrs attrs(1);
    const ContractAttrEntry entries[] = {
        {100, kSyncMaskProp},                                   // hp → kPublic
        {200, kSyncMaskSelf},                                   // gold → kSelf
        {300, kSyncMaskProp | kSyncMaskTeam | kSyncMaskWorld},  // level → kWorld
        {400, kSyncMaskAppr | kSyncMaskImmediate},              // weapon_appearance → kPublic
        {500, kSyncMaskTeam | kSyncMaskGuild},                  // → kGuild
    };
    apply_contract_domains(attrs, entries, 5);

    EXPECT_EQ(attrs.domain(100), AttrDomain::kPublic);
    EXPECT_EQ(attrs.domain(200), AttrDomain::kSelf);
    EXPECT_EQ(attrs.domain(300), AttrDomain::kWorld);
    EXPECT_EQ(attrs.domain(400), AttrDomain::kPublic);
    EXPECT_EQ(attrs.domain(500), AttrDomain::kGuild);
}

TEST(AttrDomainMapping, ApplyContractDomainsSliceInheritsBase) {
    EntityAttrs attrs(7);
    attrs.declare_slice(600, 4);
    const ContractAttrEntry entries[] = {
        {600, kSyncMaskSelf},  // 声明仅写 base；slot 域继承 base（entity_attrs.hpp:68-75）
    };
    apply_contract_domains(attrs, entries, 1);

    EXPECT_EQ(attrs.domain(600), AttrDomain::kSelf);
    EXPECT_EQ(attrs.domain(EntityAttrs::slice_slot_id(600, 1)), AttrDomain::kSelf);
    EXPECT_EQ(attrs.domain(EntityAttrs::slice_slot_id(600, 2)), AttrDomain::kSelf);
}

TEST(AttrDomainMapping, ApplyContractDomainsLastWins) {
    EntityAttrs attrs(1);
    const ContractAttrEntry first[] = {{100, kSyncMaskSelf}};
    apply_contract_domains(attrs, first, 1);
    EXPECT_EQ(attrs.domain(100), AttrDomain::kSelf);

    const ContractAttrEntry second[] = {{100, kSyncMaskTeam}};
    apply_contract_domains(attrs, second, 1);
    EXPECT_EQ(attrs.domain(100), AttrDomain::kTeam);
}

TEST(AttrDomainMapping, ApplyContractDomainsEmptyTableIsNoop) {
    EntityAttrs attrs(1);
    apply_contract_domains(attrs, nullptr, 0);
    EXPECT_EQ(attrs.domain(1), AttrDomain::kPublic);
}
