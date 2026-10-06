#pragma once

// 战斗回放四元组与滚动 hash 链（P2-2，battle-determinism §3）
//
// 口径（battle-determinism §3）：
// - 一场可复现战斗 = 四元组 (binary_id, world_seed, 起始快照位点, 输入序列)；
// - 复算按 tick 逐步重演，每 tick 末对战斗事件流按规范序（entity_id）算
//   滚动 hash，与线上逐 tick 对照——首个发散点即作弊/缺陷定位点；
// - 两种回放形态分开：事件流重演（廉价，表现层）vs 复算回放（昂贵，按需）。
//   本头只承载公共的记录与 hash 件，重演器随 P4 验证服务落地。
//
// 规范序：同 tick 内事件按 (entity_id, kind, value) 升序排列后折叠进
// hash 链——模拟段产生次序不要求（约束③迭代序），结算/记账段以排序归一。

#include <cstdint>
#include <vector>

namespace apollo::game::battle {

// 回放四元组（battle-determinism §3）
struct ReplayTuple {
    std::uint64_t binary_id = 0;     // 程序版本指纹（构建指纹，语义由验证服务定义）
    std::uint64_t world_seed = 0;    // 世界种子（子流派生根）
    std::uint64_t snapshot_pos = 0;  // 起始快照位点（快照存储内的定位符）
    // 输入序列：按 tick 递增的玩家意图集（骨架期以编码 uint64 承载，
    // 协议化随 P2-3 契约修订）
    std::vector<std::uint64_t> inputs;
};

// 单条战斗事件（判定输出：事件流 = 每场战斗的全部判定结果）
struct BattleEvent {
    std::uint32_t tick = 0;         // 发生 tick
    std::uint64_t entity_id = 0;    // 规范序主键
    std::uint32_t kind = 0;         // 事件种类（域内编号，映射随玩法批）
    std::int32_t value = 0;         // 数值载荷（伤害/奖励量等）
};

// FNV-1a 64：滚动 hash 链的折叠函数（公开算法，无需保密性，仅作一致性指纹）
inline std::uint64_t fnv1a64_step(std::uint64_t h, std::uint64_t v) noexcept {
    h ^= v + 0x9E3779B97F4A7C15ULL + (h << 6) + (h >> 2);
    h *= 0x100000001B3ULL;
    return h;
}

// 把一条事件折叠进链（按字段定宽编码，跨平台位精确）
inline std::uint64_t hash_chain_step(std::uint64_t h, const BattleEvent& e) noexcept {
    h = fnv1a64_step(h, e.tick);
    h = fnv1a64_step(h, e.entity_id);
    h = fnv1a64_step(h, e.kind);
    h = fnv1a64_step(h, static_cast<std::uint32_t>(e.value));
    return h;
}

// 规范序比较器：同 tick 内 (entity_id, kind, value) 升序（约束③结算段全序）
inline bool battle_event_canonical_less(const BattleEvent& a, const BattleEvent& b) noexcept {
    if (a.entity_id != b.entity_id) return a.entity_id < b.entity_id;
    if (a.kind != b.kind) return a.kind < b.kind;
    return a.value < b.value;
}

} // namespace apollo::game::battle
