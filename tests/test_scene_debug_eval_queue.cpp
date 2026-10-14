// Scene §7.1 admin attach 执行队列测试（scripting-lua §7.1 宿主接线）。
//
// 覆盖：
//   - 入队不执行、tick 首拍固定调试点执行、结果按 id 关联回传
//   - 执行失败回 ok=false（错误隔离、结果空、fault 仍计数）
//   - 队列深度上限：超限拒收（打爆防护）
//   - 结果按深度上限丢弃最旧（FIFO 有界，不无限积压）
//   - 无 script attach 拒收入队；detach 清队列停执行
//   - 跨线程并发入队无丢失（Scene 唯一跨线程面）
//   - 同 tick 限流通透（超出预算部分标记失败）
//
// 语义分工：本批 = owning 场景线程任务队列 + tick 边界固定调试点 + 结果回传。
// transport（admin 单入口 + control 通道转发到目标进程）留 admin/control 面批次。

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "apollo/game/world/scene.hpp"
#include "apollo/script/script_runtime.hpp"

using apollo::game::world::Scene;
using apollo::script::ScriptRuntime;

class SceneDebugQueueTest : public ::testing::Test {
protected:
    Scene scene{3u, "dbg"};
    ScriptRuntime rt;

    void attach() {
        ASSERT_TRUE(rt.call_string(
            "boot", "counter = 0\nfunction inc() counter = counter + 1 end\n"));
        scene.attach_script(rt);
    }

    // 执行结果数与 ok 计数（断言辅助）。
    static int ok_count(const std::vector<Scene::DebugEvalResult>& out) {
        int n = 0;
        for (const auto& r : out) {
            if (r.ok) {
                ++n;
            }
        }
        return n;
    }
};

// ── tick 边界执行 + id 关联回传 ──────────────────────────────

TEST_F(SceneDebugQueueTest, ExecutesAtTickBoundaryReturnsByRequestId) {
    attach();

    ASSERT_TRUE(scene.enqueue_debug_eval(1, "counter = 41"));
    // 未 tick：不执行（沙盒不越 tick 边界提前跑）
    EXPECT_TRUE(scene.drain_debug_eval_results().empty());

    scene.tick(0.016);
    const auto out = scene.drain_debug_eval_results();
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].id, 1u);
    EXPECT_TRUE(out[0].ok);
    EXPECT_TRUE(out[0].result.empty());  // 语句路径无返回值

    // 副作用可观察：下一次 tick eval 表达式读回
    ASSERT_TRUE(scene.enqueue_debug_eval(2, "counter"));
    scene.tick(0.016);
    const auto out2 = scene.drain_debug_eval_results();
    ASSERT_EQ(out2.size(), 1u);
    EXPECT_EQ(out2[0].id, 2u);
    EXPECT_EQ(out2[0].result, "41");
}

// ── 错误隔离 ────────────────────────────────────────────────

TEST_F(SceneDebugQueueTest, RuntimeFailureYieldsOkFalse) {
    attach();

    ASSERT_TRUE(scene.enqueue_debug_eval(7, "error('boom')"));
    scene.tick(0.016);

    const auto out = scene.drain_debug_eval_results();
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].id, 7u);
    EXPECT_FALSE(out[0].ok);
    EXPECT_TRUE(out[0].result.empty());
    EXPECT_EQ(rt.stats().debug_evals, 1u);  // 失败也计执行
    EXPECT_EQ(rt.stats().fault_runtime, 1u);

    // 状态可继续：下一 tick 仍能执行
    ASSERT_TRUE(scene.enqueue_debug_eval(8, "counter"));
    scene.tick(0.016);
    const auto out2 = scene.drain_debug_eval_results();
    ASSERT_EQ(out2.size(), 1u);
    EXPECT_TRUE(out2[0].ok);
    EXPECT_EQ(out2[0].result, "0");
}

TEST_F(SceneDebugQueueTest, SyntaxErrorYieldsOkFalse) {
    attach();

    ASSERT_TRUE(scene.enqueue_debug_eval(9, "return return"));
    scene.tick(0.016);

    const auto out = scene.drain_debug_eval_results();
    ASSERT_EQ(out.size(), 1u);
    EXPECT_FALSE(out[0].ok);
    EXPECT_TRUE(out[0].result.empty());
    EXPECT_EQ(rt.stats().fault_syntax, 1u);
}

// ── 队列边界 ────────────────────────────────────────────────

