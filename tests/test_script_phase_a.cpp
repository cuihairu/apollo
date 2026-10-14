// ScriptRuntime §3.2 Phase A 两段式热替换测试（scripting-lua.md §3.2：
// 工作线程预加载 + owning 面 O(1) 换表；docs/modules/script.md 契约面）。
//
// 覆盖：
//   - prepare → commit 往返（产物字节 + hash + 版本/审计入账）
//   - 预备面四类拒载（编译/求值/冒烟/缺依赖）原因原文回传
//   - 预备镜像拓扑序：依赖模块须先经 prepare 备齐（直接 swap 的不算）
//   - commit 交接完整性：空产物/调包（hash 不符）拒载 + fault 入账
//   - commit 冒烟不重跑（预备面已验，owning 面零自检成本）
//   - owning 面依赖在位校验（commit 顺序纪律）
//   - commit 成功 → 冷冻区 → rollback 还原（一次替换对一次回滚）

#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "apollo/script/script_runtime.hpp"

using namespace apollo::script;

namespace {

ScriptConfig phase_a_config() {
    ScriptConfig cfg;
    cfg.instruction_budget = 100000;
    cfg.memory_limit_bytes = 1 << 20;
    return cfg;
}

constexpr const char* kBody = R"(
return {
    add = function(a, b) return a + b end,
    smoke = function() return true end,
}
)";

}  // namespace

// ── 往返与入账 ────────────────────────────────────────────

TEST(ScriptPhaseA, PrepareCommitRoundTrip) {
    ScriptRuntime rt(phase_a_config());
    PreparedModule p = rt.prepare_module({"math", 1, {}}, kBody);
    ASSERT_TRUE(p.ok()) << p.error;
    EXPECT_EQ(p.name, "math");
    EXPECT_EQ(p.version, 1u);
    EXPECT_FALSE(p.bytecode.empty());
    EXPECT_NE(p.bytecode_hash, 0u);
    EXPECT_FALSE(rt.has_module("math"));  // 预备不碰活跃区
    EXPECT_EQ(rt.module_version("math"), 0u);

    EXPECT_TRUE(rt.commit_module(p));
    EXPECT_TRUE(rt.has_module("math"));
    EXPECT_EQ(rt.module_version("math"), 1u);
    std::string out;
    EXPECT_TRUE(rt.eval_debug("x = math.add(2, 3)", out));  // 表已换入活跃区
    EXPECT_EQ(rt.get_global_string("x"), "5");
    EXPECT_EQ(rt.stats().reload_success, 1u);
    EXPECT_EQ(rt.stats().reload_failed, 0u);
    auto history = rt.reload_history();
    ASSERT_EQ(history.size(), 1u);
    EXPECT_TRUE(history.back().success);
    EXPECT_EQ(history.back().from_version, 0u);
    EXPECT_EQ(history.back().to_version, 1u);
}

TEST(ScriptPhaseA, ActiveModulesTrackPreparedVersions) {
    ScriptRuntime rt(phase_a_config());
    ASSERT_TRUE(rt.commit_module(rt.prepare_module({"a", 7, {}}, kBody)));
    ASSERT_TRUE(rt.commit_module(rt.prepare_module({"b", 9, {}}, kBody)));
    auto modules = rt.active_modules();
    ASSERT_EQ(modules.size(), 2u);
    EXPECT_EQ(modules.at("a"), 7u);
    EXPECT_EQ(modules.at("b"), 9u);
}

// ── 预备面拒载（原因原文） ────────────────────────────────

TEST(ScriptPhaseA, PrepareCompileError) {
    ScriptRuntime rt(phase_a_config());
    PreparedModule p = rt.prepare_module({"bad", 1, {}}, "return { = );\n");
    EXPECT_FALSE(p.ok());
    EXPECT_FALSE(p.error.empty());
    EXPECT_TRUE(p.bytecode.empty());
    EXPECT_TRUE(rt.reload_history().empty());  // 预备失败不入 owning 审计环
}

