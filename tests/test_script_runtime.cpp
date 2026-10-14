// ScriptRuntime 沙盒宿主测试（scripting-lua.md §2/§4/§6）。
//
// 覆盖：
//   - 标准库白名单（base/string/table/math/utf8/coroutine 可用）
//   - 沙盒缺省（io/os/debug/dofile/loadfile/collectgarbage 不可见）
//   - load 只收文本（二进制 chunk 拒绝）
//   - 指令预算中断（死循环中止、状态可恢复）
//   - 内存上限（超限 error，不 OOM 进程）
//   - pcall 隔离（运行时错误不崩进程，计数入 fault）
//   - 具名模块注入（§4.1 启动期注入唯一通道）

#include <string>

#include <gtest/gtest.h>
#include <lua.hpp>

#include "apollo/script/script_runtime.hpp"

using namespace apollo::script;

namespace {

ScriptConfig make_config() {
    ScriptConfig cfg;
    cfg.instruction_budget = 100000;  // 死循环必超
    cfg.memory_limit_bytes = 1 << 20;
    return cfg;
}

}  // namespace

// ── 白名单库 ──────────────────────────────────────────────

TEST(ScriptRuntime, WhitelistLibrariesAvailable) {
    ScriptRuntime rt(make_config());
    EXPECT_TRUE(rt.call_string("t", R"(
result = type(string) == "table" and type(table) == "table"
      and type(math) == "table" and type(coroutine) == "table"
)"));
    EXPECT_TRUE(rt.get_global_bool("result"));
}

TEST(ScriptRuntime, SandboxOmitsOsIoDebug) {
    ScriptRuntime rt(make_config());
    EXPECT_TRUE(rt.call_string("t", R"(
result = os == nil and io == nil and debug == nil
      and dofile == nil and loadfile == nil and collectgarbage == nil
)"));
    EXPECT_TRUE(rt.get_global_bool("result"));
}

TEST(ScriptRuntime, LoadRejectsBytecode) {
    ScriptRuntime rt(make_config());
    // 二进制 chunk 签名（\x1bLua）——load 只收文本（§2：一切字节串按源码处理；
    // 预编译产物走 §3.5 缓存装载面（manifest 三元组校验），不经 `load`）
    const std::string chunk = std::string("\x1bLua garbage-not-real-bytecode", 28);
    EXPECT_FALSE(rt.call_string("b", chunk));
    EXPECT_EQ(rt.stats().fault_syntax, 1u);
}

// ── 指令预算 ──────────────────────────────────────────────

TEST(ScriptRuntime, InstructionBudgetInterruptsLoop) {
    ScriptRuntime rt(make_config());
    EXPECT_FALSE(rt.call_string("loop", "while true do end"));
    EXPECT_EQ(rt.stats().fault_budget, 1u);
    // 中断后状态可恢复（错误隔离，不崩进程）
    rt.reset_budget();
    EXPECT_TRUE(rt.call_string("ok", "result = 1"));
    EXPECT_EQ(rt.get_global_string("result"), "1");
}

TEST(ScriptRuntime, BudgetRechargesPerCall) {
    ScriptRuntime rt(make_config());
    // 死循环两次调用各自独立耗尽预算
    EXPECT_FALSE(rt.call_string("l1", "while true do end"));
    EXPECT_FALSE(rt.call_string("l2", "while true do end"));
    EXPECT_EQ(rt.stats().fault_budget, 2u);
}

// ── 内存上限 ──────────────────────────────────────────────

TEST(ScriptRuntime, MemoryLimitEnforced) {
    ScriptRuntime rt(make_config());
    // 1MB 上限下灌 100 万条 100 字节串必然超限
    EXPECT_FALSE(rt.call_string("oom",
        "local t = {} for i = 1, 1000000 do t[i] = string.rep('x', 100) end"));
    // 超限后 Lua 会继续重试分配（含错误消息本身），计数 ≥1 而非精确 1
    EXPECT_GE(rt.stats().fault_memory, 1u);
    // 超限后状态可继续（后续小调用正常）
    rt.reset_budget();
    EXPECT_TRUE(rt.call_string("ok", "result = 2"));
    EXPECT_EQ(rt.get_global_string("result"), "2");
}

// ── 错误隔离 ──────────────────────────────────────────────

TEST(ScriptRuntime, PcallIsolatesRuntimeError) {
    ScriptRuntime rt(make_config());
    EXPECT_FALSE(rt.call_string("bad", "error('boom')"));
    EXPECT_EQ(rt.stats().fault_runtime, 1u);
    // 之后状态健康可继续
    EXPECT_TRUE(rt.call_string("ok", "result = 3"));
    EXPECT_EQ(rt.get_global_string("result"), "3");
}

TEST(ScriptRuntime, FaultSinkReceivesEvents) {
    ScriptRuntime rt(make_config());
    std::vector<ScriptFaultEvent> events;
    rt.set_fault_sink([&](const ScriptFaultEvent& e) { events.push_back(e); });
    rt.call_string("mine", "error('boom')");
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].kind, ScriptFaultEvent::Kind::kRuntime);
    EXPECT_EQ(events[0].chunk, "mine");
    EXPECT_NE(events[0].message.find("boom"), std::string::npos);
}

