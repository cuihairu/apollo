/**
 * @file timer_manager.cpp
 * @brief 分层时间轮定时器管理器实现
 */

#include "apollo/core/timer/timer_manager.h"
#include <chrono>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace apollo {
namespace core {
namespace timer {

//==============================================================================
// 辅助函数
//==============================================================================

namespace {

/**
 * @brief 获取单调时钟时间（毫秒）
 */
uint64_t getSteadyTimeMs() {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

/**
 * @brief 原子定时器ID生成器
 */
std::atomic<uint64_t> g_nextTimerId{1};

} // anonymous namespace

//==============================================================================
// TimerManager 实现
//==============================================================================

TimerManager::TimerManager(uint32_t resolutionMs)
    : resolutionMs_(resolutionMs)
    , currentTime_(0)
    , tickCount_(0)
{
    wheels_.resize(WHEEL_COUNT);
    for (auto& wheel : wheels_) {
        wheel.fill(Slot{});
    }
}

TimerManager::~TimerManager() {
    std::lock_guard<std::mutex> lock(mutex_);
    timers_.clear();
}

TimerId TimerManager::setTimer(uint64_t intervalMs, TimerCallback callback,
                               uint32_t repeat, void* userData) {
    if (!callback || intervalMs == 0 || intervalMs > MAX_INTERVAL) {
        return INVALID_TIMER_ID;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    TimerId id = generateTimerId();
    TimerDesc desc(id, intervalMs, std::move(callback), repeat);
    desc.userData = userData;
    desc.repeat = (repeat == 0) ? 1 : repeat;  // 0表示只执行一次

    timers_[id] = std::move(desc);
    addToWheel(timers_[id]);

    return id;
}

TimerId TimerManager::setTimer(uint64_t intervalMs, ITimerCallback* callback,
                               uint32_t repeat, void* userData) {
    if (!callback || intervalMs == 0 || intervalMs > MAX_INTERVAL) {
        return INVALID_TIMER_ID;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    TimerId id = generateTimerId();
    TimerDesc desc;
    desc.id = id;
    desc.intervalMs = intervalMs;
    desc.interfaceCb = callback;
    desc.userData = userData;
    desc.repeat = (repeat == 0) ? 1 : repeat;
    desc.flags = TimerFlag::Infinite;

    timers_[id] = std::move(desc);
    addToWheel(timers_[id]);

    return id;
}

bool TimerManager::setTimer(TimerId timerId, uint64_t intervalMs, TimerCallback callback,
                            uint32_t repeat) {
    if (timerId == INVALID_TIMER_ID || !callback || intervalMs == 0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // 检查是否已存在
    auto it = timers_.find(timerId);
    if (it != timers_.end()) {
        removeFromWheel(it->second);
    }

    TimerDesc desc(timerId, intervalMs, std::move(callback),
                   (repeat == 0) ? 1 : repeat);
    timers_[timerId] = std::move(desc);
    addToWheel(timers_[timerId]);

    return true;
}

bool TimerManager::killTimer(TimerId timerId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = timers_.find(timerId);
    if (it == timers_.end()) {
        return false;
    }

    removeFromWheel(it->second);
    timers_.erase(it);
    return true;
}

bool TimerManager::hasTimer(TimerId timerId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return timers_.find(timerId) != timers_.end();
}

size_t TimerManager::getTimerCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return timers_.size();
}

bool TimerManager::update(int maxProcess) {
    uint64_t now = getSteadyTimeMs();

    // 计算需要处理的tick数
    // 注意：currentTime_ 是相对时间，单位是tick
    uint64_t nowTicks = now / resolutionMs_;

    if (nowTicks <= currentTime_) {
        return false;  // 没有新的tick需要处理
    }

    int processed = 0;

    while (currentTime_ < nowTicks) {
        if (maxProcess > 0 && processed >= maxProcess) {
            return true;  // 达到处理上限
        }

        processTick();
        processed++;
    }

    return false;
}

uint64_t TimerManager::getCurrentTimeMs() {
    return getSteadyTimeMs();
}

TimerId TimerManager::generateTimerId() {
    return g_nextTimerId.fetch_add(1, std::memory_order_relaxed);
}

std::string TimerManager::dumpInfo() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ostringstream oss;
    oss << "=== Timer Manager Info ===\n";
    oss << "Current tick: " << tickCount_ << "\n";
    oss << "Timer count: " << timers_.size() << "\n";
    oss << "Resolution: " << resolutionMs_ << "ms\n";

    oss << "\nActive timers:\n";
    for (const auto& pair : timers_) {
        const auto& timer = pair.second;
        oss << "  ID=" << timer.id
            << " interval=" << timer.intervalMs << "ms"
            << " repeat=" << timer.repeat
            << " callback=" << (timer.callback ? "func" : (timer.interfaceCb ? "interface" : "none"))
            << "\n";
    }

    return oss.str();
}

//==============================================================================
// 私有方法实现
//==============================================================================

TimerManager::WheelPosition TimerManager::calculatePosition(uint64_t intervalMs) const {
    WheelPosition pos;
    pos.ticks = static_cast<uint32_t>((intervalMs + resolutionMs_ - 1) / resolutionMs_);

    uint32_t ticks = pos.ticks - 1;  // 转换为0-based索引

    pos.level = 0;
    pos.slot = 0;

    // 确定时间轮层级
    for (uint32_t i = 0; i < WHEEL_COUNT; ++i) {
        if ((ticks >> (i * WHEEL_BITS)) == 0) {
            pos.level = i;
            break;
        }
        pos.level = i;
    }

    // 计算槽位
    if (pos.level > 0) {
        uint32_t shift = (pos.level - 1) * WHEEL_BITS;
        pos.slot = (ticks >> shift) & WHEEL_MASK;
    } else {
        pos.slot = ticks & WHEEL_MASK;
    }

    return pos;
}

void TimerManager::addToWheel(const TimerDesc& timer) {
    auto pos = calculatePosition(timer.intervalMs);
    wheels_[pos.level][pos.slot].push_back(timer.id);
}

void TimerManager::removeFromWheel(const TimerDesc& timer) {
    // 由于是分层时间轮，删除时需要遍历所有层查找
    // 这在实际应用中开销不大，因为删除操作不频繁
    for (auto& wheel : wheels_) {
        for (auto& slot : wheel) {
            auto it = std::find(slot.begin(), slot.end(), timer.id);
            if (it != slot.end()) {
                slot.erase(it);
                return;
            }
        }
    }
}

void TimerManager::processTick() {
    tickCount_++;
    uint32_t slotIndex = tickCount_ & WHEEL_MASK;

    // 处理第一层时间轮的当前槽
    processSlot(0, slotIndex);

    // 检查是否需要降级
    // 每当低8位全为0时，检查上一层
    if (slotIndex == 0) {
        for (uint32_t level = 1; level < WHEEL_COUNT; ++level) {
            uint32_t checkIndex = (tickCount_ >> (level * WHEEL_BITS)) & WHEEL_MASK;
            if (checkIndex != 0) {
                break;
            }
            cascadeTimer(level);
        }
    }

    currentTime_ = tickCount_;
}

size_t TimerManager::processSlot(uint32_t level, uint32_t slot) {
    auto& timerIds = wheels_[level][slot];
    if (timerIds.empty()) {
        return 0;
    }

    // 复制定时器ID列表，避免在处理过程中修改容器
    std::vector<TimerId> toProcess;
    toProcess.swap(timerIds);

    size_t count = 0;
    for (TimerId timerId : toProcess) {
        auto it = timers_.find(timerId);
        if (it == timers_.end()) {
            continue;  // 定时器已被删除
        }

        TimerDesc& timer = it->second;

        // 执行回调
        executeTimer(timer);
        count++;

        // 处理重复逻辑
        if (timer.repeat != 0xFFFFFFFF && timer.repeat > 0) {
            timer.repeat--;
        }

        if (timer.repeat == 0) {
            // 单次定时器或重复次数用完
            timers_.erase(it);
        } else {
            // 重新添加到时间轮
            addToWheel(timer);
        }
    }

    return count;
}

void TimerManager::cascadeTimer(uint32_t level) {
    if (level >= WHEEL_COUNT) {
        return;
    }

    uint32_t slotIndex = (tickCount_ >> (level * WHEEL_BITS)) & WHEEL_MASK;
    auto& timerIds = wheels_[level][slotIndex];

    if (timerIds.empty()) {
        return;
    }

    // 将定时器重新分配到低层时间轮
    std::vector<TimerId> toCascade;
    toCascade.swap(timerIds);

    for (TimerId timerId : toCascade) {
        auto it = timers_.find(timerId);
        if (it == timers_.end()) {
            continue;
        }

        // 计算在低层时间轮中的新位置
        // 这里简化处理：直接重新添加
        // 实际应该计算剩余时间
        addToWheel(it->second);
    }
}

void TimerManager::executeTimer(TimerDesc& timer) {
    // 释放锁后执行回调，避免死锁
    mutex_.unlock();

    try {
        if (timer.callback) {
            timer.callback(timer.id, timer.userData);
        } else if (timer.interfaceCb) {
            timer.interfaceCb->onTimer(timer.id);
        }
    } catch (...) {
        // 捕获回调中的异常，防止影响定时器系统
    }

    mutex_.lock();
}

} // namespace timer
} // namespace core
} // namespace apollo
