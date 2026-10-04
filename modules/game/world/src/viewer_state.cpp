#include "apollo/game/world/viewer_state.hpp"

namespace apollo::game::world {

void ViewerState::reset(apollo::game::core::EntityId observer) {
    views_[observer.value()].clear();
}

void ViewerState::drop(apollo::game::core::EntityId observer) {
    views_.erase(observer.value());
}

bool ViewerState::tracked(apollo::game::core::EntityId observer) const noexcept {
    return views_.count(observer.value()) != 0;
}

bool ViewerState::visible(apollo::game::core::EntityId observer,
                          apollo::game::core::EntityId subject) const noexcept {
    const auto it = views_.find(observer.value());
    if (it == views_.end()) {
        return false;
    }
    return it->second.count(subject.value()) != 0;
}

std::vector<apollo::game::core::EntityId> ViewerState::visible_set(
    apollo::game::core::EntityId observer) const {
    std::vector<apollo::game::core::EntityId> result;
    const auto it = views_.find(observer.value());
    if (it == views_.end()) {
        return result;
    }
    result.reserve(it->second.size());
    for (const auto value : it->second) {
        result.push_back(apollo::game::core::EntityId(value));
    }
    return result;
}

bool ViewerState::add(apollo::game::core::EntityId observer,
                      apollo::game::core::EntityId subject) {
    if (!observer.is_valid() || !subject.is_valid()) {
        return false;
    }
    return views_[observer.value()].insert(subject.value()).second;
}

bool ViewerState::remove(apollo::game::core::EntityId observer,
                         apollo::game::core::EntityId subject) {
    const auto it = views_.find(observer.value());
    if (it == views_.end()) {
        return false;
    }
    // 视野可为空，登记在册不因空而注销（tracked 语义独立于视野内容）
    return it->second.erase(subject.value()) != 0;
}

std::vector<apollo::game::core::EntityId> ViewerState::remove_everywhere(
    apollo::game::core::EntityId subject) {
    std::vector<apollo::game::core::EntityId> affected;
    for (auto& [observer_value, view] : views_) {
        if (view.erase(subject.value()) != 0) {
            affected.push_back(apollo::game::core::EntityId(observer_value));
        }
    }
    return affected;
}

} // namespace apollo::game::world
