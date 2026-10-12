// 告警 notifier 最小面测试（AlertNotifierTests，P3-3 余项批）。
//
// 覆盖（logging.md §5.2 层 B 内核，见 apps/admin/include/admin/
// alert_notifier.hpp 头注）：
//   1. 规则判定：greater/less 阈值、首拍即火（sustain=0）、空规则零开销；
//   2. sustain 跨拍持续窗：未满不火、恢复清零重来；
//   3. silence 静默窗：fire-once，窗内不重火，窗外可再火；
//   4. 全局速率上限：固定窗限额、弃火计数、窗口翻转后再火；
//   5. proc 隔离：同规则跨进程状态互不串；
//   6. exposition 解析：带标签/裸样本/末样本胜/注释与 histogram 族跳过；
//   7. env 规则面：解析、坏条目跳过；
//   8. 缺席清态：度量缺席不挂陈值；
//   9. 出站：sink 注入捕获、异常吞掉不外溢、缺省行文本逐位、缺省计数。
//
// 纯逻辑面（无平台依赖），Windows 同跑。

#include "admin/alert_notifier.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
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

using apollo::admin::AlertEvent;
using apollo::admin::AlertNotifierConfig;
using apollo::admin::AlertNotifier;
using apollo::admin::AlertRule;

AlertRule rule_at(const char* metric, double threshold, bool greater,
                  std::uint64_t sustain_ms = 0,
                  std::uint64_t silence_ms = 60000) {
    AlertRule r;
    r.name = metric;
    r.metric = metric;
    r.threshold = threshold;
    r.greater = greater;
    r.sustain_ms = sustain_ms;
    r.silence_ms = silence_ms;
    return r;
}

bool test_rule_eval_basic() {
    AlertNotifier notifier;
    TEST_ASSERT(notifier.rule_count() == 0, "空规则表");
    notifier.observe("cell", "queue_depth 5\n", 1000);
    TEST_ASSERT(notifier.fired() == 0, "空规则零开销不火");

    std::vector<AlertEvent> events;
    notifier.set_sink([&](const AlertEvent& e) { events.push_back(e); });
    notifier.set_rules({rule_at("queue_depth", 10, true)});
    notifier.observe("cell", "queue_depth 5\n", 1000);
    TEST_ASSERT(notifier.fired() == 0, "阈值内不火");
    notifier.observe("cell", "queue_depth{proc=\"x\"} 11\n", 2000);
    TEST_ASSERT(notifier.fired() == 1 && events.size() == 1, "超限首拍即火");
    TEST_ASSERT(events[0].rule_name == "queue_depth" && events[0].proc == "cell" &&
                    events[0].value == 11 && events[0].threshold == 10 &&
                    events[0].greater && events[0].now_ms == 2000,
                "事件字段封闭集完整");

    notifier.set_rules({rule_at("session_count", 1, false)});
    notifier.observe("gateway", "session_count 0\n", 3000);
    TEST_ASSERT(notifier.fired() == 2, "less 规则低于阈值触发");
    return true;
}

bool test_sustain_window() {
    AlertNotifier notifier;
    std::vector<AlertEvent> events;
    notifier.set_sink([&](const AlertEvent& e) { events.push_back(e); });
    // silence=0：本组只验 sustain 面（silence 独立成组）
    notifier.set_rules({rule_at("frame_ms", 20, true, 100, 0)});
    notifier.observe("cell", "frame_ms 30\n", 0);
    TEST_ASSERT(notifier.fired() == 0, "持续未满不火");
    notifier.observe("cell", "frame_ms 30\n", 50);
    TEST_ASSERT(notifier.fired() == 0, "持续过半不火");
    notifier.observe("cell", "frame_ms 30\n", 100);
    TEST_ASSERT(notifier.fired() == 1, "持续满窗即火");
    // 恢复清零：回落再超限，sustain 重启
    notifier.observe("cell", "frame_ms 5\n", 150);
    notifier.observe("cell", "frame_ms 30\n", 200);
    TEST_ASSERT(notifier.fired() == 1, "恢复后重启 sustain");
    notifier.observe("cell", "frame_ms 30\n", 300);
    TEST_ASSERT(notifier.fired() == 2, "重启后满窗再火");
    return true;
}

