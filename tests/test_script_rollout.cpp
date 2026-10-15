// §3.2 Phase A 工程面投递编排：一次预备（独立预备沙盒）→ 多目标（per 场景
// 线程 Scene×ScriptRuntime）分步投递 → Phase C 观察判定 → 全量推广/回滚。
// pending_* 队列即宿主拆投面（把每步投进对应目标场景线程任务队列，§7.1
// attach 同型）；本测试单线程直驱 = 该路径的同步形态。

#include "apollo/script/lua_bytecode_cache.hpp"  // fnv1a64 / to_hex（e2e manifest）
#include "apollo/script/module_canary.hpp"
#include "apollo/script/module_rollout.hpp"
#include "apollo/script/patch_loader.hpp"
#include "apollo/script/script_runtime.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace apollo::script;

namespace {

ScriptConfig rollout_config() {
    ScriptConfig c;
    c.instruction_budget = 100000;
    c.memory_limit_bytes = 1 << 20;
    return c;
}

RolloutModule mod(std::string name, std::uint64_t version,
                  std::vector<std::string> deps, std::string source) {
    RolloutModule m;
    m.name = std::move(name);
    m.version = version;
    m.deps = std::move(deps);
    m.source = std::move(source);
    return m;
}

const char* kGood =
    "return { onTick = function() end, smoke = function() return true end }";
const char* kBadOnTick =
    "return { onTick = function() error('boom') end, smoke = function() return true end }";

// 目标集群：N 个独立 runtime（Scene×ScriptRuntime 桥后形态——一场景线程一状态）。
// 各 runtime 同配置批量建（target 指针指向这些实例，禁建后替换）。
struct Fleet {
    std::vector<std::unique_ptr<ScriptRuntime>> rt;
    std::vector<RolloutTarget> targets;
    Fleet(std::size_t n, ScriptConfig cfg = rollout_config()) {
        for (std::size_t i = 0; i < n; ++i) {
            rt.push_back(std::make_unique<ScriptRuntime>(cfg));
            targets.push_back({"s" + std::to_string(i), rt.back().get()});
        }
    }
    ScriptRuntime& at(std::size_t i) { return *rt[i]; }
    std::size_t idx(const std::string& key) const {
        return static_cast<std::size_t>(std::stoul(key.substr(1)));
    }
};

// 驱动一帧观察：仅灰度目标推 tick + 驱 handler（owning 线程动作，宿主喂
// runtime tick 的同型镜像），随后逐灰度目标推进观察窗。
void frame(ModuleRollout& r, Fleet& f, const std::string& module) {
    for (const auto& s : r.statuses()) {
        if (!s.canary) continue;
        ScriptRuntime& rt = f.at(f.idx(s.key));
        rt.set_tick(rt.current_tick() + 1);
        (void)rt.call_module(module, "onTick");
        r.observe_target(s.key);
    }
}

class Rollout : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = std::filesystem::temp_directory_path() / "apollo_rollout_test";
        std::filesystem::remove_all(dir_);
        std::filesystem::create_directories(dir_);
    }
    void TearDown() override { std::filesystem::remove_all(dir_); }

    std::filesystem::path dir_;
};

}  // namespace

// ── 主回路 ──────────────────────────────────────────────