// ── 具名模块注入（§4.1） ──────────────────────────────────

namespace {

int gettag(lua_State* L) {
    lua_pushinteger(L, 42);
    return 1;
}

const luaL_Reg kTestApi[] = {
    {"gettag", gettag},
    {nullptr, nullptr},
};

}  // namespace

TEST(ScriptRuntime, RegisterModuleExposesApi) {
    ScriptRuntime rt(make_config());
    rt.register_module("testapi", kTestApi);
    EXPECT_TRUE(rt.call_string("t", "result = testapi.gettag()"));
    EXPECT_EQ(rt.get_global_string("result"), "42");
}

// ── 模块热替换（§3.2 原子性与回滚） ──────────────────────

TEST(ScriptRuntime, SwapModuleAtomic) {
    ScriptRuntime rt(make_config());
    rt.swap_module({"quest", 41, {}}, "return { version = 41 }");
    rt.swap_module({"quest", 42, {}}, "return { version = 42 }");
    EXPECT_TRUE(rt.has_module("quest"));
    EXPECT_TRUE(rt.call_string("t", "result = quest.version"));
    EXPECT_EQ(rt.get_global_string("result"), "42");
}

TEST(ScriptRuntime, SwapModuleFailureKeepsOld) {
    ScriptRuntime rt(make_config());
    rt.swap_module({"quest", 41, {}}, "return { version = 41 }");
    EXPECT_FALSE(rt.swap_module({"quest", 42, {}}, "error('broken patch')"));
    // 旧表未被替换（冻结区语义：编译/冒烟失败不触活跃引用）
    EXPECT_TRUE(rt.call_string("t", "result = quest.version"));
    EXPECT_EQ(rt.get_global_string("result"), "41");
}

TEST(ScriptRuntime, SwapModuleRejectsMissingDeps) {
    ScriptRuntime rt(make_config());
    EXPECT_FALSE(rt.swap_module({"skill", 1, {"quest"}}, "return {}"));
    EXPECT_FALSE(rt.has_module("skill"));
    rt.swap_module({"quest", 1, {}}, "return { version = 1 }");
    EXPECT_TRUE(rt.swap_module({"skill", 1, {"quest"}}, "return { version = 1 }"));
}

TEST(ScriptRuntime, SwapModuleSmokeGate) {
    ScriptRuntime rt(make_config());
    EXPECT_FALSE(rt.swap_module({"m", 1, {}}, "return { smoke = function() return false end }"));
    EXPECT_FALSE(rt.has_module("m"));
    EXPECT_TRUE(rt.swap_module({"m", 2, {}}, "return { smoke = function() return true end }"));
    EXPECT_TRUE(rt.has_module("m"));
}

TEST(ScriptRuntime, SwapModuleCallsOnReload) {
    ScriptRuntime rt(make_config());
    rt.swap_module({"m", 1, {}}, "return { version = 1 }");
    rt.swap_module({"m", 2, {}}, R"(
        local t = { version = 2 }
        t.onReload = function(old)
            reloaded = (old ~= nil) and old.version or -1
        end
        return t
    )");
    rt.call_string("t", "result = reloaded");
    EXPECT_EQ(rt.get_global_string("result"), "1");
}

TEST(ScriptRuntime, RollbackRestoresFrozen) {
    ScriptRuntime rt(make_config());
    rt.swap_module({"m", 1, {}}, "return { version = 1 }");
    rt.swap_module({"m", 2, {}}, "return { version = 2 }");
    EXPECT_TRUE(rt.rollback_module("m"));
    EXPECT_TRUE(rt.call_string("t", "result = m.version"));
    EXPECT_EQ(rt.get_global_string("result"), "1");
    // 再回滚一次 → 冷冻区已空，失败
    EXPECT_FALSE(rt.rollback_module("m"));
}

TEST(ScriptRuntime, ReloadListenerFires) {
    ScriptRuntime rt(make_config());
    std::vector<std::string> events;
    rt.set_reload_listener([&](const char* name, std::uint64_t version) {
        events.push_back(std::string(name) + ":" + std::to_string(version));
    });
    rt.swap_module({"m", 7, {}}, "return { version = 7 }");
    EXPECT_EQ(rt.active_modules().size(), 1u);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0], "m:7");
}

