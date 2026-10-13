#include "apollo/game/attrsync/viewer_attr_state.hpp"

namespace apollo::game::attrsync {

void ViewerAttrState::set_acked(std::uint64_t entity_id, std::uint64_t seq) {
    acked_[entity_id] = seq;
}

std::uint64_t ViewerAttrState::acked_seq(std::uint64_t entity_id) const {
    auto it = acked_.find(entity_id);
    return (it != acked_.end()) ? it->second : 0;
}

void ViewerAttrState::remove(std::uint64_t entity_id) {
    acked_.erase(entity_id);
}

bool ViewerAttrState::tracked(std::uint64_t entity_id) const {
    return acked_.count(entity_id) > 0;
}

}  // namespace apollo::game::attrsync