bool test_silence_window() {
    AlertNotifier notifier;
    notifier.set_rules({rule_at("queue_depth", 10, true, 0, 1000)});
    notifier.observe("cell", "queue_depth 11\n", 0);
    TEST_ASSERT(notifier.fired() == 1, "首火");
    notifier.observe("cell", "queue_depth 12\n", 500);
    notifier.observe("cell", "queue_depth 13\n", 999);
    TEST_ASSERT(notifier.fired() == 1, "静默窗内不重火");
    notifier.observe("cell", "queue_depth 14\n", 1000);
    TEST_ASSERT(notifier.fired() == 2, "静默窗外可再火");
    return true;
}

bool test_rate_cap() {
    AlertNotifierConfig cfg;
    cfg.rate_window_ms = 60000;
    cfg.max_per_window = 2;
    AlertNotifier notifier(cfg);
    notifier.set_rules({rule_at("m1_total", 0, true),
                        rule_at("m2_total", 0, true),
                        rule_at("m3_total", 0, true)});
    notifier.observe("cell", "m1_total 1\nm2_total 1\nm3_total 1\n", 0);
    TEST_ASSERT(notifier.fired() == 2, "限额内两火");
    TEST_ASSERT(notifier.dropped() == 1, "超限弃火计数");
    // 同窗内继续弃：m3 首次被弃未进静默窗（③弃火不更新②静默）——再拍
    // 重试仍被限额拦（m1 在静默窗内不再试探，②③各归其位）
    notifier.observe("cell", "m3_total 2\n", 1000);
    TEST_ASSERT(notifier.dropped() == 2, "窗内继续弃");
    // 窗口翻转后再火
    notifier.observe("cell", "m1_total 3\n", 60000);
    TEST_ASSERT(notifier.fired() == 3 && notifier.dropped() == 2,
                "窗口翻转后再火");
    return true;
}

bool test_proc_isolation() {
    AlertNotifier notifier;
    std::vector<AlertEvent> events;
    notifier.set_sink([&](const AlertEvent& e) { events.push_back(e); });
    notifier.set_rules({rule_at("queue_depth", 10, true)});
    notifier.observe("cell_a", "queue_depth 11\n", 0);
    notifier.observe("cell_b", "queue_depth 1\n", 0);
    TEST_ASSERT(events.size() == 1 && events[0].proc == "cell_a",
                "proc 状态互不串");
    return true;
}

bool test_exposition_parsing() {
    double v = 0;
    const std::string body =
        "# TYPE fault_total counter\n"
        "fault_total 7\n"
        "# TYPE entity_count gauge\n"
        "entity_count{scene=\"town\"} 42\n"
        "entity_count{scene=\"mine\"} -3\n"
        "# TYPE frame_ms histogram\n"
        "frame_ms_bucket{le=\"0.1\"} 3\n"
        "frame_ms_sum 99.5\n"
        "frame_ms_count 5\n";
    TEST_ASSERT(apollo::admin::exposition_lookup(body, "fault_total", v) && v == 7,
                "裸样本命中");
    TEST_ASSERT(apollo::admin::exposition_lookup(body, "entity_count", v) && v == -3,
                "带标签样本末个胜（负值）");
    TEST_ASSERT(!apollo::admin::exposition_lookup(body, "nope_total", v),
                "缺席不命中");
    // histogram 族段整段不参与匹配（# TYPE <name> histogram 起至下一
    // TYPE）；_count 结尾的常规度量名不误吞（后缀启发式已弃）
    TEST_ASSERT(!apollo::admin::exposition_lookup(body, "frame_ms_bucket", v),
                "bucket 行段内跳过");
    TEST_ASSERT(!apollo::admin::exposition_lookup(body, "frame_ms_sum", v),
                "sum 行段内跳过");
    TEST_ASSERT(!apollo::admin::exposition_lookup(body, "frame_ms_count", v),
                "count 行段内跳过");
    TEST_ASSERT(apollo::admin::exposition_lookup(body, "entity_count", v) &&
                    v == -3.0,
                "_count 结尾常规度量不误吞（末样本胜）");
    return true;
}

