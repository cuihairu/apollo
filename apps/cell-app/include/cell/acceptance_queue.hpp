#pragma once

#include "apollo/protocol/messages.hpp"

#include <atomic>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace cell {

namespace protocol = apollo::protocol;

// ADR-015(a)/ADR-017：意图信封——RPC worker 解码后的「想做什么」最小载体。
// 承载原始帧（MessageHeader + body）：执行侧（gameThread_）复用既有 handler
// 解析路径，头部二次解析 O(1)；sessionId 在头内，延时应答关联键 M1 升级批启用。
struct IntentEnvelope {
    protocol::MessageType type{};
    std::vector<uint8_t> frame;
    protocol::SessionID session_id{};
};

// ADR-016：受理队列——有界两级水位（告警线 + 顶格拒收，不丢旧不静默）。
// 数值口径 = tick 预算锚定（ADR-016 #3）：静态保守起步值，benchmark 接入
// 受理面后按 capacity §2.1 三模型实测锚点同型校准。
class AcceptanceQueue {
public:
    // 静态起步：cap 1024 × 单意图处理 ≤~50µs 量级 ≈ 50ms < 100ms@10Hz 预算
    static constexpr std::size_t kDefaultCap = 1024;
    static constexpr std::size_t kDefaultHighWatermark = 768;

    struct PushResult {
        bool accepted{false};
        bool high_watermark{false};  // 本次入队后越告警线（仅 accepted 时有意义）
    };

    explicit AcceptanceQueue(
        std::size_t cap = kDefaultCap,
        std::size_t high_watermark = kDefaultHighWatermark)
        : cap_(cap)
        , high_watermark_(high_watermark > cap ? cap : high_watermark) {}

    // worker 线程：入队（顶格拒收=false）。不丢旧不静默——拒绝由调用方显式回执。
    PushResult push(IntentEnvelope intent) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() >= cap_) {
            ++rejected_;
            return {false, false};
        }
        queue_.push_back(std::move(intent));
        const bool hw = queue_.size() >= high_watermark_ && !high_watermark_reported_;
        if (hw) {
            high_watermark_reported_ = true;  // 越线告警一次，回落后再报（滞回）
        }
        return {true, hw};
    }

    // gameThread_：tick 边界弹一条（无则 nullopt）
    std::optional<IntentEnvelope> try_pop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        auto intent = std::move(queue_.front());
        queue_.pop_front();
        if (queue_.size() < high_watermark_) {
            high_watermark_reported_ = false;  // 水位回落滞回，允许下次越线再告警
        }
        return intent;
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    std::size_t rejected_count() const {
        return rejected_.load(std::memory_order_relaxed);
    }

    std::size_t cap() const { return cap_; }
    std::size_t high_watermark() const { return high_watermark_; }

private:
    mutable std::mutex mutex_;
    std::deque<IntentEnvelope> queue_;
    std::size_t cap_;
    std::size_t high_watermark_;
    bool high_watermark_reported_{false};  // guarded by mutex_
    std::atomic<std::size_t> rejected_{0};
};

} // namespace cell
