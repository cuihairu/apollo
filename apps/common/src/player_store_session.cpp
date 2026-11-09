#include "player_store/player_store.hpp"
#include "player_store/business_wire.hpp"
#include "player_store/migration_puller.hpp"
#include "player_store/migration_wire.hpp"
#include "player_store_detail.hpp"
#include "apollo/core/log/log_manager.h"
#include "apollo/game/base/avatar_reward_sink.hpp"
#include "apollo/protocol/messages.hpp"
#include "apollo/protocol/codec.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <utility>

namespace player_store {

namespace {

// 库面日志出口（detail 单定义，本单元转发——定义在 player_store.cpp）
apollo::core::log::Logger& blog() {
    return store_detail::blog();
}

} // namespace

std::shared_ptr<apollo::game::base::Avatar> PlayerStore::activatePlayer(PlayerID playerId) {
    // 存档串行化：fresh 判定、播种、清脏同一临界区——并发激活不得双播种
    std::lock_guard<std::mutex> lock(storeMutex_);
    PlayerData data;
    if (!database_->loadPlayer(playerId, data)) {
        return nullptr;
    }

    // 写端代次账播种（L64 Base 半）：首载档案代次（重启续代）；已有账
    // 取大不回退（重激活不降代次）
    if (const auto gen = writeGens_.find(playerId); gen != writeGens_.end()) {
        gen->second = std::max(gen->second, data.writeGen);
    } else {
        writeGens_.emplace(playerId, data.writeGen);
    }

    // 长期态播种（真实奖励持久化）：exp 以 Avatar 为持久真源——新锚点按
    // 现档播种，flush 收集覆盖写回；仅新锚点播种（既存锚点重复加值即双记）。
    const bool fresh = avatarManager_->find(playerId) == nullptr;
    auto avatar = activateAvatar(playerId);
    if (!avatar) {
        return nullptr;
    }
    if (fresh) {
        avatar->add_progress(apollo::game::base::AvatarRewardSink::kExpKey,
                             data.exp);
    }

    avatar->clear_dirty();
    return avatar;
}

void PlayerStore::set_directory_event_sink(
    apollo::game::session::PlayerDirectory::EventSink sink) {
    directoryEventSink_ = std::move(sink);
}

bool PlayerStore::bindSession(
    PlayerID playerId,
    const apollo::game::base::SessionBinding& binding
) {
    if (!avatarManager_->find(playerId)) {
        // 未激活先激活（activatePlayer 自持串行点）；拒绝路径的态判定在
        // 下方临界区内复核——find 与锁间窗口内冻结可能落位
        if (!activatePlayer(playerId)) {
            blog().warning("Player " + std::to_string(playerId)
                           + " bind rejected (no profile)");
            return false;
        }
    }

    // 冻结锚点拒绑（P3-6 迟到输入）：写权已交新进程（§9.2 ①），迟到
    // 重绑/重连不得把 Transferring 翻回 Online（否则源域重获写权 + 重回
    // 在册集，双写不变量破口）
    std::lock_guard<std::mutex> lock(storeMutex_);
    auto avatar = avatarManager_->find(playerId);
    if (!avatar) {
        return false;
    }
    if (avatar->state() == apollo::game::base::AvatarState::Transferring) {
        blog().warning("Player " + std::to_string(playerId)
                       + " bind rejected (frozen for migration)");
        return false;
    }

    avatar->bind_session(binding);
    avatar->set_state(apollo::game::base::AvatarState::Online);
    sessionLocator_->bind(playerId, binding);

    // 目录事件源①（P1-2）：会话建立上报 SessionUp 同形事件（权威目录在
    // manager 域；sink 缺省静默，跨进程总线上报随 P3）
    if (directoryEventSink_) {
        apollo::game::session::PlayerDirectory::Event event;
        event.kind = apollo::game::session::PlayerDirectory::Event::Kind::SessionUp;
        event.player_id = playerId;
        event.binding = binding;
        if (avatar->world_assignment().is_assigned()) {
            event.assignment = avatar->world_assignment();
        }
        directoryEventSink_(event);
    }
    return true;
}

bool PlayerStore::unbindSession(protocol::SessionID sessionId) {
    const auto playerId = sessionLocator_->find_player_by_session(sessionId);
    if (!playerId.has_value()) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(storeMutex_);
        if (auto avatar = avatarManager_->find(*playerId)) {
            avatar->unbind_session(sessionId);
            avatar->set_state(apollo::game::base::AvatarState::Disconnected);
        }
    }

    sessionLocator_->unbind_session(sessionId);

    // 目录事件源①（P1-2）：会话消亡上报 SessionDown 同形事件
    if (directoryEventSink_) {
        apollo::game::session::PlayerDirectory::Event event;
        event.kind = apollo::game::session::PlayerDirectory::Event::Kind::SessionDown;
        event.player_id = *playerId;
        event.reason = apollo::game::session::PlayerDirectory::kReasonLogout;
        directoryEventSink_(event);
    }
    return true;
}

std::shared_ptr<apollo::game::base::Avatar> PlayerStore::findAvatar(PlayerID playerId) const {
    return avatarManager_->find(playerId);
}

std::optional<PlayerID> PlayerStore::findPlayerBySession(protocol::SessionID sessionId) const {
    return sessionLocator_->find_player_by_session(sessionId);
}

std::vector<apollo::game::session::MirrorEntry> collect_mirror_entries(
    const PlayerStore& store, std::uint32_t hosting_zone) {
    std::vector<apollo::game::session::MirrorEntry> sessions;
    const auto avatars = store.avatar_manager();
    if (!avatars) {
        return sessions;
    }
    // 承载域口径与冻结锚点过滤语义见头注（collect_mirror_entries）。
    sessions.reserve(avatars->avatar_count());
    for (const auto& avatar : avatars->snapshot()) {
        if (!avatar || avatar->player_id() == 0) {
            continue;
        }
        if (avatar->state() ==
            apollo::game::base::AvatarState::Transferring) {
            continue;
        }
        apollo::game::session::MirrorEntry e;
        e.player_id = avatar->player_id();
        e.session_id = avatar->session_binding().session_id;
        e.gateway_id = avatar->session_binding().gateway_id;
        e.zone_id = hosting_zone;
        e.state = apollo::game::session::PlayerDirectory::EntryState::Online;
        e.assignment = avatar->world_assignment();
        sessions.push_back(e);
    }
    return sessions;
}

} // namespace player_store

