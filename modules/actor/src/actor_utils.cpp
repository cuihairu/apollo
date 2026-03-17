/**
 * @file actor_utils.cpp
 * @brief Actor 框架工具函数实现
 */

#include "apollo/actor/actor_utils.h"
#include "apollo/actor/actor_cell.h"
#include <chrono>
#include <atomic>

namespace apollo {
namespace actor {

//==============================================================================
// 时间工具函数
//==============================================================================

int64_t currentTimeMs() {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

//==============================================================================
// Actor 名称工具
//==============================================================================

std::string generateActorName(const std::string& prefix) {
    static std::atomic<uint64_t> counter{0};
    uint64_t id = counter.fetch_add(1, std::memory_order_relaxed);
    return prefix + "-" + std::to_string(id);
}

//==============================================================================
// 状态字符串转换
//==============================================================================

const char* stateToString(ActorState state) {
    switch (state) {
        case ActorState::Starting:    return "Starting";
        case ActorState::Running:     return "Running";
        case ActorState::Suspending:  return "Suspending";
        case ActorState::Stopped:     return "Stopped";
        case ActorState::Terminated:  return "Terminated";
        default: return "Unknown";
    }
}

//==============================================================================
// 工具子命名空间
//==============================================================================

namespace utils {

int64_t currentTimeMs() {
    return actor::currentTimeMs();
}

std::string generateActorName(const std::string& prefix) {
    return actor::generateActorName(prefix);
}

} // namespace utils

} // namespace actor
} // namespace apollo
