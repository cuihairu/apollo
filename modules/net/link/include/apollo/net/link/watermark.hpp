#pragma once

// 背压水位（net M1 内核 · 增量 3；net-abstraction §4.2 四级水位 + §5.7
// 「服务器间不 trim」分界）。
//
// 判据 = 发送队列字节占用 / 容量（§5.8：水位建在环上而非 socket 上——
// 两族传输同一判据）。四级行为表（§4.2）在进程间段的投影：
//   ok   < 50%        全量发送
//   soft 50–75%       Droppable 拒新（不 trim 已排队——链路侧无 trim，
//                     「丢旧」精化归 M2+，先例 G-2 镜像流声明位）
//   hard 75–95%       同 soft（链路侧无 LOW/NORM 分级 trim）
//   cut  ≥ 95%        Reliable 冻结新入队（Control 除外）；持续 N ms → 断开
//
// offer 返回码哲学（§5.2）：不排队、不阻塞、决策还给调用方。判定为纯
// 函数（占用 → 闸门），时钟只进 cut 持续计时（宿主泵注入 now_ms，可测）。

#include <cstdint>

namespace apollo::net::link {

enum class SendCode { kAccepted, kBackPressured };

// 消息分类（§5.7：丢弃资格归消息分类声明；链路侧可靠/易弃/控制三档）。
enum class SendClass {
    kControl,    // 链路自身帧（Hello/Ack/Ping/Pong）——独立小预算，永不背压拒
    kReliable,   // 不可丢（RemoteEntityCall 默认、attributes 族）
    kDroppable,  // 可丢声明（镜像流丢旧等；soft 起拒新）
};

enum class WatermarkLevel { kOk, kSoft, kHard, kCut };

struct WatermarkConfig {
    std::uint64_t capacity_bytes = 4 * 1024 * 1024;  // 队列容量（水位分母）
    // 阈值按千分比存（避免浮点）：soft 500 / hard 750 / cut 950。
    std::uint64_t soft_permille = 500;
    std::uint64_t hard_permille = 750;
    std::uint64_t cut_permille = 950;
    // Control 独立预算（§4.1：控制帧独立小队列保证饿不死）。
    std::uint64_t control_reserve_bytes = 16 * 1024;
};

class Watermark {
public:
    explicit Watermark(WatermarkConfig config);

    WatermarkLevel level(std::uint64_t occupancy_bytes) const;

    // 入队闸门。occupancy = 判定时的既有占用（不含本次）。
    // size = 待入队字节。
    SendCode admit(WatermarkLevel level, SendClass cls, std::uint64_t size) const;

    // Control 走独立预算：occupancy 为既有占用，control_occupancy 为
    // control 队列既有占用。
    SendCode admit_control(std::uint64_t occupancy_bytes, std::uint64_t control_occupancy_bytes,
                           std::uint64_t size) const;

    std::uint64_t capacity() const { return config_.capacity_bytes; }

private:
    WatermarkConfig config_;
};

// cut 持续计时（§4.2：持续 N 秒 → 断开）。宿主泵注入 now_ms（单调毫秒）。
class CutSustainer {
public:
    // 占用处于 cut 时调用；返回持续毫秒数（非 cut 传 false 即复位）。
    void update(bool at_cut, std::uint64_t now_ms);
    void reset() { since_ms_ = kNone; }
    bool sustained_for(std::uint64_t now_ms, std::uint64_t limit_ms) const;

private:
    static constexpr std::uint64_t kNone = ~std::uint64_t{0};
    std::uint64_t since_ms_ = kNone;
};

}  // namespace apollo::net::link
