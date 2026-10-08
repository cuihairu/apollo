#include "apollo/base/timer_wheel.hpp"

#include <cassert>

namespace apollo::base {
namespace {

std::uint32_t log2_floor(std::uint32_t value) {
    std::uint32_t bits = 0;
    while ((std::uint64_t{1} << (bits + 1)) <= value) {
        ++bits;
    }
    return bits;
}

} // namespace

TimerWheel::TimerWheel(std::uint32_t tick_ms, std::uint32_t slots, std::uint32_t levels,
                       std::uint64_t start_ms)
    : tick_ms_(tick_ms), slots_(slots), levels_(levels) {
    // 防御钳制（Release 无 assert 也不入 UB 路径）：拍距 >= 1、槽数 2 的幂、层数 >= 2
    if (tick_ms_ == 0) {
        tick_ms_ = 1;
    }
    while (slots_ < 2 || (slots_ & (slots_ - 1)) != 0) {
        ++slots_;
    }
    if (levels_ < 2) {
        levels_ = 2;
    }
    bits_ = log2_floor(slots_);
    now_tick_ = start_ms / tick_ms_;
    heads_.assign(static_cast<std::size_t>(levels_) * slots_, kNil);
}

TimerWheel::TimerId TimerWheel::schedule_after(std::uint64_t delay_ms, Callback cb) {
    const std::uint64_t ticks = std::max<std::uint64_t>(1, (delay_ms + tick_ms_ - 1) / tick_ms_);
    return allocate(std::move(cb), now_tick_ + ticks, 0);
}

TimerWheel::TimerId TimerWheel::schedule_interval(std::uint64_t interval_ms, Callback cb) {
    const std::uint64_t ticks = std::max<std::uint64_t>(1, (interval_ms + tick_ms_ - 1) / tick_ms_);
    return allocate(std::move(cb), now_tick_ + ticks, ticks);
}

bool TimerWheel::cancel(TimerId id) {
    if (id == 0 || arena_.empty()) {
        return false;
    }
    const std::uint32_t index = static_cast<std::uint32_t>(id & 0xFFFFFFFFu);
    if (index >= arena_.size()) {
        return false;
    }
    Node& node = arena_[index];
    if (!node.live || node.generation != static_cast<std::uint32_t>(id >> 32)) {
        return false;
    }
    unlink(index);
    node.live = false;
    --live_count_;
    garbage_.push_back(index);
    return true;
}

std::size_t TimerWheel::advance(std::uint64_t now_ms) {
    assert(!advancing_ && "advance 不得重入：驱动点唯一（主循环固定阶段），回调内禁再驱动");
    const std::uint64_t target = now_ms / tick_ms_;
    if (live_count_ == 0) {
        // 空轮吸附：无到期项，直接对齐时间沿（大原点冷启动 / 长空闲同理）
        if (target > now_tick_) {
            now_tick_ = target;
        }
        drain_garbage();
        return 0;
    }
    advancing_ = true;
    std::size_t fired = 0;
    while (now_tick_ < target) {
        ++now_tick_;
        // 页界级联（高层 -> 低层）：L 层页窗 = slots^L 拍，页首拍把新页桶降层。
        // 高层先行——本拍从高层降层的条目不再重复搬动。
        for (std::uint32_t level = levels_ - 1; level >= 1; --level) {
            if ((now_tick_ & ((std::uint64_t{1} << (bits_ * level)) - 1)) == 0) {
                const std::uint32_t slot =
                    static_cast<std::uint32_t>((now_tick_ >> (bits_ * level)) & (slots_ - 1));
                cascade(level, slot);
            }
        }
        fired += fire_slot(static_cast<std::uint32_t>(now_tick_ & (slots_ - 1)));
    }
    advancing_ = false;
    drain_garbage();
    return fired;
}

TimerWheel::TimerId TimerWheel::allocate(Callback cb, std::uint64_t deadline_ticks,
                                         std::uint64_t interval_ticks) {
    drain_garbage();
    std::uint32_t index;
    if (!free_list_.empty()) {
        index = free_list_.back();
        free_list_.pop_back();
        ++arena_[index].generation;
    } else {
        if (arena_.size() >= kNil) {
            return 0; // 槽位耗尽（2^32-1 个定时器——工程不可达，防御出口）
        }
        index = static_cast<std::uint32_t>(arena_.size());
        arena_.emplace_back();
    }
    Node& node = arena_[index];
    node.callback = std::move(cb);
    node.deadline_tick = deadline_ticks;
    node.interval_ticks = interval_ticks;
    node.live = true;
    node.prev = kNil;
    node.next = kNil;
    ++live_count_;
    place(index);
    return make_id(index, node.generation);
}

void TimerWheel::place(std::uint32_t index) {
    Node& node = arena_[index];
    // 最小层 L：deadline 与 now 在 L+1 层同页（页窗 slots^(L+1) 拍）——
    // 同页即本层可精确寻址；全部异页则驻留顶层（超窗定时器随页界降层）。
    std::uint32_t level = 0;
    while (level + 1 < levels_ &&
           (node.deadline_tick >> (bits_ * (level + 1))) != (now_tick_ >> (bits_ * (level + 1)))) {
        ++level;
    }
    const std::uint32_t slot =
        static_cast<std::uint32_t>((node.deadline_tick >> (bits_ * level)) & (slots_ - 1));
    node.level = level;
    node.slot = slot;
    const std::uint32_t head = heads_[static_cast<std::size_t>(level) * slots_ + slot];
    node.prev = kNil;
    node.next = head;
    if (head != kNil) {
        arena_[head].prev = index;
    }
    heads_[static_cast<std::size_t>(level) * slots_ + slot] = index;
}

void TimerWheel::unlink(std::uint32_t index) {
    Node& node = arena_[index];
    // 侵入链摘除：在桶内（含桶头）或已整桶摘出的触发队列中同样成立
    if (node.prev != kNil) {
        arena_[node.prev].next = node.next;
    } else if (heads_[static_cast<std::size_t>(node.level) * slots_ + node.slot] == index) {
        heads_[static_cast<std::size_t>(node.level) * slots_ + node.slot] = node.next;
    }
    if (node.next != kNil) {
        arena_[node.next].prev = node.prev;
    }
    node.prev = kNil;
    node.next = kNil;
}

void TimerWheel::cascade(std::uint32_t level, std::uint32_t slot) {
    const std::size_t bucket = static_cast<std::size_t>(level) * slots_ + slot;
    std::uint32_t index = heads_[bucket];
    heads_[bucket] = kNil;
    while (index != kNil) {
        const std::uint32_t next = arena_[index].next;
        arena_[index].prev = kNil;
        arena_[index].next = kNil;
        if (arena_[index].live) {
            place(index); // 依绝对拍重排：降层或驻留高层
        }
        index = next;
    }
}

std::size_t TimerWheel::fire_slot(std::uint32_t slot) {
    std::size_t fired = 0;
    std::uint32_t index = heads_[slot];
    heads_[slot] = kNil;
    // 先整桶摘出再逐节点处理——回调内 schedule/cancel 不破坏桶结构；
    // 节点引用不跨回调持有（回调可 allocate 致 arena 重分配，全程按索引走）。
    while (index != kNil) {
        const std::uint32_t next = arena_[index].next;
        arena_[index].prev = kNil;
        arena_[index].next = kNil;
        if (arena_[index].live) {
            const bool periodic = arena_[index].interval_ticks > 0;
            const TimerId id = make_id(index, arena_[index].generation);
            if (periodic) {
                // 周期项：先重排（绝对拍，停摆后从当前拍续排）再触发——
                // 回调内 cancel(id) 撤销的是重排后的在册项
                arena_[index].deadline_tick = std::max(
                    arena_[index].deadline_tick + arena_[index].interval_ticks, now_tick_ + 1);
                place(index);
            } else {
                arena_[index].live = false;
                --live_count_;
                garbage_.push_back(index);
            }
            if (periodic) {
                arena_[index].callback(id);
            } else {
                // 一次性项：闭包移出后再触发（回收/复用不触及执行中的捕获状态）
                Callback cb = std::move(arena_[index].callback);
                cb(id);
            }
            ++fired;
        }
        index = next;
    }
    return fired;
}

void TimerWheel::drain_garbage() {
    for (std::uint32_t index : garbage_) {
        if (arena_[index].live) {
            continue; // 防御：回收槽不应在册
        }
        arena_[index].callback = nullptr; // 释放闭包捕获
        free_list_.push_back(index);
    }
    garbage_.clear();
}

} // namespace apollo::base
