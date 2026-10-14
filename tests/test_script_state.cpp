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
