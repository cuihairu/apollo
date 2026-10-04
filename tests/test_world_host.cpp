// WorldHost 定帧语义单测（P0-1 产物验证）。
//
// 覆盖（contract clock-and-time §3 口径）：
//   1. 默认 tick 率 = 10Hz（契约/文档裁决，非 20Hz）；
//   2. 固定步长推进：tick 号全序单调、delta_seconds 恒为步长（不随真实间隔抖动）；
//   3. 首帧不推进（0 为无效哨兵，首个完整 tick 计 1）；
//   4. 有界追赶：积压在连跑上限内补齐；
//   5. 跳拍：积压超出上限丢弃整步并计数 dropped_ticks；
//   6. IWorldService initialize/tick/shutdown 生命周期与失败回滚；
//   7. ThreadAffinity（P0-1 执行上下文标注）：bind/is_owner/check 语义。
//
// 时间确定性：通过注入 fake clock（手动推进虚拟时刻）实现，无 sleep、无 flake。

#include "apollo/runtime/world_host.hpp"
#include "apollo/game/core/thread_affinity.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <thread>
#include <vector>

#define TEST_ASSERT(cond, msg)                                                               \
    do {                                                                                     \
        if (!(cond)) {                                                                       \
            std::cerr << "FAIL: " << (msg) << " (" << __FILE__ << ":" << __LINE__ << ")"     \
                      << std::endl;                                                          \
            return false;                                                                    \
        }                                                                                    \
    } while (0)

namespace {

struct FakeClock {
    static std::chrono::steady_clock::time_point value;

    static std::chrono::steady_clock::time_point now() {
        return value;
    }

    static void advance(std::chrono::milliseconds ms) {
        value += ms;
    }

    static void reset() {
        value = {};
    }
};

std::chrono::steady_clock::time_point FakeClock::value{};

class CountingService final : public apollo::runtime::IWorldService {
public:
    std::string_view service_name() const override {
        return "CountingService";
    }

    bool initialize() override {
        ++initialize_count;
        return init_result;
    }

    void tick(const apollo::runtime::WorldTickContext& context) override {
        ++tick_count;
        last_index = context.tick_index;
        last_delta = context.delta_seconds;
    }

    void shutdown() override {
        ++shutdown_count;
    }

