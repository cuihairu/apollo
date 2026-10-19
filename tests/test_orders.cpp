// 订单/通知可靠受理与持久待办测试（todo L63 批⑯）+ 订单事务域测试（L64 批⑰）。
//
// 契约面：
//  1. 受理幂等 + 行格式校验（'|'/'\n'/单号 0/玩家 0 拒绝；重发不重建）；
//  2. 「确认收到 ≠ 发货完成」（accept 后仍待办）；
//  3. journal 重启恢复（待办 + 墓碑 + 受理序；压薄一单一行）；
//  4. 损坏 journal 显式抛错（不静默部分回放）；
//  5. 每次尝试重新解析 Base（迁移/升级换归属下拍即投新家，resolver_calls
//     == 尝试数）+ 完成记账归属代次；
//  6. 无 Base 挂起不丢（离线/不可达恢复后投出）；
//  7. 指数退避（1000→2000→4000…封顶 60000）+ 每拍预算；
//  8. 渠道重发不重置投递进度；
//  9. Base 唯一激活/写权：后到顶替先到（并发登录）、仅当前代次可释放、
//     代次水位重启不回退、离线激活即登记（不接触会话面）；
// 10. 履约事务：订单校验（未受理单拒/载荷双保险/唯一写权同址同代次）+
//     奖励与去重凭据原子持久提交（通知重复/乱序/回执丢失/重投收敛为一次
//     发货；崩溃窗由 open() 回放补齐）。

#include "apollo/game/orders/base_authority.hpp"
#include "apollo/game/orders/order_delivery.hpp"
#include "apollo/game/orders/order_fulfillment.hpp"
#include "apollo/game/orders/order_store.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;
namespace orders = apollo::game::orders;

static int failures = 0;
static int checks = 0;

#define TEST_ASSERT(cond)                                                    \
    do {                                                                     \
        ++checks;                                                            \
        if (!(cond)) {                                                       \
            ++failures;                                                      \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);     \
        }                                                                    \
    } while (0)

static fs::path kRoot;

static std::string journal_for(const char* name) {
    return (kRoot / name).string();
}

// ---- 1. 受理幂等 + 校验 ----
static void case_accept_validates_and_dedups() {
    orders::OrderStore store(journal_for("accept.journal"));
    TEST_ASSERT(store.open());
    auto r = store.accept(1001, 77, R"({"item":"gem","qty":3})", 1000);
    TEST_ASSERT(r.accepted && !r.duplicate);
    TEST_ASSERT(store.pending_count() == 1 && store.total_count() == 1);
    // 渠道同号重发：幂等命中，不重建、不重置任何字段
    TEST_ASSERT(store.accept(1001, 999, "OTHER", 1100).duplicate);
    TEST_ASSERT(store.total_count() == 1);
    TEST_ASSERT(store.find(1001)->payload == R"({"item":"gem","qty":3})");
    TEST_ASSERT(store.find(1001)->player_id == 77);
    TEST_ASSERT(store.find(1001)->received_ms == 1000);
    // 行格式/身份面拒绝
    TEST_ASSERT(!store.accept(1002, 88, "a|b", 1200).accepted);
    TEST_ASSERT(!store.accept(1003, 88, "a\nb", 1300).accepted);
    TEST_ASSERT(!store.accept(0, 88, "x", 1400).accepted);
    TEST_ASSERT(!store.accept(1004, 0, "x", 1500).accepted);
    TEST_ASSERT(store.pending_count() == 1 && store.total_count() == 1);
}