// ── 属性钩子窗口（§5.1/§5.2） ─────────────────────────────

namespace {
std::vector<ScriptRuntime::AttrChange> make_changes() {
    return {{100, 3, 10, 20}, {101, 3, 5, 6}, {102, 7, 1, 2}};
}
}  // namespace

TEST(ScriptRuntime, AttrHookModifiesValue) {
    ScriptRuntime rt(make_config());
    rt.call_string("hook", R"(
function onAttrChanged(entity, attr, old, new)
    return new + 1
end
)");
    ASSERT_TRUE(rt.set_attr_hook("onAttrChanged"));
    auto changes = make_changes();
    rt.run_attr_hook_window(changes, {3, 7}, 1000000);
    EXPECT_EQ(changes[0].new_value, 21);
    EXPECT_EQ(changes[1].new_value, 7);
    EXPECT_EQ(changes[2].new_value, 3);
}

TEST(ScriptRuntime, AttrHookOutOfWhitelistSkipped) {
    ScriptRuntime rt(make_config());
    rt.call_string("hook", "function onAttrChanged(e, a, o, n) return n + 100 end");
    ASSERT_TRUE(rt.set_attr_hook("onAttrChanged"));
    auto changes = make_changes();
    rt.run_attr_hook_window(changes, {7}, 1000000);  // 只有 attr 7 在白名单
    EXPECT_EQ(changes[0].new_value, 20);  // 白名单外原值不动
    EXPECT_EQ(changes[2].new_value, 102);
    EXPECT_GE(rt.stats().attr_hook_skipped_whitelist, 2u);
}

TEST(ScriptRuntime, AttrHookErrorKeepsOriginal) {
    ScriptRuntime rt(make_config());
    rt.call_string("hook", "function onAttrChanged(e, a, o, n) error('bad formula') end");
    ASSERT_TRUE(rt.set_attr_hook("onAttrChanged"));
    auto changes = make_changes();
    rt.run_attr_hook_window(changes, {3}, 1000000);
    EXPECT_EQ(changes[0].new_value, 20);  // 错误 → 原值
    EXPECT_EQ(changes[1].new_value, 6);
    EXPECT_EQ(rt.stats().attr_hook_faults, 2u);  // 两个 attr 3 条目均出错
    // 状态可继续
    EXPECT_TRUE(rt.call_string("ok", "result = 1"));
}

TEST(ScriptRuntime, AttrHookBudgetSkipsRest) {
    ScriptRuntime rt(make_config());
    rt.call_string("hook", "function onAttrChanged(e, a, o, n) return n + 1 end");
    ASSERT_TRUE(rt.set_attr_hook("onAttrChanged"));
    auto changes = make_changes();
    rt.run_attr_hook_window(changes, {3, 7}, 0);  // 预算 0 → 全部跳过
    EXPECT_EQ(changes[0].new_value, 20);
    EXPECT_EQ(rt.stats().attr_hook_skipped_budget, 3u);
    // 下一帧恢复（§5.2）
    rt.reset_attr_frame();
    auto changes2 = make_changes();
    rt.run_attr_hook_window(changes2, {3, 7}, 1000000);
    EXPECT_EQ(changes2[0].new_value, 21);
}

TEST(ScriptRuntime, AttrHookNilReturnsOriginal) {
    ScriptRuntime rt(make_config());
    rt.call_string("hook", "function onAttrChanged(e, a, o, n) return nil end");
    ASSERT_TRUE(rt.set_attr_hook("onAttrChanged"));
    auto changes = make_changes();
    rt.run_attr_hook_window(changes, {3}, 1000000);
    EXPECT_EQ(changes[0].new_value, 20);  // nil 返回 = 不修饰
}

TEST(ScriptRuntime, AttrHookAbsentFunctionSafe) {
    ScriptRuntime rt(make_config());
    EXPECT_FALSE(rt.set_attr_hook("no_such_fn"));
    auto changes = make_changes();
    rt.run_attr_hook_window(changes, {3, 7}, 1000000);  // 无钩子 → 空窗口
    EXPECT_EQ(changes[0].new_value, 20);
    EXPECT_EQ(rt.stats().attr_hook_calls, 0u);
}

// ── 模块熔断（§6.1） ──────────────────────────────────────

namespace {
ScriptConfig breaker_config() {
    ScriptConfig cfg = make_config();
    cfg.breaker_window_ticks = 1000;      // W
    cfg.breaker_fault_limit = 3;          // M
    cfg.breaker_quarantine_ticks = 600;   // Q
    return cfg;
}

const char* kBreakerModule = R"(
local t = { version = 1 }
t.boom = function() error('handler fault') end
t.ok = function() worked = true end
return t
)";
}  // namespace