TEST_F(Rollout, HealthyCanaryPromotesRest) {
    Fleet f(4);  // s0/s1 灰度；s2/s3 待推广
    for (auto& rt : f.rt) ASSERT_TRUE(rt->swap_module({"math", 1, {}}, kGood));
    ModuleRollout r(f.targets, rollout_config());
    EXPECT_TRUE(r.prepare({mod("math", 2, {}, kGood)}, {"s0", "s1"},
                          CanaryParams{5, 0}));
    EXPECT_EQ(r.state(), RolloutState::kCanaryPending);
    EXPECT_EQ(r.pending_canary().size(), 2u);

    r.commit_target("s0");
    EXPECT_EQ(r.state(), RolloutState::kCanaryPending);  // 余灰度目标未投
    for (const auto& k : r.pending_canary()) r.commit_target(k);
    EXPECT_EQ(r.state(), RolloutState::kObserving);
    EXPECT_EQ(f.at(0).module_version("math"), 2u);
    EXPECT_EQ(f.at(1).module_version("math"), 2u);
    EXPECT_EQ(f.at(2).module_version("math"), 1u);  // 判定前全量目标不动

    for (int i = 0; i < 4; ++i) frame(r, f, "math");
    EXPECT_EQ(r.state(), RolloutState::kObserving);  // 窗 5 拍未满
    frame(r, f, "math");
    EXPECT_EQ(r.state(), RolloutState::kPromoting);
    EXPECT_EQ(r.pending_promote().size(), 2u);
    for (const auto& k : r.pending_promote()) r.commit_target(k);

    EXPECT_EQ(r.state(), RolloutState::kPromoted);
    for (std::size_t i = 0; i < 4; ++i) EXPECT_EQ(f.at(i).module_version("math"), 2u);
    EXPECT_TRUE(r.errors().empty());
    for (const auto& s : r.statuses()) EXPECT_EQ(s.committed, true);
}

TEST_F(Rollout, UnhealthyCanaryFailsFastAndRollsBack) {
    Fleet f(2);  // s0 灰度；s1 全量（判坏后不得被触碰）
    for (auto& rt : f.rt) ASSERT_TRUE(rt->swap_module({"math", 1, {}}, kGood));
    ModuleRollout r(f.targets, rollout_config());
    ASSERT_TRUE(r.prepare({mod("math", 2, {}, kBadOnTick)}, {"s0"},
                           CanaryParams{600, 0}));
    for (const auto& k : r.pending_canary()) r.commit_target(k);
    ASSERT_EQ(r.state(), RolloutState::kObserving);
    ASSERT_EQ(f.at(0).module_version("math"), 2u);

    frame(r, f, "math");  // v2 onTick error → 归因 fault → 超阈（0）早失败
    EXPECT_EQ(r.state(), RolloutState::kRollingBack);
    EXPECT_EQ(r.pending_rollback(), std::vector<std::string>({"s0"}));
    for (const auto& k : r.pending_rollback()) r.rollback_target(k);

    EXPECT_EQ(r.state(), RolloutState::kRolledBack);
    EXPECT_EQ(f.at(0).module_version("math"), 1u);  // 灰度目标回灰度前
    EXPECT_EQ(f.at(1).module_version("math"), 1u);  // 全量目标未被触碰
    const auto st = r.statuses();
    ASSERT_EQ(st.size(), 2u);
    EXPECT_EQ(st[0].rolled_back, true);
    EXPECT_GE(st[0].window_faults, 1u);  // 观察面：窗内归因 fault 入账
    EXPECT_FALSE(st[1].committed);
    EXPECT_TRUE(st[1].errors.empty());  // 未投递目标无错误行
}

TEST_F(Rollout, DepsCommittedInTopologicalOrder) {
    Fleet f(2);
    // 输入序故意逆拓扑：app（依赖 base）在前——编排须先备后投依赖方。
    const std::vector<RolloutModule> set = {
        mod("app", 2, {"base"}, kGood),
        mod("base", 2, {}, kGood),
    };
    ModuleRollout r(f.targets, rollout_config());
    EXPECT_TRUE(r.prepare(set, {"s0"}, CanaryParams{2, 0}));
    for (const auto& k : r.pending_canary()) r.commit_target(k);
    // 逆序 commit 会让 app 因缺 base 拒载 → 判坏回滚；此处须观察相位存活。
    ASSERT_EQ(r.state(), RolloutState::kObserving);
    EXPECT_EQ(f.at(0).module_version("base"), 2u);
    EXPECT_EQ(f.at(0).module_version("app"), 2u);
    frame(r, f, "app");
    frame(r, f, "app");
    EXPECT_EQ(r.state(), RolloutState::kPromoting);
    for (const auto& k : r.pending_promote()) r.commit_target(k);
    EXPECT_EQ(r.state(), RolloutState::kPromoted);
    EXPECT_EQ(f.at(1).module_version("base"), 2u);
    EXPECT_EQ(f.at(1).module_version("app"), 2u);
    EXPECT_TRUE(r.errors().empty());
}

// ── 预备/装载失败面 ─────────────────────────────────────