// ---- 2. 重启恢复 + 压薄 ----
static void case_restart_recovers_pending_and_tombstones() {
    const std::string path = journal_for("restart.journal");
    {
        orders::OrderStore a(path);
        TEST_ASSERT(a.open());
        TEST_ASSERT(a.accept(1, 10, "p1", 10).accepted);
        TEST_ASSERT(a.accept(2, 11, "p2", 20).accepted);
        TEST_ASSERT(a.accept(3, 12, "p3", 30).accepted);
        TEST_ASSERT(a.mark_delivered(2, 4, 40));
        TEST_ASSERT(a.pending_count() == 2 && a.delivered_count() == 1);
    }  // 进程退出（模拟崩溃重启）
    {
        orders::OrderStore b(path);
        TEST_ASSERT(b.open());
        TEST_ASSERT(b.total_count() == 3);
        TEST_ASSERT(b.pending_count() == 2 && b.delivered_count() == 1);
        const auto p = b.pending();
        TEST_ASSERT(p.size() == 2 && p[0].order_id == 1 && p[1].order_id == 3);
        TEST_ASSERT(b.find(2)->delivered());
        TEST_ASSERT(b.find(2)->delivered_base_generation == 4);
        TEST_ASSERT(b.find(2)->delivered_ms == 40);
        // 已投递墓碑跨重启仍幂等：重发不复活
        TEST_ASSERT(b.accept(2, 11, "p2-retry", 50).duplicate);
        TEST_ASSERT(b.find(2)->payload == "p2");
        TEST_ASSERT(b.accept(4, 13, "p4", 60).accepted);
        TEST_ASSERT(b.total_count() == 4);
        // open 压薄：每单一行（3 单压薄 + 新单追加 = 4 行）
        std::ifstream in(path);
        int lines = 0;
        for (std::string line; std::getline(in, line);) {
            ++lines;
        }
        TEST_ASSERT(lines == 4);
        // 压薄后受理序仍按原始顺序
        const auto p2 = b.pending();
        TEST_ASSERT(p2.size() == 3);
        TEST_ASSERT(p2[0].order_id == 1 && p2[1].order_id == 3 && p2[2].order_id == 4);
    }
}

// ---- 3/5. 确认收到 ≠ 发货完成；完成记账归属代次 ----
static void case_receipt_is_not_delivery_and_records_generation() {
    orders::OrderStore store(journal_for("delivery.journal"));
    TEST_ASSERT(store.open());
    TEST_ASSERT(store.accept(50, 21, "buy-gem", 1000).accepted);
    // 受理回执后订单仍待办：发货未完成
    TEST_ASSERT(store.pending_count() == 1);
    TEST_ASSERT(store.delivered_count() == 0);
    TEST_ASSERT(!store.find(50)->delivered());
    TEST_ASSERT(store.find(50)->delivered_base_generation == 0);

    orders::BaseResolver resolver = [](std::uint64_t) {
        return orders::BaseEndpoint{"base-7:8001", 3};
    };
    std::string seen_address;
    std::string seen_payload;
    orders::DeliveryChannel channel =
        [&](const orders::OrderRecord& r, const orders::BaseEndpoint& b) {
            seen_address = b.address;
            seen_payload = r.payload;
            return true;
        };
    orders::OrderDelivery driver(store, resolver, channel);
    const auto stats = driver.tick(1001);
    TEST_ASSERT(stats.processed == 1 && stats.delivered == 1 && stats.deferred == 0);
    TEST_ASSERT(seen_address == "base-7:8001");
    TEST_ASSERT(seen_payload == "buy-gem");
    TEST_ASSERT(store.pending_count() == 0 && store.delivered_count() == 1);
    TEST_ASSERT(store.find(50)->delivered_base_generation == 3);
    TEST_ASSERT(store.find(50)->delivered_ms == 1001);
    TEST_ASSERT(driver.attempts(50) == 0);  // 成功不记失败
    TEST_ASSERT(driver.last_error(50) == nullptr);
    TEST_ASSERT(driver.resolver_calls() == 1);
    // 已投递单不再进待办队列（也不触发解析）
    const auto second = driver.tick(1002);
    TEST_ASSERT(second.processed == 0 && second.delivered == 0);
    TEST_ASSERT(driver.resolver_calls() == 1);
}

