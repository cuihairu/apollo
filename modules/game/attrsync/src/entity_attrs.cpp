#include "apollo/game/attrsync/entity_attrs.hpp"

namespace apollo::game::attrsync {

EntityAttrs::EntityAttrs(std::uint64_t entity_id, std::size_t history_capacity)
    : entity_id_(entity_id), history_(history_capacity) {}

void EntityAttrs::set(std::uint32_t attr_id, std::int64_t value) {
    values_[attr_id] = value;
    dirty_[attr_id] = true;
    history_.append(attr_id, value);
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
