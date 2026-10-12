#pragma once
//
// 告警 notifier 最小面（logging.md §5.2 层 B 内核，P3-3 余项批）。
//
// 分工：层 A（Alertmanager 标准栈）零代码不在此；层 B = 本面——规则表 +
// 判定 + 静默聚合 + 速率上限 + 事件出站。出站 v1 = 注入 sink，缺省
// cat=alert 结构化行落本地日志（§5.2「告警事件本身作为结构化行照常落
// 本地日志」）；webhook POST + 四家模板挂 net-abstraction §5.10 curl
// 出站批（curl 尚未进 vcpkg）。
//
// 发送纪律（§5.2 六条中本面覆盖四条）：
//   ②同规则静默窗聚合：sustain_ms 跨拍持续超限才火（0 = 首拍即火）+
//     fire-once + silence_ms 静默（默认 60s）；
//   ③全局速率上限：默认 10 条/min（固定窗），超限丢弃计数（不积压不
//     重放；被弃火不更新静默窗——窗口翻转后可再火）；
//   ⑤出站目标白名单：v1 无出站目标，sink 注入面即白名单边界（目标由
//     装配方给定，运行期不可加）；
//   ⑥模板变量封闭集：AlertEvent 字段固定，模板渲染面随 curl 批。
//   （①退避重试 / ④密钥管理随 webhook 出站批。）
//
// 判定面：observe(proc, body, now) 逐 exposition 行提取 `<name> <value>`
// 与规则匹配——`# TYPE` 注释行跳过；histogram 族整段（`# TYPE <name>
// histogram` 起至下一 TYPE）不参与匹配；规则度量在本拍体缺席 = 该规则
// 本拍无观察（状态清零不挂陈值）。状态按 rule × proc 独立记账，进程间
// 互不串。proc 存活告警（apollo_proc_up<1）不在本面：proc_up 由
// hub 出口面合成不进推送体——挂 hub 态判定，批记边界。
//
// 线程口径：observe 单写者（serve 循环泵拍内 931 受理点直调），不加锁。

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace apollo::admin {

/// 规则表条目（metric 阈值判定 + 静默窗）
struct AlertRule {
    std::string name;      // 规则名（事件标识；env 派生规则 = metric 名）
    std::string metric;    // exposition 样本名（裸 counter/gauge）
    double threshold = 0.0;
    bool greater = true;   // true: value > threshold 触发；false: value < threshold
    std::uint64_t sustain_ms = 0;      // 跨拍持续超限窗（0 = 首拍即火）
    std::uint64_t silence_ms = 60000;  // 火后静默窗（同 rule × proc）
};

/// 事件（模板变量封闭集——⑥）
struct AlertEvent {
    std::string rule_name;
    std::string proc;
    std::string metric;
    double value = 0.0;
    double threshold = 0.0;
    bool greater = true;
    std::uint64_t now_ms = 0;
};

struct AlertNotifierConfig {
    std::uint64_t rate_window_ms = 60000;
    std::uint32_t max_per_window = 10;  // §5.2 ③默认 10 条/min
};

/// 判定 + 聚合 + 限速 + 出站（规则表注入；出站 sink 注入）
class AlertNotifier {
public:
    using Sink = std::function<void(const AlertEvent&)>;

    explicit AlertNotifier(AlertNotifierConfig config = {});

    /// 整表替换（装配方一次性给定；运行期不改 = ⑤白名单同款立场）
    void set_rules(std::vector<AlertRule> rules);
    /// 出站注入；不设 = 缺省 cat=alert 结构化行（本地留证）
    void set_sink(Sink sink);

    /// 逐拍受理：proc 推送体（exposition 文本）按规则表判定记账
    void observe(const std::string& proc, const std::string& body,
                 std::uint64_t now_ms);

    std::uint64_t fired() const { return fired_; }
    std::uint64_t dropped() const { return dropped_; }
    std::size_t rule_count() const { return rules_.size(); }

private:
    struct ProcState {
        bool breach = false;             // 上拍是否超限
        bool fired_once = false;         // 「未火过」显式（0 时点可火）
        std::uint64_t breach_start_ms = 0;
        std::uint64_t last_fire_ms = 0;
        double last_value = 0.0;
    };

    void try_fire(const AlertRule& rule, ProcState& state,
                  const std::string& proc, double value, std::uint64_t now_ms);

    AlertNotifierConfig config_;
    std::vector<AlertRule> rules_;
    Sink sink_;  // 空 = 缺省行出口
    // proc → 逐规则状态（下标对齐 rules_）；observe 单写者免锁
    std::unordered_map<std::string, std::vector<ProcState>> states_;
    bool window_open_ = false;  // 速率窗已开（now_ms=0 与「未开」可辨）
    std::uint64_t window_start_ms_ = 0;
    std::uint32_t fired_in_window_ = 0;
    std::uint64_t fired_ = 0;
    std::uint64_t dropped_ = 0;
};

/// exposition 体单度量取值（行式 `<name>{labels} value`；取规则匹配名
/// 的最后一个样本值）。命中返回 true。测试与 notifier 共用。
bool exposition_lookup(const std::string& body, const std::string& metric,
                       double& out);

/// env 规则面（最小）：APOLLO_ALERT_RULES = `<metric><op><threshold>` 以
/// `;` 连接，op ∈ {>, <}（例 "gateway_sessions<1;cell_queue_depth>100"）。
/// 规则名 = metric 名；sustain/silence 取默认。坏条目跳过不中断。
std::vector<AlertRule> parse_alert_rules(const std::string& spec);

/// 缺省行出口文本（cat=alert 结构化行 msg 段；测试可逐位比对）
std::string alert_log_line(const AlertEvent& event);

} // namespace apollo::admin