TEST(ScriptRuntime, BreakerTripsAfterThreeFaults) {
    ScriptRuntime rt(breaker_config());
    rt.swap_module({"m", 1, {}}, kBreakerModule);
    EXPECT_FALSE(rt.call_module("m", "boom"));
    EXPECT_FALSE(rt.call_module("m", "boom"));
    EXPECT_FALSE(rt.call_module("m", "boom"));
    EXPECT_TRUE(rt.is_quarantined("m"));
    EXPECT_EQ(rt.quarantine_remaining("m"), 600u);
    // 隔离期短路：不再进 Lua（handler 副作用不发生）
    EXPECT_FALSE(rt.call_module("m", "ok"));
    EXPECT_FALSE(rt.get_global_bool("worked"));
    EXPECT_EQ(rt.stats().circuit_rejects, 1u);
}

TEST(ScriptRuntime, BreakerWindowSlides) {
    ScriptRuntime rt(breaker_config());
    rt.swap_module({"m", 1, {}}, kBreakerModule);
    rt.call_module("m", "boom");
    rt.call_module("m", "boom");
    rt.set_tick(1001);  // 滑窗滑过：tick 0 的 FAULT 到期（0+W ≤ 1001）
    rt.call_module("m", "boom");  // 总第 3 次，但窗内仅 1 次
    EXPECT_FALSE(rt.is_quarantined("m"));
}

TEST(ScriptRuntime, BreakerHalfOpenRecovers) {
    ScriptRuntime rt(breaker_config());
    rt.swap_module({"m", 1, {}}, kBreakerModule);
    for (int i = 0; i < 3; ++i) {
        rt.call_module("m", "boom");
    }
    ASSERT_TRUE(rt.is_quarantined("m"));
    rt.set_tick(600);  // 隔离期满
    EXPECT_FALSE(rt.is_quarantined("m"));
    EXPECT_TRUE(rt.call_module("m", "ok"));  // 半开放行一次，探针成功
    EXPECT_TRUE(rt.get_global_bool("worked"));
    EXPECT_FALSE(rt.is_quarantined("m"));    // 销账解除
}

TEST(ScriptRuntime, BreakerHalfOpenFaultDoublesQuarantine) {
    ScriptRuntime rt(breaker_config());
    rt.swap_module({"m", 1, {}}, kBreakerModule);
    for (int i = 0; i < 3; ++i) {
        rt.call_module("m", "boom");  // trip → until 600
    }
    rt.set_tick(600);
    rt.call_module("m", "boom");  // 半开探针再 FAULT → span 翻倍
    EXPECT_TRUE(rt.is_quarantined("m"));
    EXPECT_EQ(rt.quarantine_remaining("m"), 1200u);
}

TEST(ScriptRuntime, BreakerHotSwapClears) {
    ScriptRuntime rt(breaker_config());
    rt.swap_module({"m", 1, {}}, kBreakerModule);
    for (int i = 0; i < 3; ++i) {
        rt.call_module("m", "boom");
    }
    ASSERT_TRUE(rt.is_quarantined("m"));
    EXPECT_TRUE(rt.swap_module({"m", 2, {}},
                               "return { version = 2, ok = function() return true end }"));
    EXPECT_FALSE(rt.is_quarantined("m"));    // 热更自动清零
    EXPECT_TRUE(rt.call_module("m", "ok"));  // 新版本直接可用
}

TEST(ScriptRuntime, BreakerConfigurableLimit) {
    ScriptConfig cfg = breaker_config();
    cfg.breaker_fault_limit = 1;  // M=1：单次 FAULT 即熔断
    ScriptRuntime rt(cfg);
    rt.swap_module({"m", 1, {}}, kBreakerModule);
    EXPECT_FALSE(rt.call_module("m", "boom"));
    EXPECT_TRUE(rt.is_quarantined("m"));
}

TEST(ScriptRuntime, CallModuleDispatches) {
    ScriptRuntime rt(breaker_config());
    rt.swap_module({"m", 1, {}}, "return { greet = function() greeted = 'hi' end }");
    EXPECT_TRUE(rt.call_module("m", "greet"));
    EXPECT_EQ(rt.get_global_string("greeted"), "hi");
}

TEST(ScriptRuntime, CallModuleMissingHandlerFaults) {
    ScriptRuntime rt(breaker_config());
    rt.swap_module({"m", 1, {}}, "return { version = 1 }");
    EXPECT_FALSE(rt.call_module("m", "nope"));
    EXPECT_EQ(rt.stats().fault_runtime, 1u);
}

// ── 契约表装载（§3.4） ────────────────────────────────────

