#include "apollo/game/attrsync/entity_attrs.hpp"

namespace apollo::game::attrsync {

EntityAttrs::EntityAttrs(std::uint64_t entity_id, std::size_t history_capacity)
    : entity_id_(entity_id), history_(history_capacity) {}

void EntityAttrs::set(std::uint32_t attr_id, std::int64_t value) {
    // slice 容器 base 直写拒绝：长度正门 = set_slice_len（收缩联动擦除纪律，
    // 直写绕过即状态面分叉）。
    if (slices_.count(attr_id) != 0) {
        return;
    }
    set_raw_(attr_id, value);
}

void EntityAttrs::set_raw_(std::uint32_t attr_id, std::int64_t value) {
    values_[attr_id] = value;
    dirty_[attr_id] = true;
    history_.append(attr_id, value);
}

void EntityAttrs::set_slice_len(std::uint32_t base_attr, std::uint16_t len) {
    auto it = slices_.find(base_attr);
    if (it == slices_.end() || len > it->second) {
        return;  // 未声明 / 超上限拒绝
    }
    const std::uint16_t old_len = slice_len(base_attr);
    if (len < old_len) {
        // 收缩即终态：越界格值连脏标记一并擦除（§3.4 收缩越过 = 死格）
        for (std::uint32_t slot = static_cast<std::uint32_t>(len) + 1;
             slot <= old_len; ++slot) {
            const std::uint32_t id = slice_slot_id(base_attr,
                                                   static_cast<std::uint16_t>(slot));
            values_.erase(id);
            dirty_.erase(id);
        }
    }
    set_raw_(base_attr, len);  // 长度格 = base 本体（组合 id slot 0）
}

std::uint16_t EntityAttrs::slice_len(std::uint32_t base_attr) const {
    if (slices_.count(base_attr) == 0) {
        return 0;
    }
    return static_cast<std::uint16_t>(get(base_attr));
}

void EntityAttrs::set_slot(std::uint32_t base_attr, std::uint16_t slot,
                           std::int64_t value) {
    if (slices_.count(base_attr) == 0 || slot == kSliceLenSlot ||
        slot > slice_len(base_attr)) {
        return;  // 未声明 / 长度格正门 / 越界拒绝
    }
    set_raw_(slice_slot_id(base_attr, slot), value);
}

std::optional<EntityAttrs::SliceCoord> EntityAttrs::slice_coord(
    std::uint32_t attr_id) const {
    for (const auto& [base, max_slots] : slices_) {
        if ((attr_id & 0xFFFFu) != base) {
            continue;  // 低 16 位 = base
        }
        const std::uint32_t slot = attr_id >> 16;
        if (slot <= max_slots) {
            return SliceCoord{base, static_cast<std::uint16_t>(slot)};
        }
    }
    return std::nullopt;
}

std::int64_t EntityAttrs::get(std::uint32_t attr_id) const {
    auto it = values_.find(attr_id);
    return (it != values_.end()) ? it->second : 0;
}

void EntityAttrs::set_all(const std::vector<std::pair<std::uint32_t, std::int64_t>>& attrs) {
    for (const auto& [attr_id, value] : attrs) {
        set(attr_id, value);
    }
}

bool EntityAttrs::is_dirty(std::uint32_t attr_id) const {
    auto it = dirty_.find(attr_id);
    return (it != dirty_.end()) ? it->second : false;
}

void EntityAttrs::clear_dirty() {
    dirty_.clear();
}

std::size_t EntityAttrs::dirty_count() const {
    return dirty_.size();
}

}  // namespace apollo::game::attrsync
