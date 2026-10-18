#include "apollo/game/base/avatar.hpp"

namespace apollo::game::base {

Avatar::Avatar(std::uint64_t player_id)
    : player_id_(player_id) {
}

std::uint64_t Avatar::player_id() const noexcept {
    return player_id_;
}

AvatarState Avatar::state() const noexcept {
    return state_;
}

void Avatar::set_state(AvatarState state) noexcept {
    state_ = state;
}

const SessionBinding& Avatar::session_binding() const noexcept {
    return session_binding_;
}

void Avatar::bind_session(const SessionBinding& binding) {
    session_binding_ = binding;
}

void Avatar::unbind_session(std::uint64_t session_id) {
    if (session_binding_.session_id == session_id) {
        session_binding_ = {};
    }
}

const WorldAssignment& Avatar::world_assignment() const noexcept {
    return world_assignment_;
}

void Avatar::assign_world(const WorldAssignment& assignment) noexcept {
    world_assignment_ = assignment;
}

void Avatar::clear_world_assignment() noexcept {
    world_assignment_ = {};
}

std::uint32_t Avatar::home_zone_id() const noexcept {
    return home_zone_id_;
}

void Avatar::set_home_zone_id(std::uint32_t home_zone_id) noexcept {
    if (home_zone_id_ == home_zone_id) {
        return;
    }
    home_zone_id_ = home_zone_id;
    mark_dirty("home_zone_id");
}

void Avatar::set_journal(JournalFn journal) {
    journal_ = std::move(journal);
}

const Avatar::JournalFn& Avatar::journal() const noexcept {
    return journal_;
}

void Avatar::mark_dirty(std::string_view reason) {
    dirty_ = true;
    if (!reason.empty()) {
        dirty_reasons_.emplace_back(reason);
        if (journal_ != nullptr) {
            journal_(*this, reason);
        }
    }
}

bool Avatar::needs_save() const noexcept {
    return dirty_;
}

void Avatar::clear_dirty() noexcept {
    dirty_ = false;
    dirty_reasons_.clear();
}

const std::vector<std::string>& Avatar::dirty_reasons() const noexcept {
    return dirty_reasons_;
}

// ---- 长期态字段模型（P2-4）：四域变更一律 mark_dirty（长期态变更即脏）----

const std::vector<ItemStack>& Avatar::inventory() const noexcept {
    return inventory_;
}

bool Avatar::add_item(std::uint64_t item_id, std::uint64_t count) {
    if (item_id == 0 || count == 0) {
        return false;
    }
    for (auto& stack : inventory_) {
        if (stack.item_id == item_id) {
            stack.count += count;
            mark_dirty("inventory_add");
            return true;
        }
    }
    inventory_.push_back(ItemStack{item_id, count});
    mark_dirty("inventory_add");
    return true;
}

bool Avatar::remove_item(std::uint64_t item_id, std::uint64_t count) {
    if (item_id == 0 || count == 0) {
        return false;
    }
    for (auto it = inventory_.begin(); it != inventory_.end(); ++it) {
        if (it->item_id == item_id) {
            if (it->count < count) {
                return false;  // 不足整体拒绝（无部分扣减）
            }
            it->count -= count;
            if (it->count == 0) {
                inventory_.erase(it);
            }
            mark_dirty("inventory_remove");
            return true;
        }
    }
    return false;  // 无此物品
}

std::uint64_t Avatar::equipment_at(std::size_t slot) const noexcept {
    return slot < kEquipSlotCount ? equipment_[slot] : 0;
}

bool Avatar::equip(std::size_t slot, std::uint64_t item_id) {
    if (slot >= kEquipSlotCount || item_id == 0) {
        return false;
    }
    equipment_[slot] = item_id;
    mark_dirty("equipment_change");
    return true;
}

bool Avatar::unequip(std::size_t slot) {
    if (slot >= kEquipSlotCount || equipment_[slot] == 0) {
        return false;  // 空槽无需卸下
    }
    equipment_[slot] = 0;
    mark_dirty("equipment_change");
    return true;
}

void Avatar::set_quest_progress(std::uint64_t quest_id, std::uint64_t progress) {
    if (quest_id == 0) {
        return;
    }
    quests_[quest_id] = progress;
    mark_dirty("quest_progress");
}

std::uint64_t Avatar::quest_progress(std::uint64_t quest_id) const noexcept {
    auto it = quests_.find(quest_id);
    return it == quests_.end() ? 0 : it->second;
}

const std::unordered_map<std::uint64_t, std::uint64_t>& Avatar::quests() const noexcept {
    return quests_;
}

void Avatar::add_progress(std::string_view key, std::int64_t delta) {
    // 冻结只读（P3-6 迟到输入）：Transferring = 写权已交新进程（§9.2 ①）
    // ——迟到业务写（含结算 exp）不进内存账（防脏）也不标脏（autoSave 冻
    // 结跳过等于静默丢单；调用面按 SettlementLedger 挂账口径处置）。
    if (state_ == AvatarState::Transferring) {
        return;
    }
    if (key.empty() || delta == 0) {
        return;
    }
    progresses_[std::string(key)] += delta;
    mark_dirty("progress_add");
}

std::int64_t Avatar::progress(std::string_view key) const noexcept {
    auto it = progresses_.find(std::string(key));
    return it == progresses_.end() ? 0 : it->second;
}

const std::unordered_map<std::string, std::int64_t>& Avatar::progresses() const noexcept {
    return progresses_;
}

std::uint64_t Avatar::guild_id() const noexcept {
    return guild_id_;
}

void Avatar::set_guild_id(std::uint64_t guild_id) {
    if (guild_id_ == guild_id) {
        return;
    }
    guild_id_ = guild_id;
    mark_dirty("guild_change");
}

} // namespace apollo::game::base