namespace {
// 契约依赖的 handler 面：全局 onChat + quest.onStart。
const char* kContractEnv = R"(
function onChat() routed_chat = true end
quest = { onStart = function() routed_start = true end }
)";
}  // namespace

TEST(ScriptRuntime, LoadContractStoresDataFace) {
    ScriptRuntime rt(make_config());
    ASSERT_TRUE(rt.call_string("env", kContractEnv));
    EXPECT_TRUE(rt.load_contract(R"(
return {
    version = 3,
    routes = { [1001] = 'onChat', [1002] = 'quest.onStart' },
    write_whitelist = { 3, 7 },
}
)"));
    EXPECT_EQ(rt.contract_version(), 3u);
    const auto routes = rt.contract_routes();
    ASSERT_EQ(routes.size(), 2u);
    EXPECT_EQ(routes.at(1001), "onChat");
    EXPECT_EQ(routes.at(1002), "quest.onStart");
    EXPECT_EQ(rt.contract_write_whitelist(), (std::vector<std::uint64_t>{3, 7}));
}

TEST(ScriptRuntime, LoadContractRejectsMissingHandler) {
    ScriptRuntime rt(make_config());
    ASSERT_TRUE(rt.call_string("env", kContractEnv));
    // bin 里有而 Lua 无 handler → 一致性闸拒换（sdk-contract §10.6）
    EXPECT_FALSE(rt.load_contract("return { routes = { [1001] = 'no_such_fn' } }"));
    EXPECT_EQ(rt.contract_version(), 0u);  // 旧数据面不动
    EXPECT_TRUE(rt.contract_routes().empty());
}

TEST(ScriptRuntime, LoadContractRejectsBadStructure) {
    ScriptRuntime rt(make_config());
    ASSERT_TRUE(rt.call_string("env", kContractEnv));
    EXPECT_FALSE(rt.load_contract("return 42"));                       // 非表
    EXPECT_FALSE(rt.load_contract("return { write_whitelist = {1} }")); // 缺 routes
    EXPECT_FALSE(rt.load_contract("return { routes = 'x' }"));          // routes 非表
    EXPECT_FALSE(rt.load_contract("error('boom')"));                    // 编译/执行错误
}

TEST(ScriptRuntime, LoadContractAtomicKeepsOldOnReject) {
    ScriptRuntime rt(make_config());
    ASSERT_TRUE(rt.call_string("env", kContractEnv));
    ASSERT_TRUE(rt.load_contract(
        "return { version = 1, routes = { [1001] = 'onChat' }, write_whitelist = { 3 } }"));
    EXPECT_FALSE(rt.load_contract("return { version = 2, routes = { [1001] = 'ghost' } }"));
    EXPECT_EQ(rt.contract_version(), 1u);
    ASSERT_EQ(rt.contract_routes().count(1001), 1u);
    EXPECT_EQ(rt.contract_routes().at(1001), "onChat");
    EXPECT_EQ(rt.contract_write_whitelist(), (std::vector<std::uint64_t>{3}));
}

TEST(ScriptRuntime, CallRouteDispatches) {
    ScriptRuntime rt(make_config());
    ASSERT_TRUE(rt.call_string("env", kContractEnv));
    ASSERT_TRUE(rt.load_contract(
        "return { version = 1, routes = { [1001] = 'onChat', [1002] = 'quest.onStart' } }"));
    EXPECT_TRUE(rt.call_route(1001));
    EXPECT_TRUE(rt.get_global_bool("routed_chat"));
    EXPECT_TRUE(rt.call_route(1002));
    EXPECT_TRUE(rt.get_global_bool("routed_start"));
    EXPECT_FALSE(rt.call_route(9999));  // 未注册 id
}

TEST(ScriptRuntime, ContractWhitelistFeedsAttrHook) {
    ScriptRuntime rt(make_config());
    ASSERT_TRUE(rt.call_string("env", kContractEnv));
    ASSERT_TRUE(rt.load_contract(
        "return { version = 1, routes = {}, write_whitelist = { 3 } }"));
    rt.call_string("hook", "function onAttrChanged(e, a, o, n) return n + 1 end");
    ASSERT_TRUE(rt.set_attr_hook("onAttrChanged"));
    auto changes = make_changes();  // attrs 3,3,7
    rt.run_attr_hook_window(changes, rt.contract_write_whitelist(), 1000000);
    EXPECT_EQ(changes[0].new_value, 21);  // 白名单内修订
    EXPECT_EQ(changes[2].new_value, 2);   // 白名单外（7 ∉ {3}）原值
}

// ── §6.1 事件面：WARN 去抖 + TRIP/RECOVER + 结构化字段 ────

namespace {
// 预算 12000：一拍 hook（10k 步进）即 ≥80%（9600），两拍（20k）必超限。
// 空 for 循环每迭代 ≥1 条 VM 指令 → 12000 次迭代恰好落 [10k, 20k) 一拍区间。
ScriptConfig warn_config() {
    ScriptConfig cfg = make_config();
    cfg.instruction_budget = 12000;
    return cfg;
}
}  // namespace

TEST(ScriptRuntime, WarnFiresAt80PercentBudget) {
    ScriptRuntime rt(warn_config());
    std::vector<ScriptFaultEvent> events;
    rt.set_fault_sink([&](const ScriptFaultEvent& e) { events.push_back(e); });
    rt.swap_module({"m", 1, {}},
                   "return { spin = function() for i = 1, 12000 do end end }");
    EXPECT_TRUE(rt.call_module("m", "spin"));  // 成功执行（未超限）
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].kind, ScriptFaultEvent::Kind::kWarnBudget);
    EXPECT_EQ(events[0].consumed, 10000u);
    EXPECT_EQ(events[0].budget, 12000u);
    EXPECT_EQ(rt.stats().budget_warns, 1u);
    EXPECT_EQ(rt.stats().faults, 0u);  // WARN 不计 fault
}

