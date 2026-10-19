// 订单/通知可靠受理与持久待办测试（todo L63 批⑯）。
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
//  8. 渠道重发不重置投递进度。

#include "apollo/game/orders/order_delivery.hpp"
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

int main() {
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

    fs::remove_all(kRoot);
    std::printf("orders tests: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
