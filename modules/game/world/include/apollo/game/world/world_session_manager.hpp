#pragma once

#include "apollo/game/world/world_session.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace apollo::game::world {

class WorldSessionManager {
public:
    using SessionPtr = std::shared_ptr<WorldSession>;

    SessionPtr create_session(WorldSession::SessionId session_id, WorldSession::PlayerId player_id);
    SessionPtr find_session(WorldSession::SessionId session_id) const;
    SessionPtr find_by_player(WorldSession::PlayerId player_id) const;
    SessionPtr suspend_session(WorldSession::SessionId session_id);
    SessionPtr resume_session(WorldSession::SessionId session_id);
    SessionPtr transfer_session(WorldSession::SessionId session_id,
                                std::uint32_t target_world_id,
                                Instance::InstanceId target_map_instance_id,
                                std::uint64_t target_space_id,
                                bool inbound = false);
    SessionPtr complete_transfer(WorldSession::SessionId session_id);
    // 断线发生在转移中（P1-1）：TransferringOut/In → Suspended 挂机窗口，
    // pending 保留（重连后 complete/abort 收口）；非转移中态返回 nullptr
    SessionPtr suspend_transfer_session(WorldSession::SessionId session_id);
    // 失败回滚分支（P0-4）：目标不可达/对端拒绝时清 pending、回到转移前态；
    // 非转移中态（含无 pending 的纯挂机）返回 nullptr
    SessionPtr abort_transfer(WorldSession::SessionId session_id);
    // close 流程修正（P0-4）：close_session 只置 Leaving（会话仍驻留可查，
    // 可观察窗口）；finalize_session 校验 Leaving 后置 Closed 并摘除索引
    SessionPtr close_session(WorldSession::SessionId session_id);
    SessionPtr finalize_session(WorldSession::SessionId session_id);
    [[nodiscard]] std::size_t session_count() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<WorldSession::SessionId, SessionPtr> sessions_;
    // 玩家索引：键为强分型 PlayerId（P0-2）——索引语义明确「按玩家身份查会话」，
    // 与按实体查（EntityId）在类型面上彻底分离
    std::unordered_map<apollo::game::core::PlayerId, WorldSession::SessionId,
                       apollo::game::core::PlayerIdHash>
        player_index_;
};

} // namespace apollo::game::world