TEST(ScriptRuntime, WarnDebouncedWithin256Ticks) {
    ScriptRuntime rt(warn_config());
    std::vector<ScriptFaultEvent> events;
    rt.set_fault_sink([&](const ScriptFaultEvent& e) { events.push_back(e); });
    rt.swap_module({"m", 1, {}},
                   "return { spin = function() for i = 1, 12000 do end end }");
    rt.call_module("m", "spin");
    rt.call_module("m", "spin");
    EXPECT_EQ(events.size(), 1u);  // 同 (module,handler) 256 tick 内去抖
    rt.set_tick(256);
    rt.call_module("m", "spin");
    EXPECT_EQ(events.size(), 2u);
}

TEST(ScriptRuntime, WarnUnderThresholdSilent) {
    ScriptRuntime rt(warn_config());
    std::vector<ScriptFaultEvent> events;
    rt.set_fault_sink([&](const ScriptFaultEvent& e) { events.push_back(e); });
    rt.swap_module({"m", 1, {}}, "return { tiny = function() return 1 end }");
    EXPECT_TRUE(rt.call_module("m", "tiny"));
    EXPECT_TRUE(events.empty());
    EXPECT_EQ(rt.stats().budget_warns, 0u);
}

TEST(ScriptRuntime, TripAndRecoverEventsFire) {
    ScriptRuntime rt(breaker_config());
    std::vector<ScriptFaultEvent> events;
    rt.set_fault_sink([&](const ScriptFaultEvent& e) { events.push_back(e); });
    rt.swap_module({"m", 1, {}}, kBreakerModule);
    for (int i = 0; i < 3; ++i) {
        rt.call_module("m", "boom");
    }
    // 3 FAULT + 1 TRIP
    ASSERT_EQ(events.size(), 4u);
    EXPECT_EQ(events[3].kind, ScriptFaultEvent::Kind::kCircuitTripped);
    rt.set_tick(600);
    EXPECT_TRUE(rt.call_module("m", "ok"));  // 半开探针成功
    ASSERT_EQ(events.size(), 5u);
    EXPECT_EQ(events[4].kind, ScriptFaultEvent::Kind::kCircuitRecovered);
}

TEST(ScriptRuntime, HotSwapClearsEmitsRecover) {
    ScriptRuntime rt(breaker_config());
    std::vector<ScriptFaultEvent> events;
    rt.set_fault_sink([&](const ScriptFaultEvent& e) { events.push_back(e); });
    rt.swap_module({"m", 1, {}}, kBreakerModule);
    for (int i = 0; i < 3; ++i) {
        rt.call_module("m", "boom");
    }
    rt.swap_module({"m", 2, {}}, "return { version = 2 }");
    ASSERT_GE(events.size(), 4u);
    EXPECT_EQ(events.back().kind, ScriptFaultEvent::Kind::kCircuitRecovered);
}

TEST(ScriptRuntime, FaultEventCarriesStructuredFields) {
    ScriptRuntime rt(make_config());
    std::vector<ScriptFaultEvent> events;
    rt.set_fault_sink([&](const ScriptFaultEvent& e) { events.push_back(e); });
    rt.set_tick(42);
    rt.call_string("bad", "error('boom')");
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].tick, 42u);
    EXPECT_EQ(events[0].budget, 100000u);
    EXPECT_GT(events[0].ts_wall_ms, 0u);
}

// ── §7.1 attach eval（tick 边界沙盒执行；admin 通道面留宿主批） ──

