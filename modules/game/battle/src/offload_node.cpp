#include "apollo/game/battle/offload_node.hpp"

namespace apollo::game::battle::offload {

namespace {

bool send_msg(IOffloadTransport& uplink, apollo::contract::gen::MsgId msg,
              const std::vector<std::uint8_t>& payload) {
    return uplink.send(OffloadEnvelope{msg, payload});
}

} // namespace

InstanceOffloadNode::InstanceOffloadNode(IOffloadTransport& uplink,
                                         std::uint64_t world_seed)
    : uplink_(uplink), world_seed_(world_seed) {}

bool InstanceOffloadNode::on_envelope(const OffloadEnvelope& envelope) {
    if (!is_offload_msg(envelope.msg) || finished_ || abandoned_) {
        return false;
    }
    switch (envelope.msg) {
    case apollo::contract::gen::MsgId::instance_spawn_request: {
        // 单 scene 单开进程：一次受理，二拉拒（§1.3 副本进程行）
        if (spawned_) {
            return false;
        }
        InstanceSpawnRequest request;
        if (!decode(envelope.payload, request) || request.instance_id == 0) {
            return false;
        }
        instance_id_ = request.instance_id;
        tick_interval_ms_ = request.tick_interval_ms;
        runtime_ = std::make_unique<apollo::game::battle::BattleRuntime>(
            request.instance_id, world_seed_, this);
        spawned_ = true;
        // ack-on-accept（ADR-017）：受理点回执即回，无同步就绪回包——
        // 就绪经目录 Advertise 可见（骨架期由宿主 notify_ready 钩子承载）
        InstanceSpawnReply reply;
        reply.instance_id = instance_id_;
        reply.accepted = true;
        return send_msg(uplink_, apollo::contract::gen::MsgId::instance_spawn_reply,
                        encode(reply));
    }
    case apollo::contract::gen::MsgId::battle_input: {
        if (!spawned_) {
            return false;
        }
        BattleInputMsg input;
        if (!decode(envelope.payload, input) ||
            input.instance_id != instance_id_) {
            return false;
        }
        // 输入对齐实例 tick 轴：排队待拍（tick() 消费到拍输入）
        inbox_.push_back(std::move(input));
        return true;
    }
    default:
        return false; // 920/922/923 是上行族，实例侧不受理
    }
}

bool InstanceOffloadNode::ensure_begun() {
    if (begun_) {
        return true;
    }
    // 参战玩家按输入首现懒入场（enter 段收集）；BattleRuntime 拒重复入场
    for (const auto& queued : inbox_) {
        runtime_->enter_player(static_cast<std::uint64_t>(queued.player_id));
    }
    if (runtime_->players().empty() || !runtime_->begin()) {
        return false;
    }
    begun_ = true;
    return true;
}

bool InstanceOffloadNode::tick(std::uint64_t tick) {
    const bool repeated_tick =
        last_tick_ >= 0 && static_cast<std::uint64_t>(last_tick_) >= tick;
    if (!spawned_ || finished_ || abandoned_ || repeated_tick) {
        return false; // 拍号严格递增（回放时间轴纪律，clock-and-time）
    }
    if (!ensure_begun()) {
        // 尚无参战者：空拍容忍（入拍等待），拍号仍前进
        last_tick_ = static_cast<std::int64_t>(tick);
        return true;
    }

    // 消费到拍输入，拍后迟到丢弃；opcodes 展开 = 每 opcode 一条运行时输入
    std::vector<apollo::game::battle::BattleInput> inputs;
    std::vector<BattleInputMsg> remaining;
    remaining.reserve(inbox_.size());
    for (auto& queued : inbox_) {
        if (queued.tick < static_cast<std::int64_t>(tick)) {
            continue; // 迟到丢弃（错误面语义随玩法批；骨架期静默）
        }
        if (queued.tick > static_cast<std::int64_t>(tick)) {
            remaining.push_back(std::move(queued));
            continue;
        }
        for (const auto opcode : queued.opcodes) {
            apollo::game::battle::BattleInput in;
            in.tick = static_cast<std::uint32_t>(tick);
            in.player_id = queued.player_id;
            in.opcode = static_cast<std::uint32_t>(opcode);
            inputs.push_back(in);
        }
    }
    inbox_.swap(remaining);

    runtime_->tick(static_cast<std::uint32_t>(tick), inputs);
    last_tick_ = static_cast<std::int64_t>(tick);

    // 每拍出投影（易失态投影编码随 battle-determinism 定型，骨架期恒空）
    BattleStateSync sync;
    sync.instance_id = instance_id_;
    sync.tick = last_tick_;
    sync.snapshot_seq = ++snapshot_seq_;
    for (const auto pid : runtime_->players()) {
        sync.players.push_back(static_cast<std::int64_t>(pid));
    }
    return send_msg(uplink_, apollo::contract::gen::MsgId::battle_state_sync,
                    encode(sync));
}

bool InstanceOffloadNode::settle() {
    if (!spawned_ || finished_ || abandoned_) {
        return false;
    }
    if (!ensure_begun()) {
        return false; // 无参战者不成战斗（begin 语义同 runtime）
    }
    // 五段收尾：Rewarding 段经本类 on_reward 捕获（player_id 全序，含 0 分）
    runtime_->finish();
    finished_ = true;

    BattleResult result;
    result.instance_id = instance_id_;
    result.tick = last_tick_;
    result.replay_hash = static_cast<std::int64_t>(runtime_->hash_chain());
    result.player_ids = result_player_ids_;
    result.scores = result_scores_;
    return send_msg(uplink_, apollo::contract::gen::MsgId::battle_result,
                    encode(result));
    // 自灭：finished_ = true，实例条目由宿主收割（teardown = 结算后自灭 §3）
}

void InstanceOffloadNode::abandon() {
    if (finished_) {
        return;
    }
    abandoned_ = true;
    inbox_.clear();
    runtime_.reset();
}

void InstanceOffloadNode::on_reward(std::uint64_t player_id, std::int64_t score) {
    result_player_ids_.push_back(static_cast<std::int64_t>(player_id));
    result_scores_.push_back(score);
}

} // namespace apollo::game::battle::offload
