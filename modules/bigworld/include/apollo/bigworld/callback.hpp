#pragma once

#include "apollo/bw/runtime.h"

namespace apollo::bigworld {

using TimerId = apollo::bw::TimerId;
using EntityId = apollo::bw::EntityId;

// Callback functions - delegates to apollo::bw
inline TimerId register_callback(double delay_seconds, std::function<void()> callback) {
    return apollo::bw::BigWorld::callback(delay_seconds, std::move(callback));
}

inline bool cancel_callback(TimerId callback_id) {
    return apollo::bw::BigWorld::cancelCallback(callback_id);
}

} // namespace apollo::bigworld
