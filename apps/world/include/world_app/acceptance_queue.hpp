#pragma once

#include "apollo_contract.h"
#include "apollo/net/linktrace/trace_context.hpp"
#include "apollo/protocol/messages.hpp"

#include <atomic>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace world_app {

namespace protocol = apollo::protocol;
namespace trace = apollo::net::trace;

// ADR-015(a)/ADR-017：意图信封——受理面解码后的「想做什么」最小载体。
// 承载原始帧（MessageHeader + body）：执行侧（gameThread_）复用既有 handler
// 解析路径，头部二次解析 O(1)。
// 来源双路：legacy RPC worker（request_id = 0，ack-on-accept 现状不变）与
// M1 链路（M1 上层接线批 2b-ii，request_id = 契约上行关联键，延时应答
// 929 cell_intent_ack 按它回带；0 不作关联键）。
// trace 子集载体（P3-3 语义层批）：随 932 internal_envelope 上行（bare
// 924-928 双受理路径无 trace——默认零值未采样，span 发射面零开销跳过）。
struct IntentEnvelope {
    protocol::MessageType type{};
    std::vector<uint8_t> frame;
    protocol::SessionID session_id{};
    std::uint64_t request_id = 0;  // M1 延时应答关联键；0 = legacy / 无应答
    trace::TraceContext trace{};   // trace_id + 发端 parent + 采样旗标
    std::uint64_t received_ms = 0; // 受理时刻（WorldApp span 计时起点）
};

// 受理/排空契约码（m1_link_server.hpp 自此处取；L98 执行/管理分离后执行
// 内核 CellApp 只依赖本头，不带传输头）。
// M1 错误码（errors.xml 契约错误表；0 成功）。
inline constexpr std::int32_t kIntentAckOk = 0;
inline constexpr std::int32_t kIntentAckInternal = 2001;        // internal
inline constexpr std::int32_t kIntentAckBackpressure = 2101;    // cell_backpressure

// World 排空状态机值（956/957 契约面；gateway 954 同值口径）。
inline constexpr std::int32_t kWorldDrainServing = 0;
inline constexpr std::int32_t kWorldDrainDraining = 1;
inline constexpr std::int32_t kWorldDrainDrained = 2;

// cell 注册受理码（960/961 契约面，L101 注册首切片；961 code 字段）。
inline constexpr std::int32_t kCellRegisterAccepted = 1;
inline constexpr std::int32_t kCellRegisterAcceptedIdempotent = 2;  // 同 ID 同代次刷新
inline constexpr std::int32_t kCellRegisterRejectedStaleEpoch = 3;  // 旧代次不可覆盖
inline constexpr std::int32_t kCellRegisterRejectedInvalid = 4;     // cell_id/代次非法
inline constexpr std::int32_t kCellRegisterRejectedIncompatible = 5;  // 契约兼容投影不符

// 契约兼容投影（960 版本/兼容裁决切片，L109 未收口）：kInternalHash 十六进制
// 串经 FNV-1a 64 折为 int64——cell 侧声明、World 侧裁决须同值才受理（§4.3
// 「World 验证所属关系、版本和身份」的版本面）；0 = 未声明按不可验证拒。
// 混编/升级的兼容范围白名单与版本协商随协议更新批（§9 支持协议更新的进程
// 替换）。双域口径（sdk-contract §11.3）：internal-only 变更只动 kInternalHash，
// 客户端握手 kClientHash 不受扰——本投影取 internal 域恰为 cell↔World 控制
// 协议指纹。
inline constexpr std::int64_t contract_compat_projection() {
    const char* s = apollo::contract::gen::kInternalHash;
    std::uint64_t h = 14695981039346656037ull;  // FNV-1a 64 偏移基
    for (; *s; ++s) {
        h ^= static_cast<std::uint8_t>(*s);
        h *= 1099511628211ull;  // FNV-1a 64 素数
    }
    return static_cast<std::int64_t>(h);
}

// cell 缩容意图命令/回执（963/966 契约面，L104 缩容管理命令面）：op 取
// 963 操作字段的契约词汇；code 取 966 code 字段的契约词汇。
inline constexpr std::int32_t kCellRetireOpBegin = 1;
inline constexpr std::int32_t kCellRetireOpCancel = 2;
inline constexpr std::int32_t kCellRetireBegun = 1;
inline constexpr std::int32_t kCellRetireCancelled = 2;
inline constexpr std::int32_t kCellRetireRejectedUnknownCell = 3;   // 目标未登记
inline constexpr std::int32_t kCellRetireRejectedInvalid = 4;       // op 未知/cell_id 非法

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

} // namespace world_app