TEST(ScriptPhaseA, PrepareEvalError) {
    ScriptRuntime rt(phase_a_config());
    PreparedModule p = rt.prepare_module({"boom", 1, {}}, "error('nope')\n");
    EXPECT_FALSE(p.ok());
    EXPECT_NE(p.error.find("nope"), std::string::npos);
}

TEST(ScriptPhaseA, PrepareSmokeFail) {
    ScriptRuntime rt(phase_a_config());
    PreparedModule p = rt.prepare_module(
        {"sm", 1, {}}, "return { smoke = function() return false end }\n");
    EXPECT_FALSE(p.ok());
    EXPECT_NE(p.error.find("smoke"), std::string::npos);
}

TEST(ScriptPhaseA, CommitWithoutPrepareRejects) {
    ScriptRuntime rt(phase_a_config());
    std::vector<std::uint8_t> garbage = {'n', 'o', 't', 'l', 'u', 'a'};
    PreparedModule p;
    p.name = "ghost";
    p.version = 1;
    p.bytecode = garbage;
    p.bytecode_hash = 0;  // 故意错配（真值也拒：内容非产物）
    std::vector<ScriptFaultEvent> faults;
    rt.set_fault_sink([&](const ScriptFaultEvent& e) { faults.push_back(e); });
    EXPECT_FALSE(rt.commit_module(p));
    EXPECT_FALSE(rt.has_module("ghost"));
    EXPECT_EQ(rt.stats().reload_failed, 1u);
    auto history = rt.reload_history();
    ASSERT_EQ(history.size(), 1u);
    EXPECT_FALSE(history.back().success);
    ASSERT_EQ(faults.size(), 1u);
    EXPECT_EQ(faults[0].chunk, "ghost");
}

TEST(ScriptPhaseA, BytecodeHashMismatch) {
    ScriptRuntime rt(phase_a_config());
    PreparedModule p = rt.prepare_module({"tamper", 1, {}}, kBody);
    ASSERT_TRUE(p.ok());
    p.bytecode[p.bytecode.size() / 2] ^= 0xFF;  // 调包一个字节
    EXPECT_FALSE(rt.commit_module(p));
    EXPECT_FALSE(rt.has_module("tamper"));
    auto history = rt.reload_history();
    ASSERT_EQ(history.size(), 1u);
    EXPECT_NE(history.back().reason.find("tampered"), std::string::npos);
}

// ── 预备镜像拓扑序 ────────────────────────────────────────

TEST(ScriptPhaseA, PrepareMissingDepOnPreparer) {
    ScriptRuntime rt(phase_a_config());
    PreparedModule p = rt.prepare_module({"app", 1, {"base"}}, kBody);
    EXPECT_FALSE(p.ok());
    EXPECT_NE(p.error.find("missing dependency on preparer: base"),
              std::string::npos);
    EXPECT_FALSE(rt.has_module("app"));  // 未 commit，活跃区干净
}

TEST(ScriptPhaseA, PrepareRejectsDepsNotPrepared) {
    // 控制面纪律：直接 swap 的模块不在预备镜像内——依赖方仍须先经 prepare。
    ScriptRuntime rt(phase_a_config());
    ASSERT_TRUE(rt.swap_module({"base", 1, {}}, kBody));
    PreparedModule p = rt.prepare_module({"app", 1, {"base"}}, kBody);
    EXPECT_FALSE(p.ok());
    EXPECT_NE(p.error.find("missing dependency on preparer"), std::string::npos);
}

TEST(ScriptPhaseA, DepTopoOnPreparer) {
    ScriptRuntime rt(phase_a_config());
    const std::string base_src = R"(
return { v = 1 }
)";
    const std::string app_src = R"(
