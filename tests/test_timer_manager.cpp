// TimerManager 跨线程/回调窗口回归（audit P0#2 修复 2026-11-12）。
// 原实现 update()→processSlot 全程无锁，executeTimer 直接 mutex_.unlock()
// （解锁未持有的锁 = UB），且回调窗口内 TimerDesc& 引用可被回调自身
// killTimer erase → 悬空 UAF。修复：回调参数拷出、锁外触发、回调后按 id
// 重查推进 repeat（删 unlock/lock 手法），wheel/timers_ 临界区独立加锁。
// 直接编入 timer_manager.cpp（自包含，不依赖 legacy apollo lib）。
#include "apollo/core/timer/timer_manager.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <thread>

namespace {

using apollo::core::timer::TimerManager;

// 泵 update() 直至 done() 为真或超时（timer 间隔 1ms，分辨率 1ms）
bool pump(TimerManager& mgr, const std::function<bool()>& done, int timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        mgr.update();
        if (done()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return done();
}

// 单次定时器触发一次后自动移除
bool test_one_shot_fires_and_removed() {
    TimerManager mgr;
    std::atomic<int> fired{0};
    const auto id = mgr.setTimer(1, [&](apollo::core::timer::TimerId, void*) {
        fired++;
    }, 0, nullptr);  // repeat=0 → 单次
    if (id == apollo::core::timer::INVALID_TIMER_ID) {
        return false;
    }
    if (!pump(mgr, [&] { return fired.load() > 0; }, 3000)) {
        return false;
    }
    return fired.load() == 1 && mgr.getTimerCount() == 0;
}

// 周期定时器回调内 killTimer(selfId)：回调窗口 erase 后旧代码悬空引用 UAF，
// 修复后 kill 生效且不再重插（回调自停）
bool test_periodic_callback_self_kill() {
    struct Ctx {
        TimerManager* mgr = nullptr;
        std::atomic<bool> fired{false};
        std::atomic<bool> kill_result{false};
    };
    TimerManager mgr;
    Ctx ctx;
    ctx.mgr = &mgr;
    const auto id = mgr.setTimer(
        1,
        [](apollo::core::timer::TimerId timerId, void* user_data) {
            auto* c = static_cast<Ctx*>(user_data);
            c->fired = true;
            c->kill_result = c->mgr->killTimer(timerId);  // 回调内自停
        },
        2, &ctx);  // repeat=2（周期）
    if (id == apollo::core::timer::INVALID_TIMER_ID) {
        return false;
    }
    if (!pump(mgr, [&] { return ctx.fired.load(); }, 3000)) {
        return false;
    }
    // killTimer 在回调内应命中（条目回调窗口仍在册）；回调后按 id 重查
    // 已删 → 不重插，目录清空
    return ctx.kill_result.load() && mgr.getTimerCount() == 0;
}

// 并发 setTimer/killTimer 与 update()：修复前 wheel/timers_ 无锁竞态
bool test_concurrent_set_kill_during_update() {
    TimerManager mgr;
    std::atomic<bool> stop{false};
    std::thread mutator([&]() {
        std::uint64_t n = 0;
        while (!stop.load()) {
            auto id = mgr.setTimer(1, [](apollo::core::timer::TimerId, void*) {},
                                   0xFFFFFFFF, nullptr);
            if (id != apollo::core::timer::INVALID_TIMER_ID && n % 3 == 0) {
                mgr.killTimer(id);
            }
            ++n;
        }
    });
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(1);
    while (std::chrono::steady_clock::now() < deadline) {
        mgr.update();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    stop = true;
    mutator.join();
    return true;  // 不崩即过（竞态回归信号）
}

} // namespace

int main() {
    struct Named {
        const char* name;
        bool (*fn)();
    };
    const Named tests[] = {
        {"one_shot_fires_and_removed", test_one_shot_fires_and_removed},
        {"periodic_callback_self_kill", test_periodic_callback_self_kill},
        {"concurrent_set_kill_during_update", test_concurrent_set_kill_during_update},
    };

    for (const auto& t : tests) {
        std::cout << "Running test_" << t.name << "... " << std::flush;
        if (t.fn()) {
            std::cout << "PASSED" << std::endl;
        } else {
            std::cout << "FAILED" << std::endl;
            std::cerr << "apollo_timer_manager_tests failed" << std::endl;
            return 1;
        }
    }

    std::cout << "apollo_timer_manager_tests passed" << std::endl;
    return 0;
}
