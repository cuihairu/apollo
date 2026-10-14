// Scene × ScriptRuntime §5.1 钩子窗口适配桥测试（scripting-lua 宿主接线批）。
//
// 覆盖：
//   - tick 在 Recalc 尾 / Collect 前执行钩子窗口：修订值经正规写路径入存储，
//     collect 出修订后 delta（端到端：写 → 窗口 → collect）
//   - 白名单来自契约表（contract.write_whitelist 喂窗口；白名单外保持原值）
//   - 帧预算耗尽跳过、BudgetFlush 相位归还后下一 tick 恢复
//   - 钩子错误保持原值 + fault 事件带 Scene tick 号（set_tick 记账）
//   - 无 script attach 零开销（原值直通、窗口不开）
//   - detach_script 停窗（后续写直通）
//   - attach 次序无关（先 script 后 attrsync 同样架桥）
//
// 桥接缝：attrsync 注入式钩子（HookChange）→ ScriptRuntime::run_attr_hook_window
// （AttrChange 快照拷贝）→ 修订值写回 HookChange（attrsync 正规写路径落账）。

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "apollo/game/attrsync/attr_sync_manager.hpp"
#include "apollo/game/world/scene.hpp"
#include "apollo/script/script_runtime.hpp"

using namespace apollo::game::attrsync;
using apollo::game::world::Scene;
using apollo::script::ScriptConfig;
using apollo::script::ScriptFaultEvent;
using apollo::script::ScriptRuntime;

namespace {

constexpr std::uint64_t kViewer = 8201;
constexpr std::uint64_t kSubject = 8202;

// 契约：白名单只放行 attr 3（attr 7 白名单外，供保留原值对照）。
constexpr const char* kContract = R"lua(
return {
  version = 7,
  routes = { [1001] = "onPing" },
  write_whitelist = { 3 },
}
)lua";

}  // namespace

class SceneScriptBridgeTest : public ::testing::Test {
protected:
    AttrSyncManager mgr;
    Scene scene{2u, "bridge"};
    ScriptRuntime rt;
    std::vector<ScriptFaultEvent> faults;
    std::vector<std::tuple<std::uint64_t, std::uint32_t, std::int64_t>> sink_log;

    void SetUp() override {
        rt.set_fault_sink([&](const ScriptFaultEvent& e) { faults.push_back(e); });
        mgr.set_change_sink([&](std::uint64_t e, std::uint32_t a, std::int64_t v) {
            sink_log.emplace_back(e, a, v);
        });
    }

    void boot(const std::string& hook_src, const std::string& contract = kContract) {
        // 契约路由完整性闸（§3.4）要求 routes 里的 handler 可达——一并载入。
        const std::string src = "function onPing() end\n" + hook_src;
        ASSERT_TRUE(rt.call_string("boot", src));
        ASSERT_TRUE(rt.set_attr_hook("onAttrChanged"));
        ASSERT_TRUE(rt.load_contract(contract));
    }

    // 建视野（ViewerState）供 collect 出 delta；绕开 AOI 布局，聚焦桥行为。
    void attach_both() {
        mgr.register_entity(kSubject);
        mgr.on_viewer_enter(kViewer, kSubject);
        scene.attach_attrsync(mgr);
        scene.attach_script(rt);
    }

    void attach_reversed() {
        mgr.register_entity(kSubject);
        scene.attach_script(rt);
        scene.attach_attrsync(mgr);
        mgr.on_viewer_enter(kViewer, kSubject);
    }
};

// ── 窗口时序：Recalc 尾执行、collect 出修订值 ────────────────

TEST_F(SceneScriptBridgeTest, TickAppliesRevisionBeforeCollect) {
    boot("function onAttrChanged(e, a, old, new) return new + 5 end");
    attach_both();

    mgr.set_attribute(kSubject, 3, 40);
    scene.tick(0.016);

    // 修订经正规写路径落存储
    EXPECT_EQ(mgr.get_attribute(kSubject, 3), 45);
    // collect（Collect 相位语义）拿到修订后 delta
    const auto frames = mgr.collect(kViewer, kSubject);
    ASSERT_EQ(frames.size(), 1u);
    ASSERT_EQ(frames[0].deltas.size(), 1u);
    EXPECT_EQ(frames[0].deltas[0].attr_id, 3u);
    EXPECT_EQ(frames[0].deltas[0].value, 45);
    EXPECT_EQ(rt.stats().attr_hook_calls, 1u);
}

