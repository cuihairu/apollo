#pragma once

#include "apollo/core/timer/timer.h"
#include <chrono>
#include <memory>
#include <vector>
#include <array>
#include <mutex>
#include <unordered_map>

namespace apollo {
namespace core {
namespace timer {

/**
 * @brief 定时器描述
 */
struct TimerDesc {
    TimerId       id         = INVALID_TIMER_ID;  ///< 定时器ID
    uint64_t      intervalMs = 0;                 ///< 间隔（毫秒）
    uint32_t      repeat     = 0xFFFFFFFF;        ///< 重复次数（0xFFFFFFFF = 无限）
    TimerFlag     flags      = TimerFlag::Infinite; ///< 标志位
    void*         userData   = nullptr;           ///< 用户数据
    TimerCallback callback;                       ///< 回调函数
    ITimerCallback* interfaceCb = nullptr;        ///< 接口回调

    TimerDesc() = default;

    TimerDesc(TimerId id, uint64_t interval, TimerCallback cb,
              uint32_t r = 0xFFFFFFFF, TimerFlag f = TimerFlag::Infinite)
        : id(id), intervalMs(interval), repeat(r), flags(f), callback(std::move(cb)) {}
};

/**
 * @brief 时间轮定时器管理器
 *
 * 使用分层时间轮算法实现高效定时器管理
 * - 时间复杂度: O(1) 添加/删除
 * - 空间复杂度: O(n), n为定时器数量
 *
 * 时间轮结构:
 * - 第一层: 256槽, 每槽1ms (覆盖 0-255ms)
 * - 第二层: 256槽, 每槽256ms (覆盖 0-65535ms = ~65秒)
 * - 第三层: 256槽, 每槽65536ms (覆盖 0-16777215ms = ~4.6小时)
 * - 第四层: 256槽, 每槽16777216ms (覆盖 0-4294967295ms = ~49.7天)
 */
class TimerManager {
public:
    /**
     * @brief 构造函数
     *
     * @param resolutionMs 定时器精度（毫秒），默认1ms
     */
    explicit TimerManager(uint32_t resolutionMs = 1);

    /**
     * @brief 析构函数
     */
    ~TimerManager();

    // 禁止拷贝和移动
    TimerManager(const TimerManager&) = delete;
    TimerManager& operator=(const TimerManager&) = delete;
    TimerManager(TimerManager&&) = delete;
    TimerManager& operator=(TimerManager&&) = delete;

    /**
     * @brief 设置定时器（使用函数回调）
     *
     * @param intervalMs 间隔（毫秒）
     * @param callback 回调函数
     * @param repeat 重复次数（0 = 单次，0xFFFFFFFF = 无限）
     * @param userData 用户数据
     * @return 定时器ID，失败返回 INVALID_TIMER_ID
     */
    TimerId setTimer(uint64_t intervalMs, TimerCallback callback,
                     uint32_t repeat = 0xFFFFFFFF, void* userData = nullptr);

    /**
     * @brief 设置定时器（使用接口回调）
     *
     * @param intervalMs 间隔（毫秒）
     * @param callback 接口回调
     * @param repeat 重复次数（0 = 单次，0xFFFFFFFF = 无限）
     * @param userData 用户数据
     * @return 定时器ID，失败返回 INVALID_TIMER_ID
     */
    TimerId setTimer(uint64_t intervalMs, ITimerCallback* callback,
                     uint32_t repeat = 0xFFFFFFFF, void* userData = nullptr);

    /**
     * @brief 设置指定ID的定时器
     *
     * @param timerId 定时器ID
     * @param intervalMs 间隔（毫秒）
     * @param callback 回调函数
     * @param repeat 重复次数（0 = 单次，0xFFFFFFFF = 无限）
     * @return 成功返回true
     */
    bool setTimer(TimerId timerId, uint64_t intervalMs, TimerCallback callback,
                  uint32_t repeat = 0xFFFFFFFF);

    /**
     * @brief 取消定时器
     *
     * @param timerId 定时器ID
     * @return 成功返回true
     */
    bool killTimer(TimerId timerId);

    /**
     * @brief 检测定时器是否存在
     *
     * @param timerId 定时器ID
     * @return 存在返回true
     */
    bool hasTimer(TimerId timerId) const;

    /**
     * @brief 获取定时器数量
     */
    size_t getTimerCount() const;

    /**
     * @brief 更新定时器（需在主循环中定期调用）
     *
     * @param maxProcess 本次处理的最大事件数（-1表示处理所有）
     * @return 还有待处理事件时返回true
     */
    bool update(int maxProcess = -1);

    /**
     * @brief 获取当前时间戳（毫秒）
     */
    static uint64_t getCurrentTimeMs();

    /**
     * @brief 生成新的定时器ID
     */
    static TimerId generateTimerId();

    /**
     * @brief 转储定时器信息（调试用）
     */
    std::string dumpInfo() const;

private:
    // 时间轮参数
    static constexpr uint32_t WHEEL_SIZE = 256;         // 每层槽数
    static constexpr uint32_t WHEEL_BITS = 8;           // log2(WHEEL_SIZE)
    static constexpr uint32_t WHEEL_MASK = WHEEL_SIZE - 1;
    static constexpr uint32_t WHEEL_COUNT = 4;          // 时间轮层数
    static constexpr uint64_t MAX_INTERVAL = (1ULL << (WHEEL_BITS * WHEEL_COUNT)) - 1; // ~49天

    // 时间轮槽数组
    using Slot = std::vector<TimerId>;
    using Wheel = std::array<Slot, WHEEL_SIZE>;

    // 定时器映射
    using TimerMap = std::unordered_map<TimerId, TimerDesc>;

    uint32_t resolutionMs_;          ///< 定时器精度
    uint64_t currentTime_;           ///< 当前时间（相对于启动时间）
    uint32_t tickCount_;             ///< tick计数（用于时间轮旋转）
    TimerMap timers_;                ///< 活跃定时器
    std::vector<Wheel> wheels_;      ///< 时间轮数组
    mutable std::mutex mutex_;       ///< 保护定时器操作

    /**
     * @brief 计算定时器在时间轮中的位置
     */
    struct WheelPosition {
        uint32_t level;      ///< 时间轮层级
        uint32_t slot;       ///< 槽位
        uint32_t ticks;      ///< 需要的tick数
    };

    /**
     * @brief 计算时间轮位置
     */
    WheelPosition calculatePosition(uint64_t intervalMs) const;

    /**
     * @brief 将定时器添加到时间轮
     */
    void addToWheel(const TimerDesc& timer);

    /**
     * @brief 从时间轮中移除定时器
     */
    void removeFromWheel(const TimerDesc& timer);

    /**
     * @brief 处理时间轮tick
     */
    void processTick();

    /**
     * @brief 处理单个槽位中的定时器
     */
    size_t processSlot(uint32_t level, uint32_t slot);

    /**
     * @brief 降级定时器（从高层时间轮移动到低层）
     */
    void cascadeTimer(uint32_t level);

    /**
     * @brief 执行定时器回调
     */
    void executeTimer(TimerDesc& timer);
};

/**
 * @brief 定时器管理器智能指针类型
 */
using TimerManagerPtr = std::shared_ptr<TimerManager>;

} // namespace timer
} // namespace core
} // namespace apollo
