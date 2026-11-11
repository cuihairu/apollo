// QosManager/TokenBucket 回归（audit P1#9 修复 2026-11-12）。
// ①consume(timeoutMs=0) 令牌不足时 refill+yield 无限自旋活锁（RateLimiter::
//   acquire 默认路径即中招）→ 0 语义改为立即失败；②cleanupExpired 把
//   steady_clock 纳秒 count 当毫秒构造 time_point → TTL 永不过期 → 统一存毫秒。
// 附带（休眠树从未编译过，直编即暴露）：qos.h 缺 channel.h 包含、引用不存在的
// 枚举值、const 方法锁非 mutable mutex、TokenBucket::setRate 锁不存在成员；
// P2：retry 重新入队不补 pendingMessages → getNext 二次出队计数下溢；
// acquire 返回 available()>=permits 对"恰好消费最后一批"误报失败。
#include "apollo/ipc/qos.h"
#include <chrono>
#include <iostream>
#include <thread>

namespace {

using apollo::ipc::TokenBucket;
using apollo::ipc::QosManager;
using apollo::ipc::QosPolicy;
using apollo::ipc::RateLimiter;
using apollo::ipc::Reliability;

#define TEST_ASSERT(cond, msg) \
    do { if (!(cond)) { std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; return false; } } while (0)

// audit P1#9 活锁：rate=0 桶永不补充，timeoutMs=0 消费空桶必须立即失败返回
// false——原实现无限自旋（此用例在修复前表现为挂死，由超时判负）。
bool test_consume_zero_timeout_fails_immediately() {
    TokenBucket bucket(0, 1);
    const auto t0 = std::chrono::steady_clock::now();
    TEST_ASSERT(bucket.consume(1, 0), "first consume from full bucket");
    TEST_ASSERT(!bucket.consume(1, 0), "second consume fails immediately (no spin)");
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();
    TEST_ASSERT(elapsed < 2000, "fail path returns promptly");
    return true;
}

// RateLimiter::acquire 默认 timeoutMs=0 路径（audit 触发面）+ 返回值真伪：
// 成功消费最后一批令牌后 available()==0 < permits，旧实现误报失败。
bool test_acquire_default_path_and_truthfulness() {
    auto limiter = RateLimiter::create(RateLimiter::Algorithm::TokenBucket, 1000, 1);

    // 成功消费仅剩令牌：必须报 true（旧 available()>=permits 判定报 false）
    TEST_ASSERT(limiter->acquire(1), "acquire last permit reports success");

    // 默认 timeoutMs=0：桶空必须立即失败（rate>0 也不该自旋等待）
    const auto t0 = std::chrono::steady_clock::now();
    TEST_ASSERT(!limiter->acquire(), "default acquire on empty bucket fails immediately");
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();
    TEST_ASSERT(elapsed < 2000, "default acquire does not spin");
    return true;
}

// timeoutMs>0 阻塞路径回归：rate=1000/s 下不足一令牌约 1ms 后补齐，宽上界
// 只防死等、不断言调度速度。
bool test_consume_with_timeout_waits_for_refill() {
    TokenBucket bucket(1000, 1);
    TEST_ASSERT(bucket.consume(1, 0), "initial token");

    const auto t0 = std::chrono::steady_clock::now();
    TEST_ASSERT(bucket.consume(1, 500), "refill within timeout");
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();
    TEST_ASSERT(elapsed < 5000, "bounded wait");

    // 超时耗尽路径：rate=0 的桶永不补充，任何超时等待最终失败而非挂死
    TokenBucket dead(0, 1);
    dead.consume(1, 0);
    TEST_ASSERT(!dead.consume(1, 100), "timeout expiry fails");

    // 超量请求在超时内无法满足（burst=1 只能补到 1）：失败而非挂死
    TokenBucket small(1000, 1);
    small.reset();
    TEST_ASSERT(!small.consume(3, 100), "over-burst request times out");
    return true;
}

// audit P1#9 时间单位：ttlMs=50 的消息在 50ms 后必须被 cleanupExpired 清掉。
// 旧实现纳秒 count 按毫秒构造 time_point → sendTime 在约三万年后 → 永不清。
bool test_ttl_expiry_actually_expires() {
    QosManager mgr;
    QosPolicy qos;
    qos.reliability = Reliability::AtLeastOnce;  // 入 pendingAck_
    qos.ttlMs = 50;

    TEST_ASSERT(mgr.send({1, 2, 3}, qos), "send");
    TEST_ASSERT(mgr.cleanupExpired() == 0, "not expired immediately after send");

    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    TEST_ASSERT(mgr.cleanupExpired() == 1, "expired after ttl");
    TEST_ASSERT(mgr.getStats().timeoutMessages == 1, "timeout stat counted");
    TEST_ASSERT(mgr.cleanupExpired() == 0, "idempotent second sweep");

    // ttlMs=0（无限）永不过期
    QosPolicy forever;
    forever.reliability = Reliability::AtLeastOnce;
    forever.ttlMs = 0;
    TEST_ASSERT(mgr.send({4}, forever), "send ttl=0");
    TEST_ASSERT(mgr.cleanupExpired() == 0, "ttl=0 never expires");
    return true;
}

// audit P2：retry 重新入队不补 pendingMessages，getNext 二次出队时计数下溢。
bool test_retry_requeue_keeps_pending_stats() {
    QosManager mgr;
    QosPolicy qos;
    qos.reliability = Reliability::AtLeastOnce;

    TEST_ASSERT(mgr.send({1}, qos), "send");
    TEST_ASSERT(mgr.getStats().pendingMessages == 1, "pending 1 after send");

    apollo::ipc::PrioritizedMessage first{};
    TEST_ASSERT(mgr.getNext(first), "get first");
    TEST_ASSERT(mgr.getStats().pendingMessages == 0, "pending 0 after pop");

    TEST_ASSERT(mgr.retry(first.sequence), "retry re-queues");
    TEST_ASSERT(mgr.getStats().pendingMessages == 1, "re-queue restored count (old: no increment)");

    apollo::ipc::PrioritizedMessage again{};
    TEST_ASSERT(mgr.getNext(again), "get retried");
    TEST_ASSERT(again.sequence == first.sequence, "same message");
    TEST_ASSERT(mgr.getStats().pendingMessages == 0, "pending 0, never underflowed");
    TEST_ASSERT(!mgr.hasPending(), "drained");
    TEST_ASSERT(mgr.getStats().retriedMessages == 1, "retry stat counted");
    return true;
}

} // namespace

int main() {
    int passed = 0, total = 0;
    auto run = [&](const char* name, bool (*fn)()) {
        std::cout << "== " << name << " ==" << std::endl;
        ++total;
        if (fn()) { ++passed; std::cout << "  PASSED" << std::endl; }
    };
    run("test_consume_zero_timeout_fails_immediately", test_consume_zero_timeout_fails_immediately);
    run("test_acquire_default_path_and_truthfulness", test_acquire_default_path_and_truthfulness);
    run("test_consume_with_timeout_waits_for_refill", test_consume_with_timeout_waits_for_refill);
    run("test_ttl_expiry_actually_expires", test_ttl_expiry_actually_expires);
    run("test_retry_requeue_keeps_pending_stats", test_retry_requeue_keeps_pending_stats);
    std::cout << "=== Results: " << passed << "/" << total << " passed ===" << std::endl;
    return (passed == total) ? 0 : 1;
}