// ---- 6. 无 Base 挂起、玩家回来后投出 ----
static void case_holds_without_base_then_recovers() {
    orders::OrderStore store(journal_for("nobase.journal"));
    TEST_ASSERT(store.open());
    TEST_ASSERT(store.accept(60, 31, "skin", 1000).accepted);
    std::optional<orders::BaseEndpoint> current;  // 玩家离线/迁移中
    orders::BaseResolver resolver = [&](std::uint64_t) { return current; };
    int channel_calls = 0;
    orders::DeliveryChannel channel =
        [&](const orders::OrderRecord&, const orders::BaseEndpoint&) {
            ++channel_calls;
            return true;
        };
    orders::DeliveryConfig cfg;
    cfg.initial_backoff_ms = 1000;
    orders::OrderDelivery driver(store, resolver, channel, cfg);

    const auto s1 = driver.tick(2000);
    TEST_ASSERT(s1.processed == 1 && s1.delivered == 0 && s1.deferred == 1);
    TEST_ASSERT(channel_calls == 0);  // 无 Base 不发通道
    TEST_ASSERT(store.pending_count() == 1);  // 待办不丢
    TEST_ASSERT(driver.attempts(60) == 1);
    TEST_ASSERT(driver.last_error(60) != nullptr);
    TEST_ASSERT(*driver.last_error(60) == "no current base");

    const auto s2 = driver.tick(2500);  // 退避未到期：不发
    TEST_ASSERT(s2.processed == 0);
    TEST_ASSERT(driver.attempts(60) == 1);

    current = orders::BaseEndpoint{"base-9:8002", 5};  // 玩家回来了
    const auto s3 = driver.tick(3000);
    TEST_ASSERT(s3.delivered == 1 && s3.deferred == 0);
    TEST_ASSERT(channel_calls == 1);
    TEST_ASSERT(store.pending_count() == 0);
    TEST_ASSERT(store.find(60)->delivered_base_generation == 5);
    TEST_ASSERT(driver.resolver_calls() == 2);
}

// ---- 5. 每次尝试重新解析：迁移/升级换归属下拍即投新家 ----
static void case_re_resolves_base_every_attempt() {
    orders::OrderStore store(journal_for("migrate.journal"));
    TEST_ASSERT(store.open());
    TEST_ASSERT(store.accept(70, 41, "topup", 1000).accepted);

    std::vector<std::string> seen;  // 每次通道调用看到的地址
    std::string endpoint = "base-A:9001";
    std::uint64_t gen = 1;
    bool channel_ok = false;
    orders::BaseResolver resolver = [&](std::uint64_t) {
        return orders::BaseEndpoint{endpoint, gen};
    };
    orders::DeliveryChannel channel =
        [&](const orders::OrderRecord&, const orders::BaseEndpoint& b) {
            seen.push_back(b.address);
            return channel_ok;
        };
    orders::DeliveryConfig cfg;
    cfg.initial_backoff_ms = 1000;
    orders::OrderDelivery driver(store, resolver, channel, cfg);

    driver.tick(2000);  // base-A 拒绝
    TEST_ASSERT(seen.size() == 1 && seen[0] == "base-A:9001");
    TEST_ASSERT(driver.attempts(70) == 1);
    TEST_ASSERT(store.pending_count() == 1);  // 未完成：不冒充完成

    endpoint = "base-B:9002";  // 迁移/升级：归属换 base-B，代次 +1
    gen = 2;
    driver.tick(3000);  // base-B 仍拒绝（驱动不缓存旧地址）
    TEST_ASSERT(seen.size() == 2 && seen[1] == "base-B:9002");
    TEST_ASSERT(driver.attempts(70) == 2);
    TEST_ASSERT(driver.resolver_calls() == 2);  // 尝试数 == 解析数

    channel_ok = true;
    driver.tick(5000);  // base-B 完成
    TEST_ASSERT(store.pending_count() == 0);
    TEST_ASSERT(store.find(70)->delivered_base_generation == 2);
    TEST_ASSERT(store.find(70)->delivered_ms == 5000);
    TEST_ASSERT(driver.attempts(70) == 0);  // 成功清零
    TEST_ASSERT(driver.last_error(70) == nullptr);
}

