// MetricRegistry 最小集单测（P3-3 批 B）。
//
// 覆盖（本批口径，见 modules/core/metrics/include 头注设计依据）：
//   1. Counter：只增语义、increment(n) 批量、并发 fetch_add 正确性；
//   2. Gauge：add/store 双通道；
//   3. 注册幂等与引用稳定：同键返回同引用、值跨查找保持；
//   4. 标签规范化：键序不影响同一性（{a,b} ≡ {b,a}）；
//   5. 类型分居：同名 counter/gauge 互不串值；
//   6. snapshot：值正确、稳定排序（确定性输出）、标签随行；
//   7. 全局单例 instance() 可用且独立于局部实例。

#include "apollo/core/metrics/metric_registry.h"

#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

#define TEST_ASSERT(cond, msg)                                                               \
    do {                                                                                     \
        if (!(cond)) {                                                                       \
            std::cerr << "FAIL: " << (msg) << " (" << __FILE__ << ":" << __LINE__ << ")"     \
                      << std::endl;                                                          \
            return false;                                                                    \
        }                                                                                    \
    } while (0)

namespace {

using apollo::core::metrics::Counter;
using apollo::core::metrics::Gauge;
using apollo::core::metrics::Labels;
using apollo::core::metrics::MetricRegistry;

bool test_counter_semantics() {
    MetricRegistry reg;
    Counter& c = reg.counter("script_fault_total", {{"module", "login"}, {"kind", "fault"}});
    TEST_ASSERT(c.load() == 0, "新计数器从 0 起");
    c.increment();
    c.increment();
    c.increment(5);
    TEST_ASSERT(c.load() == 7, "increment(1)+increment(1)+increment(5) = 7");
    return true;
}

bool test_counter_concurrent_increments() {
    MetricRegistry reg;
    Counter& c = reg.counter("concurrent_total");
    constexpr int kThreads = 8;
    constexpr std::uint64_t kPerThread = 100000;
    std::vector<std::thread> threads;
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&c] {
            for (std::uint64_t n = 0; n < kPerThread; ++n) {
                c.increment();
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    TEST_ASSERT(c.load() == kThreads * kPerThread, "8 线程 × 10 万次自增无丢失");
    return true;
}

bool test_gauge_semantics() {
    MetricRegistry reg;
    Gauge& g = reg.gauge("scene_entity_count", {{"scene", "1"}});
    TEST_ASSERT(g.load() == 0, "新仪表从 0 起");
    g.add(100);
    g.add(-30);
    TEST_ASSERT(g.load() == 70, "add 正负可增减");
    g.store(1000);
    TEST_ASSERT(g.load() == 1000, "store 覆写（心跳版本号类整存）");
    return true;
}

bool test_registration_idempotent_and_reference_stable() {
    MetricRegistry reg;
    Counter& first = reg.counter("lookup_total", {{"op", "q"}});
    first.increment(3);
    Counter& second = reg.counter("lookup_total", {{"op", "q"}});
    TEST_ASSERT(&first == &second, "同键返回同引用");
    TEST_ASSERT(second.load() == 3, "重复查找不重置值");
    second.increment(2);
    TEST_ASSERT(first.load() == 5, "引用互通（同一存储）");
    return true;
}

bool test_label_order_canonicalized() {
    MetricRegistry reg;
    Counter& a = reg.counter("k_total", {{"b", "2"}, {"a", "1"}});
    a.increment(9);
    Counter& b = reg.counter("k_total", {{"a", "1"}, {"b", "2"}});
    TEST_ASSERT(&a == &b, "标签键序不影响同一性");
    TEST_ASSERT(b.load() == 9, "规范化命中同一存储");
    return true;
}

bool test_counter_gauge_type_separation() {
    MetricRegistry reg;
    Counter& c = reg.counter("dup_total");
    Gauge& g = reg.gauge("dup_total");
    c.increment(4);
    g.store(11);
    TEST_ASSERT(c.load() == 4 && g.load() == 11, "同名 counter/gauge 分居互不串");
    return true;
}

bool test_snapshot_sorted_and_complete() {
    MetricRegistry reg;
    reg.gauge("scene_entity_count", {{"scene", "2"}}).store(20);
    reg.gauge("scene_entity_count", {{"scene", "1"}}).store(10);
    reg.counter("script_fault_total", {{"module", "login"}, {"kind", "fault"}}).increment(3);
    reg.counter("script_fault_total", {{"module", "battle"}, {"kind", "budget"}}).increment(1);

    const auto snap = reg.snapshot();
    TEST_ASSERT(snap.size() == 4, "快照全量");
    // 稳定序：name 升序，同名标签字典序
    TEST_ASSERT(snap[0].name == "scene_entity_count" &&
                    snap[0].labels == Labels({{"scene", "1"}}) &&
                    snap[0].gauge_value == 10,
                "gauge 同名按标签序");
    TEST_ASSERT(snap[1].name == "scene_entity_count" && snap[1].gauge_value == 20,
                "gauge 第二行");
    TEST_ASSERT(snap[2].name == "script_fault_total" &&
                    snap[2].labels == Labels({{"kind", "budget"}, {"module", "battle"}}) &&
                    snap[2].counter_value == 1 && snap[2].is_counter,
                "counter 标签规范序（budget < fault）");
    TEST_ASSERT(snap[3].labels == Labels({{"kind", "fault"}, {"module", "login"}}) &&
                    snap[3].counter_value == 3,
                "counter 第二行");

    // 两次快照逐位一致（确定性输出——exporter 文本可回归比对）
    const auto snap2 = reg.snapshot();
    TEST_ASSERT(snap.size() == snap2.size(), "两次快照等长");
    bool identical = true;
    for (std::size_t i = 0; i < snap.size(); ++i) {
        identical = identical && snap[i].name == snap2[i].name &&
                    snap[i].labels == snap2[i].labels &&
                    snap[i].is_counter == snap2[i].is_counter &&
                    snap[i].counter_value == snap2[i].counter_value &&
                    snap[i].gauge_value == snap2[i].gauge_value;
    }
    TEST_ASSERT(identical, "快照确定性");
    return true;
}

bool test_global_instance_usable() {
    Counter& c = MetricRegistry::instance().counter("metrics_test_global_total");
    c.increment(1);
    const auto before = c.load();
    TEST_ASSERT(before >= 1, "进程级单例可用");
    // 单例与局部实例互不相干：局部同名计数器自增不改单例值
    MetricRegistry local;
    local.counter("metrics_test_global_total").increment(100);
    TEST_ASSERT(c.load() == before, "局部实例不污染单例");
    return true;
}

} // namespace

int main() {
    int passed = 0;
    int failed = 0;

    struct Case {
        const char* name;
        bool (*fn)();
    } cases[] = {
        {"counter_semantics", test_counter_semantics},
        {"counter_concurrent_increments", test_counter_concurrent_increments},
        {"gauge_semantics", test_gauge_semantics},
        {"registration_idempotent_and_reference_stable",
         test_registration_idempotent_and_reference_stable},
        {"label_order_canonicalized", test_label_order_canonicalized},
        {"counter_gauge_type_separation", test_counter_gauge_type_separation},
        {"snapshot_sorted_and_complete", test_snapshot_sorted_and_complete},
        {"global_instance_usable", test_global_instance_usable},
    };

    for (const auto& c : cases) {
        std::cout << "[ RUN  ] " << c.name << std::endl;
        if (c.fn()) {
            std::cout << "[  OK  ] " << c.name << std::endl;
            ++passed;
        } else {
            std::cout << "[ FAIL ] " << c.name << std::endl;
            ++failed;
        }
    }

    std::cout << "MetricsTests: " << passed << " passed, " << failed << " failed" << std::endl;
    return failed == 0 ? 0 : 1;
}
