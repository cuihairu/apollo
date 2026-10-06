#include "apollo/game/social/guild.hpp"

#include <algorithm>
#include <utility>

namespace apollo::game::social {

Guild::Guild(std::uint64_t guild_id, std::string name, std::uint64_t leader_player_id)
    : guild_id_(guild_id)
    , name_(std::move(name))
    , leader_player_id_(leader_player_id) {
    // 创会人即首位成员（会长必为成员——transfer_leader 的前置不变式）
    members_.push_back(leader_player_id_);
    member_index_.emplace(leader_player_id_, 0);
}

std::uint64_t Guild::guild_id() const noexcept {
    return guild_id_;
}

const std::string& Guild::name() const noexcept {
    return name_;
}

std::uint64_t Guild::leader_player_id() const noexcept {
    return leader_player_id_;
}

const std::vector<std::uint64_t>& Guild::members() const noexcept {
    return members_;
}

std::size_t Guild::member_count() const noexcept {
    return members_.size();
}

bool Guild::has_member(std::uint64_t player_id) const noexcept {
    return member_index_.count(player_id) != 0;
}

bool Guild::add_member(std::uint64_t player_id) {
    if (members_.size() >= kMaxMembers) {
        return false;  // 满员拒绝
    }
    if (member_index_.count(player_id) != 0) {
        return false;  // 重复加入拒绝
    }
    member_index_.emplace(player_id, members_.size());
    members_.push_back(player_id);
    return true;
}

bool Guild::remove_member(std::uint64_t player_id) {
    if (player_id == leader_player_id_) {
        return false;  // 会长不可被移除（先 transfer_leader 或 disband）
    }
    auto it = member_index_.find(player_id);
    if (it == member_index_.end()) {
        return false;
    }
    const auto pos = it->second;
    members_.erase(members_.begin() + static_cast<std::ptrdiff_t>(pos));
    member_index_.erase(it);
    // 尾段下标回填（保序删除的索引维护）
    for (auto i = pos; i < members_.size(); ++i) {
        member_index_[members_[i]] = i;
    }
    return true;
}

bool Guild::transfer_leader(std::uint64_t new_leader_player_id) {
    if (new_leader_player_id == leader_player_id_) {
        return false;  // 自转移无意义，拒绝
    }
    if (!has_member(new_leader_player_id)) {
        return false;  // 新会长须为在册成员
    }
    leader_player_id_ = new_leader_player_id;
    return true;
}

Guild* GuildManager::create(std::uint64_t guild_id, std::string name,
                            std::uint64_t leader_player_id) {
    if (by_id_.count(guild_id) != 0 || by_name_.count(name) != 0) {
        return nullptr;  // 重 id/重名拒绝
    }
    guilds_.push_back(std::make_unique<Guild>(guild_id, std::move(name), leader_player_id));
    const auto pos = guilds_.size() - 1;
    by_id_.emplace(guild_id, pos);
    by_name_.emplace(guilds_[pos]->name(), pos);
    return guilds_[pos].get();
}

bool GuildManager::disband(std::uint64_t guild_id, std::uint64_t requester_player_id) {
    auto it = by_id_.find(guild_id);
    if (it == by_id_.end()) {
        return false;
    }
    const auto pos = it->second;
    if (guilds_[pos]->leader_player_id() != requester_player_id) {
        return false;  // 仅会长可解散
    }
    by_name_.erase(guilds_[pos]->name());
    guilds_.erase(guilds_.begin() + static_cast<std::ptrdiff_t>(pos));
    by_id_.erase(it);
    // 尾段下标回填
    for (auto i = pos; i < guilds_.size(); ++i) {
        by_id_[guilds_[i]->guild_id()] = i;
        by_name_[guilds_[i]->name()] = i;
    }
    return true;
}

Guild* GuildManager::find(std::uint64_t guild_id) const noexcept {
    auto it = by_id_.find(guild_id);
    return it == by_id_.end() ? nullptr : guilds_[it->second].get();
}

Guild* GuildManager::find_by_name(std::string_view name) const noexcept {
    auto it = by_name_.find(std::string(name));
    return it == by_name_.end() ? nullptr : guilds_[it->second].get();
}

std::size_t GuildManager::guild_count() const noexcept {
    return guilds_.size();
}

} // namespace apollo::game::social