TEST(ScriptRuntime, EvalDebugReturnsExpression) {
    ScriptRuntime rt(make_config());
    std::string out;
    EXPECT_TRUE(rt.eval_debug("1 + 2", out));
    EXPECT_EQ(out, "3");
    EXPECT_TRUE(rt.eval_debug("'w' .. 'orld'", out));
    EXPECT_EQ(out, "world");
}

TEST(ScriptRuntime, EvalDebugExecutesStatement) {
    ScriptRuntime rt(make_config());
    std::string out;
    EXPECT_TRUE(rt.eval_debug("dbg = 7", out));
    EXPECT_TRUE(out.empty());  // 语句路径无返回值
    EXPECT_EQ(rt.get_global_string("dbg"), "7");
}

TEST(ScriptRuntime, EvalDebugFaultIsolated) {
    ScriptRuntime rt(make_config());
    std::string out;
    EXPECT_FALSE(rt.eval_debug("error('nope')", out));
    EXPECT_EQ(rt.stats().fault_runtime, 1u);
    // 失败面 result = 错误原文（回执面消费；含 lua 报错原文）
    EXPECT_NE(out.find("nope"), std::string::npos);
    // 状态可继续
    EXPECT_TRUE(rt.eval_debug("2 * 2", out));
    EXPECT_EQ(out, "4");
}

TEST(ScriptRuntime, EvalDebugBudgetGoverned) {
    ScriptRuntime rt(make_config());
    std::string out;
    EXPECT_FALSE(rt.eval_debug("while true do end", out));  // 预算管辖 eval
    EXPECT_EQ(rt.stats().fault_budget, 1u);
    EXPECT_TRUE(rt.eval_debug("1", out));
}

TEST(ScriptRuntime, EvalDebugResultCapped) {
    ScriptRuntime rt(make_config());
    std::string out;
    EXPECT_TRUE(rt.eval_debug("string.rep('x', 100000)", out));
    EXPECT_EQ(out.size(), 4096u);  // 结果大小上限
}

TEST(ScriptRuntime, EvalDebugRateLimitedPerTick) {
    ScriptRuntime rt(make_config());
    std::string out;
    for (int i = 0; i < 8; ++i) {
        EXPECT_TRUE(rt.eval_debug("1", out));  // 默认每 tick 上限 8
    }
    EXPECT_FALSE(rt.eval_debug("1", out));  // 第 9 次拒
    EXPECT_EQ(out, "rate limited");  // 失败面 result = 错误原文
    rt.set_tick(1);  // 下一 tick 恢复
    EXPECT_TRUE(rt.eval_debug("1", out));
}

// ── 熔断状态持久化（§6.1 防坏版本崩溃循环） ─────────────────

TEST(ScriptRuntime, BreakerStateRoundtripRestoresQuarantine) {
    ScriptRuntime rt(breaker_config());
    rt.swap_module({"m", 1, {}}, kBreakerModule);
    rt.set_tick(5000);
    EXPECT_FALSE(rt.call_module("m", "boom"));
    EXPECT_FALSE(rt.call_module("m", "boom"));
    EXPECT_FALSE(rt.call_module("m", "boom"));
    EXPECT_TRUE(rt.is_quarantined("m"));

    const auto blob = rt.export_breaker_state();
    EXPECT_FALSE(blob.empty());

    ScriptRuntime rt2(breaker_config());
    rt2.set_tick(5000);
    ASSERT_TRUE(rt2.import_breaker_state(blob));
    // 隔离随状态恢复：重启后坏模块仍隔离（崩溃循环防护）
    EXPECT_TRUE(rt2.is_quarantined("m"));
    EXPECT_EQ(rt2.quarantine_remaining("m"), 600u);
    EXPECT_FALSE(rt2.call_module("m", "ok"));  // 短路不进 Lua
    EXPECT_FALSE(rt2.get_global_bool("worked"));
    EXPECT_EQ(rt2.stats().circuit_rejects, 1u);
}

TEST(ScriptRuntime, BreakerQuarantineSurvivesTickAdvance) {
    ScriptRuntime rt(breaker_config());
    rt.swap_module({"m", 1, {}}, kBreakerModule);
    rt.set_tick(5000);
    rt.call_module("m", "boom");
    rt.call_module("m", "boom");
    rt.call_module("m", "boom");
    const auto blob = rt.export_breaker_state();

    // 重启落在更晚 tick：剩余隔离期按绝对 tick 折算
    ScriptRuntime rt2(breaker_config());
    rt2.set_tick(5200);
    ASSERT_TRUE(rt2.import_breaker_state(blob));
    EXPECT_TRUE(rt2.is_quarantined("m"));
    EXPECT_EQ(rt2.quarantine_remaining("m"), 400u);
}

