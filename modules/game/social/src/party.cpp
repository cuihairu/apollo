#include "apollo/game/social/party.hpp"

#include <utility>

namespace apollo::game::social {

Party::Party(std::uint64_t party_id)
    : party_id_(party_id) {
}

std::uint64_t Party::party_id() const noexcept {
    return party_id_;
}

std::uint64_t Party::leader_player_id() const noexcept {
    return leader_player_id_;
}

const std::vector<std::uint64_t>& Party::members() const noexcept {
    return members_;
}

std::size_t Party::member_count() const noexcept {
    return members_.size();
}

bool Party::has_member(std::uint64_t player_id) const noexcept {
    return member_index_.count(player_id) != 0;
}

bool Party::empty() const noexcept {
    return members_.empty();
}

bool Party::add_member(std::uint64_t player_id) {
    if (members_.size() >= kMaxMembers) {
        return false;  // 满员拒绝
    }
    if (member_index_.count(player_id) != 0) {
        return false;  // 重复加入拒绝
    }
    if (members_.empty()) {
        leader_player_id_ = player_id;  // 首位成员即队长
    }
    member_index_.emplace(player_id, members_.size());
    members_.push_back(player_id);
    return true;
}

bool Party::leave(std::uint64_t player_id) {
    auto it = member_index_.find(player_id);
    if (it == member_index_.end()) {
        return false;
    }
    const auto pos = it->second;
    members_.erase(members_.begin() + static_cast<std::ptrdiff_t>(pos));
    member_index_.erase(it);
    for (auto i = pos; i < members_.size(); ++i) {
        member_index_[members_[i]] = i;
    }
    if (player_id == leader_player_id_ && !members_.empty()) {
        leader_player_id_ = members_.front();  // 队长退队移交最早成员
    }
    return true;
}

bool Party::kick(std::uint64_t target_player_id, std::uint64_t requester_player_id) {
    if (requester_player_id != leader_player_id_) {
        return false;  // 仅队长可踢人
    }
    if (target_player_id == requester_player_id) {
        return false;  // 踢自己走 leave（语义分立：kick 是队长对他人）
    }
    return leave(target_player_id);
}

bool Party::transfer_leader(std::uint64_t new_leader_player_id) {
    if (new_leader_player_id == leader_player_id_) {
        return false;
    }
    if (!has_member(new_leader_player_id)) {
        return false;  // 新队长须为在册成员
    }
    leader_player_id_ = new_leader_player_id;
    return true;
}

Party* PartyManager::create(std::uint64_t party_id, std::uint64_t leader_player_id) {
    if (by_id_.count(party_id) != 0) {
        return nullptr;  // 重 id 拒绝
    }
    parties_.push_back(std::make_unique<Party>(party_id));
    const auto pos = parties_.size() - 1;
    parties_[pos]->add_member(leader_player_id);  // 队长即首位成员
    by_id_.emplace(party_id, pos);
    return parties_[pos].get();
}

bool PartyManager::disband(std::uint64_t party_id, std::uint64_t requester_player_id) {
    auto it = by_id_.find(party_id);
    if (it == by_id_.end()) {
        return false;
    }
    const auto pos = it->second;
    if (parties_[pos]->leader_player_id() != requester_player_id) {
        return false;  // 仅队长可解散
    }
    parties_.erase(parties_.begin() + static_cast<std::ptrdiff_t>(pos));
    by_id_.erase(it);
    for (auto i = pos; i < parties_.size(); ++i) {
        by_id_[parties_[i]->party_id()] = i;
    }
    return true;
}

Party* PartyManager::find(std::uint64_t party_id) const noexcept {
    auto it = by_id_.find(party_id);
    return it == by_id_.end() ? nullptr : parties_[it->second].get();
}

std::size_t PartyManager::party_count() const noexcept {
    return parties_.size();
}

} // namespace apollo::game::social
