#include "admin/alert_notifier.hpp"

#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <utility>

namespace apollo::admin {

namespace {

// 行首样本名：至 `{`（带标签）或空白（裸样本）为止；`#` 注释行不参与
// 匹配。histogram 族段（`# TYPE <name> histogram` 起至下一 TYPE）整段
// 跳过——后缀启发式（_bucket/_sum/_count）会误吞 _count 结尾的常规度
// 量名，弃用
bool type_declares_histogram(const std::string& line) {
    static const std::string kPrefix = "# TYPE ";
    if (line.rfind(kPrefix, 0) != 0) {
        return false;
    }
    const std::size_t name_begin = kPrefix.size();
    const std::size_t space = line.find(' ', name_begin);
    if (space == std::string::npos) {
        return false;
    }
    return line.compare(space + 1, std::string::npos, "histogram") == 0;
}

bool line_sample(const std::string& line, std::string& name, double& value) {
    if (line.empty() || line[0] == '#') {
        return false;
    }
    const std::size_t brace = line.find('{');
    const std::size_t space = line.find(' ');
    std::size_t name_end;
    if (brace != std::string::npos &&
        (space == std::string::npos || brace < space)) {
        name_end = brace;
    } else if (space != std::string::npos) {
        name_end = space;
    } else {
        return false;
    }
    name = line.substr(0, name_end);
    // 值 = 行末空白后的末 token
    std::size_t end = line.size();
    while (end > 0 && (line[end - 1] == ' ' || line[end - 1] == '\r')) {
        --end;
    }
    std::size_t begin = end;
    while (begin > 0 && line[begin - 1] != ' ') {
        --begin;
    }
    if (begin == end) {
        return false;
    }
    const std::string token = line.substr(begin, end - begin);
    errno = 0;
    char* stop = nullptr;
    value = std::strtod(token.c_str(), &stop);
    return stop == token.c_str() + token.size() && !token.empty();
}

} // namespace

AlertNotifier::AlertNotifier(AlertNotifierConfig config)
    : config_(config) {}

void AlertNotifier::set_rules(std::vector<AlertRule> rules) {
    rules_ = std::move(rules);
    states_.clear();
}

void AlertNotifier::set_sink(Sink sink) {
    sink_ = std::move(sink);
}

void AlertNotifier::observe(const std::string& proc, const std::string& body,
                            std::uint64_t now_ms) {
    if (rules_.empty()) {
        return;
    }
    // 本拍体逐行取样 → 规则命中判定（规则度量缺席 = 状态清零不挂陈值）
    std::vector<double> observed(rules_.size(), 0.0);
    std::vector<bool> present(rules_.size(), false);
    std::size_t begin = 0;
    bool in_histogram = false;
    while (begin <= body.size()) {
        std::size_t end = body.find('\n', begin);
        if (end == std::string::npos) {
            end = body.size();
        }
        const std::string line = body.substr(begin, end - begin);
        if (type_declares_histogram(line)) {
            in_histogram = true;
        } else if (line.rfind("# TYPE ", 0) == 0) {
            in_histogram = false;
        } else if (!in_histogram) {
            std::string name;
            double value = 0.0;
            if (line_sample(line, name, value)) {
                for (std::size_t i = 0; i < rules_.size(); ++i) {
                    if (rules_[i].metric == name) {
                        observed[i] = value;
                        present[i] = true;
                    }
                }
            }
        }
        if (end == body.size()) {
            break;
        }
        begin = end + 1;
    }
    std::vector<ProcState>& per_rule = states_[proc];
    per_rule.resize(rules_.size());
    for (std::size_t i = 0; i < rules_.size(); ++i) {
        ProcState& state = per_rule[i];
        if (!present[i]) {
            state.breach = false;
            state.breach_start_ms = 0;
            continue;
        }
        const AlertRule& rule = rules_[i];
        state.last_value = observed[i];
        const bool over = rule.greater ? observed[i] > rule.threshold
                                       : observed[i] < rule.threshold;
        if (!over) {
            state.breach = false;
            state.breach_start_ms = 0;
            continue;
        }
        if (!state.breach) {
            state.breach = true;
            state.breach_start_ms = now_ms;
        }
        if (now_ms - state.breach_start_ms >= rule.sustain_ms) {
            try_fire(rule, state, proc, observed[i], now_ms);
        }
    }
}

void AlertNotifier::try_fire(const AlertRule& rule, ProcState& state,
                             const std::string& proc, double value,
                             std::uint64_t now_ms) {
    // ②静默窗：fire-once，窗内不重火（fired_once 显式——last_fire_ms=0
    // 与「未火过」不可辨，now_ms=0 首拍即火是合法时点）
    if (state.fired_once &&
        now_ms - state.last_fire_ms < rule.silence_ms) {
        return;
    }
    // ③全局速率上限（固定窗）：窗口翻转先于限额判定——被弃火不更新
    // 静默窗（state.last_fire_ms），窗口翻转后同 breach 可再火
    if (!window_open_ ||
        now_ms - window_start_ms_ >= config_.rate_window_ms) {
        window_open_ = true;
        window_start_ms_ = now_ms;
        fired_in_window_ = 0;
    }
    if (fired_in_window_ >= config_.max_per_window) {
        ++dropped_;
        return;
    }
    state.fired_once = true;
    state.last_fire_ms = now_ms;
    ++fired_in_window_;
    ++fired_;
    AlertEvent event;
    event.rule_name = rule.name;
    event.proc = proc;
    event.metric = rule.metric;
    event.value = value;
    event.threshold = rule.threshold;
    event.greater = rule.greater;
    event.now_ms = now_ms;
    if (sink_) {
        try {
            sink_(event);
        } catch (...) {
            // 观测面三禁同源：出口抛异常吞掉不外溢
        }
        return;
    }
    // 缺省出口：cat=alert 结构化行（本地留证，§5.2）
    std::cerr << alert_log_line(event) << std::endl;
}

bool exposition_lookup(const std::string& body, const std::string& metric,
                       double& out) {
    std::size_t begin = 0;
    bool found = false;
    bool in_histogram = false;
    while (begin <= body.size()) {
        std::size_t end = body.find('\n', begin);
        if (end == std::string::npos) {
            end = body.size();
        }
        const std::string line = body.substr(begin, end - begin);
        if (type_declares_histogram(line)) {
            in_histogram = true;
        } else if (line.rfind("# TYPE ", 0) == 0) {
            in_histogram = false;
        } else if (!in_histogram) {
            std::string name;
            double value = 0.0;
            if (line_sample(line, name, value) && name == metric) {
                out = value;  // 取最后样本（同名多行 = 标签区分，规则面不辨）
                found = true;
            }
        }
        if (end == body.size()) {
            break;
        }
        begin = end + 1;
    }
    return found;
}

std::vector<AlertRule> parse_alert_rules(const std::string& spec) {
    std::vector<AlertRule> rules;
    std::size_t begin = 0;
    while (begin <= spec.size()) {
        std::size_t end = spec.find(';', begin);
        if (end == std::string::npos) {
            end = spec.size();
        }
        const std::string item = spec.substr(begin, end - begin);
        begin = end + 1;
        if (item.empty()) {
            continue;
        }
        const std::size_t op = item.find_first_of("><");
        if (op == std::string::npos || op == 0 || op + 1 >= item.size()) {
            std::cerr << "alert rules: bad entry skipped: " << item
                      << std::endl;
            continue;
        }
        AlertRule rule;
        rule.metric = item.substr(0, op);
        rule.name = rule.metric;
        rule.greater = item[op] == '>';
        char* stop = nullptr;
        rule.threshold = std::strtod(item.c_str() + op + 1, &stop);
        if (stop != item.c_str() + item.size()) {
            std::cerr << "alert rules: bad threshold skipped: " << item
                      << std::endl;
            continue;
        }
        rules.push_back(std::move(rule));
    }
    return rules;
}

std::string alert_log_line(const AlertEvent& event) {
    std::ostringstream out;
    out << "alert rule=" << event.rule_name << " proc=" << event.proc
        << " metric=" << event.metric << " value=" << event.value
        << " threshold=" << event.threshold
        << " op=" << (event.greater ? ">" : "<")
        << " at_ms=" << event.now_ms;
    return out.str();
}

} // namespace apollo::admin
