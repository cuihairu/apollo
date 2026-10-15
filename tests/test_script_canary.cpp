// §3.2 Phase C 灰度观察期原语：canary 提交后观察窗 + per-module fault 计数 +
// 超阈判定。promote/rollback 归控制面驱动既有面（commit_module/rollback_module）。

#include "apollo/script/module_canary.hpp"
#include "apollo/script/script_runtime.hpp"

#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace apollo::script;

namespace {

ScriptConfig canary_config() {
    ScriptConfig c;
    c.instruction_budget = 100000;
    c.memory_limit_bytes = 1 << 20;
    return c;
}

// tick 推进 + 驱动观察（控制面每 tick 两个动作的测试镜像）。
void drive_ticks(ScriptRuntime& rt, ModuleCanary& canary, std::uint64_t n) {
    for (std::uint64_t i = 0; i < n; ++i) {
        rt.set_tick(rt.current_tick() + 1);
        canary.tick();
    }
}

TEST(ModuleCanary, HealthyModuleCompletesWindow) {
    ScriptRuntime rt(canary_config());
    ASSERT_TRUE(rt.swap_module({"math", 1, {}},
                               "return { onTick = function() end, smoke = function() return true end }"));
    ModuleCanary canary(rt, "math", CanaryParams{10, 0});
    canary.begin();
    drive_ticks(rt, canary, 5);
    EXPECT_FALSE(canary.finished());  // 窗未满
    drive_ticks(rt, canary, 5);
    EXPECT_TRUE(canary.finished());
    EXPECT_TRUE(canary.healthy());
    EXPECT_EQ(canary.window_faults(), 0u);
}

TEST(ModuleCanary, FaultBeyondThresholdFailsEarly) {
    ScriptRuntime rt(canary_config());
    ASSERT_TRUE(rt.swap_module({"math", 1, {}},
                               "return { onTick = function() end, smoke = function() return true end }"));
    ModuleCanary canary(rt, "math", CanaryParams{100, 0});
    canary.begin();
    rt.set_tick(rt.current_tick() + 1);
    EXPECT_FALSE(rt.call_module("math", "onTick2"));  // 缺 handler = 模块归因 fault
    canary.tick();
    EXPECT_TRUE(canary.finished());  // 超阈提前结束（不等窗满）
    EXPECT_FALSE(canary.healthy());
    EXPECT_EQ(canary.window_faults(), 1u);
}

TEST(ModuleCanary, FaultsBeforeBeginNotCounted) {
    ScriptRuntime rt(canary_config());
    ASSERT_TRUE(rt.swap_module({"math", 1, {}}, "return { smoke = function() return true end }"));
    // begin 前的既有 fault（非本观察窗产生）不计数。
    rt.set_tick(1);
    EXPECT_FALSE(rt.call_module("math", "ghost"));
    rt.set_tick(2);  // 推进到下一拍再 begin——tick 1 的 fault 严格在窗前
    ModuleCanary canary(rt, "math", CanaryParams{10, 0});
    canary.begin();  // begin_tick = 2，tick 1 的 fault 不入窗
    drive_ticks(rt, canary, 10);
    EXPECT_TRUE(canary.healthy());
    EXPECT_EQ(canary.window_faults(), 0u);
}

TEST(ModuleCanary, OtherModuleFaultsNotAttributed) {
    ScriptRuntime rt(canary_config());
    ASSERT_TRUE(rt.swap_module({"math", 1, {}}, "return { smoke = function() return true end }"));
    ASSERT_TRUE(rt.swap_module({"net", 1, {}}, "return { smoke = function() return true end }"));
    ModuleCanary canary(rt, "math", CanaryParams{10, 0});
    canary.begin();
    rt.set_tick(rt.current_tick() + 1);
    EXPECT_FALSE(rt.call_module("net", "ghost"));  // 归因 net，不归因 math
    canary.tick();
    drive_ticks(rt, canary, 9);
    EXPECT_TRUE(canary.finished());
    EXPECT_TRUE(canary.healthy());
    EXPECT_EQ(canary.window_faults(), 0u);
}

TEST(ModuleCanary, UnhealthyCanaryLeadsToRollback) {
    // Phase C 全回路：v1 健康 → 换 v2（onTick 判坏）→ canary 判 unhealthy →
    // 控制面驱动 rollback → 版本回 1 且 handler 恢复正常。
    ScriptRuntime rt(canary_config());
    const std::string good =
        "return { onTick = function() end, smoke = function() return true end }";
    const std::string bad =
        "return { onTick = function() error('canary break') end, smoke = function() return true end }";
    ASSERT_TRUE(rt.swap_module({"math", 1, {}}, good));
    EXPECT_TRUE(rt.call_module("math", "onTick"));

    ASSERT_TRUE(rt.swap_module({"math", 2, {}}, bad));
    ModuleCanary canary(rt, "math", CanaryParams{600, 0});
    canary.begin();
    rt.set_tick(rt.current_tick() + 1);
    EXPECT_FALSE(rt.call_module("math", "onTick"));  // v2 onTick 运行时错误
    canary.tick();
    ASSERT_TRUE(canary.finished());
    ASSERT_FALSE(canary.healthy());

    // 控制面按 verdict 驱动回滚（rollback_module 既有面）。
    EXPECT_TRUE(rt.rollback_module("math"));
    EXPECT_EQ(rt.active_modules()["math"], 1u);  // 活跃版本回 1
    EXPECT_TRUE(rt.call_module("math", "onTick"));  // 恢复正常
}

TEST(ModuleCanary, PendingBeforeBeginNotFinished) {
    ScriptRuntime rt(canary_config());
    ASSERT_TRUE(rt.swap_module({"math", 1, {}}, "return { smoke = function() return true end }"));
    ModuleCanary canary(rt, "math", CanaryParams{10, 0});
    EXPECT_FALSE(canary.finished());  // 未 begin 不判满
    canary.begin();
    EXPECT_FALSE(canary.finished());
}

}  // namespace