bool test_env_rules_parsing() {
    const auto rules = apollo::admin::parse_alert_rules("queue_depth>10;session_count<1;;bad;cut>abc>x");
    TEST_ASSERT(rules.size() == 2, "坏条目跳过（无 op/空段/双 op 尾串）");
    TEST_ASSERT(rules[0].metric == "queue_depth" && rules[0].name == "queue_depth" &&
                    rules[0].greater && rules[0].threshold == 10,
                "greater 条目");
    TEST_ASSERT(rules[1].metric == "session_count" && !rules[1].greater &&
                    rules[1].threshold == 1,
                "less 条目");
    TEST_ASSERT(rules[0].silence_ms == 60000 && rules[0].sustain_ms == 0,
                "sustain/silence 取默认");
    TEST_ASSERT(apollo::admin::parse_alert_rules("").empty(), "空 spec 零规则");
    return true;
}

bool test_missing_metric_clears_state() {
    AlertNotifier notifier;
    notifier.set_rules({rule_at("frame_ms", 20, true, 100)});
    notifier.observe("cell", "frame_ms 30\n", 0);
    notifier.observe("cell", "queue_depth 1\n", 50);  // 度量缺席 = 清态
    notifier.observe("cell", "frame_ms 30\n", 60);
    TEST_ASSERT(notifier.fired() == 0, "缺席清态后 sustain 重启（不挂陈值）");
    notifier.observe("cell", "frame_ms 30\n", 160);
    TEST_ASSERT(notifier.fired() == 1, "重启满窗再火");
    return true;
}

bool test_sink_and_default_line() {
    // sink 抛异常吞掉不外溢，计数照记
    AlertNotifier notifier;
    notifier.set_sink([](const AlertEvent&) { throw std::runtime_error("x"); });
    notifier.set_rules({rule_at("m_total", 0, true)});
    notifier.observe("cell", "m_total 1\n", 0);
    TEST_ASSERT(notifier.fired() == 1, "异常吞掉计数照记");

    // 缺省行文本（cat=alert 行 msg 段）逐位比对
    AlertEvent e;
    e.rule_name = "queue_depth";
    e.proc = "cell@1";
    e.metric = "queue_depth";
    e.value = 11;
    e.threshold = 10;
    e.greater = true;
    e.now_ms = 1234;
    TEST_ASSERT(apollo::admin::alert_log_line(e) ==
                    "alert rule=queue_depth proc=cell@1 metric=queue_depth "
                    "value=11 threshold=10 op=> at_ms=1234",
                "缺省行逐位");
    AlertEvent less = e;
    less.greater = false;
    TEST_ASSERT(apollo::admin::alert_log_line(less).find("op=<") != std::string::npos,
                "less 行 op 标记");
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
        {"rule_eval_basic", test_rule_eval_basic},
        {"sustain_window", test_sustain_window},
        {"silence_window", test_silence_window},
        {"rate_cap", test_rate_cap},
        {"proc_isolation", test_proc_isolation},
        {"exposition_parsing", test_exposition_parsing},
        {"env_rules_parsing", test_env_rules_parsing},
        {"missing_metric_clears_state", test_missing_metric_clears_state},
        {"sink_and_default_line", test_sink_and_default_line},
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

    std::cout << "AlertNotifierTests: " << passed << " passed, " << failed
              << " failed" << std::endl;
    return failed == 0 ? 0 : 1;
}