TEST_F(SceneScriptBridgeTest, WhitelistOutsideKeepsOriginal) {
    boot("function onAttrChanged(e, a, old, new) return new + 5 end");
    attach_both();

    mgr.set_attribute(kSubject, 7, 90);
    scene.tick(0.016);

    EXPECT_EQ(mgr.get_attribute(kSubject, 7), 90);
    EXPECT_EQ(rt.stats().attr_hook_calls, 0u);
    EXPECT_EQ(rt.stats().attr_hook_skipped_whitelist, 1u);
}

// ── 帧预算：耗尽跳过、BudgetFlush 归还后下一 tick 恢复 ────────

TEST_F(SceneScriptBridgeTest, FrameBudgetSkipsThenNextTickRestores) {
    boot("function onAttrChanged(e, a, old, new) return new + 5 end");
    attach_both();
    scene.set_attr_hook_frame_budget(0);  // 预算 0 = 窗口全跳过

    mgr.set_attribute(kSubject, 3, 40);
    scene.tick(0.016);
    EXPECT_EQ(mgr.get_attribute(kSubject, 3), 40);  // 原值保持
    EXPECT_EQ(rt.stats().attr_hook_skipped_budget, 1u);
    EXPECT_EQ(rt.stats().attr_hook_calls, 0u);

    // 预算恢复（tick 1 的 BudgetFlush 相位已归还帧账）
    scene.set_attr_hook_frame_budget(200000);
    mgr.set_attribute(kSubject, 3, 41);
    scene.tick(0.016);
    EXPECT_EQ(mgr.get_attribute(kSubject, 3), 46);
    EXPECT_EQ(rt.stats().attr_hook_calls, 1u);
}

// ── 错误隔离 + set_tick 记账（fault 事件 tick 字段 = Scene tick 号）──

TEST_F(SceneScriptBridgeTest, HookFaultKeepsOriginalAndCarriesSceneTick) {
    boot("function onAttrChanged(e, a, old, new) error('hook fault') end");
    attach_both();

    mgr.set_attribute(kSubject, 3, 40);
    scene.tick(0.016);

    EXPECT_EQ(mgr.get_attribute(kSubject, 3), 40);  // 原值保持
    ASSERT_EQ(rt.stats().attr_hook_faults, 1u);
    ASSERT_EQ(faults.size(), 1u);
    EXPECT_EQ(faults[0].kind, ScriptFaultEvent::Kind::kRuntime);
    EXPECT_EQ(faults[0].chunk, "onAttrChanged");
    EXPECT_EQ(faults[0].tick, 1u);  // Scene set_tick 记账（首 tick = 1）
}

// ── 空载与停窗 ──────────────────────────────────────────────

TEST_F(SceneScriptBridgeTest, NoScriptAttachedZeroOverhead) {
    boot("function onAttrChanged(e, a, old, new) return new + 5 end");
    mgr.register_entity(kSubject);
    mgr.on_viewer_enter(kViewer, kSubject);
    scene.attach_attrsync(mgr);  // 无 attach_script：桥未架、窗口不开

    mgr.set_attribute(kSubject, 3, 40);
    scene.tick(0.016);

    EXPECT_FALSE(scene.script_attached());
    EXPECT_EQ(mgr.get_attribute(kSubject, 3), 40);
    EXPECT_EQ(rt.stats().attr_hook_calls, 0u);
    // collect 面不受影响（直通值）
    const auto frames = mgr.collect(kViewer, kSubject);
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].deltas[0].value, 40);
}

TEST_F(SceneScriptBridgeTest, DetachScriptStopsWindow) {
    boot("function onAttrChanged(e, a, old, new) return new + 5 end");
    attach_both();

    mgr.set_attribute(kSubject, 3, 40);
    scene.tick(0.016);
    EXPECT_EQ(mgr.get_attribute(kSubject, 3), 45);

    scene.detach_script();
    EXPECT_FALSE(scene.script_attached());

    mgr.set_attribute(kSubject, 3, 60);
    scene.tick(0.016);
    EXPECT_EQ(mgr.get_attribute(kSubject, 3), 60);  // 直通
    EXPECT_EQ(rt.stats().attr_hook_calls, 1u);      // 不再新增
}

TEST_F(SceneScriptBridgeTest, AttachOrderIndependent) {
    boot("function onAttrChanged(e, a, old, new) return new + 5 end");
    attach_reversed();  // 先 script 后 attrsync

    mgr.set_attribute(kSubject, 3, 40);
    scene.tick(0.016);
    EXPECT_EQ(mgr.get_attribute(kSubject, 3), 45);
}
