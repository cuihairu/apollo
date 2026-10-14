// ScriptRuntime §8 异步底座测试（scripting-lua.md §8.2 分层 / §8.3 三纪律 +
// §7.2 协程清单——登记表即清单数据源）。
//
// 覆盖：
//   - apollo.async.call 往返（受理 → yield → complete_async 入队 → drain
//     边界 resume——交接物是消息不是 future）
//   - 实参类型化快照（bool/int/double/string/nil）
//   - 未知服务快速失败 / 主线程（非协程）调用拒绝
//   - 超时终止（超时先到 → 错误 resume）+ 迟到完成与重复完成丢弃计数
//   - 服务侧失败终止（fail_async）
//   - 协作式取消（pending 下一 drain 以取消错误终止；不触模块熔断）
//   - re-validate 失效（validator 拒 → 错误终止，不注入僵尸结果）
//   - 协程清单（status/reason/pending/origin/born/age/timeout/超龄 leaked）
//   - coroutine.create/wrap 登记（脚本自建协程入清单 + 终态懒清扫）
//   - 指令预算跨 yield 管辖（协程内死循环可打断）
//   - 熔断隔离期 call_coroutine 短路 / 真实执行错误归因熔断
//   - 跨线程 complete_async（完成队列线程安全）
//   - 顺序两次 await（重挂起记账：reason/pending 迁移）

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "apollo/script/script_runtime.hpp"

using namespace apollo::script;

namespace {

ScriptConfig async_config() {
    ScriptConfig cfg;
    cfg.instruction_budget = 100000;
    cfg.memory_limit_bytes = 1 << 20;
    return cfg;
}

}  // namespace

// ── 往返与投递语义 ────────────────────────────────────────