TEST_F(SceneDebugQueueTest, DepthLimitRejectsOversubscription) {
    attach();
    scene.set_debug_eval_queue_depth(2);

    ASSERT_TRUE(scene.enqueue_debug_eval(1, "1"));
    ASSERT_TRUE(scene.enqueue_debug_eval(2, "2"));
    EXPECT_FALSE(scene.enqueue_debug_eval(3, "3"));  // 超限拒收（打爆防护）

    scene.tick(0.016);
    const auto out = scene.drain_debug_eval_results();
    ASSERT_EQ(out.size(), 2u);
    EXPECT_TRUE(out[0].ok);
    EXPECT_TRUE(out[1].ok);
}

TEST_F(SceneDebugQueueTest, ResultsDropOldestWhenNotDrained) {
    attach();
    scene.set_debug_eval_queue_depth(2);

    ASSERT_TRUE(scene.enqueue_debug_eval(1, "1"));
    ASSERT_TRUE(scene.enqueue_debug_eval(2, "2"));
    scene.tick(0.016);  // 结果 2 条，未超 cap

    ASSERT_TRUE(scene.enqueue_debug_eval(3, "3"));
    ASSERT_TRUE(scene.enqueue_debug_eval(4, "4"));
    scene.tick(0.016);  // 结果 4 条 → cap 2 → 保留最新两条

    const auto out = scene.drain_debug_eval_results();
    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(out[0].id, 3u);
    EXPECT_EQ(out[1].id, 4u);
}

// ── attach 生命周期 ─────────────────────────────────────────

TEST_F(SceneDebugQueueTest, RejectsEnqueueWithoutRuntime) {
    EXPECT_FALSE(scene.enqueue_debug_eval(1, "1"));  // 未 attach_script

    scene.tick(0.016);  // 无 script 的 tick 不炸
    EXPECT_TRUE(scene.drain_debug_eval_results().empty());
}

TEST_F(SceneDebugQueueTest, DetachClearsPendingAndStopsExecution) {
    attach();
    ASSERT_TRUE(scene.enqueue_debug_eval(1, "1"));

    scene.detach_script();
    EXPECT_FALSE(scene.script_attached());

    scene.tick(0.016);  // 在队请求随 detach 丢弃，不执行
    EXPECT_TRUE(scene.drain_debug_eval_results().empty());
    EXPECT_FALSE(scene.enqueue_debug_eval(2, "2"));  // 已 detach 拒收
}

// ── 跨线程面（mutex 保护，任意线程可投递）────────────────────

TEST_F(SceneDebugQueueTest, ConcurrentEnqueueNoLoss) {
    attach();
    scene.set_debug_eval_queue_depth(16);

    constexpr int kThreads = 2;
    constexpr int kPer = 4;
    std::vector<std::thread> workers;
    workers.reserve(kThreads);
    std::vector<int> accepted(kThreads * kPer, 0);
    for (int t = 0; t < kThreads; ++t) {
        workers.emplace_back([&, t] {
            for (int i = 0; i < kPer; ++i) {
                accepted[t * kPer + i] =
                    scene.enqueue_debug_eval(t * 100 + static_cast<std::uint64_t>(i), "x")
                        ? 1
                        : 0;
            }
        });
    }
    for (auto& w : workers) {
        w.join();
    }

    int total = 0;
    for (int a : accepted) {
        total += a;
    }
    EXPECT_EQ(total, kThreads * kPer);

    scene.tick(0.016);
    const auto out = scene.drain_debug_eval_results();
    ASSERT_EQ(out.size(), static_cast<std::size_t>(kThreads * kPer));
    std::set<std::uint64_t> ids;
    for (const auto& r : out) {
        EXPECT_TRUE(r.ok);
        EXPECT_TRUE(ids.insert(r.id).second);  // 不重不丢
    }
}

// ── 同 tick 限流通透 ────────────────────────────────────────

TEST_F(SceneDebugQueueTest, RateLimitCapsExecutionsPerTick) {
    attach();  // 默认 debug_eval_max_per_tick = 8
    scene.set_debug_eval_queue_depth(16);

    for (int i = 0; i < 12; ++i) {
        ASSERT_TRUE(scene.enqueue_debug_eval(static_cast<std::uint64_t>(i), "x"));
    }
    scene.tick(0.016);

    const auto out = scene.drain_debug_eval_results();
    ASSERT_EQ(out.size(), 12u);  // 队列一次跑完，超额部分标记失败
    EXPECT_EQ(ok_count(out), 8);  // 限流生效（8/tick）
    EXPECT_EQ(rt.stats().debug_evals, 8u);  // 限流的调用不计执行
    EXPECT_EQ(rt.stats().fault_runtime, 4u);
}
