#pragma once
//
// 告警 webhook 出站面（logging.md §5.2 层 B 发送纪律，P3-3 余项批）。
//
// 出站 = RestTemplate（net-abstraction §5.10 curl 设施）+ 四家默认模板
// （JSON + 一次 POST；⑥模板变量封闭集——正文恒为 alert_log_line 结构化
// 行，无自由表达式防注入面）。发送纪律六条中本面覆盖：
//   ① 退避重试 ≤3 次（默认 5s/15s/60s，可配置供测试毫秒档），仍失败
//     回写本地日志（fallback，缺省 cat=alert 行）——告警不丢；
//   ④ 密钥走 env（APOLLO_WEBHOOK_URL/PROVIDER/SECRET），不进 git 不进
//     日志——失败路径只经事件（不含 URL/token）；钉钉/飞书加签
//     （HMAC-SHA256 base64，OpenSSL 单一 crypto 源 §5.9）；
//   ⑤ 出站目标白名单：目标 = 装配期 env 给定的唯一 URL（hostOf 提取进
//     RestTemplate 白名单，构建期封闭、运行期不可加——§5.10 SSRF 同纪
//     律，exporter 也不开放任意拨号）。
//   （②③静默/限速在 AlertNotifier 判定面；⑥封闭集同 notifier 批。）
//
// 线程口径：send() 单写者入队（serve 拍内直调，永不阻塞——HTTP 与重试
// 在自有 worker 线程）；有界队列，满则丢弃计数。HTTP 客户端只在
// exporter 进程（三禁延伸：游戏进程永不直发 webhook）。
//
// 面边界：企业微信 key / Slack URL 即凭证（token 在 env URL 内，不落日
// 志）；钉钉/飞书加签型密钥走 secret env；家族 code 面（errcode/code）
// 显式非零判失败，其余 2xx 视送达（限频留 §5.2 ③速率上限余量）。

#include "admin/alert_notifier.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace apollo::net::http {
class RestTemplate;
}

namespace apollo::admin {

struct AlertWebhookConfig {
    std::string provider = "slack";  // wecom|dingtalk|feishu|slack（未知按 slack 体）
    std::string url;                 // 目标（含家族 token）；空 = 面不启用
    std::string secret;              // 加签密钥（dingtalk/feishu 用）；空 = 不签
    std::uint32_t timeout_ms = 5000;
    std::uint32_t retry_delay_ms[3] = {5000, 15000, 60000};  // §5.2 ①
    std::size_t max_queue = 64;
};

/// 出站 worker（入队面 send 单写者；重试/签名/POST 在 worker 线程）
class AlertWebhook {
public:
    using Fallback = std::function<void(const AlertEvent&)>;
    using Clock = std::function<std::uint64_t()>;  // ms（签名时间戳注入）

    explicit AlertWebhook(AlertWebhookConfig config);
    ~AlertWebhook();  // stop()

    AlertWebhook(const AlertWebhook&) = delete;
    AlertWebhook& operator=(const AlertWebhook&) = delete;

    /// 重试穷尽后的本地回写面（缺省 cat=alert 行）
    void set_fallback(Fallback fallback);
    /// 签名时间戳注入（缺省 system ms）
    void set_clock(Clock clock);

    void start();
    /// 排干队列后收线程（重试中收到 stop 则立即回写 fallback 收束）
    void stop();

    /// Sink 面（AlertNotifier::set_sink 注入用）：入队非阻塞，满则丢计数
    void send(const AlertEvent& event);

    std::uint64_t delivered() const { return delivered_.load(); }
    std::uint64_t failed() const { return failed_.load(); }
    std::uint64_t dropped() const { return dropped_.load(); }
    std::uint64_t attempts() const { return attempts_.load(); }

private:
    void worker_loop();
    bool post_once(const AlertEvent& event);
    void attempt_with_retries(const AlertEvent& event);
    void run_fallback(const AlertEvent& event);
    /// 可中断退避（stop 即醒）；返回 false = 已叫停
    bool wait_stoppable(std::uint32_t ms);

    AlertWebhookConfig config_;
    std::unique_ptr<apollo::net::http::RestTemplate> client_;  // 白名单构建期封闭
    Fallback fallback_;
    Clock clock_;
    std::thread worker_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<AlertEvent> queue_;
    bool stopping_ = false;  // mutex_ 护
    std::atomic<std::uint64_t> delivered_{0};
    std::atomic<std::uint64_t> failed_{0};
    std::atomic<std::uint64_t> dropped_{0};
    std::atomic<std::uint64_t> attempts_{0};
};

/// 四家默认模板渲染（JSON + 一次 POST；正文 = alert_log_line 封闭集）。
/// 未知 provider 按 slack 形态（{"text":…}）。
std::string render_webhook_body(const std::string& provider,
                                const AlertEvent& event);

/// 加签 URL（钉钉 timestamp=ms + urlencode(sign)；飞书 timestamp=s +
/// 原样 base64 sign；企业微信/Slack/空 secret 原样返回——家族无签名）
std::string signed_webhook_url(const std::string& provider, const std::string& url,
                               const std::string& secret, std::uint64_t now_ms);

/// env 装配面：APOLLO_WEBHOOK_URL / APOLLO_WEBHOOK_PROVIDER /
/// APOLLO_WEBHOOK_SECRET（provider 小写归一；url 空 = 面不启用）
AlertWebhookConfig parse_webhook_config();

} // namespace apollo::admin
