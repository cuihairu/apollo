#include "apollo/game/battle/offload_host.hpp"

namespace apollo::game::battle::offload {

namespace {

bool is_terminal(InstanceOffloadHost::InstancePhase phase) {
    return phase == InstanceOffloadHost::InstancePhase::Settled ||
           phase == InstanceOffloadHost::InstancePhase::Rejected ||
           phase == InstanceOffloadHost::InstancePhase::Voided;
}

} // namespace

InstanceOffloadHost::InstanceOffloadHost(
    IOffloadTransport& downlink, apollo::game::battle::IRewardSink* result_sink)
    : downlink_(downlink), result_sink_(result_sink) {}

bool InstanceOffloadHost::request_spawn(const InstanceSpawnRequest& request) {
    if (request.instance_id == 0 || instances_.count(request.instance_id) != 0) {
        return false;
    }
    // 先记账后发包：受理回执可能同步回流（内存桩直呼），条目须先存在才能
    // 承接；发不出则撤账，不留空悬 Requesting。
    instances_.emplace(request.instance_id, InstanceState{});
    const OffloadEnvelope envelope{apollo::contract::gen::MsgId::instance_spawn_request,
                                   encode(request)};
    if (!downlink_.send(envelope)) {
        instances_.erase(request.instance_id);
        return false;
    }
    return true;
}

bool InstanceOffloadHost::notify_ready(std::uint64_t instance_id) {
    auto it = instances_.find(instance_id);
    if (it == instances_.end() || it->second.phase != InstancePhase::Accepted) {
        return false;
    }
    it->second.phase = InstancePhase::Running;
    return true;
}

bool InstanceOffloadHost::notify_crash(std::uint64_t instance_id) {
    auto it = instances_.find(instance_id);
    if (it == instances_.end() || is_terminal(it->second.phase)) {
        return false;
    }
    it->second.phase = InstancePhase::Voided;
    return true;
}

bool InstanceOffloadHost::send_battle_input(const BattleInputMsg& input) {
    auto it = instances_.find(input.instance_id);
    if (it == instances_.end() || it->second.phase != InstancePhase::Running) {
        return false;
    }
    const OffloadEnvelope envelope{apollo::contract::gen::MsgId::battle_input,
                                   encode(input)};
    return downlink_.send(envelope);
}

bool InstanceOffloadHost::on_envelope(const OffloadEnvelope& envelope) {
    if (!is_offload_msg(envelope.msg)) {
        return false;
    }
    switch (envelope.msg) {
    case apollo::contract::gen::MsgId::instance_spawn_reply: {
        InstanceSpawnReply reply;
        if (!decode(envelope.payload, reply)) {
            return false;
        }
        auto it = instances_.find(reply.instance_id);
        if (it == instances_.end() || it->second.phase != InstancePhase::Requesting) {
            return false;
        }
        it->second.phase = reply.accepted ? InstancePhase::Accepted
                                          : InstancePhase::Rejected;
        return true;
    }
    case apollo::contract::gen::MsgId::battle_state_sync: {
        BattleStateSync sync;
        if (!decode(envelope.payload, sync)) {
            return false;
        }
        auto it = instances_.find(sync.instance_id);
        if (it == instances_.end() || it->second.phase != InstancePhase::Running) {
            return false;
        }
        // 投影水位严格递增（attribute-sync seq 纪律同源）：重复/倒退整包拒。
        if (sync.snapshot_seq <= it->second.snapshot_seq) {
            return false;
        }
        it->second.snapshot_seq = sync.snapshot_seq;
        return true;
    }
    case apollo::contract::gen::MsgId::battle_result: {
        BattleResult result;
        if (!decode(envelope.payload, result)) {
            return false;
        }
        auto it = instances_.find(result.instance_id);
        if (it == instances_.end() || it->second.phase != InstancePhase::Running) {
            return false;
        }
        // 平行数组长度必须相等（923 字段纪律）
        if (result.player_ids.size() != result.scores.size()) {
            return false;
        }
        // 落回出账（单向：只出不进，不回写实例）
        if (result_sink_ != nullptr) {
            for (std::size_t i = 0; i < result.player_ids.size(); ++i) {
                result_sink_->on_reward(static_cast<std::uint64_t>(result.player_ids[i]),
                                        result.scores[i]);
            }
        }
        it->second.phase = InstancePhase::Settled;
        return true;
    }
    default:
        return false; // 919/921 是下行族，上行不受理
    }
}

std::optional<InstanceOffloadHost::InstancePhase> InstanceOffloadHost::phase(
    std::uint64_t instance_id) const {
    const auto it = instances_.find(instance_id);
    if (it == instances_.end()) {
        return std::nullopt;
    }
    return it->second.phase;
}

std::uint64_t InstanceOffloadHost::last_snapshot_seq(
    std::uint64_t instance_id) const {
    const auto it = instances_.find(instance_id);
    if (it == instances_.end()) {
        return 0;
    }
    return it->second.snapshot_seq;
}

} // namespace apollo::game::battle::offload