return { v = base.v + 1, smoke = function() return true end }
)";
    PreparedModule base = rt.prepare_module({"base", 1, {}}, base_src);
    ASSERT_TRUE(base.ok()) << base.error;
    PreparedModule app = rt.prepare_module({"app", 2, {"base"}}, app_src);
    ASSERT_TRUE(app.ok()) << app.error;
    EXPECT_TRUE(rt.commit_module(base));
    EXPECT_TRUE(rt.commit_module(app));
    EXPECT_EQ(rt.module_version("base"), 1u);
    EXPECT_EQ(rt.module_version("app"), 2u);
    std::string out;
    EXPECT_TRUE(rt.eval_debug("v = app.v", out));
    EXPECT_EQ(rt.get_global_string("v"), "2");  // 依赖表已就位
}

TEST(ScriptPhaseA, CommitValidatesDepsOnOwningSide) {
    ScriptRuntime rt(phase_a_config());
    PreparedModule base = rt.prepare_module({"base", 1, {}}, kBody);
    ASSERT_TRUE(base.ok());
    PreparedModule app = rt.prepare_module({"app", 1, {"base"}}, kBody);
    ASSERT_TRUE(app.ok());
    // 反序 commit：依赖未进活跃区 → 拒载（旧表不动、预备镜像不受影响）。
    EXPECT_FALSE(rt.commit_module(app));
    EXPECT_EQ(rt.stats().reload_failed, 1u);
    EXPECT_TRUE(rt.commit_module(base));
    EXPECT_TRUE(rt.commit_module(app));  // 同产物重试即过（幂等交接）
    EXPECT_EQ(rt.module_version("app"), 1u);
}

// ── commit 冒烟不重跑 ─────────────────────────────────────

TEST(ScriptPhaseA, CommitSkipsSmoke) {
    ScriptRuntime rt(phase_a_config());
    const char* src = R"(
smoke_runs = (smoke_runs or 0) + 1
return {
    smoke = function() smoke_runs = smoke_runs + 1; return true end,
}
)";
    PreparedModule p = rt.prepare_module({"counter", 1, {}}, src);
    ASSERT_TRUE(p.ok()) << p.error;
    EXPECT_TRUE(rt.commit_module(p));
    // owning 面只跑了模块体一次（smoke 若在 commit 重跑会变 2）。
    std::string out;
    EXPECT_TRUE(rt.eval_debug("smoke_runs", out));
    EXPECT_EQ(out, "1");
}

// ── 冷冻区与回滚 ──────────────────────────────────────────

TEST(ScriptPhaseA, CommitSuccessThenRollback) {
    ScriptRuntime rt(phase_a_config());
    const std::string v1 = "return { f = function() return 'old' end }\n";
    const std::string v2 = R"(
return { f = function() return 'new' end, smoke = function() return true end }
)";
    ASSERT_TRUE(rt.commit_module(rt.prepare_module({"m", 1, {}}, v1)));
    PreparedModule p2 = rt.prepare_module({"m", 2, {}}, v2);
    ASSERT_TRUE(p2.ok()) << p2.error;
    EXPECT_TRUE(rt.commit_module(p2));
    EXPECT_EQ(rt.module_version("m"), 2u);
    std::string out;
    EXPECT_TRUE(rt.eval_debug("m.f()", out));
    EXPECT_EQ(out, "new");

    EXPECT_TRUE(rt.rollback_module("m"));
    EXPECT_EQ(rt.module_version("m"), 1u);  // 冷冻区换回
    EXPECT_TRUE(rt.eval_debug("m.f()", out));
    EXPECT_EQ(out, "old");
    auto history = rt.reload_history();
    ASSERT_EQ(history.size(), 3u);  // commit v1 + commit v2 + rollback
    EXPECT_TRUE(history[0].success);
    EXPECT_TRUE(history[1].success);
    EXPECT_TRUE(history[2].success);
    EXPECT_EQ(history[2].from_version, 2u);
    EXPECT_EQ(history[2].to_version, 1u);
}
