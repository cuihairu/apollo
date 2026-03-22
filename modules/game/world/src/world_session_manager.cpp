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

    it->second->suspend();
    return it->second;
}

WorldSessionManager::SessionPtr WorldSessionManager::resume_session(WorldSession::SessionId session_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return nullptr;
    }

    it->second->resume();
    return it->second;
}

WorldSessionManager::SessionPtr WorldSessionManager::transfer_session(
    WorldSession::SessionId session_id,
    std::uint32_t target_world_id,
    MapInstance::InstanceId target_map_instance_id,
    std::uint64_t target_space_id,
    bool inbound
) {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return nullptr;
    }

    it->second->begin_transfer(target_world_id, target_map_instance_id, target_space_id, inbound);
    it->second->set_route_version(it->second->route_version() + 1);
    return it->second;
}

WorldSessionManager::SessionPtr WorldSessionManager::complete_transfer(WorldSession::SessionId session_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return nullptr;
    }

    it->second->complete_transfer();
    return it->second;
}

void WorldSessionManager::close_session(WorldSession::SessionId session_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return;
    }

    it->second->begin_leave();
    it->second->set_state(WorldSessionState::Closed);
    player_index_.erase(it->second->player_id());
    sessions_.erase(it);
}

std::size_t WorldSessionManager::session_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.size();
}

} // namespace apollo::game::world
