// ScriptRuntime 状态面只读内省测试（scripting-lua.md §7.2）。
//
// 覆盖：
//   - GC 面：宿主 allocf 计数（执法真值）+ Lua 自视角 + 增量参数只读查询
//   - 热替换审计环：成功/拒载入账（首次 from=0）、tick 溯源、环形上限
//   - rollback 入账（from/to 翻转）
//   - env 快照：原始类型化取值、条目上限截断、模块缺席、不触元方法、值截断
//   - 装载路径 raw 探测：swap 冒烟/onReload + 契约结构探测不触元方法

#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include "apollo/script/script_runtime.hpp"

using namespace apollo::script;

namespace {

ScriptConfig make_config() {
    ScriptConfig cfg;
    cfg.instruction_budget = 100000;
    cfg.memory_limit_bytes = 1 << 20;
    return cfg;
}

std::string find_entry(const LuaEnvSnapshot& snap, const std::string& key) {
    for (const auto& e : snap.entries) {
        if (e.first == key) {
            return e.second;
        }
    }
    return "";
}

}  // namespace

// ── GC 面 ──────────────────────────────────────────────

TEST(ScriptState, GcStateHostViewAndParams) {
    ScriptRuntime rt(make_config());
    rt.call_string("t", "x = {} for i = 1, 50 do x[i] = tostring(i) end");
    const LuaGcState s = rt.gc_state();
    // 宿主 allocf 计数 = 执法真值，与 memory_used 同源。
    EXPECT_GT(s.used_bytes, 0u);
    EXPECT_EQ(s.used_bytes, rt.memory_used());
    EXPECT_EQ(s.limit_bytes, 1u << 20);
    // GC 自视角（KiB）与增量参数（只读查询：value 传 -1）。
    EXPECT_GT(s.lua_gc_kib, 0u);
    EXPECT_GT(s.gc_pause, 0u);    // 5.5 线默认 250（百分位标度）
    EXPECT_GT(s.gc_stepmul, 0u);  // 默认 200
    EXPECT_GT(s.gc_stepsize, 0u);
    EXPECT_TRUE(s.gc_is_running);  // 增量模式默认运行中
}

// ── 热替换审计环 ──────────────────────────────────────

TEST(ScriptState, ReloadHistorySuccessAndFailure) {
    ScriptRuntime rt(make_config());
    rt.set_tick(5);
    EXPECT_TRUE(rt.swap_module({"m", 41, {}}, "return { version = 41 }"));
    rt.set_tick(7);
    EXPECT_TRUE(rt.swap_module({"m", 42, {}}, "return { version = 42 }"));
    EXPECT_FALSE(rt.swap_module({"m", 43, {"ghost"}}, "return { version = 43 }"));

    const auto& h = rt.reload_history();
    ASSERT_EQ(h.size(), 3u);
    EXPECT_EQ(h[0].name, "m");
    EXPECT_EQ(h[0].from_version, 0u);  // 首次装载
    EXPECT_EQ(h[0].to_version, 41u);
    EXPECT_TRUE(h[0].success);
    EXPECT_TRUE(h[0].reason.empty());
    EXPECT_EQ(h[0].tick, 5u);  // 宿主喂入 tick 溯源
    EXPECT_EQ(h[1].from_version, 41u);
    EXPECT_EQ(h[1].to_version, 42u);
    EXPECT_EQ(h[1].tick, 7u);
    EXPECT_FALSE(h[2].success);
    EXPECT_EQ(h[2].reason, "missing dependency: ghost");
}