// ---- 7. 指数退避表 + 每拍预算 ----
static void case_backoff_schedule_and_tick_budget() {
    orders::OrderStore store(journal_for("backoff.journal"));
    TEST_ASSERT(store.open());
    for (int i = 0; i < 4; ++i) {
        TEST_ASSERT(store.accept(100 + i, 10 + i, "p", 0).accepted);
    }
    orders::BaseResolver resolver = [](std::uint64_t) {
        return orders::BaseEndpoint{"base-x:1", 1};
    };
    orders::DeliveryChannel channel =
        [](const orders::OrderRecord&, const orders::BaseEndpoint&) {
            return false;  // 通道持续拒绝
        };
    orders::DeliveryConfig cfg;
    cfg.initial_backoff_ms = 1000;
    cfg.max_backoff_ms = 60000;
    cfg.max_attempts_per_tick = 2;
    orders::OrderDelivery driver(store, resolver, channel, cfg);

    // 退避表：1000→2000→4000→…→32000→64000 封顶 60000
    TEST_ASSERT(driver.backoff_for(0) == 1000);
    TEST_ASSERT(driver.backoff_for(1) == 1000);
    TEST_ASSERT(driver.backoff_for(2) == 2000);
    TEST_ASSERT(driver.backoff_for(3) == 4000);
    TEST_ASSERT(driver.backoff_for(4) == 8000);
    TEST_ASSERT(driver.backoff_for(5) == 16000);
    TEST_ASSERT(driver.backoff_for(6) == 32000);
    TEST_ASSERT(driver.backoff_for(7) == 60000);
    TEST_ASSERT(driver.backoff_for(50) == 60000);

    // 每拍预算 2：4 个待办分两拍推进（受理序前 2 先走）
    const auto s1 = driver.tick(1000);
    TEST_ASSERT(s1.processed == 2 && s1.delivered == 0 && s1.deferred == 2);
    TEST_ASSERT(store.pending_count() == 4);
    const auto s2 = driver.tick(2000);
    TEST_ASSERT(s2.processed == 2);
    TEST_ASSERT(driver.attempts(100) == 2 && driver.attempts(101) == 2);
    TEST_ASSERT(driver.attempts(102) == 0 && driver.attempts(103) == 0);

    // 队头退避中时预算让给队尾：待办不因队头在退避而饿死
    driver.tick(3000);  // 100/101 未到期，预算轮到 102/103
    TEST_ASSERT(driver.attempts(100) == 2 && driver.attempts(101) == 2);
    TEST_ASSERT(driver.attempts(102) == 1 && driver.attempts(103) == 1);

    // 队头 100 号实拍退避节奏：+1000 → +2000 → +4000 → +8000 → +16000 →
    // +32000 → 封顶 60000
    driver.tick(5000);
    TEST_ASSERT(driver.attempts(100) == 3);
    driver.tick(9000);
    TEST_ASSERT(driver.attempts(100) == 4);
    driver.tick(17000);
    TEST_ASSERT(driver.attempts(100) == 5);
    driver.tick(33000);
    TEST_ASSERT(driver.attempts(100) == 6);
    driver.tick(65000);
    TEST_ASSERT(driver.attempts(100) == 7);
    driver.tick(125000);
    TEST_ASSERT(driver.attempts(100) == 8);
    TEST_ASSERT(store.pending_count() == 4);  // 持续失败：待办始终不丢
}