TEST_F(Rollout, PrepareFailureDeliversNothing) {
    Fleet f(2);
    ModuleRollout r(f.targets, rollout_config());
    EXPECT_FALSE(r.prepare({mod("app", 1, {"ghost"}, kGood)}, {"s0"},
                           CanaryParams{5, 0}));
    EXPECT_EQ(r.state(), RolloutState::kFailed);
    EXPECT_FALSE(r.errors().empty());
    EXPECT_TRUE(r.pending_canary().empty());
    for (auto& rt : f.rt) EXPECT_EQ(rt->module_version("app"), 0u);
}

TEST_F(Rollout, InvalidArgumentsRefusedWithoutDelivery) {
    Fleet f(2);
    ModuleRollout r(f.targets, rollout_config());
    EXPECT_FALSE(r.prepare({}, {"s0"}, CanaryParams{5, 0}));  // 空模块集
    EXPECT_FALSE(r.prepare({mod("m", 1, {}, kGood)}, {}, CanaryParams{5, 0}));  // 空灰度集
    EXPECT_FALSE(r.prepare({mod("m", 1, {}, kGood)}, {"ghost"},
                           CanaryParams{5, 0}));  // 未知灰度键
    EXPECT_FALSE(r.prepare({mod("m", 1, {}, kGood), mod("m", 2, {}, kGood)},
                           {"s0"}, CanaryParams{5, 0}));  // 重名
    EXPECT_FALSE(r.prepare({mod("a", 1, {"b"}, kGood), mod("b", 1, {"a"}, kGood)},
                           {"s0"}, CanaryParams{5, 0}));  // 依赖环
    EXPECT_EQ(r.state(), RolloutState::kFailed);
    EXPECT_FALSE(r.errors().empty());
    for (auto& rt : f.rt) EXPECT_TRUE(rt->active_modules().empty());
}

TEST_F(Rollout, NullRuntimeTargetRefused) {
    ModuleRollout r({{"ghost", nullptr}}, rollout_config());
    EXPECT_FALSE(r.prepare({mod("m", 1, {}, kGood)}, {"ghost"},
                           CanaryParams{5, 0}));
    EXPECT_EQ(r.state(), RolloutState::kFailed);
}

// ── 多模块集与复用 ──────────────────────────────────────

TEST_F(Rollout, FaultInAnySetModuleRollsBackWholeSet) {
    Fleet f(1);  // 整集单灰度目标
    ASSERT_TRUE(f.at(0).swap_module({"base", 1, {}}, kGood));
    ASSERT_TRUE(f.at(0).swap_module({"app", 1, {"base"}}, kGood));
    ModuleRollout r(f.targets, rollout_config());
    // 整集换 v2：base 健康、app 的 onTick 判坏——任一模块 fault 即整集回滚。
    ASSERT_TRUE(r.prepare({mod("base", 2, {}, kGood),
                           mod("app", 2, {"base"}, kBadOnTick)},
                          {"s0"}, CanaryParams{600, 0}));
    for (const auto& k : r.pending_canary()) r.commit_target(k);
    ASSERT_EQ(r.state(), RolloutState::kObserving);
    frame(r, f, "app");  // app 归因 fault → 早失败
    EXPECT_EQ(r.state(), RolloutState::kRollingBack);
    for (const auto& k : r.pending_rollback()) r.rollback_target(k);
    EXPECT_EQ(r.state(), RolloutState::kRolledBack);
    EXPECT_EQ(f.at(0).module_version("base"), 1u);
    EXPECT_EQ(f.at(0).module_version("app"), 1u);
}

