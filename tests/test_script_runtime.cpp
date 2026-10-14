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
    // 二进制 chunk 签名（\x1bLua）——load 只收文本（§2：一切字节串按源码处理，
    // 预编译产物走 §3.5 专用装载路径，本批未实现）
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
