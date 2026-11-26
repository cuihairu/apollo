// 结算 ID 模拟代次域（L132「稳定玩法/结算 ID 校验模拟代次」）。
//
// 缺口：实例号是 cell 进程内裸计数（World::create_instance 的
// next_instance_id_ 从 1 起步，重启归 1），直接当结算 ID 会在两个维度撞号
// ——① 同 cell 进程重启后新副本重铸同号；② 多 cell 并存各自从 1 铸号。
// Base 侧去重环按 settlement_id 单键判重，跨代次撞号把新代次的合法结算
// 误判 duplicate = 静默漏奖（先删记录=永久漏奖且无法追查的同族缺口）。
//
// 口径（同 D10 gateway_id_base「高位身份域 | 低位计数」idiom）：
//   settlement_id = (salt << 32) | instance_id
//   salt = FNV-1a 32 位(world_id, cell_id, start_epoch) 折叠进 31 位
//   （0 保留：未装配盐 = 旧口径 instance_id 直用，既有行为零回归；折叠
//   结果为 0 时取 1，装配面永不退化为旧口径）。start_epoch 是注册化身
//   代次（steady_now_ms 自铸，重启必增）——跨进程代次必异；cell_id 区分
//   并行 cell；world_id 区分世界。同代次内 salt 恒定：重试/回执丢失重放
//   同 ID，Base 去重契约不变。
//
// 边界：instance_id ≥ 2^32 时按 id ^ (id >> 32) 确定性折叠进低位域——
// 该域只在单进程生命周期创建 >40 亿实例时可达（现实不可达）；折叠保证
// 同代次同实例稳定（重试不换键），跨实例碰撞仅存在于该不可达域。
#pragma once

#include <cstdint>

namespace squad_instance {

// 进程化身盐：FNV-1a 32 位逐字节折叠 (world_id, cell_id, start_epoch)，
// 保留 0（未装配语义）、折叠 0 → 1（装配面永不退化为旧口径）。
inline std::uint64_t settlement_id_salt(std::uint32_t world_id,
                                        std::uint64_t cell_id,
                                        std::uint64_t start_epoch) noexcept {
    const std::uint64_t inputs[3] = {world_id, cell_id, start_epoch};
    std::uint32_t hash = 2166136261u;  // FNV-1a offset basis
    for (const auto value : inputs) {
        for (int shift = 0; shift < 64; shift += 8) {
            hash ^= static_cast<std::uint32_t>((value >> shift) & 0xFFu);
            hash *= 16777619u;  // FNV-1a prime
        }
    }
    const auto salt = static_cast<std::uint64_t>(hash & 0x7FFFFFFFu);
    return salt == 0 ? 1 : salt;
}

// 结算幂等键组装：未装配盐（0）= instance_id 直用（旧口径零回归）；
// 装配后 = (salt << 32) | 低位实例号（超宽实例号确定性折叠，见头注）。
inline std::uint64_t compose_settlement_id(std::uint64_t salt,
                                           std::uint64_t instance_id) noexcept {
    if (salt == 0) return instance_id;
    const auto low = instance_id < (std::uint64_t{1} << 32)
                         ? instance_id
                         : (instance_id ^ (instance_id >> 32)) & 0xFFFFFFFFu;
    return (salt << 32) | low;
}

}  // namespace squad_instance
