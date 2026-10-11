#pragma once

// Instance offload wire 面 + 传输缝（P3-1 批 2：offload 骨架 · 内存桩过渡，
// docs/design/battle-instance-offload §5 拍板 (b)）。
//
// 信封纪律（net-abstraction §7「信封与契约合流」）：消息身份只有一份——
// 生成器产物 apollo_contract.h 的 MsgId（internal 域 919-923 副本实例族，
// 契约批已入库），本文件不手写第二份协议号；载荷编码按契约字段序逐字段
// 小端写读（wire 纪律同 discovery：不 memcpy 结构体，防对齐/端序漂移）。
//
// 传输分界：本文件只定信封/载荷与 IOffloadTransport 缝，不含 socket——
// M1 自研内核就绪后换底座 = 换 IOffloadTransport 实现（§5 裁决代价：
// 一次底座替换，生命周期/wire 面不动）。MemoryOffloadTransport 是过渡期
// 底座本身（§5(b)：同机管道候选形态 pipe/shmem SPSC；骨架期同步直呼）。

#include "apollo_contract.h"

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace apollo::game::battle::offload {

// ---- 契约身份钉（internal 域副本实例族；族挪号必须过本断言）----

static_assert(static_cast<std::uint16_t>(apollo::contract::gen::MsgId::instance_spawn_request) == 919);
static_assert(static_cast<std::uint16_t>(apollo::contract::gen::MsgId::instance_spawn_reply) == 920);
static_assert(static_cast<std::uint16_t>(apollo::contract::gen::MsgId::battle_input) == 921);
static_assert(static_cast<std::uint16_t>(apollo::contract::gen::MsgId::battle_state_sync) == 922);
static_assert(static_cast<std::uint16_t>(apollo::contract::gen::MsgId::battle_result) == 923);

// ---- 载荷结构（字段序 = 契约 919-923 字段序；类型映射随 kFields_*）----

struct InstanceSpawnRequest {
    std::uint64_t instance_id = 0;
    std::uint64_t scene_id = 0;
    std::uint64_t map_id = 0;
    std::int32_t tick_interval_ms = 0;  // 实例 tick 独立节拍（20-50ms 可配，§2 适配缝②）
    std::int32_t participant_count = 0; // 预期参战规模（资源档位）
    std::string spawn_args;             // 拉起参数（进程模板与命令行）
};

struct InstanceSpawnReply {
    std::uint64_t instance_id = 0;
    bool accepted = false; // ack-on-accept（ADR-017：受理点回执，非就绪信号）
    std::string error_message;
};

struct BattleInputMsg {
    std::uint64_t instance_id = 0;
    std::uint64_t player_id = 0;
    std::int64_t tick = 0;             // 实例 tick 轴拍号（与 Zone 轴独立，§6）
    std::vector<std::int64_t> opcodes; // 操作码序列（BattleInput.opcodes 承载语义）
};

struct BattleStateSync {
    std::uint64_t instance_id = 0;
    std::int64_t tick = 0;
    std::uint64_t snapshot_seq = 0;    // 投影水位（attribute-sync seq 纪律同源）
    std::vector<std::int64_t> players; // 本帧在投影内的参战玩家
    std::vector<std::uint8_t> payload; // 易失态投影编码（随 battle-determinism 定型，骨架期恒空）
};

struct BattleResult {
    std::uint64_t instance_id = 0;
    std::int64_t tick = 0;                // 结算拍号（实例 tick 轴）
    std::int64_t replay_hash = 0;         // 事件 hash 链尾（battle_replay，跨机回放校验）
    std::vector<std::int64_t> player_ids; // 逐玩家落回（与 scores 平行）
    std::vector<std::int64_t> scores;     // on_reward(player_id, score) 载荷（IRewardSink 语义）
};

// 信封（§5「信封与契约合流」）：id 取生成器 MsgId；wire 形 = u16 id + 载荷
// （自描述——换底座不改信封面）。
struct OffloadEnvelope {
    apollo::contract::gen::MsgId msg{};
    std::vector<std::uint8_t> payload;
};

// ---- 逐字段小端编解码（纪律同 discovery wire）----
// 编码：u32/u64 逐字节小端；bool 1B；string/bytes = u32 长度 + 原文；
// int64[] = u32 元素数 + 逐元素 8B。
// 解码：截断/尾部余字节 → false（整包拒，不猜）。
std::vector<std::uint8_t> encode(const InstanceSpawnRequest&);
std::vector<std::uint8_t> encode(const InstanceSpawnReply&);
std::vector<std::uint8_t> encode(const BattleInputMsg&);
std::vector<std::uint8_t> encode(const BattleStateSync&);
std::vector<std::uint8_t> encode(const BattleResult&);

bool decode(const std::vector<std::uint8_t>& wire, InstanceSpawnRequest& out);
bool decode(const std::vector<std::uint8_t>& wire, InstanceSpawnReply& out);
bool decode(const std::vector<std::uint8_t>& wire, BattleInputMsg& out);
bool decode(const std::vector<std::uint8_t>& wire, BattleStateSync& out);
bool decode(const std::vector<std::uint8_t>& wire, BattleResult& out);

// 副本实例族判定（信封 id 准入：族外 id 整包拒）
bool is_offload_msg(apollo::contract::gen::MsgId msg);

std::vector<std::uint8_t> encode_envelope(const OffloadEnvelope& envelope);
// 拆包：id 必须属副本实例族（载荷合法性由按型 decode 把关）
bool decode_envelope(const std::vector<std::uint8_t>& wire, OffloadEnvelope& out);

// ---- 传输缝（§5 依赖序的解耦点）----

// 单向投递。单线程纪律（同 discovery 目录侧）：send/deliver 全部由驱动方
// 线程泵，实现不起内部线程、不依赖钟。
class IOffloadTransport {
public:
    virtual ~IOffloadTransport() = default;
    virtual bool send(const OffloadEnvelope& envelope) = 0;
};

// 内存桩底座（§5(b) 过渡）：attach 的收端 sink 直呼，无队列无缓冲——
// 队列/背压语义随真传输引入，不在桩里预支。对端未挂 sink 时 send 拒。
class MemoryOffloadTransport : public IOffloadTransport {
public:
    using Sink = std::function<void(const OffloadEnvelope&)>;
    void attach(Sink sink) { sink_ = std::move(sink); }
    bool send(const OffloadEnvelope& envelope) override {
        if (!sink_) {
            return false;
        }
        sink_(envelope);
        return true;
    }

private:
    Sink sink_;
};

} // namespace apollo::game::battle::offload