TEST(ScriptAsync, CallRoundTrip) {
    ScriptRuntime rt(async_config());
    std::uint64_t rid = 0;
    rt.register_async_service(
        "echo", [&](std::uint64_t r, const std::vector<LuaValue>&) { rid = r; });
    ASSERT_TRUE(rt.swap_module({"m", 1, {}}, R"(
return {
    go = function()
        done = apollo.async.call("echo", 1)
    end,
}
)"));
    EXPECT_TRUE(rt.call_coroutine("m", "go"));
    EXPECT_EQ(rt.get_global_string("done"), "");  // 未完成
    auto snap = rt.coroutine_snapshot();
    ASSERT_EQ(snap.size(), 1u);
    EXPECT_EQ(snap[0].status, "suspended");
    EXPECT_EQ(snap[0].suspend_reason, "await echo");
    EXPECT_EQ(snap[0].pending_request, rid);
    EXPECT_EQ(snap[0].origin, "m:go");
    rt.complete_async(rid, LuaValue::of_string("ok"));
    EXPECT_EQ(rt.get_global_string("done"), "");  // drain 前不投递（消息非 future）
    rt.drain_async();
    EXPECT_EQ(rt.get_global_string("done"), "ok");
    EXPECT_TRUE(rt.coroutine_snapshot().empty());
    EXPECT_EQ(rt.stats().async_requests, 1u);
    EXPECT_EQ(rt.stats().async_completed, 1u);
}

TEST(ScriptAsync, ArgsTypedSnapshot) {
    ScriptRuntime rt(async_config());
    std::vector<LuaValue> args;
    rt.register_async_service(
        "db", [&](std::uint64_t, const std::vector<LuaValue>& a) { args = a; });
    ASSERT_TRUE(rt.swap_module({"m", 1, {}}, R"(
return {
    go = function() apollo.async.call("db", true, 42, 1.5, "s", nil) end,
}
)"));
    EXPECT_TRUE(rt.call_coroutine("m", "go"));
    ASSERT_EQ(args.size(), 5u);
    EXPECT_EQ(args[0].type, LuaValue::Type::kBool);
    EXPECT_TRUE(args[0].b);
    EXPECT_EQ(args[1].type, LuaValue::Type::kInt);
    EXPECT_EQ(args[1].i, 42);
    EXPECT_EQ(args[2].type, LuaValue::Type::kDouble);
    EXPECT_DOUBLE_EQ(args[2].d, 1.5);
    EXPECT_EQ(args[3].type, LuaValue::Type::kString);
    EXPECT_EQ(args[3].s, "s");
    EXPECT_EQ(args[4].type, LuaValue::Type::kNil);
}

TEST(ScriptAsync, MissingServiceFailsFast) {
    ScriptRuntime rt(async_config());
    ASSERT_TRUE(rt.swap_module({"m", 1, {}}, R"(
return {
    go = function() caught = select(2, pcall(apollo.async.call, "ghost")) end,
}
)"));
    EXPECT_TRUE(rt.call_coroutine("m", "go"));  // pcall 吞掉 → 协程正常跑完
    EXPECT_NE(rt.get_global_string("caught").find("async service not registered"),
              std::string::npos);
    EXPECT_EQ(rt.stats().async_requests, 0u);
    EXPECT_TRUE(rt.coroutine_snapshot().empty());
}

TEST(ScriptAsync, CallOutsideCoroutineRejected) {
    ScriptRuntime rt(async_config());
    EXPECT_TRUE(rt.call_string("t",
                               "caught = select(2, pcall(apollo.async.call, 'x'))"));
    EXPECT_NE(rt.get_global_string("caught").find("outside coroutine"),
              std::string::npos);
    EXPECT_EQ(rt.stats().async_requests, 0u);
}

// ── 超时 / 失败 / 取消 / 失效（§8.3 三纪律） ───────────────

TEST(ScriptAsync, TimeoutResumesWithError) {
    ScriptRuntime rt(async_config());
    rt.register_async_service("slow",
                              [](std::uint64_t, const std::vector<LuaValue>&) {},
                              /*timeout_ms=*/50);
    ASSERT_TRUE(rt.swap_module({"m", 1, {}}, R"(
return {
    go = function()
        local ok, err = pcall(apollo.async.call, "slow")
        if not ok then caught = err end
    end,
}
)"));
    EXPECT_TRUE(rt.call_coroutine("m", "go"));
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    rt.drain_async();
    EXPECT_NE(rt.get_global_string("caught").find("timeout"), std::string::npos);
    EXPECT_TRUE(rt.coroutine_snapshot().empty());
    EXPECT_EQ(rt.stats().async_timeouts, 1u);
    EXPECT_EQ(rt.stats().async_completed, 0u);
}

TEST(ScriptAsync, LateCompletionDroppedAndCounted) {
    ScriptRuntime rt(async_config());
    std::uint64_t rid = 0;
    rt.register_async_service(
        "slow", [&](std::uint64_t r, const std::vector<LuaValue>&) { rid = r; },
        /*timeout_ms=*/50);
    ASSERT_TRUE(rt.swap_module({"m", 1, {}}, R"(
return {
    go = function() local ok = pcall(apollo.async.call, "slow") done = ok end,
}
)"));
    EXPECT_TRUE(rt.call_coroutine("m", "go"));
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    rt.drain_async();  // 超时先到 → 错误 resume
    rt.complete_async(rid, LuaValue::of_int(1));  // 迟到结果
    rt.drain_async();
    EXPECT_EQ(rt.stats().async_late, 1u);  // 丢弃 + 计数
    EXPECT_TRUE(rt.coroutine_snapshot().empty());
}

TEST(ScriptAsync, DoubleCompletionCountsLate) {
    ScriptRuntime rt(async_config());
    std::uint64_t rid = 0;
    rt.register_async_service(
        "svc", [&](std::uint64_t r, const std::vector<LuaValue>&) { rid = r; });
    ASSERT_TRUE(rt.swap_module({"m", 1, {}}, R"(
return { go = function() apollo.async.call("svc") end }
)"));
    EXPECT_TRUE(rt.call_coroutine("m", "go"));
    rt.complete_async(rid, LuaValue::of_int(1));
    rt.complete_async(rid, LuaValue::of_int(2));  // 重复完成
    rt.drain_async();
    EXPECT_EQ(rt.stats().async_completed, 1u);
    EXPECT_EQ(rt.stats().async_late, 1u);
}

TEST(ScriptAsync, ServiceFailureTerminates) {
    ScriptRuntime rt(async_config());
    std::uint64_t rid = 0;
    rt.register_async_service(
        "db", [&](std::uint64_t r, const std::vector<LuaValue>&) { rid = r; });
    ASSERT_TRUE(rt.swap_module({"m", 1, {}}, R"(
return {
    go = function()
        local ok, err = pcall(apollo.async.call, "db")
        if not ok then caught = err end
    end,
}
)"));
    EXPECT_TRUE(rt.call_coroutine("m", "go"));
    rt.fail_async(rid, "db down");
    rt.drain_async();
    EXPECT_NE(rt.get_global_string("caught").find("db down"), std::string::npos);
    EXPECT_EQ(rt.stats().async_failed, 1u);
}

TEST(ScriptAsync, CancelPendingTerminatesNextDrain) {
    ScriptRuntime rt(async_config());
    std::uint64_t rid1 = 0, rid2 = 0;
    rt.register_async_service(
        "a", [&](std::uint64_t r, const std::vector<LuaValue>&) { rid1 = r; });
    rt.register_async_service(
        "b", [&](std::uint64_t r, const std::vector<LuaValue>&) { rid2 = r; });
    ASSERT_TRUE(rt.swap_module({"m", 1, {}}, R"(
return {
    go1 = function()
        local ok, err = pcall(apollo.async.call, "a")
        if not ok then caught1 = err end
    end,
    go2 = function()
        local ok, err = pcall(apollo.async.call, "b")
        if not ok then caught2 = err end
    end,
}
)"));
    EXPECT_TRUE(rt.call_coroutine("m", "go1"));
    EXPECT_TRUE(rt.call_coroutine("m", "go2"));
    rt.cancel_pending("shutdown");
    EXPECT_EQ(rt.coroutine_snapshot().size(), 2u);  // 取消在 drain 生效（协作式）
    rt.drain_async();
    EXPECT_NE(rt.get_global_string("caught1").find("cancelled: shutdown"),
              std::string::npos);
    EXPECT_NE(rt.get_global_string("caught2").find("cancelled: shutdown"),
              std::string::npos);
    EXPECT_EQ(rt.stats().async_cancelled, 2u);
    EXPECT_TRUE(rt.coroutine_snapshot().empty());
}

TEST(ScriptAsync, CancelledErrorDoesNotTripBreaker) {
    ScriptRuntime rt(async_config());
    rt.register_async_service("svc",
                              [](std::uint64_t, const std::vector<LuaValue>&) {});
    ASSERT_TRUE(rt.swap_module({"m", 1, {}}, R"(
return {
    go = function()
        apollo.async.call("svc")  -- 裸调用：注入错误未捕获即终止
    end,
}
)"));
    EXPECT_TRUE(rt.call_coroutine("m", "go"));
    rt.cancel_pending("shutdown");
    rt.drain_async();
    // 注入错误（超时/取消/失效）是宿主发起的运营终态——fault 照发但不计
    // 模块熔断（否则停机取消把全部业务模块打进隔离）。
    EXPECT_EQ(rt.stats().fault_runtime, 1u);
    EXPECT_FALSE(rt.is_quarantined("m"));
    EXPECT_EQ(rt.stats().async_cancelled, 1u);
    EXPECT_TRUE(rt.coroutine_snapshot().empty());
}

TEST(ScriptAsync, RevalidateFailTerminatesWithoutInjection) {
    ScriptRuntime rt(async_config());
    std::uint64_t rid = 0;
    rt.register_async_service(
        "svc", [&](std::uint64_t r, const std::vector<LuaValue>&) { rid = r; });
    rt.set_async_validator(
        [](const LuaCoroutineInfo&) { return false; });  // 场景已失效
    ASSERT_TRUE(rt.swap_module({"m", 1, {}}, R"(
return {
    go = function()
        apollo.async.call("svc")  -- 裸调用：注入错误未捕获即终止
        resumed = true  -- 不注入僵尸结果：此行不应执行
    end,
}
)"));
    EXPECT_TRUE(rt.call_coroutine("m", "go"));
    rt.complete_async(rid, LuaValue::of_int(1));
    rt.drain_async();
    EXPECT_FALSE(rt.get_global_bool("resumed"));  // 未注入僵尸结果
    EXPECT_EQ(rt.stats().async_invalid, 1u);
    EXPECT_TRUE(rt.coroutine_snapshot().empty());
}

// ── §7.2 协程清单 ────────────────────────────────────────

TEST(ScriptAsync, SnapshotFieldsAndAging) {
    ScriptRuntime rt(async_config());
    rt.set_tick(10);
    std::uint64_t rid = 0;
    rt.register_async_service(
        "svc", [&](std::uint64_t r, const std::vector<LuaValue>&) { rid = r; },
        /*timeout_ms=*/1234);
    ASSERT_TRUE(rt.swap_module({"m", 1, {}}, R"(
return { go = function() apollo.async.call("svc") end }
)"));
    EXPECT_TRUE(rt.call_coroutine("m", "go"));
    rt.set_tick(15);
    auto snap = rt.coroutine_snapshot();
    ASSERT_EQ(snap.size(), 1u);
    EXPECT_EQ(snap[0].born_tick, 10u);
    EXPECT_EQ(snap[0].age_ticks, 5u);
    EXPECT_EQ(snap[0].timeout_ms, 1234u);
    EXPECT_EQ(snap[0].pending_request, rid);
}

TEST(ScriptAsync, LeakFlagOnOverage) {
    ScriptConfig cfg = async_config();
    cfg.async_leak_threshold_ticks = 3;
    ScriptRuntime rt(cfg);
    rt.set_tick(1);
    rt.register_async_service("svc",
                              [](std::uint64_t, const std::vector<LuaValue>&) {},
                              /*timeout_ms=*/0);  // 不限时：只受超龄检测
    ASSERT_TRUE(rt.swap_module({"m", 1, {}}, R"(
return { go = function() apollo.async.call("svc") end }
)"));
    EXPECT_TRUE(rt.call_coroutine("m", "go"));
    rt.set_tick(5);
    auto snap = rt.coroutine_snapshot();
    ASSERT_EQ(snap.size(), 1u);
    EXPECT_TRUE(snap[0].leaked);  // 超龄（泄漏检测）
}

TEST(ScriptAsync, ScriptCreatedCoroutineTracked) {
    ScriptRuntime rt(async_config());
    EXPECT_TRUE(rt.call_string("t", R"(
co = coroutine.create(function() coroutine.yield(1) end)
status_before = coroutine.status(co)
yielded = select(2, coroutine.resume(co))
)"));
    EXPECT_EQ(rt.get_global_string("status_before"), "suspended");
    EXPECT_TRUE(rt.call_string("t2", R"(
finished = coroutine.resume(co)  -- 跑完
)"));
    // 懒清扫：快照点除名终态
    EXPECT_TRUE(rt.coroutine_snapshot().empty());
}

TEST(ScriptAsync, WrapTrackedAndErrorPropagates) {
    ScriptRuntime rt(async_config());
    EXPECT_TRUE(rt.call_string("t", R"(
w = coroutine.wrap(function(a) local b = coroutine.yield(a + 1) return b * 2 end)
first = w(1)
snap_reason = "pending"
)"));
    auto snap = rt.coroutine_snapshot();
    ASSERT_EQ(snap.size(), 1u);
    EXPECT_EQ(snap[0].status, "suspended");
    EXPECT_EQ(snap[0].origin, "coroutine.wrap");
    rt.call_string("t2", "second = w(10)");
    EXPECT_TRUE(rt.coroutine_snapshot().empty());  // 跑完即懒清扫
    // wrap 错误抛给调用者（pcall 捕获面）
    EXPECT_TRUE(rt.call_string("t3", R"(
w2 = coroutine.wrap(function() error("boom") end)
ok, err = pcall(w2)
)"));
    EXPECT_FALSE(rt.get_global_bool("ok"));
    EXPECT_NE(rt.get_global_string("err").find("boom"), std::string::npos);
}

// ── 预算 / 熔断 / 线程安全 ───────────────────────────────

TEST(ScriptAsync, InstructionBudgetCoversCoroutine) {
    ScriptConfig cfg = async_config();
    cfg.instruction_budget = 20000;
    ScriptRuntime rt(cfg);
    ASSERT_TRUE(rt.swap_module({"m", 1, {}}, R"(
return { go = function() for i = 1, 100000 do end end }
)"));
    EXPECT_FALSE(rt.call_coroutine("m", "go"));  // 死循环协程可打断
    EXPECT_EQ(rt.stats().fault_budget, 1u);
    EXPECT_TRUE(rt.coroutine_snapshot().empty());
}

TEST(ScriptAsync, QuarantinedModuleShortCircuitsSpawn) {
    ScriptRuntime rt(async_config());
    ASSERT_TRUE(rt.swap_module({"m", 1, {}}, R"(
return { boom = function() error("x") end, go = function() end }
)"));
    for (int i = 0; i < 3; ++i) {
        EXPECT_FALSE(rt.call_module("m", "boom"));  // 滑窗 3 次 → 隔离
    }
    EXPECT_TRUE(rt.is_quarantined("m"));
    EXPECT_FALSE(rt.call_coroutine("m", "go"));  // 隔离期短路（不进 Lua）
    EXPECT_GE(rt.stats().circuit_rejects, 1u);
}

TEST(ScriptAsync, CoroutineExecutionErrorTripsBreaker) {
    ScriptRuntime rt(async_config());
    std::uint64_t rid = 0;
    rt.register_async_service(
        "svc", [&](std::uint64_t r, const std::vector<LuaValue>&) { rid = r; });
    ASSERT_TRUE(rt.swap_module({"m", 1, {}}, R"(
return {
    go = function()
        apollo.async.call("svc")  -- 正常完成后继续跑到下面的执行错误
        error("boom")  -- 真实执行错误（非注入）
    end,
}
)"));
    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(rt.call_coroutine("m", "go"));
        rt.complete_async(rid, LuaValue::of_int(1));
        rt.drain_async();
    }
    EXPECT_TRUE(rt.is_quarantined("m"));  // 执行错误归因熔断
    EXPECT_EQ(rt.stats().fault_runtime, 3u);
    EXPECT_TRUE(rt.coroutine_snapshot().empty());
}

TEST(ScriptAsync, CompleteFromAnotherThread) {
    ScriptRuntime rt(async_config());
    std::uint64_t rid = 0;
    rt.register_async_service(
        "svc", [&](std::uint64_t r, const std::vector<LuaValue>&) { rid = r; });
    ASSERT_TRUE(rt.swap_module({"m", 1, {}}, R"(
return { go = function() done = apollo.async.call("svc") end }
)"));
    EXPECT_TRUE(rt.call_coroutine("m", "go"));
    std::thread worker([&] { rt.complete_async(rid, LuaValue::of_int(7)); });
    worker.join();
    rt.drain_async();
    EXPECT_EQ(rt.get_global_string("done"), "7");
}

// ── 重挂起记账 ───────────────────────────────────────────

TEST(ScriptAsync, SequentialAwaitsUpdateRegistry) {
    ScriptRuntime rt(async_config());
    std::uint64_t rid1 = 0, rid2 = 0;
    rt.register_async_service(
        "a", [&](std::uint64_t r, const std::vector<LuaValue>&) { rid1 = r; });
    rt.register_async_service(
        "b", [&](std::uint64_t r, const std::vector<LuaValue>&) { rid2 = r; });
    ASSERT_TRUE(rt.swap_module({"m", 1, {}}, R"(
return {
    go = function()
        x = apollo.async.call("a")
        y = apollo.async.call("b")
        done = x .. y
    end,
}
)"));
    EXPECT_TRUE(rt.call_coroutine("m", "go"));
    auto s1 = rt.coroutine_snapshot();
    ASSERT_EQ(s1.size(), 1u);
    EXPECT_EQ(s1[0].suspend_reason, "await a");
    rt.complete_async(rid1, LuaValue::of_string("1"));
    rt.drain_async();
    auto s2 = rt.coroutine_snapshot();
    ASSERT_EQ(s2.size(), 1u);  // 第二次 await 重挂起
    EXPECT_EQ(s2[0].suspend_reason, "await b");
    EXPECT_EQ(s2[0].pending_request, rid2);
    EXPECT_EQ(rt.get_global_string("done"), "");
    rt.complete_async(rid2, LuaValue::of_string("2"));
    rt.drain_async();
    EXPECT_EQ(rt.get_global_string("done"), "12");
    EXPECT_TRUE(rt.coroutine_snapshot().empty());
}
