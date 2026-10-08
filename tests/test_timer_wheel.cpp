// G-4 定时器轮单测（modules/base 组件，architecture-review §16.8-④ 落点）：
// 分层时间轮 O(1) 调度/撤销 + 喂钟驱动 + 页界级联降层 + 回调重入
// （回调内 schedule/cancel）+ 补拍语义 + 代数防陈旧撤销。
// 直编源惯例同 rng_substream_tests（modules/base 组件，无平台分支）。
#include "apollo/base/timer_wheel.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <vector>

namespace {

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

// ---- 一次性：到期拍精确触发（不早不晚），advance 返回触发数 ----
bool test_one_shot_exact_tick() {
    std::cout << "Running: test_one_shot_exact_tick..." << std::endl;

    apollo::base::TimerWheel wheel; // 缺省 10ms 拍
    std::uint64_t fired_at = 0;
    TEST_ASSERT(wheel.schedule_after(25, [&](apollo::base::TimerWheel::TimerId) {
        fired_at = wheel.current_ms();
    }) != 0, "调度返回有效 id");

    TEST_ASSERT(wheel.advance(20) == 0, "20ms 未到期不触发");
    TEST_ASSERT(!fired_at, "回调未执行");
    TEST_ASSERT(wheel.advance(29) == 0, "29ms 仍未到期（拍粒度 10ms：到期拍 = 30）");
    TEST_ASSERT(wheel.advance(30) == 1, "30ms 到期拍触发一次");
    TEST_ASSERT(fired_at == 30, "触发时刻 = 30ms（精确拍）");
    TEST_ASSERT(wheel.empty(), "一次性项触发后出册");
    TEST_ASSERT(wheel.advance(100) == 0, "此后不再触发");
    return true;
}

// ---- 多项不同到期拍：按时间序触发；同拍多项全触发 ----
bool test_fire_order_multiple() {
    std::cout << "Running: test_fire_order_multiple..." << std::endl;

    apollo::base::TimerWheel wheel;
    std::vector<std::uint64_t> order;
    wheel.schedule_after(50, [&](apollo::base::TimerWheel::TimerId) { order.push_back(50); });
    wheel.schedule_after(10, [&](apollo::base::TimerWheel::TimerId) { order.push_back(10); });
    wheel.schedule_after(30, [&](apollo::base::TimerWheel::TimerId) { order.push_back(30); });
    wheel.schedule_after(30, [&](apollo::base::TimerWheel::TimerId) { order.push_back(30); });
    TEST_ASSERT(wheel.size() == 4, "4 项在册");

    std::size_t fired = 0;
    for (std::uint64_t now = 10; now <= 50; now += 10) {
        fired += wheel.advance(now);
    }
    TEST_ASSERT(fired == 4, "4 项全触发（同拍 2 项不丢）");
    TEST_ASSERT(order.size() == 4 && order[0] == 10 && order[1] == 30 && order[2] == 30 &&
                    order[3] == 50,
                "按到期拍时间序触发");
    return true;
}

// ---- 周期项：逐周期触发 + cancel 停 + 绝对拍重排零漂移 ----
bool test_periodic_and_cancel() {
    std::cout << "Running: test_periodic_and_cancel..." << std::endl;

    apollo::base::TimerWheel wheel;
    int fires = 0;
    const auto id =
        wheel.schedule_interval(20, [&](apollo::base::TimerWheel::TimerId) { ++fires; });
    TEST_ASSERT(id != 0, "周期调度返回有效 id");

    TEST_ASSERT(wheel.advance(20) == 1 && fires == 1, "首周期 20ms 触发");
    TEST_ASSERT(wheel.advance(40) == 1 && fires == 2, "第二周期 40ms 触发");
    TEST_ASSERT(wheel.size() == 1, "周期项触发后仍在册");

    // 喂钟略迟到（35/65/95）：绝对拍重排——触发沿仍对齐 30/60/90（此处 60/80…）不漂移
    apollo::base::TimerWheel drift;
    std::vector<std::uint64_t> at;
    drift.schedule_interval(30, [&](apollo::base::TimerWheel::TimerId) {
        at.push_back(drift.current_ms());
    });
    drift.advance(35);
    drift.advance(65);
    drift.advance(95);
    TEST_ASSERT(at.size() == 3 && at[0] == 30 && at[1] == 60 && at[2] == 90,
                "迟到喂钟下触发沿对齐绝对拍（零漂移）");

    TEST_ASSERT(wheel.cancel(id), "cancel 在册周期项成功");
    TEST_ASSERT(!wheel.cancel(id), "重复 cancel 失败");
    TEST_ASSERT(wheel.empty(), "cancel 后出册");
    TEST_ASSERT(wheel.advance(1000) == 0 && fires == 2, "cancel 后不再触发");
    return true;
}

// ---- 回调内自撤销（周期项经 id 参数）：第二次触发后停 ----
bool test_self_cancel_in_callback() {
    std::cout << "Running: test_self_cancel_in_callback..." << std::endl;

    apollo::base::TimerWheel wheel;
    int fires = 0;
    apollo::base::TimerWheel::TimerId id = 0;
    id = wheel.schedule_interval(10, [&](apollo::base::TimerWheel::TimerId self) {
        ++fires;
        if (fires == 2) {
            wheel.cancel(self); // 触发中撤销重排后的在册项
        }
    });
    TEST_ASSERT(id != 0, "有效 id");

    wheel.advance(10);
    wheel.advance(20);
    TEST_ASSERT(fires == 2, "恰触发两次");
    TEST_ASSERT(wheel.empty(), "自撤销后出册");
    TEST_ASSERT(wheel.advance(50) == 0 && fires == 2, "此后不再触发");
    return true;
}

// ---- 回调内撤销他项 + 回调内调度新项：目标不触发、新项如期 ----
bool test_cross_cancel_and_schedule_in_callback() {
    std::cout << "Running: test_cross_cancel_and_schedule_in_callback..." << std::endl;

    apollo::base::TimerWheel wheel;
    bool b_fired = false;
    bool c_fired = false;

    apollo::base::TimerWheel::TimerId b = 0;
    b = wheel.schedule_after(30, [&](apollo::base::TimerWheel::TimerId) { b_fired = true; });
    wheel.schedule_after(10, [&](apollo::base::TimerWheel::TimerId) {
        wheel.cancel(b);                                  // 撤销他项（在册未触发）
        wheel.schedule_after(10, [&](apollo::base::TimerWheel::TimerId) { c_fired = true; });
    });

    TEST_ASSERT(wheel.advance(10) == 1, "首项触发");
    TEST_ASSERT(wheel.advance(30) == 1, "回调内新项如期触发（B 已被撤销）");
    TEST_ASSERT(!b_fired, "被撤销项不触发");
    TEST_ASSERT(c_fired, "回调内调度的新项触发");
    TEST_ASSERT(wheel.empty(), "全部出册");
    return true;
}

// ---- 页界级联降层：跨层到期拍精确（小参数轮 slots=8/levels=3/tick=100ms） ----
// 拍窗：L0 8 拍（800ms）/ L1 64 拍（6.4s）/ L2 顶层（超窗驻留）。
bool test_cascade_across_levels() {
    std::cout << "Running: test_cascade_across_levels..." << std::endl;

    apollo::base::TimerWheel wheel(100, 8, 3);
    std::vector<std::uint64_t> at;
    const auto record = [&](apollo::base::TimerWheel::TimerId) {
        at.push_back(wheel.current_ms());
    };

    wheel.schedule_after(300, record);    // L1（拍 3：跨 L0 页窗 8 拍）
    wheel.schedule_after(2000, record);   // L1（拍 20）
    wheel.schedule_after(6500, record);   // L2（拍 65 > 64 拍 L1 页窗）
    wheel.schedule_after(60000, record);  // 顶层驻留（600 拍，超 512 拍覆盖窗）
    TEST_ASSERT(wheel.size() == 4, "4 项在册");

    // 模拟主循环 500ms 喂钟步进
    std::size_t fired = 0;
    for (std::uint64_t now = 500; now <= 60000; now += 500) {
        fired += wheel.advance(now);
    }
    TEST_ASSERT(fired == 4, "4 项全触发");
    TEST_ASSERT(at.size() == 4 && at[0] == 300 && at[1] == 2000 && at[2] == 6500 &&
                    at[3] == 60000,
                "跨层/超窗到期拍全部精确（不早不丢）");
    return true;
}

// ---- 时间倒退：无操作不畸变；空轮大步吸附；吸附后相对调度如期 ----
bool test_regression_and_empty_snap() {
    std::cout << "Running: test_regression_and_empty_snap..." << std::endl;

    apollo::base::TimerWheel wheel;
    int fires = 0;
    wheel.schedule_after(20, [&](apollo::base::TimerWheel::TimerId) { ++fires; });
    TEST_ASSERT(wheel.advance(500) == 1 && fires == 1, "正常驱动触发");

    TEST_ASSERT(wheel.advance(300) == 0, "时间倒退无操作");
    TEST_ASSERT(wheel.current_ms() == 500, "时间沿不回退");

    // 空轮吸附：大步跳拍直接对齐（无到期项可触发）
    apollo::base::TimerWheel snap;
    TEST_ASSERT(snap.advance(123456) == 0, "空轮无触发");
    TEST_ASSERT(snap.current_ms() == 123450, "空轮吸附时间沿（拍粒度对齐）");
    bool fired = false;
    snap.schedule_after(20, [&](apollo::base::TimerWheel::TimerId) { fired = true; });
    TEST_ASSERT(snap.advance(123570) == 1 && fired, "吸附后相对调度如期触发");
    return true;
}

// ---- 补拍语义：大步喂钟逐拍步进，停摆期间周期项逐周期补发 ----
bool test_catch_up_burst() {
    std::cout << "Running: test_catch_up_burst..." << std::endl;

    apollo::base::TimerWheel wheel;
    int fires = 0;
    wheel.schedule_interval(100, [&](apollo::base::TimerWheel::TimerId) { ++fires; });
    TEST_ASSERT(wheel.advance(1000) == 10 && fires == 10,
                "停摆 1s 一次喂钟：100ms 周期项补发 10 次（逐拍步进语义）");
    TEST_ASSERT(wheel.advance(1100) == 1 && fires == 11, "恢复后照常触发");
    return true;
}

// ---- 代数防陈旧：槽位复用后旧 id 撤销不得误伤新项 ----
bool test_generation_guard() {
    std::cout << "Running: test_generation_guard..." << std::endl;

    apollo::base::TimerWheel wheel;
    const auto old_id = wheel.schedule_after(10, [](apollo::base::TimerWheel::TimerId) {});
    TEST_ASSERT(wheel.cancel(old_id), "撤销旧项");
    TEST_ASSERT(wheel.empty(), "出册");

    int fires = 0;
    const auto new_id = wheel.schedule_after(10, [&](apollo::base::TimerWheel::TimerId) { ++fires; });
    TEST_ASSERT(new_id != 0 && new_id != old_id, "复用槽位的新 id 与旧 id 不同（代数位）");
    TEST_ASSERT(!wheel.cancel(old_id), "旧 id 撤销失败（代数失配）");
    TEST_ASSERT(wheel.size() == 1, "新项不受误伤");
    TEST_ASSERT(wheel.advance(10) == 1 && fires == 1, "新项如期触发");
    return true;
}

// ---- 零延迟 = 下一拍；构造原点注入（大原点不从第 0 拍步进） ----
bool test_zero_delay_and_origin() {
    std::cout << "Running: test_zero_delay_and_origin..." << std::endl;

    apollo::base::TimerWheel wheel;
    bool fired = false;
    wheel.schedule_after(0, [&](apollo::base::TimerWheel::TimerId) { fired = true; });
    TEST_ASSERT(wheel.advance(5) == 0 && !fired, "不足一拍（5ms < 10ms 拍距）不触发");
    TEST_ASSERT(wheel.advance(10) == 1 && fired, "零延迟 = 下一拍触发");

    // 大原点：unix 毫秒级时钟直接构造（1.7e12 ms）
    apollo::base::TimerWheel epoch(10, 64, 5, 1780000000000ULL);
    bool origin_fired = false;
    epoch.schedule_after(25, [&](apollo::base::TimerWheel::TimerId) { origin_fired = true; });
    TEST_ASSERT(epoch.advance(1780000000020ULL) == 0 && !origin_fired, "原点相对 20ms 未到期");
    TEST_ASSERT(epoch.advance(1780000000030ULL) == 1 && origin_fired, "原点相对 30ms 如期触发");
    return true;
}

// ---- 自定义拍距（tick=100ms 主循环节拍轮）+ advance 触发计数面 ----
bool test_custom_tick_ms() {
    std::cout << "Running: test_custom_tick_ms..." << std::endl;

    apollo::base::TimerWheel wheel(100, 64, 5);
    TEST_ASSERT(wheel.tick_ms() == 100, "拍距参数面");
    int fires = 0;
    wheel.schedule_after(250, [&](apollo::base::TimerWheel::TimerId) { ++fires; });
    TEST_ASSERT(wheel.advance(100) == 0 && wheel.advance(200) == 0, "200ms 未到期");
    TEST_ASSERT(wheel.advance(300) == 1 && fires == 1, "300ms（第 3 拍）触发");
    return true;
}

// ---- 差分压力：随机 schedule/cancel/advance vs 朴素参考模型（固定种子确定性回归） ----
// 拍距倍数取值避开 ceil 边角；步长混合小步 + 偶发大步停摆（补拍语义面）。
bool test_differential_stress() {
    std::cout << "Running: test_differential_stress..." << std::endl;

    std::mt19937 rng(20261008u);
    for (int trial = 0; trial < 5; ++trial) {
        const std::uint32_t tick = 1u << (rng() % 4);       // 1..8 ms
        const std::uint32_t slots = 1u << (2 + rng() % 3);  // 4..16
        const std::uint32_t levels = 2 + rng() % 3;         // 2..4
        apollo::base::TimerWheel wheel(tick, slots, levels);
        std::uint64_t now = 0;

        struct Ref {
            std::uint64_t deadline;
            std::uint64_t interval;
            bool live;
        };
        std::map<apollo::base::TimerWheel::TimerId, Ref> ref;
        long long total_wheel = 0;
        long long total_ref = 0;

        for (int op = 0; op < 2000; ++op) {
            const int action = static_cast<int>(rng() % 10);
            if (action < 5) {
                const std::uint64_t units = 1 + rng() % 4000;
                const std::uint64_t delay_ms = units * tick;
                const bool periodic = (rng() % 3 == 0);
                const auto id = periodic
                    ? wheel.schedule_interval(delay_ms, [](apollo::base::TimerWheel::TimerId) {})
                    : wheel.schedule_after(delay_ms, [](apollo::base::TimerWheel::TimerId) {});
                ref[id] = {now / tick + units, periodic ? units : 0, true};
            } else if (action < 7) {
                std::vector<apollo::base::TimerWheel::TimerId> live_ids;
                for (auto& [k, r] : ref) {
                    if (r.live) {
                        live_ids.push_back(k);
                    }
                }
                if (!live_ids.empty()) {
                    const auto id = live_ids[rng() % live_ids.size()];
                    TEST_ASSERT(wheel.cancel(id) == ref[id].live, "撤销结果与参考模型一致");
                    ref[id].live = false;
                }
            } else {
                const std::uint64_t step = (rng() % 3 == 0 ? rng() % 2000 : rng() % 40) * tick;
                now += step;
                total_wheel += static_cast<long long>(wheel.advance(now));
                const std::uint64_t target = now / tick;
                for (auto& [k, r] : ref) {
                    if (!r.live) {
                        continue;
                    }
                    while (r.deadline <= target) {
                        ++total_ref;
                        if (r.interval > 0) {
                            r.deadline += r.interval;
                        } else {
                            r.live = false;
                            break;
                        }
                    }
                }
            }
            const auto ref_live = static_cast<std::size_t>(
                std::count_if(ref.begin(), ref.end(), [](auto& p) { return p.second.live; }));
            TEST_ASSERT(wheel.size() == ref_live, "在册数与参考模型一致");
        }
        TEST_ASSERT(total_wheel == total_ref, "触发总数与参考模型一致");
    }
    return true;
}

} // namespace

int main() {
    std::cout << "=== Apollo TimerWheel Test Suite ===" << std::endl;

    int total = 0;
    int passed = 0;

    auto run = [&](bool (*fn)()) {
        total++;
        if (fn()) {
            passed++;
        }
    };

    run(test_one_shot_exact_tick);
    run(test_fire_order_multiple);
    run(test_periodic_and_cancel);
    run(test_self_cancel_in_callback);
    run(test_cross_cancel_and_schedule_in_callback);
    run(test_cascade_across_levels);
    run(test_regression_and_empty_snap);
    run(test_catch_up_burst);
    run(test_generation_guard);
    run(test_zero_delay_and_origin);
    run(test_custom_tick_ms);
    run(test_differential_stress);

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total: " << total << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << (total - passed) << std::endl;

    return total == passed ? 0 : 1;
}