// ---- 8. 渠道重发不重置投递进度 ----
static void case_duplicate_notification_keeps_retry_progress() {
    orders::OrderStore store(journal_for("dup.journal"));
    TEST_ASSERT(store.open());
    TEST_ASSERT(store.accept(80, 51, "topup-80", 1000).accepted);
    std::optional<orders::BaseEndpoint> none;  // 当前无 Base
    orders::BaseResolver resolver = [&](std::uint64_t) { return none; };
    orders::DeliveryChannel channel =
        [](const orders::OrderRecord&, const orders::BaseEndpoint&) {
            return true;
        };
    orders::DeliveryConfig cfg;
    cfg.initial_backoff_ms = 1000;
    orders::OrderDelivery driver(store, resolver, channel, cfg);

    driver.tick(2000);  // 无 Base：退避 1000 → 下次 3000
    TEST_ASSERT(driver.attempts(80) == 1);
    // 渠道同号重发：幂等命中，投递进度（退避/失败计数/载荷）不动
    TEST_ASSERT(store.accept(80, 51, "topup-80-dup", 2500).duplicate);
    TEST_ASSERT(store.find(80)->received_ms == 1000);
    TEST_ASSERT(store.find(80)->payload == "topup-80");
    TEST_ASSERT(store.pending_count() == 1);
    const auto s = driver.tick(2600);  // 退避未到期：不发
    TEST_ASSERT(s.processed == 0);
    TEST_ASSERT(driver.attempts(80) == 1);
    driver.tick(3000);  // 到期：按原进度重试
    TEST_ASSERT(driver.attempts(80) == 2);
    TEST_ASSERT(driver.last_error(80) != nullptr);
    TEST_ASSERT(*driver.last_error(80) == "no current base");
}

// ---- 4. 损坏 journal 显式抛错 ----
static void case_corrupt_journal_is_fatal() {
    const std::string path = journal_for("corrupt.journal");
    {
        orders::OrderStore a(path);
        TEST_ASSERT(a.open());
        TEST_ASSERT(a.accept(1, 1, "ok", 1).accepted);
    }
    {  // 追加一行垃圾（磁盘写坏/截断模拟）
        std::ofstream out(path, std::ios::app | std::ios::binary);
        out << "garbage-without-pipes\n";
        out.flush();
    }
    bool threw = false;
    try {
        orders::OrderStore b(path);
        (void)b.open();
    } catch (const std::runtime_error& e) {
        threw = std::string(e.what()).find("order journal corrupt") != std::string::npos;
    }
    TEST_ASSERT(threw);  // 待办清单不允许静默部分回放
}

// ---- 9. Base 唯一激活/写权登记（并发登录/离线激活）----
static void case_authority_activate_supersede_and_release() {
    orders::BaseAuthorityRegistry auth(journal_for("authority.journal"));
    TEST_ASSERT(auth.open());
    const auto a1 = auth.activate(10, "base-A:9001", 1000);
    TEST_ASSERT(a1.generation == 1 && a1.address == "base-A:9001");
    auto cur = auth.resolve(10);
    TEST_ASSERT(cur && cur->address == "base-A:9001" && cur->generation == 1);
    // 并发登录：后到 Base 顶替先到，代次 +1
    const auto a2 = auth.activate(10, "base-B:9002", 2000);
    TEST_ASSERT(a2.generation == 2);
    cur = auth.resolve(10);
    TEST_ASSERT(cur && cur->address == "base-B:9002" && cur->generation == 2);
    // 离线激活：不同玩家独立登记，无会话面参与
    const auto b1 = auth.activate(20, "base-C:9003", 3000);
    TEST_ASSERT(b1.generation == 1);
    TEST_ASSERT(auth.holder_count() == 2);
    // 被顶替旧 Base 迟到释放：无效，不动新归属
    TEST_ASSERT(!auth.release(10, 1));
    TEST_ASSERT(auth.resolve(10)->generation == 2);
    TEST_ASSERT(auth.release(10, 2));  // 当前代次可释放
    TEST_ASSERT(!auth.resolve(10));
    TEST_ASSERT(auth.holder_count() == 1);
    // 空身份/空地址拒（不占代次）
    TEST_ASSERT(auth.activate(0, "x", 1).generation == 0);
    TEST_ASSERT(auth.activate(30, "", 1).generation == 0);
    TEST_ASSERT(auth.holder_count() == 1);
    // 释放后同代次再释放：无效（双保险）
    TEST_ASSERT(!auth.release(10, 2));
}