TEST(ScriptRuntime, BreakerOnlyQuarantinedEntriesPersisted) {
    ScriptRuntime rt(breaker_config());
    rt.swap_module({"m", 1, {}}, kBreakerModule);
    rt.swap_module({"n", 1, {}}, kBreakerModule);
    rt.call_module("m", "boom");  // 单次 fault，未达 M 不隔离
    const auto blob = rt.export_breaker_state();

    ScriptRuntime rt2(breaker_config());
    ASSERT_TRUE(rt2.import_breaker_state(blob));
    EXPECT_FALSE(rt2.is_quarantined("m"));
    EXPECT_FALSE(rt2.is_quarantined("n"));
}

TEST(ScriptRuntime, WarnDebounceStampSurvivesRestart) {
    ScriptRuntime rt(warn_config());
    std::vector<ScriptFaultEvent> events;
    rt.set_fault_sink([&](const ScriptFaultEvent& e) { events.push_back(e); });
    rt.swap_module({"m", 1, {}},
                   "return { spin = function() for i = 1, 12000 do end end }");
    rt.call_module("m", "spin");  // WARN #1
    ASSERT_EQ(events.size(), 1u);

    const auto blob = rt.export_breaker_state();
    ScriptRuntime rt2(warn_config());
    std::vector<ScriptFaultEvent> events2;
    rt2.set_fault_sink([&](const ScriptFaultEvent& e) { events2.push_back(e); });
    rt2.set_tick(150);  // 距 stamp 不足 256
    ASSERT_TRUE(rt2.import_breaker_state(blob));
    rt2.swap_module({"m", 1, {}},
                    "return { spin = function() for i = 1, 12000 do end end }");
    rt2.call_module("m", "spin");
    EXPECT_TRUE(events2.empty());  // 去抖窗口跨重启连续（防重启告警风暴）
    EXPECT_EQ(rt2.stats().budget_warns, 0u);
}

TEST(ScriptRuntime, BreakerImportRejectsBadBlob) {
    ScriptRuntime rt(breaker_config());
    rt.set_tick(10);
    EXPECT_FALSE(rt.import_breaker_state({}));                   // 空
    EXPECT_FALSE(rt.import_breaker_state({'A', 'P', 'L', 'B', 9}));  // 版本未知
    EXPECT_FALSE(rt.import_breaker_state({'X', 'P', 'L', 'B', 1}));  // 魔数不符
    // 状态不受污染
    EXPECT_TRUE(rt.quarantined_modules().empty());
    EXPECT_TRUE(rt.is_quarantined("m") == false);
}

// ── WARN 阈值可配（§6.1） ─────────────────────────────────

TEST(ScriptRuntime, WarnThresholdHigherSilences) {
    ScriptConfig cfg = warn_config();
    cfg.warn_threshold_percent = 90;  // spin 83% < 90%：静默
    ScriptRuntime rt(cfg);
    std::vector<ScriptFaultEvent> events;
    rt.set_fault_sink([&](const ScriptFaultEvent& e) { events.push_back(e); });
    rt.swap_module({"m", 1, {}},
                   "return { spin = function() for i = 1, 12000 do end end }");
    EXPECT_TRUE(rt.call_module("m", "spin"));
    EXPECT_TRUE(events.empty());
    EXPECT_EQ(rt.stats().budget_warns, 0u);
}

TEST(ScriptRuntime, WarnThresholdLowerFires) {
    ScriptConfig cfg = warn_config();
    cfg.warn_threshold_percent = 50;  // spin 83% ≥ 50%：触发
    ScriptRuntime rt(cfg);
    std::vector<ScriptFaultEvent> events;
    rt.set_fault_sink([&](const ScriptFaultEvent& e) { events.push_back(e); });
    rt.swap_module({"m", 1, {}},
                   "return { spin = function() for i = 1, 12000 do end end }");
    EXPECT_TRUE(rt.call_module("m", "spin"));
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].kind, ScriptFaultEvent::Kind::kWarnBudget);
    EXPECT_EQ(rt.stats().budget_warns, 1u);
}

TEST(ScriptRuntime, WarnThresholdZeroDisables) {
    ScriptConfig cfg = warn_config();
    cfg.warn_threshold_percent = 0;  // 0 = 关闭
    ScriptRuntime rt(cfg);
    std::vector<ScriptFaultEvent> events;
    rt.set_fault_sink([&](const ScriptFaultEvent& e) { events.push_back(e); });
    rt.swap_module({"m", 1, {}},
                   "return { spin = function() for i = 1, 12000 do end end }");
    EXPECT_TRUE(rt.call_module("m", "spin"));
    EXPECT_TRUE(events.empty());
    EXPECT_EQ(rt.stats().budget_warns, 0u);
}