TEST(ScriptState, ReloadHistoryFailureReasons) {
    ScriptRuntime rt(make_config());
    rt.swap_module({"quest", 1, {}}, "return { version = 1 }");  // 供依赖校验
    rt.swap_module({"a", 1, {}}, "return { = broken");           // 编译错
    rt.swap_module({"b", 1, {}}, "error('boom')");               // 求值错
    rt.swap_module({"c", 1, {}}, "return 42");                   // 非表
    rt.swap_module({"d", 1, {}},                                 // 冒烟拒
                   "return { smoke = function() return false end }");
    // 超长拒载原因截断（256 + "..."）。
    const std::string long_dep(300, 'z');
    rt.swap_module({"e", 1, {long_dep}}, "return {}");

    const auto& h = rt.reload_history();
    ASSERT_EQ(h.size(), 6u);
    EXPECT_TRUE(h[0].success);
    EXPECT_NE(h[1].reason.find("compile error: "), std::string::npos);
    EXPECT_NE(h[2].reason.find("eval error: "), std::string::npos);
    EXPECT_NE(h[2].reason.find("boom"), std::string::npos);
    EXPECT_EQ(h[3].reason, "module did not return a table");
    EXPECT_EQ(h[4].reason, "module smoke check failed");
    EXPECT_EQ(h[5].reason.size(), 259u);
    EXPECT_EQ(h[5].reason.substr(256), "...");
}

TEST(ScriptState, RollbackRecorded) {
    ScriptRuntime rt(make_config());
    rt.swap_module({"m", 1, {}}, "return { version = 1 }");
    rt.swap_module({"m", 2, {}}, "return { version = 2 }");
    ASSERT_TRUE(rt.rollback_module("m"));

    const auto& h = rt.reload_history();
    ASSERT_EQ(h.size(), 3u);
    EXPECT_TRUE(h[2].success);
    EXPECT_EQ(h[2].from_version, 2u);
    EXPECT_EQ(h[2].to_version, 1u);
    EXPECT_EQ(h[2].reason, "rollback");
}

TEST(ScriptState, ReloadHistoryRingCap) {
    ScriptRuntime rt(make_config());
    for (std::uint64_t i = 1; i <= 70; ++i) {
        ASSERT_TRUE(rt.swap_module({"m", i, {}}, "return { version = " +
                                                       std::to_string(i) + " }"));
    }
    const auto& h = rt.reload_history();
    EXPECT_EQ(h.size(), 64u);  // 环形上限：丢最旧
    EXPECT_EQ(h.front().from_version, 6u);
    EXPECT_EQ(h.front().to_version, 7u);
    EXPECT_EQ(h.back().to_version, 70u);
}

// ── env 快照 ──────────────────────────────────────────