TEST_F(Rollout, CanaryCommitFailureFailsRollout) {
    Fleet f(2);
    ModuleRollout r(f.targets, rollout_config());
    // 第 1 轮：base 判坏回滚。预备面保留 base；两目标最终无活跃 base
    // （s0 新装无冷冻区，rollback_module 拒动并记错误——既有面语义）。
    ASSERT_TRUE(r.prepare({mod("base", 1, {}, kBadOnTick)}, {"s0"},
                           CanaryParams{600, 0}));
    for (const auto& k : r.pending_canary()) r.commit_target(k);
    ASSERT_EQ(r.state(), RolloutState::kObserving);
    frame(r, f, "base");
    ASSERT_EQ(r.state(), RolloutState::kRollingBack);
    for (const auto& k : r.pending_rollback()) r.rollback_target(k);
    ASSERT_EQ(r.state(), RolloutState::kRolledBack);

    // 第 2 轮（同编排器复用；预备面已有 base）：app 依赖 base，灰度选 s1
    // ——s1 无 base → commit 拒载（missing dependency）→ 灰度面装载失败
    // = 判坏，转回滚相位（无可回滚面亦须走完相位）。
    EXPECT_TRUE(r.prepare({mod("app", 1, {"base"}, kGood)}, {"s1"},
                          CanaryParams{5, 0}));
    EXPECT_EQ(r.state(), RolloutState::kCanaryPending);
    for (const auto& k : r.pending_canary()) r.commit_target(k);
    EXPECT_EQ(r.state(), RolloutState::kRollingBack);
    EXPECT_EQ(r.pending_rollback(), std::vector<std::string>({"s1"}));
    for (const auto& k : r.pending_rollback()) r.rollback_target(k);
    EXPECT_EQ(r.state(), RolloutState::kRolledBack);
    EXPECT_EQ(f.at(1).module_version("app"), 0u);
    const auto st = r.statuses();
    ASSERT_EQ(st.size(), 2u);
    EXPECT_FALSE(st[1].errors.empty());  // 缺依赖错误行进 statuses 面
}

TEST_F(Rollout, PromoteFailureRecordedPerTarget) {
    Fleet f(2);
    ModuleRollout r(f.targets, rollout_config());
    // 第 1 轮：base 判坏回滚 → s0 留 base（新装无冷冻区），s1 从未装。
    ASSERT_TRUE(r.prepare({mod("base", 1, {}, kBadOnTick)}, {"s0"},
                           CanaryParams{600, 0}));
    for (const auto& k : r.pending_canary()) r.commit_target(k);
    ASSERT_EQ(r.state(), RolloutState::kObserving);
    frame(r, f, "base");
    ASSERT_EQ(r.state(), RolloutState::kRollingBack);
    for (const auto& k : r.pending_rollback()) r.rollback_target(k);
    ASSERT_EQ(r.state(), RolloutState::kRolledBack);

    // 第 2 轮：app 依赖 base，灰度 s0（有 base）健康 → 判好进入推广相位；
    // s1 无 base → 推广 commit 拒载（per-target 错误，不改 verdict）。
    ASSERT_TRUE(r.prepare({mod("app", 1, {"base"}, kGood)}, {"s0"},
                           CanaryParams{3, 0}));
    for (const auto& k : r.pending_canary()) r.commit_target(k);
    ASSERT_EQ(r.state(), RolloutState::kObserving);
    for (int i = 0; i < 3; ++i) frame(r, f, "app");
    ASSERT_EQ(r.state(), RolloutState::kPromoting);
    for (const auto& k : r.pending_promote()) r.commit_target(k);
    EXPECT_EQ(r.state(), RolloutState::kPromoted);  // 判好结论不改
    EXPECT_EQ(f.at(0).module_version("app"), 1u);
    EXPECT_EQ(f.at(1).module_version("app"), 0u);
    const auto st = r.statuses();
    ASSERT_EQ(st.size(), 2u);
    EXPECT_TRUE(st[0].errors.empty());
    EXPECT_FALSE(st[1].errors.empty());  // s1 缺依赖，重试面交宿主
}

// ── 持久化纪律（§3.2 回滚要持久化 current 版本指针） ────────