    std::int64_t initialize_count = 0;
    std::int64_t tick_count = 0;
    std::int64_t shutdown_count = 0;
    std::uint64_t last_index = 0;
    double last_delta = 0.0;
    bool init_result = true;
};

bool test_default_tick_rate_is_10hz() {
    FakeClock::reset();
    apollo::runtime::WorldHost host(apollo::runtime::WorldHost::kDefaultTickRate,
                                    &FakeClock::now);
    TEST_ASSERT(host.tick_rate() == 10,
                "WorldHost 默认 tick 率应为 10Hz（clock-and-time §3 与 term-contract §1.5 裁决）");
    return true;
}

bool test_first_tick_does_not_advance() {
    FakeClock::reset();
    apollo::runtime::WorldHost host(10, &FakeClock::now);
    auto service = std::make_shared<CountingService>();
    host.add_world_service(service);
    TEST_ASSERT(host.start(), "start 应成功");

    // 首帧：只记录起点
    FakeClock::advance(std::chrono::milliseconds(10));
    host.tick();
    TEST_ASSERT(host.tick_context().tick_index == 0, "首帧不推进（0 为无效哨兵）");
    TEST_ASSERT(service->tick_count == 0, "首帧不调用 service");

    return true;
}

bool test_fixed_step_advance() {
    FakeClock::reset();
    apollo::runtime::WorldHost host(10, &FakeClock::now);
    auto service = std::make_shared<CountingService>();
    host.add_world_service(service);
    TEST_ASSERT(host.start(), "start 应成功");

    // 首帧
    host.tick();

    // 累计 30ms 不足以推进一个步长（100ms）
    FakeClock::advance(std::chrono::milliseconds(30));
    host.tick();
    TEST_ASSERT(host.tick_context().tick_index == 0, "30ms 不足一步，不应推进");

    // 再 30ms（累计 60ms）仍不足
    FakeClock::advance(std::chrono::milliseconds(30));
    host.tick();
    TEST_ASSERT(host.tick_context().tick_index == 0, "60ms 不足一步，不应推进");

    // 再 40ms（累计 100ms）→ 推进 1 步
    FakeClock::advance(std::chrono::milliseconds(40));
    host.tick();
    TEST_ASSERT(host.tick_context().tick_index == 1, "累计满一步应推进，tick 号从 1 起");
    TEST_ASSERT(service->tick_count == 1, "service 收到 1 次 tick");
    TEST_ASSERT(service->last_delta == 0.1, "delta_seconds 应为固定步长 0.1s");

    // 再 200ms → 推进 2 步（50ms 余量留到下一轮）
    FakeClock::advance(std::chrono::milliseconds(200));
    host.tick();
    TEST_ASSERT(host.tick_context().tick_index == 3, "200ms 应对应 2 步");
    TEST_ASSERT(service->tick_count == 3, "service 收到 3 次 tick");
    TEST_ASSERT(service->last_delta == 0.1, "delta_seconds 恒为固定步长");

    return true;
}

bool test_bounded_catch_up() {
    FakeClock::reset();
    apollo::runtime::WorldHost host(10, &FakeClock::now);
    auto service = std::make_shared<CountingService>();
    host.add_world_service(service);
    TEST_ASSERT(host.start(), "start 应成功");
    host.tick(); // 首帧

    // 积压 400ms（4 步）仍在连跑上限（5 步）内 → 全部补齐
    FakeClock::advance(std::chrono::milliseconds(400));
    host.tick();
    TEST_ASSERT(host.tick_context().tick_index == 4, "400ms 积压应补齐 4 步");
    TEST_ASSERT(host.dropped_ticks() == 0, "未超上限不跳拍");

    return true;
}

bool test_drop_ticks_when_backlog_exceeds_catch_up_limit() {
    FakeClock::reset();
    apollo::runtime::WorldHost host(10, &FakeClock::now);
    auto service = std::make_shared<CountingService>();
    host.add_world_service(service);
    TEST_ASSERT(host.start(), "start 应成功");
    host.tick(); // 首帧

    // 积压 3000ms（30 步 >> 上限 5 步）：本轮最多连跑 5 步，其余跳拍
    FakeClock::advance(std::chrono::milliseconds(3000));
    host.tick();
    TEST_ASSERT(host.tick_context().tick_index == 5, "单次调用最多连跑 5 步");
    TEST_ASSERT(host.dropped_ticks() > 0, "超过上限应记录跳拍");
    TEST_ASSERT(service->tick_count == 5, "service 只收到 5 次 tick");

    // 清理余量后（不足一步）不再推进
    const auto after_drop = host.tick_context().tick_index;
    host.tick(); // 无时间流逝
    TEST_ASSERT(host.tick_context().tick_index == after_drop, "无时间流逝不推进");

    return true;
}

bool test_service_failure_rolls_back() {
    FakeClock::reset();
    apollo::runtime::WorldHost host(10, &FakeClock::now);
    auto good = std::make_shared<CountingService>();
    auto bad = std::make_shared<CountingService>();
    bad->init_result = false;
    host.add_world_service(good);
    host.add_world_service(bad);

    TEST_ASSERT(!host.start(), "任一 service 初始化失败则 start 失败");
    TEST_ASSERT(!host.is_running(), "start 失败后不运行");
    TEST_ASSERT(good->shutdown_count == 1, "失败路径应回滚已初始化 service");

    return true;
}

bool test_stop_shuts_down_all_services() {
    FakeClock::reset();
    apollo::runtime::WorldHost host(10, &FakeClock::now);
    auto service = std::make_shared<CountingService>();
    host.add_world_service(service);
    TEST_ASSERT(host.start(), "start 应成功");
    host.stop();
    TEST_ASSERT(!host.is_running(), "stop 后不运行");
    TEST_ASSERT(service->shutdown_count == 1, "stop 应逆序 shutdown 全部 service");

    return true;
}

bool test_thread_affinity() {
    apollo::game::core::ThreadAffinity affinity;
    TEST_ASSERT(!affinity.is_bound(), "初始未绑定");

    affinity.bind();
    TEST_ASSERT(affinity.is_bound(), "bind 后已绑定");
    TEST_ASSERT(affinity.is_owner(), "绑定线程视角应为 owner");
    affinity.check(); // 不应触发断言

    std::thread other([&] {
        TEST_ASSERT(!affinity.is_owner(), "其他线程视角不应是 owner");
        return true;
    });
    other.join();

    return true;
}

} // namespace

int main() {
    int passed = 0;
    int failed = 0;

    struct Case {
        const char* name;
        bool (*fn)();
    } cases[] = {
        {"default_tick_rate_is_10hz", test_default_tick_rate_is_10hz},
        {"first_tick_does_not_advance", test_first_tick_does_not_advance},
        {"fixed_step_advance", test_fixed_step_advance},
        {"bounded_catch_up", test_bounded_catch_up},
        {"drop_ticks_when_backlog_exceeds_catch_up_limit",
         test_drop_ticks_when_backlog_exceeds_catch_up_limit},
        {"service_failure_rolls_back", test_service_failure_rolls_back},
        {"stop_shuts_down_all_services", test_stop_shuts_down_all_services},
        {"thread_affinity", test_thread_affinity},
    };

    for (const auto& c : cases) {
        std::cout << "[ RUN  ] " << c.name << std::endl;
        if (c.fn()) {
            std::cout << "[  OK  ] " << c.name << std::endl;
            ++passed;
        } else {
            std::cout << "[ FAIL ] " << c.name << std::endl;
            ++failed;
        }
    }

    std::cout << "WorldHostTests: " << passed << " passed, " << failed << " failed" << std::endl;
    return failed == 0 ? 0 : 1;
}