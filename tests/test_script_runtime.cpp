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
