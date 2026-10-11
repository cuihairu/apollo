// 背压水位实现（net M1 内核 · 增量 3）。

#include "apollo/net/link/watermark.hpp"

namespace apollo::net::link {

Watermark::Watermark(WatermarkConfig config) : config_(config) {}

WatermarkLevel Watermark::level(std::uint64_t occupancy_bytes) const {
    // 千分比整数判定：occupancy * 1000 vs 阈值 * capacity（64 位乘不溢，
    // capacity ≤ 2^40 时 occupancy*1000 < 2^64）。
    const std::uint64_t cap = config_.capacity_bytes;
    const std::uint64_t milli = cap == 0 ? 1000 : (occupancy_bytes * 1000) / cap;
    if (milli >= config_.cut_permille) {
        return WatermarkLevel::kCut;
    }
    if (milli >= config_.hard_permille) {
        return WatermarkLevel::kHard;
    }
    if (milli >= config_.soft_permille) {
        return WatermarkLevel::kSoft;
    }
    return WatermarkLevel::kOk;
}

SendCode Watermark::admit(WatermarkLevel level, SendClass cls, std::uint64_t size) const {
    switch (cls) {
        case SendClass::kControl:
            // 控制帧不走主闸（独立预算见 admit_control）——防御式兜底放行。
            return SendCode::kAccepted;
        case SendClass::kReliable:
            // cut 档冻结新入队（§4.2）；soft/hard 只拒 Droppable，不拒可靠。
            return level >= WatermarkLevel::kCut ? SendCode::kBackPressured
                                                 : SendCode::kAccepted;
        case SendClass::kDroppable:
            // soft 起拒新（链路侧不 trim——§5.7；「丢旧」精化归 M2+）。
            return level >= WatermarkLevel::kSoft ? SendCode::kBackPressured
                                                  : SendCode::kAccepted;
    }
    return SendCode::kBackPressured;
}

SendCode Watermark::admit_control(std::uint64_t occupancy_bytes,
                                  std::uint64_t control_occupancy_bytes,
                                  std::uint64_t size) const {
    // Control 独立小预算（§4.1「饿不死」的进程间版）：预算内放行；
    // 预算也满 = 链路已病入膏肓（心跳都发不出），交链路层判死。
    if (control_occupancy_bytes + size > config_.control_reserve_bytes) {
        return SendCode::kBackPressured;
    }
    (void)occupancy_bytes;
    return SendCode::kAccepted;
}

void CutSustainer::update(bool at_cut, std::uint64_t now_ms) {
    if (!at_cut) {
        since_ms_ = kNone;
        return;
    }
    if (since_ms_ == kNone) {
        since_ms_ = now_ms;
    }
}

bool CutSustainer::sustained_for(std::uint64_t now_ms, std::uint64_t limit_ms) const {
    return since_ms_ != kNone && now_ms - since_ms_ >= limit_ms;
}

}  // namespace apollo::net::link