// ---- 9b. 代次水位跨重启不回退（接口/存储重启）----
static void case_authority_generation_survives_restart() {
    const std::string path = journal_for("authority-restart.journal");
    {
        orders::BaseAuthorityRegistry a(path);
        TEST_ASSERT(a.open());
        TEST_ASSERT(a.activate(10, "base-A:1", 1000).generation == 1);
        TEST_ASSERT(a.activate(10, "base-B:1", 2000).generation == 2);
        TEST_ASSERT(a.activate(20, "base-C:1", 3000).generation == 1);
        TEST_ASSERT(a.release(20, 1));  // 释放事件持久
    }
    {
        orders::BaseAuthorityRegistry b(path);
        TEST_ASSERT(b.open());
        const auto cur = b.resolve(10);
        TEST_ASSERT(cur && cur->address == "base-B:1" && cur->generation == 2);
        TEST_ASSERT(!b.resolve(20));  // 释放事件回放：无主
        const auto a3 = b.activate(10, "base-D:1", 5000);
        TEST_ASSERT(a3.generation == 3);  // 水位不回退（重启不能发旧代次）
        const auto b2 = b.activate(20, "base-E:1", 6000);
        TEST_ASSERT(b2.generation == 2);  // 释放不降水位
    }
}

// ---- 10. 履约事务：发货 + 去重凭据（通知重复/乱序）----
static void case_fulfillment_grants_and_dedups() {
    orders::OrderStore store(journal_for("fulfill.journal"));
    TEST_ASSERT(store.open());
    TEST_ASSERT(store.accept(7001, 50, "gem-7001", 1).accepted);
    TEST_ASSERT(store.accept(7002, 50, "gem-7002", 2).accepted);
    orders::BaseAuthorityRegistry auth(journal_for("fulfill-auth.journal"));
    TEST_ASSERT(auth.open());
    TEST_ASSERT(auth.activate(50, "base-X:1", 1).generation == 1);
    orders::OrderFulfillment fx(store, journal_for("fulfill-cred.journal"),
                                [&](std::uint64_t p) { return auth.resolve(p); });
    std::vector<std::uint64_t> applied;
    TEST_ASSERT(fx.open([&](const orders::OrderRecord& r) { applied.push_back(r.order_id); }));

    const orders::BaseEndpoint ep{"base-X:1", 1};
    const auto r1 = fx.fulfill(7001, ep, 2000);
    TEST_ASSERT(r1.granted && !r1.duplicate && !r1.rejected);
    TEST_ASSERT(applied.size() == 1 && applied[0] == 7001);
    TEST_ASSERT(fx.has_credential(7001) && fx.credential_count() == 1);
    // 通知重复：同单再投 → 幂等短路，不二次发奖
    const auto r2 = fx.fulfill(7001, ep, 2100);
    TEST_ASSERT(r2.granted && r2.duplicate && !r2.rejected);
    TEST_ASSERT(applied.size() == 1);
    // 乱序：后接收单先履约，独立事务互不影响
    const auto r3 = fx.fulfill(7002, ep, 2200);
    TEST_ASSERT(r3.granted && !r3.duplicate && !r3.rejected);
    TEST_ASSERT(applied.size() == 2 && applied[1] == 7002);
    TEST_ASSERT(fx.credential_count() == 2);
}

// ---- 10b. 订单校验 + Base 唯一写权（迁移/并发登录迟到履约拒）----
static void case_fulfillment_validates_order_and_write_authority() {
    orders::OrderStore store(journal_for("fulfill-validate.journal"));
    TEST_ASSERT(store.open());
    TEST_ASSERT(store.accept(7101, 60, "skin-7101", 1).accepted);
    TEST_ASSERT(store.accept(7102, 61, "skin-7102", 2).accepted);
    orders::BaseAuthorityRegistry auth(journal_for("fulfill-validate-auth.journal"));
    TEST_ASSERT(auth.open());
    TEST_ASSERT(auth.activate(60, "base-Y:1", 1).generation == 1);
    orders::OrderFulfillment fx(store, journal_for("fulfill-validate-cred.journal"),
                                [&](std::uint64_t p) { return auth.resolve(p); });
    std::vector<std::uint64_t> applied;
    TEST_ASSERT(fx.open([&](const orders::OrderRecord& r) { applied.push_back(r.order_id); }));

    const orders::BaseEndpoint ep{"base-Y:1", 1};
    // 未受理单号拒（伪造单不发货）
    const auto r0 = fx.fulfill(9999, ep, 100);
    TEST_ASSERT(r0.rejected && r0.reason == "unknown order");
    // 玩家 61 无归属（离线/目录丢失）：拒
    const auto r1 = fx.fulfill(7102, ep, 100);
    TEST_ASSERT(r1.rejected && r1.reason == "no write authority");
    // 迁移/并发登录：权威移到 base-Z gen2，旧 Base（gen1）迟到履约拒
    TEST_ASSERT(auth.activate(60, "base-Z:2", 2).generation == 2);
    const orders::BaseEndpoint stale{"base-Y:1", 1};
    const auto r2 = fx.fulfill(7101, stale, 100);
    TEST_ASSERT(r2.rejected && r2.reason == "stale write authority");
    TEST_ASSERT(applied.empty());
    // 新归属（gen2）履约通过
    const auto r3 = fx.fulfill(7101, orders::BaseEndpoint{"base-Z:2", 2}, 100);
    TEST_ASSERT(r3.granted && !r3.rejected);
    TEST_ASSERT(applied.size() == 1 && applied[0] == 7101);
}

