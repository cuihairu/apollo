#pragma once

// PCG32 与子流派生（battle-determinism §4——确定性四约束之④随机数）
//
// 算法来源：PCG-XSH-RR 64/32 与 splitmix64 均为 Melissa O'Neill 公开发表的
// 公共领域算法（<www.pcg-random.org>）；本仓为最小自写实现，仅取算法本体，
// 未引入其参考代码或依赖。
//
// 落点（battle-determinism §8 P1）：RNG 子流组件随 base 模块（定时器轮同族），
// 掉落/AI/触发链等全游戏域共消费——battle 模块不反向持有 RNG。
//
// 口径（battle-determinism §4）：
// - 无全局 RNG 状态、无「按调用次数续流」游标——任意 (tick, stream_id)
//   独立派生子流，调用次序变化不产生漂移，任意 tick 可独立重放；
// - stream_id 分域：combat_roll（命中/暴击/抵抗）/ drop（掉落）/
//   ai_decision（AI 决策）/ proc（触发链）——各系统不共享子流。

#include <cstdint>

namespace apollo::base {

// 随机子流分域（battle-determinism §4 stream_id 分域）
enum class StreamId : std::uint64_t {
    CombatRoll = 1,  // 命中/暴击/抵抗
    Drop       = 2,  // 掉落
    AiDecision = 3,  // AI 决策（重算非录制）
    Proc       = 4,  // 触发链
};

// splitmix64：把任意 64 位键打散为均匀分布（子流派生用）
inline std::uint64_t splitmix64(std::uint64_t x) noexcept {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

// PCG-XSH-RR 64/32（最小实现：seed + next_uint32 + next_double）
class Pcg32 {
public:
    Pcg32() = default;

    // 以 (initstate, stream) 播种；stream 为子流序号（奇数化处理内含）
    void seed(std::uint64_t initstate, std::uint64_t stream) noexcept {
        state_ = 0;
        inc_ = (stream << 1) | 1ULL;
        next_uint32();
        state_ += initstate;
        next_uint32();
    }

    std::uint32_t next_uint32() noexcept {
        const std::uint64_t old = state_;
        state_ = old * 6364136223846793005ULL + inc_;
        const std::uint32_t xorshifted =
            static_cast<std::uint32_t>(((old >> 18) ^ old) >> 27);
        const std::uint32_t rot = static_cast<std::uint32_t>(old >> 59);
        return (xorshifted >> rot) | (xorshifted << ((-rot) & 31U));
    }

    // [0,1) 均匀分布（24 位精度，战斗判定足够且跨平台位精确）
    double next_double() noexcept {
        return static_cast<double>(next_uint32() >> 8) / 16777216.0;
    }

private:
    std::uint64_t state_ = 0;
    std::uint64_t inc_ = 0;
};

// 子流派生：(world_seed, tick, stream_id) → 独立子流（纯函数，battle-determinism
// §4 `pcg32_init(splitmix64(world_seed, tick, stream_id))` 的落地形态）。
// 同一三元组在任何机器、任何调用次序下得到同一序列。
inline Pcg32 derive_substream(std::uint64_t world_seed, std::uint64_t tick,
                              StreamId stream_id) noexcept {
    const std::uint64_t initstate =
        splitmix64(world_seed ^ splitmix64(tick * 0x9E3779B97F4A7C15ULL));
    const std::uint64_t stream =
        splitmix64(static_cast<std::uint64_t>(stream_id) + 0xDA3E39CB3B041B68ULL);
    Pcg32 rng;
    rng.seed(initstate, stream);
    return rng;
}

} // namespace apollo::base
