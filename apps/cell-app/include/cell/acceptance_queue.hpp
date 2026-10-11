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

// ADR-015(a)/ADR-017：意图信封——受理面解码后的「想做什么」最小载体。
// 承载原始帧（MessageHeader + body）：执行侧（gameThread_）复用既有 handler
// 解析路径，头部二次解析 O(1)。
// 来源双路：legacy RPC worker（request_id = 0，ack-on-accept 现状不变）与
// M1 链路（M1 上层接线批 2b-ii，request_id = 契约上行关联键，延时应答
// 929 cell_intent_ack 按它回带；0 不作关联键）。
struct IntentEnvelope {
    protocol::MessageType type{};
    std::vector<uint8_t> frame;
    protocol::SessionID session_id{};
    std::uint64_t request_id = 0;  // M1 延时应答关联键；0 = legacy / 无应答
};

// ADR-016：受理队列——有界两级水位（告警线 + 顶格拒收，不丢旧不静默）。
// 数值口径 = tick 预算锚定（ADR-016 #3）+ 实测校准（2026-10-11，P3-2 批 D，
// examples/benchmark --acceptance 三跑 min，负载噪声期下界口径）：
// 受理+执行队列侧合计 ~0.1µs/意图（min），cap 顶格 1024 条全程 drain
// ≈0.10ms < 消息段预算 20ms（余量 ~194×）——drain 侧远非约束，容量侧才是
// 受限维度（全容量 5000 CCU 稳态 ≈2500 意图/拍 = 244% cap，饱和即拒收
// 路径的设计消费面）。数值保持起步值。
class AcceptanceQueue {
public:
    // cap 1024：单场景档（100 人 ≈50 意图/拍）~20× 余量、大场景档（1000 人
    // ≈500 意图/拍）~2× 余量；告警线 768 = 75%，水位滞回留回落空间
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
