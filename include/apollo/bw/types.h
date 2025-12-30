#pragma once

#include <cstdint>

namespace apollo::bw {

using EntityId = uint64_t;
using TimerId = uint64_t;

constexpr EntityId INVALID_ENTITY_ID = 0;
constexpr TimerId INVALID_TIMER_ID = 0;

} // namespace apollo::bw