// ---- 10c. 崩溃窗回放补齐（回执丢失/接口存储重启）----
static void case_fulfillment_replay_covers_crash_window_and_restart() {
    const std::string cred = journal_for("fulfill-replay-cred.journal");
    {
        orders::OrderStore store(journal_for("fulfill-replay.journal"));
        TEST_ASSERT(store.open());
        TEST_ASSERT(store.accept(8001, 70, "box-8001", 1).accepted);
        orders::BaseAuthorityRegistry auth(journal_for("fulfill-replay-auth.journal"));
        TEST_ASSERT(auth.open());
        TEST_ASSERT(auth.activate(70, "base-W:1", 1).generation == 1);
        orders::OrderFulfillment fx(store, cred, [&](std::uint64_t p) { return auth.resolve(p); });
        std::vector<std::uint64_t> applied;
        TEST_ASSERT(fx.open([&](const orders::OrderRecord& r) { applied.push_back(r.order_id); }));
        TEST_ASSERT(fx.fulfill(8001, orders::BaseEndpoint{"base-W:1", 1}, 1000).granted);
        TEST_ASSERT(applied.size() == 1);
    }  // 进程退出（凭据已记账）
    {
        // 新实例恢复：回放应重放 apply，且凭据幂等短路重投
        orders::OrderStore store(journal_for("fulfill-replay.journal"));
        TEST_ASSERT(store.open());
        TEST_ASSERT(store.accept(8001, 70, "box-8001", 1).duplicate);
        orders::BaseAuthorityRegistry auth(journal_for("fulfill-replay-auth.journal"));
        TEST_ASSERT(auth.open());
        TEST_ASSERT(auth.activate(70, "base-W:1", 1).generation == 2);  // 重启后重新激活（新代次）
        orders::OrderFulfillment fx(store, cred, [&](std::uint64_t p) { return auth.resolve(p); });
        std::vector<std::uint64_t> reapplied;
        TEST_ASSERT(fx.open([&](const orders::OrderRecord& r) { reapplied.push_back(r.order_id); }));
        TEST_ASSERT(reapplied.size() == 1 && reapplied[0] == 8001);  // 回放补齐
        // 回执丢失/重投：幂等短路，不再 apply
        const auto r = fx.fulfill(8001, orders::BaseEndpoint{"base-W:1", 2}, 2000);
        TEST_ASSERT(r.granted && r.duplicate && !r.rejected);
        TEST_ASSERT(reapplied.size() == 1);
        // 重启后新单照常发货
        TEST_ASSERT(store.accept(8002, 70, "box-8002", 2).accepted);
        const auto r2 = fx.fulfill(8002, orders::BaseEndpoint{"base-W:1", 2}, 2100);
        TEST_ASSERT(r2.granted && !r2.duplicate);
        TEST_ASSERT(reapplied.size() == 2 && reapplied[1] == 8002);
    }
}

