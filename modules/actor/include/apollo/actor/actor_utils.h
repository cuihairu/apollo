/**
 * @file actor_utils.h
 * @brief Actor 框架工具函数
 */

#pragma once

#include <cstdint>
#include <string>

namespace apollo {
namespace actor {

//==============================================================================
// 前向声明
//==============================================================================

enum class ActorState : uint8_t;

//==============================================================================
// 时间工具函数
//==============================================================================

/**
 * @brief 获取当前时间戳（毫秒）
 * @return 时间戳（毫秒）
 */
int64_t currentTimeMs();

//==============================================================================
// Actor 名称工具
//==============================================================================

/**
 * @brief 生成唯一 Actor 名称
 * @param prefix 名称前缀
 * @return 唯一名称
 */
std::string generateActorName(const std::string& prefix);

//==============================================================================
// 状态字符串转换
//==============================================================================

/**
 * @brief Actor 状态转字符串
 * @param state Actor 状态
 * @return 状态字符串
 */
const char* stateToString(enum ActorState state);

} // namespace actor

//==============================================================================
// 工具子命名空间
//==============================================================================

namespace actor {
namespace utils {

/**
 * @brief 获取当前时间戳（毫秒）
 * @return 时间戳（毫秒）
 */
int64_t currentTimeMs();

/**
 * @brief 生成唯一 Actor 名称
 * @param prefix 名称前缀
 * @return 唯一名称
 */
std::string generateActorName(const std::string& prefix);

} // namespace utils
} // namespace actor

} // namespace apollo
