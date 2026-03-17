#pragma once

#include "apollo/bw/runtime.h"

namespace apollo::bigworld {

using TimerId = apollo::bw::TimerId;
using EntityId = apollo::bw::EntityId;

// Timer functions - delegates to apollo::bw
inline TimerId add_timer(double initial_offset_seconds,
                        double repeat_offset_seconds,
                        std::function<void(TimerId, int32_t)> callback,
                        int32_t user_arg = 0) {
    return apollo::bw::BigWorld::addTimer(initial_offset_seconds,
                                          repeat_offset_seconds,
                                          std::move(callback),
                                          user_arg);
}

inline bool del_timer(TimerId timer_id) {
    return apollo::bw::BigWorld::delTimer(timer_id);
}

} // namespace apollo::bigworld