TEST(ScriptState, EnvSnapshotPrimitives) {
    ScriptRuntime rt(make_config());
    ASSERT_TRUE(rt.swap_module({"cfg", 1, {}}, R"(
return {
    str = "hello",
    num = 42,
    ratio = 3.5,
    flag = true,
    [7] = "seven",
    sub = {},
    fn = function() end,
}
)"));
    const LuaEnvSnapshot snap = rt.env_snapshot("cfg", 64);
    EXPECT_TRUE(snap.found);
    EXPECT_FALSE(snap.truncated);
    EXPECT_EQ(snap.entries.size(), 7u);
    EXPECT_EQ(find_entry(snap, "str"), "hello");
    EXPECT_EQ(find_entry(snap, "num"), "42");    // 整数不走字符串转换
    EXPECT_EQ(find_entry(snap, "ratio"), "3.5");
    EXPECT_EQ(find_entry(snap, "flag"), "true");
    EXPECT_EQ(find_entry(snap, "7"), "seven");   // 整数键照常渲染
    EXPECT_EQ(find_entry(snap, "sub"), "<table>");
    EXPECT_EQ(find_entry(snap, "fn"), "<function>");
}

TEST(ScriptState, EnvSnapshotEntryCap) {
    ScriptRuntime rt(make_config());
    ASSERT_TRUE(rt.swap_module({"big", 1, {}}, R"(
return { k1 = 1, k2 = 2, k3 = 3, k4 = 4, k5 = 5,
         k6 = 6, k7 = 7, k8 = 8, k9 = 9, k10 = 10 }
)"));
    const LuaEnvSnapshot capped = rt.env_snapshot("big", 3);
    EXPECT_TRUE(capped.found);
    EXPECT_TRUE(capped.truncated);
    EXPECT_EQ(capped.entries.size(), 3u);
    const LuaEnvSnapshot full = rt.env_snapshot("big", 20);
    EXPECT_TRUE(full.found);
    EXPECT_FALSE(full.truncated);
    EXPECT_EQ(full.entries.size(), 10u);
}

TEST(ScriptState, EnvSnapshotAbsentModule) {
    ScriptRuntime rt(make_config());
    const LuaEnvSnapshot snap = rt.env_snapshot("nope", 16);
    EXPECT_FALSE(snap.found);
    EXPECT_TRUE(snap.entries.empty());
    // 零上限与空名恒未找到。
    rt.swap_module({"cfg", 1, {}}, "return { a = 1 }");
    EXPECT_FALSE(rt.env_snapshot("cfg", 0).found);
    EXPECT_FALSE(rt.env_snapshot("", 16).found);
    // 同名但非表的全局面（call_string 挂全局标量）→ 未找到。
    rt.call_string("t", "scalar = 7");
    EXPECT_FALSE(rt.env_snapshot("scalar", 16).found);
}

TEST(ScriptState, EnvSnapshotSkipsMetamethods) {
    ScriptRuntime rt(make_config());
    // __pairs/__index/__tostring 一旦被触即 error——raw 遍历 + 类型化取值
    // 全程不调 Lua 函数、不经元方法（§7.2 只读纪律）。
    ASSERT_TRUE(rt.swap_module({"m", 1, {}}, R"(
local t = { real = "value", n = 5 }
setmetatable(t, {
    __pairs = function() error("__pairs must not fire") end,
    __index = function() error("__index must not fire") end,
    __tostring = function() error("__tostring must not fire") end,
})
return t
)"));
    const LuaEnvSnapshot snap = rt.env_snapshot("m", 16);
    EXPECT_TRUE(snap.found);
    EXPECT_EQ(snap.entries.size(), 2u);
    EXPECT_EQ(find_entry(snap, "real"), "value");
    EXPECT_EQ(find_entry(snap, "n"), "5");
}

TEST(ScriptState, EnvSnapshotValueCap) {
    ScriptRuntime rt(make_config());
    ASSERT_TRUE(rt.swap_module({"m", 1, {}},
                               "return { big = string.rep('a', 1000) }"));
    const LuaEnvSnapshot snap = rt.env_snapshot("m", 16);
    EXPECT_TRUE(snap.found);
    // 超长值截断：前 512 字符 + 截断标记。
    const std::string v = find_entry(snap, "big");
    EXPECT_EQ(v.size(), 512u + 14u);
    EXPECT_EQ(v.substr(0, 512), std::string(512, 'a'));
    EXPECT_EQ(v.substr(512), "...(truncated)");
}

// ── 装载路径 raw 探测（swap/契约共用纪律） ──────────────

TEST(ScriptState, ContractLoadSurvivesHostileMetatable) {
    ScriptRuntime rt(make_config());
    ASSERT_TRUE(rt.call_string("env", "function onChat() end"));
    // routes/write_whitelist/version 结构探测与数组读全 raw——__index
    // 恶意 error 不再把错误抛出保护区（裸 lua_getfield 会崩进程）。
    EXPECT_TRUE(rt.load_contract(R"(
local t = setmetatable(
    { version = 1, routes = { [1001] = 'onChat' }, write_whitelist = { 3 } },
    { __index = function() error("__index must not fire") end })
return t
)"));
    EXPECT_EQ(rt.contract_version(), 1u);
    const auto routes = rt.contract_routes();
    ASSERT_EQ(routes.size(), 1u);
    EXPECT_EQ(routes.at(1001), "onChat");
    EXPECT_EQ(rt.contract_write_whitelist(), (std::vector<std::uint64_t>{3}));
}

// ── 宿主侧 global 访问 raw 纪律（_ENV 元表可被脚本运行期挂上） ──

TEST(ScriptState, HostileEnvMetatableSurvivesHostPaths) {
    ScriptRuntime rt(make_config());
    ASSERT_TRUE(rt.swap_module({"m", 1, {}}, "return { f = function() end }"));
    // _ENV 挂 __index/__newindex 恶意 error——此后宿主侧一切 global
    // 访问（call_module/get_global_*/set_global_string/swap 提交）走 raw。
    ASSERT_TRUE(rt.call_string("poison", R"(
setmetatable(_ENV, {
    __index = function() error("__index must not fire") end,
    __newindex = function() error("__newindex must not fire") end,
})
)"));
    EXPECT_TRUE(rt.call_module("m", "f"));                  // raw 查模块 + 查函数
    EXPECT_FALSE(rt.call_module("m", "ghost"));             // 缺 handler 记 fault
    EXPECT_FALSE(rt.call_global("nosuch"));
    EXPECT_EQ(rt.get_global_string("x"), "");
    EXPECT_FALSE(rt.get_global_bool("x"));
    rt.set_global_string("x", "v");                         // raw 写不经 __newindex
    EXPECT_EQ(rt.get_global_string("x"), "v");
    EXPECT_TRUE(rt.swap_module({"m", 2, {}}, "return {}"));  // 换表提交 raw
    EXPECT_TRUE(rt.env_snapshot("m", 4).found);             // 快照面 raw
}

TEST(ScriptState, HostileModuleMetatableSurvivesCallPath) {
    ScriptRuntime rt(make_config());
    ASSERT_TRUE(rt.swap_module({"m", 1, {}}, "return { f = function() end }"));
    ASSERT_TRUE(rt.swap_module({"m", 2, {}}, "return { f = function() end }"));  // 冷冻区 = v1
    // 装载后模块表被挂恶意元表（load 期探测管不到运行期改表）。
    ASSERT_TRUE(rt.call_string("poison", R"(
setmetatable(m, { __index = function() error("__index must not fire") end })
)"));
    EXPECT_TRUE(rt.call_module("m", "f"));       // 自带字段 raw 命中
    EXPECT_FALSE(rt.call_module("m", "ghost"));  // 缺 handler 记 fault 不崩
    // 契约路由完整性闸经 resolve_handler：hostile 模块缺函数 → 拒换不崩。
    ASSERT_TRUE(rt.call_string("env", "function onChat() end"));
    EXPECT_FALSE(rt.load_contract(
        "return { routes = { [1001] = 'm.ghost' } }"));
    EXPECT_TRUE(rt.rollback_module("m"));        // 回滚提交 raw
    EXPECT_EQ(rt.module_version("m"), 1u);
}

TEST(ScriptState, HostPathsKeepStackBalanced) {
    // raw_setglobal 栈语义回归：宿主写/读全局后栈零残留（残留值会污染
    // eval 语句路径的结果面——sanitizer 树先行暴露）。
    ScriptRuntime rt(make_config());
    rt.set_global_string("a", "1");
    rt.set_global_string("b", "2");
    EXPECT_EQ(rt.get_global_string("a"), "1");
    ASSERT_TRUE(rt.swap_module({"m", 1, {}}, "return {}"));
    ASSERT_TRUE(rt.swap_module({"m", 2, {}}, "return {}"));
    ASSERT_TRUE(rt.rollback_module("m"));
    std::string out;
    EXPECT_TRUE(rt.eval_debug("x = 5", out));
    EXPECT_TRUE(out.empty());  // 语句路径无返回值（残留栈会使此断言红）
    EXPECT_EQ(lua_gettop(rt.raw()), 0);
}

// ── §7.3 profile 归因（调用边界分桶 + 超标事件状态面） ──

namespace {
const ProfileBucket* find_bucket(const std::vector<ProfileBucket>& snap,
                                 const std::string& chunk) {
    for (const auto& b : snap) {
        if (b.chunk == chunk) {
            return &b;
        }
    }
    return nullptr;
}
}  // namespace

TEST(ScriptState, ProfileCallStringBucket) {
    ScriptRuntime rt(make_config());
    EXPECT_TRUE(rt.call_string("hot", "for i = 1, 15000 do end"));  // 预算内过 10k 量化步
    const auto snap = rt.profile_snapshot();
    const auto* b = find_bucket(snap, "hot");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->calls, 1u);
    EXPECT_GT(b->instructions, 0u);   // 粗检 10k 步进量化
    EXPECT_GT(b->wall_us, 0u);        // 宿主侧 steady 掐表
}

TEST(ScriptState, ProfileCallModuleBucket) {
    ScriptRuntime rt(make_config());
    ASSERT_TRUE(rt.swap_module({"quest", 1, {}}, "return { onTick = function() end }"));
    EXPECT_TRUE(rt.call_module("quest", "onTick"));
    EXPECT_TRUE(rt.call_module("quest", "onTick"));
    EXPECT_FALSE(rt.call_module("quest", "ghost"));  // 查找失败不执行不入账
    const auto snap = rt.profile_snapshot();
    const auto* b = find_bucket(snap, "quest");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->calls, 2u);
}

TEST(ScriptState, ProfileCallRouteBucket) {
    ScriptRuntime rt(make_config());
    ASSERT_TRUE(rt.call_string("env",
        "function onChat() end\n"
        "quest = { onStart = function() end }\n"));
    ASSERT_TRUE(rt.load_contract(
        "return { routes = { [1001] = 'onChat', [1002] = 'quest.onStart' } }"));
    EXPECT_TRUE(rt.call_route(1001));
    EXPECT_TRUE(rt.call_route(1002));
    const auto snap = rt.profile_snapshot();
    EXPECT_EQ(find_bucket(snap, "onChat")->calls, 1u);          // 按 handler 名入账
    EXPECT_EQ(find_bucket(snap, "quest.onStart")->calls, 1u);
}

TEST(ScriptState, ProfileAttrHookAndEvalBuckets) {
    ScriptRuntime rt(make_config());
    ASSERT_TRUE(rt.call_string("env", "function myHook() return nil end"));
    ASSERT_TRUE(rt.set_attr_hook("myHook"));
    std::vector<ScriptRuntime::AttrChange> changes{
        {1, 7, 0, 1}, {2, 7, 0, 2}};
    rt.run_attr_hook_window(changes, {7}, 1000000);
    std::string out;
    EXPECT_TRUE(rt.eval_debug("1 + 2", out));
    const auto snap = rt.profile_snapshot();
    EXPECT_EQ(find_bucket(snap, "myHook")->calls, 2u);       // 钩子按注册名入账
    EXPECT_EQ(find_bucket(snap, "debug_eval")->calls, 1u);
}

TEST(ScriptState, ProfileOverBudgetRecorded) {
    ScriptConfig cfg = make_config();
    cfg.instruction_budget = 50000;
    ScriptRuntime rt(cfg);
    EXPECT_FALSE(rt.call_string("spin", "while true do end"));  // 预算中止
    const auto& hist = rt.over_budget_history();
    ASSERT_EQ(hist.size(), 1u);
    EXPECT_EQ(hist[0].chunk, "spin");
    EXPECT_GT(hist[0].consumed, 0u);              // 当次指令消耗
    EXPECT_EQ(hist[0].budget, 50000u);
}

TEST(ScriptState, ProfileOverBudgetRingCap) {
    ScriptConfig cfg = make_config();
    cfg.instruction_budget = 50000;
    ScriptRuntime rt(cfg);
    for (int i = 0; i < 70; ++i) {
        rt.set_tick(static_cast<std::uint64_t>(i));
        EXPECT_FALSE(rt.call_string("spin", "while true do end"));
    }
    EXPECT_EQ(rt.over_budget_history().size(), 64u);
    EXPECT_EQ(rt.over_budget_history().front().tick, 6u);  // 丢最旧
}

TEST(ScriptState, ProfileSnapshotCopyAndReset) {
    ScriptRuntime rt(make_config());
    rt.call_string("boot", "x = 1");
    auto snap = rt.profile_snapshot();
    ASSERT_EQ(snap.size(), 1u);
    rt.profile_reset();
    EXPECT_TRUE(rt.profile_snapshot().empty());
    EXPECT_EQ(snap.size(), 1u);  // 快照是拷贝，reset 不追溯
}