// ---- 10d. 全链集成：交付 → 履约 → 记账（迁移后旧 Base 迟履约拒、新代次落账）----
static void case_delivery_to_fulfillment_loop_with_migration() {
    orders::OrderStore store(journal_for("loop.journal"));
    TEST_ASSERT(store.open());
    TEST_ASSERT(store.accept(9001, 80, "topup-9001", 1).accepted);
    orders::BaseAuthorityRegistry auth(journal_for("loop-auth.journal"));
    TEST_ASSERT(auth.open());
    TEST_ASSERT(auth.activate(80, "base-A:1", 1).generation == 1);
    TEST_ASSERT(auth.activate(80, "base-B:1", 60).generation == 2);  // 迁移：B 接管
    orders::OrderFulfillment fx(store, journal_for("loop-cred.journal"),
                                [&](std::uint64_t p) { return auth.resolve(p); });
    std::vector<std::uint64_t> applied;
    orders::OrderFulfillment::RewardApply apply =
        [&](const orders::OrderRecord& r) { applied.push_back(r.order_id); };
    TEST_ASSERT(fx.open(apply));

    // 投递驱动使用「滞后路由」（模拟路由目录未刷新）：仍指向旧 base-A。
    std::optional<orders::BaseEndpoint> route = orders::BaseEndpoint{"base-A:1", 1};
    std::vector<std::string> rejections;
    std::int64_t channel_now = 0;
    orders::DeliveryChannel channel =
        [&](const orders::OrderRecord& r, const orders::BaseEndpoint& b) {
            const auto res = fx.fulfill(r.order_id, b, channel_now);
            if (res.rejected) {
                rejections.push_back(res.reason);
                return false;
            }
            return res.granted;
        };
    orders::DeliveryConfig cfg;
    cfg.initial_backoff_ms = 1000;
    orders::OrderDelivery driver(store, [&](std::uint64_t) { return route; },
                                 channel, cfg);

    channel_now = 1000;
    driver.tick(1000);  // 旧 Base 迟到履约：事务层校验拒（写权不符）
    TEST_ASSERT(store.pending_count() == 1);  // 不冒充完成
    TEST_ASSERT(rejections.size() == 1 && rejections[0] == "stale write authority");
    TEST_ASSERT(applied.empty());
    TEST_ASSERT(driver.attempts(9001) == 1);

    route = auth.resolve(80);  // 路由刷新到真实权威（base-B/gen2）
    channel_now = 3000;
    const auto s = driver.tick(3000);
    TEST_ASSERT(s.delivered == 1 && s.deferred == 0);
    TEST_ASSERT(store.pending_count() == 0);
    TEST_ASSERT(store.find(9001)->delivered_base_generation == 2);  // 记账新代次
    TEST_ASSERT(applied.size() == 1 && applied[0] == 9001);
    TEST_ASSERT(driver.attempts(9001) == 0);  // 成功后失败计数清零
    TEST_ASSERT(driver.resolver_calls() == 2);  // 每次尝试重解析（不缓存）
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);  // 崩溃前 FAIL 行不丢（abort 不冲缓冲）
    kRoot = fs::path("/tmp") /
            ("apollo-orders-tests-" + std::to_string(::getpid()));
    fs::remove_all(kRoot);
    fs::create_directories(kRoot);  // 显式建目录（临时目录不自动创建的坑）

    case_accept_validates_and_dedups();
    case_restart_recovers_pending_and_tombstones();
    case_receipt_is_not_delivery_and_records_generation();
    case_holds_without_base_then_recovers();
    case_re_resolves_base_every_attempt();
    case_backoff_schedule_and_tick_budget();
    case_duplicate_notification_keeps_retry_progress();
    case_corrupt_journal_is_fatal();
    case_authority_activate_supersede_and_release();
    case_authority_generation_survives_restart();
    case_fulfillment_grants_and_dedups();
    case_fulfillment_validates_order_and_write_authority();
    case_fulfillment_replay_covers_crash_window_and_restart();
    case_delivery_to_fulfillment_loop_with_migration();

    fs::remove_all(kRoot);
    std::printf("orders tests: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
