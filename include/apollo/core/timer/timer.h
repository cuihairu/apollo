#pragma once

#include <cstdint>
#include <functional>
#include <memory>

namespace apollo {
namespace core {
namespace timer {

/**
 * @brief 定时器ID类型
 */
using TimerId = uint64_t;

/**
 * @brief 无效定时器ID
 */
constexpr TimerId INVALID_TIMER_ID = 0;

/**
 * @brief 定时器回调函数类型
 *
 * @param timerId 触发的定时器ID
 * @param userData 用户数据指针
 */
using TimerCallback = std::function<void(TimerId timerId, void* userData)>;

/**
 * @brief 定时器回调接口
 *
 * 提供类似于 SSEngine ISSTimer 的接口风格
 */
class ITimerCallback {
public:
    virtual ~ITimerCallback() = default;

    /**
     * @brief 定时器超时时调用
     *
     * @param timerId 定时器ID
     */
    virtual void onTimer(TimerId timerId) = 0;
};

/**
 * @brief 定时器标志位
 */
enum class TimerFlag : uint32_t {
    None        = 0,    ///< 无特殊标志
    Once        = 1,    ///< 单次定时器，触发后自动删除
    Infinite    = 2,    ///< 永久循环定时器（默认）
};

/**
 * @brief 定时器标志位运算
 */
inline TimerFlag operator|(TimerFlag a, TimerFlag b) {
    return static_cast<TimerFlag>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool hasFlag(TimerFlag flags, TimerFlag flag) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
}

} // namespace timer
} // namespace core
} // namespace apollo
