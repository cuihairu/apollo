#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace apollo::base {

/// 分层时间轮（G-4 定时器轮落点，architecture-review §16.8-④：数据结构归
/// modules/base、驱动权在 game loop、回调在 owning thread 直接执行——不学
/// skynet 独立 timer 线程）。
///
/// 语义口径：
/// - 单写者：无锁无原子，全部状态归驱动线程所有；回调内可 schedule/cancel
///   （重入安全），但不得再调 advance（驱动点唯一，断言拦截）。
/// - 喂钟驱动：advance(now_ms) 由调用方在主循环固定阶段注入单调毫秒钟，
///   组件不取系统时间、不起线程、不睡眠；时间倒退为无操作（喂钟错误容错）。
/// - O(1) 调度/撤销：分层哈希轮（Varghese & Lauck；skynet 五层轮同型），
///   仅第 0 层出触发，高层在页界级联降层——到期拍精确触发，不早不丢；
///   超窗定时器驻留顶层桶随页界逐步降层，覆盖窗口不设上限。
/// - 补拍语义：大步喂钟按逐拍步进，停摆期间到期项在恢复时依序触发
///   （周期项逐周期补发，一次性项在途经拍触发）。
/// - 周期重排以绝对拍计（deadline += interval）——喂钟不迟到则零漂移；
///   停摆后从当前拍续排（max(deadline+interval, now+1)，不追帧补发到过去）。
/// - 回调异常向上传播（组件不吞）；异常中断后状态仍一致（已摘链已标记，
///   惰性回收槽留待下次调度/驱动清理）。
///
/// 参数形：tick_ms = 到期精度下界；slots = 每层槽数（2 的幂）；levels = 层数。
/// 时钟原点由构造注入 start_ms（喂钟与时钟同源——unix 毫秒大原点场景不从
/// 第 0 拍步进）。构造参数越界取防御钳制（tick_ms>=1、slots 补到 2 的幂、
/// levels>=2），不静默 UB。
class TimerWheel {
public:
    using TimerId = std::uint64_t;              ///< (generation << 32) | 槽位号；0 = 无效
    using Callback = std::function<void(TimerId)>;

    explicit TimerWheel(std::uint32_t tick_ms = 10, std::uint32_t slots = 64,
                        std::uint32_t levels = 5, std::uint64_t start_ms = 0);

    /// 一次性：delay_ms 后触发一次（不足一拍按一拍计，0 = 下一拍）。
    TimerId schedule_after(std::uint64_t delay_ms, Callback cb);

    /// 周期：首个 interval_ms 触发，此后每 interval_ms 一次，直至 cancel。
    TimerId schedule_interval(std::uint64_t interval_ms, Callback cb);

    /// 撤销在册项。已触发 / 不存在 / 重复撤销 / id 陈旧（槽位复用后）= false。
    bool cancel(TimerId id);

    /// 推进至 now_ms 并触发途中到期项（本线程逐个执行，同拍多项按触发序）。
    /// 返回本次触发次数。空轮大步跳拍直接吸附时间沿（无到期项可触发）。
    std::size_t advance(std::uint64_t now_ms);

    [[nodiscard]] std::size_t size() const noexcept { return live_count_; }
    [[nodiscard]] bool empty() const noexcept { return live_count_ == 0; }
    [[nodiscard]] std::uint32_t tick_ms() const noexcept { return tick_ms_; }
    /// 当前已驱动到的时间沿（毫秒，拍粒度对齐）
    [[nodiscard]] std::uint64_t current_ms() const noexcept {
        return now_tick_ * static_cast<std::uint64_t>(tick_ms_);
    }

private:
    static constexpr std::uint32_t kNil = 0xFFFFFFFFu;

    struct Node {
        Callback callback;
        std::uint64_t deadline_tick = 0;   ///< 绝对到期拍
        std::uint64_t interval_ticks = 0;  ///< 周期拍距（0 = 一次性）
        std::uint32_t generation = 1;      ///< 槽位复用代数（id 陈旧判据；恒 >= 1）
        std::uint32_t level = 0;
        std::uint32_t slot = 0;
        std::uint32_t prev = kNil;         ///< 桶内 / 触发队列 双向侵入链
        std::uint32_t next = kNil;
        bool live = false;
    };

    static TimerId make_id(std::uint32_t index, std::uint32_t generation) {
        return (static_cast<std::uint64_t>(generation) << 32) | index;
    }

    TimerId allocate(Callback cb, std::uint64_t deadline_ticks, std::uint64_t interval_ticks);
    void place(std::uint32_t index);
    void unlink(std::uint32_t index);
    void cascade(std::uint32_t level, std::uint32_t slot);
    std::size_t fire_slot(std::uint32_t slot);
    void drain_garbage();

    std::uint32_t tick_ms_;
    std::uint32_t slots_;
    std::uint32_t bits_;     ///< log2(slots_)
    std::uint32_t levels_;
    std::uint64_t now_tick_ = 0;
    std::size_t live_count_ = 0;
    bool advancing_ = false;
    std::vector<Node> arena_;
    std::vector<std::uint32_t> free_list_;   /// 回收槽（可直接复用）
    std::vector<std::uint32_t> garbage_;     /// 待回收槽（触发/撤销期挂账，惰性清理）
    std::vector<std::uint32_t> heads_;       /// levels_ * slots_ 桶头（kNil = 空）
};

} // namespace apollo::base