TEST_F(Rollout, RollbackRewindsPersistedCurrentPointer) {
    const std::string store = (dir_ / "current_versions").string();
    ScriptConfig cfg = rollout_config();
    cfg.version_store_path = store;
    Fleet f(1, cfg);
    ASSERT_TRUE(f.at(0).swap_module({"math", 1, {}}, kGood));  // 指针落 v1
    ModuleRollout r(f.targets, rollout_config());
    ASSERT_TRUE(r.prepare({mod("math", 2, {}, kBadOnTick)}, {"s0"},
                           CanaryParams{600, 0}));
    for (const auto& k : r.pending_canary()) r.commit_target(k);
    ASSERT_EQ(r.state(), RolloutState::kObserving);
    frame(r, f, "math");
    ASSERT_EQ(r.state(), RolloutState::kRollingBack);
    for (const auto& k : r.pending_rollback()) r.rollback_target(k);
    ASSERT_EQ(r.state(), RolloutState::kRolledBack);
    // 回滚即把「最新已知健康版本」改回 v1——崩溃循环防护纪律在投递编排层落点。
    EXPECT_EQ(f.at(0).current_version_manifest().at("math"), 1u);
}

// ── 端到端：patch 目录 → 编排（Phase A 工程面 1+2 步合成） ──

TEST_F(Rollout, EndToEndFromPatchDirectory) {
    const std::string src =
        "return { onTick = function() end, smoke = function() return true end }";
    {
        std::ofstream out(dir_ / "math.lua", std::ios::binary | std::ios::trunc);
        out << src;
    }
    const std::uint64_t hash =
        fnv1a64(reinterpret_cast<const std::uint8_t*>(src.data()), src.size());
    {
        std::ofstream out(dir_ / "manifest", std::ios::binary | std::ios::trunc);
        out << "math\t" << to_hex(hash) << '\t' << to_hex(hash)
            << "\tsig-current\n";
    }
    auto loaded = load_patch_directory(dir_.string());
    ASSERT_TRUE(loaded.errors.empty())
        << (loaded.errors.empty() ? "" : loaded.errors.front());
    ASSERT_EQ(loaded.modules.size(), 1u);
    EXPECT_TRUE(loaded.modules[0].bytecode.empty());  // 无 luac = 纯源码回退

    Fleet f(2);
    ModuleRollout r(f.targets, rollout_config());
    const std::vector<RolloutModule> set = {
        mod("math", 2, {}, loaded.modules[0].source),
    };
    EXPECT_TRUE(r.prepare(set, {"s0"}, CanaryParams{3, 0}));
    for (const auto& k : r.pending_canary()) r.commit_target(k);
    ASSERT_EQ(r.state(), RolloutState::kObserving);
    for (int i = 0; i < 3; ++i) frame(r, f, "math");
    ASSERT_EQ(r.state(), RolloutState::kPromoting);
    for (const auto& k : r.pending_promote()) r.commit_target(k);
    EXPECT_EQ(r.state(), RolloutState::kPromoted);
    for (std::size_t i = 0; i < 2; ++i) EXPECT_EQ(f.at(i).module_version("math"), 2u);
}

// ── 相位纪律 ───────────────────────────────────────────

TEST_F(Rollout, StepsOutOfPhaseAreInert) {
    Fleet f(2);
    ModuleRollout r(f.targets, rollout_config());
    EXPECT_EQ(r.state(), RolloutState::kIdle);
    r.observe_target("s0");  // 未预备：观察空转
    EXPECT_EQ(r.state(), RolloutState::kIdle);
    r.commit_target("s0");
    r.rollback_target("s0");
    EXPECT_EQ(r.state(), RolloutState::kIdle);

    ASSERT_TRUE(r.prepare({mod("math", 1, {}, kGood)}, {"s0"},
                          CanaryParams{1, 0}));
    r.observe_target("s1");  // 非灰度目标：空转
    for (const auto& k : r.pending_canary()) r.commit_target(k);
    ASSERT_EQ(r.state(), RolloutState::kObserving);
    frame(r, f, "math");
    EXPECT_EQ(r.state(), RolloutState::kPromoting);
    r.observe_target("s0");  // 推广相位：观察空转
    r.commit_target("s0");   // 非推广目标：空转
    for (const auto& k : r.pending_promote()) r.commit_target(k);
    ASSERT_EQ(r.state(), RolloutState::kPromoted);
    frame(r, f, "math");           // 终态后驱动：空转
    r.rollback_target("s0");      // 终态后回滚：空转
    EXPECT_EQ(r.state(), RolloutState::kPromoted);
    for (std::size_t i = 0; i < 2; ++i) EXPECT_EQ(f.at(i).module_version("math"), 1u);
}
