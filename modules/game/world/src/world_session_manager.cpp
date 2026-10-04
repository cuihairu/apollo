#include "apollo/game/world/world_session_manager.hpp"

namespace apollo::game::world {

WorldSessionManager::SessionPtr WorldSessionManager::create_session(
    WorldSession::SessionId session_id,
    WorldSession::PlayerId player_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto session = std::make_shared<WorldSession>(session_id, player_id);
    sessions_[session_id] = session;
    player_index_[player_id] = session_id;
    return session;
}

WorldSessionManager::SessionPtr WorldSessionManager::find_session(WorldSession::SessionId session_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = sessions_.find(session_id);
    return it == sessions_.end() ? nullptr : it->second;
}

WorldSessionManager::SessionPtr WorldSessionManager::find_by_player(WorldSession::PlayerId player_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto player_it = player_index_.find(player_id);
    if (player_it == player_index_.end()) {
        return nullptr;
    }

    const auto session_it = sessions_.find(player_it->second);
    return session_it == sessions_.end() ? nullptr : session_it->second;
}

WorldSessionManager::SessionPtr WorldSessionManager::suspend_session(WorldSession::SessionId session_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return nullptr;
    }

    // 态校验（P0-4）：非 Active 会话不可挂起（返回 nullptr，状态不变）
    if (!it->second->suspend()) {
        return nullptr;
    }
    return it->second;
}

WorldSessionManager::SessionPtr WorldSessionManager::resume_session(WorldSession::SessionId session_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return nullptr;
    }

    // 态校验（P0-4）：Suspended 恢复 / Entering 激活——Closed/Leaving 会话
    // 不可直接 resume
    if (!it->second->resume()) {
        return nullptr;
    }
    return it->second;
}

WorldSessionManager::SessionPtr WorldSessionManager::transfer_session(
    WorldSession::SessionId session_id,
    std::uint32_t target_world_id,
    Instance::InstanceId target_map_instance_id,
    std::uint64_t target_space_id,
    bool inbound
) {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return nullptr;
    }

    // 态校验（P0-4）：准入 = Active；转移后进入可观察的 Transferring 窗口
    // （complete/abort 之前不再瞬时），期间路由版本推进。
    if (!it->second->begin_transfer(target_world_id, target_map_instance_id, target_space_id, inbound)) {
        return nullptr;
    }
    it->second->set_route_version(it->second->route_version() + 1);
    return it->second;
}

WorldSessionManager::SessionPtr WorldSessionManager::complete_transfer(WorldSession::SessionId session_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return nullptr;
    }

    // 态校验（P0-4）：仅转移中可确认
    if (!it->second->complete_transfer()) {
        return nullptr;
    }
    return it->second;
}

WorldSessionManager::SessionPtr WorldSessionManager::abort_transfer(WorldSession::SessionId session_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return nullptr;
    }

    // 失败回滚分支（P0-4）：目标不可达/对端拒绝时清 pending、回到转移前态
    if (!it->second->abort_transfer()) {
        return nullptr;
    }
    return it->second;
}

WorldSessionManager::SessionPtr WorldSessionManager::close_session(WorldSession::SessionId session_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return nullptr;
    }

    // close 流程修正（P0-4，lifecycle §2 审计）：置 Leaving 后会话仍驻留
    // 可查（可观察窗口），由 finalize_session 显式终结；不再一步吞进 Closed。
    if (!it->second->begin_leave()) {
        return nullptr;  // 已在 Leaving/Closed
    }
    return it->second;
}

WorldSessionManager::SessionPtr WorldSessionManager::finalize_session(WorldSession::SessionId session_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return nullptr;
    }

    // 终结校验（P0-4）：仅 Leaving 可进入 Closed（终态）
    if (it->second->state() != WorldSessionState::Leaving) {
        return nullptr;
    }

    it->second->set_state(WorldSessionState::Closed);
    auto session = it->second;
    player_index_.erase(session->player_id());
    sessions_.erase(it);
    return session;
}

std::size_t WorldSessionManager::session_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.size();
}

} // namespace apollo::game::world
