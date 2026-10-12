// 告警 webhook 出站面测试（AlertWebhookTests，logging.md §5.2 批）。
//
// 覆盖：
//   1. 四家模板渲染（纯逻辑，Windows 同跑）：家族信封 + 正文 = 封闭集
//      结构化行（特殊字符经 JSON 转义逐位保真）；未知家族按通用体；
//   2. 加签 URL：空 secret 原样；钉钉（ms + urlencode sign）/飞书（秒 +
//      原样 base64）query 拼 分隔符正确、确定性、时变签名；企业微信/
//      Slack 家族无签名原样；
//   3. env 装配面（POSIX）：URL/provider 小写归一/secret；
//   4. 队列面：有界丢计数（不启动 worker 即可证）；
//   5. 环回真 POST e2e：送达计数 + 请求体/头到达面（家族信封 + 结构化
//      行 + application/json）；
//   6. 重试退避：家族 code 非零判失败 → 首发 + 3 重试（服务端 4 连接）
//      → failed 计数 + fallback 回写（告警不丢）；
//   7. stop 可中断：重试等待中叫停 → 立即回写收束（不悬挂）。
//
// 服务端走 link TCP 底座（POSIX only——Windows 只跑纯逻辑用例，同
// metricspush_tests 口径）。

#include "admin/alert_webhook.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#if !defined(_WIN32)
#include "apollo/net/link/tcp_transport.hpp"
#include <atomic>
#include <cstdlib>
#endif

#define TEST_ASSERT(cond, msg)                                                           \
    do {                                                                                 \
        if (!(cond)) {                                                                   \
            std::cerr << "FAIL: " << (msg) << " (" << __FILE__ << ":" << __LINE__ << ")" \
                      << std::endl;                                                      \
            return false;                                                                \
        }                                                                                \
    } while (0)

namespace {

using apollo::admin::AlertEvent;
using apollo::admin::AlertWebhook;
using apollo::admin::AlertWebhookConfig;

AlertEvent sample_event() {
    AlertEvent e;
    e.rule_name = "queue_depth";
    e.proc = "cell@1";
    e.metric = "queue_depth";
    e.value = 11;
    e.threshold = 10;
    e.greater = true;
    e.now_ms = 1234;
    return e;
}

// ---- 模板渲染面（纯逻辑，Windows 同跑）----

bool test_render_body() {
    const AlertEvent e = sample_event();
    const std::string line = apollo::admin::alert_log_line(e);

    const auto wecom = nlohmann::json::parse(
        apollo::admin::render_webhook_body("wecom", e));
    TEST_ASSERT(wecom["msgtype"] == "markdown", "wecom msgtype");
    TEST_ASSERT(wecom["markdown"]["content"] == line, "wecom content = closed line");

    const auto dingtalk = nlohmann::json::parse(
        apollo::admin::render_webhook_body("dingtalk", e));
    TEST_ASSERT(dingtalk["msgtype"] == "markdown", "dingtalk msgtype");
    TEST_ASSERT(dingtalk["markdown"]["title"] == "alert", "dingtalk title");
    TEST_ASSERT(dingtalk["markdown"]["text"] == line, "dingtalk text = closed line");

    const auto feishu = nlohmann::json::parse(
        apollo::admin::render_webhook_body("feishu", e));
    TEST_ASSERT(feishu["msg_type"] == "text", "feishu msg_type");
    TEST_ASSERT(feishu["content"]["text"] == line, "feishu text = closed line");

    const auto slack = nlohmann::json::parse(
        apollo::admin::render_webhook_body("slack", e));
    TEST_ASSERT(slack["text"] == line, "slack text = closed line");

    // 未知家族按通用体；特殊字符经 JSON 转义逐位保真（防注入 = 封闭集）
    AlertEvent special = sample_event();
    special.proc = "cell\"1\\2";
    const auto generic = nlohmann::json::parse(
        apollo::admin::render_webhook_body("mstepper", special));
    TEST_ASSERT(generic["text"] == apollo::admin::alert_log_line(special),
                "unknown family = generic body, escapes preserved");
    return true;
}

bool test_signed_url() {
    const std::string base = "https://oapi.dingtalk.com/robot/send?access_token=t";
    TEST_ASSERT(apollo::admin::signed_webhook_url("dingtalk", base, "", 12345) == base,
                "empty secret = url unchanged");

    const std::string ding =
        apollo::admin::signed_webhook_url("dingtalk", base, "s3cret", 12345);
    TEST_ASSERT(ding.rfind(base + "&timestamp=12345&sign=", 0) == 0,
                "dingtalk appends ms timestamp + sign");
    TEST_ASSERT(ding == apollo::admin::signed_webhook_url("dingtalk", base, "s3cret",
                                                          12345),
                "dingtalk sign deterministic");
    TEST_ASSERT(ding != apollo::admin::signed_webhook_url("dingtalk", base, "s3cret",
                                                          12346),
                "dingtalk sign time-variant");
    // base64 的 +/= 经 url 编码不裸奔：sign= 后只允许 base64 字母与 % 转义
    {
        const std::string encoded = ding.substr(ding.find("sign=") + 5);
        TEST_ASSERT(!encoded.empty(), "dingtalk sign present");
        bool clean = true;
        for (const char c : encoded) {
            const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                            (c >= '0' && c <= '9') || c == '%';
            if (!ok) {
                clean = false;
            }
        }
        TEST_ASSERT(clean, "dingtalk sign url-encoded (no raw +//=)");
    }

    // 裸 URL（无既有 query）走 ? 分隔
    const std::string bare = "https://open.feishu.cn/open-apis/bot/v2/hook/xxx";
    const std::string feishu =
        apollo::admin::signed_webhook_url("feishu", bare, "s3cret", 61234);
    TEST_ASSERT(feishu.rfind(bare + "?timestamp=61&sign=", 0) == 0,
                "feishu appends second timestamp via ? separator");
    const std::string feishu_sign =
        feishu.substr(feishu.find("sign=") + 5);
    TEST_ASSERT(!feishu_sign.empty(), "feishu sign present");
    TEST_ASSERT(feishu_sign.find('%') == std::string::npos,
                "feishu sign raw base64 (not url-encoded)");
    TEST_ASSERT(feishu == apollo::admin::signed_webhook_url("feishu", bare, "s3cret",
                                                            61234),
                "feishu sign deterministic");

    // 企业微信/Slack 家族即凭证——带 secret 也不改 URL
    TEST_ASSERT(apollo::admin::signed_webhook_url("wecom", base, "s3cret", 1) == base,
                "wecom has no signing");
    TEST_ASSERT(apollo::admin::signed_webhook_url("slack", base, "s3cret", 1) == base,
                "slack has no signing");
    return true;
}

// ---- 队列面（不启动 worker 即可证）----

bool test_queue_overflow() {
    AlertWebhookConfig config;
    config.max_queue = 1;
    AlertWebhook hook(config);  // 不 start：入队面独立成立
    hook.send(sample_event());
    hook.send(sample_event());
    TEST_ASSERT(hook.dropped() == 1, "bounded queue drops with count");
    TEST_ASSERT(hook.delivered() == 0 && hook.attempts() == 0,
                "no delivery without worker");
    return true;
}

#if !defined(_WIN32)

// ---- env 装配面（POSIX，getenv/setenv）----

bool test_env_config_parse() {
    ::setenv("APOLLO_WEBHOOK_URL", "https://qyapi.weixin.qq.com/cgi-bin/webhook/send?key=abc",
             1);
    ::setenv("APOLLO_WEBHOOK_PROVIDER", "WeCom", 1);
    ::setenv("APOLLO_WEBHOOK_SECRET", "s3cret", 1);
    const auto config = apollo::admin::parse_webhook_config();
    TEST_ASSERT(config.url.find("key=abc") != std::string::npos, "url from env");
    TEST_ASSERT(config.provider == "wecom", "provider lowercased");
    TEST_ASSERT(config.secret == "s3cret", "secret from env");
    ::unsetenv("APOLLO_WEBHOOK_URL");
    ::unsetenv("APOLLO_WEBHOOK_PROVIDER");
    ::unsetenv("APOLLO_WEBHOOK_SECRET");
    TEST_ASSERT(apollo::admin::parse_webhook_config().url.empty(),
                "no env = face disabled");
    return true;
}

// ---- 极小 HTTP 受面（真端口）----

struct Receiver {
    apollo::net::link::TcpListener listener;
    std::thread worker;
    std::atomic<bool> stopping{false};
    std::string response;
    std::vector<std::string> requests;
    std::mutex requests_mutex;
    std::atomic<int> connections{0};

    ~Receiver() { stop(); }

    bool start(std::string response_text) {
        response = std::move(response_text);
        if (!listener.start("127.0.0.1", 0)) {
            return false;
        }
        worker = std::thread([this] { loop(); });
        return true;
    }

    std::uint16_t port() const { return listener.port(); }
    std::string base_url() const {
        return "http://127.0.0.1:" + std::to_string(listener.port()) + "/hook";
    }

    std::size_t count() {
        std::lock_guard<std::mutex> lock(requests_mutex);
        return requests.size();
    }

    std::string last_request() {
        std::lock_guard<std::mutex> lock(requests_mutex);
        return requests.empty() ? std::string() : requests.back();
    }

    void stop() {
        if (worker.joinable()) {
            stopping.store(true);
            worker.join();
        }
        listener.close();
    }

    void loop() {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (!stopping.load() && std::chrono::steady_clock::now() < deadline) {
            auto transport = listener.accept();
            if (transport == nullptr) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }
            ++connections;
            std::string request = read_until_headers(*transport);
            {
                std::lock_guard<std::mutex> lock(requests_mutex);
                requests.push_back(request);
            }
            write_all(*transport, response);
            transport->close();
        }
    }

    static std::string read_until_headers(apollo::net::link::ILinkTransport& t) {
        std::string out;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (std::chrono::steady_clock::now() < deadline) {
            std::uint8_t buf[4096];
            const int n = t.read(buf, sizeof buf);
            if (n < 0) {
                break;
            }
            if (n > 0) {
                out.append(reinterpret_cast<const char*>(buf), static_cast<std::size_t>(n));
                if (out.find("\r\n\r\n") != std::string::npos) {
                    break;  // 头齐即够（POST 小体同包到达为常态）
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }
        return out;
    }

    static void write_all(apollo::net::link::ILinkTransport& t, const std::string& text) {
        std::size_t sent = 0;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (sent < text.size() && std::chrono::steady_clock::now() < deadline) {
            const int n = t.write(
                reinterpret_cast<const std::uint8_t*>(text.data()) + sent,
                text.size() - sent);
            if (n < 0) {
                return;
            }
            if (n > 0) {
                sent += static_cast<std::size_t>(n);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }
    }
};

std::string http_response(int code, const char* status, const std::string& body) {
    std::ostringstream out;
    out << "HTTP/1.1 " << code << " " << status << "\r\n"
        << "Content-Type: application/json\r\n"
        << "Content-Length: " << body.size() << "\r\n\r\n"
        << body;
    return out.str();
}

template <typename Pred>
bool wait_for(Pred&& done, int timeout_ms = 5000) {
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        if (done()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return done();
}

// ---- 环回真 POST e2e ----

bool test_e2e_delivered() {
    Receiver receiver;
    TEST_ASSERT(receiver.start(http_response(200, "OK", "{\"errcode\":0}")),
                "receiver up");

    AlertWebhookConfig config;
    config.provider = "wecom";
    config.url = receiver.base_url() + "?key=t0k3n";
    config.timeout_ms = 3000;
    AlertWebhook hook(config);
    hook.start();
    hook.send(sample_event());

    TEST_ASSERT(wait_for([&] { return hook.delivered() == 1; }), "delivered");
    TEST_ASSERT(hook.attempts() == 1, "single attempt on success");
    TEST_ASSERT(hook.failed() == 0 && hook.dropped() == 0, "clean delivery");

    const std::string request = receiver.last_request();
    TEST_ASSERT(request.rfind("POST /hook?key=t0k3n HTTP/1.1\r\n", 0) == 0,
                "post line with token query");
    TEST_ASSERT(request.find("Content-Type: application/json") != std::string::npos,
                "json content type");
    TEST_ASSERT(request.find("\"msgtype\":\"markdown\"") != std::string::npos,
                "wecom envelope");
    TEST_ASSERT(request.find("alert rule=queue_depth") != std::string::npos,
                "closed-set line in body");

    hook.stop();
    receiver.stop();
    return true;
}

// ---- 重试退避 + fallback 回写（告警不丢）----

bool test_retry_then_fallback() {
    Receiver receiver;
    // 家族 code 非零 = 失败（HTTP 200 也不算送达）
    TEST_ASSERT(receiver.start(http_response(200, "OK", "{\"errcode\":93000}")),
                "receiver up");

    AlertWebhookConfig config;
    config.provider = "dingtalk";
    config.url = receiver.base_url() + "?access_token=t";
    config.timeout_ms = 2000;
    config.retry_delay_ms[0] = 1;
    config.retry_delay_ms[1] = 1;
    config.retry_delay_ms[2] = 1;

    AlertWebhook hook(config);
    int fallback_calls = 0;
    std::string fallback_rule;  // 断言在测试线程做——lambda 体内 return false
    hook.set_fallback([&](const AlertEvent& e) {  // 会把推导返回型做成 bool，
        ++fallback_calls;                         // 断言通过即 fall off end = UB
        fallback_rule = e.rule_name;
    });
    hook.start();
    hook.send(sample_event());

    TEST_ASSERT(wait_for([&] { return hook.failed() == 1; }), "exhausted to failed");
    TEST_ASSERT(hook.delivered() == 0, "nothing delivered on hard failure");
    TEST_ASSERT(hook.attempts() == 4, "initial + 3 retries (backoff 5s/15s/60s face)");
    TEST_ASSERT(fallback_calls == 1, "fallback line written (alert not lost)");
    TEST_ASSERT(fallback_rule == "queue_depth", "fallback gets the event");
    TEST_ASSERT(wait_for([&] { return receiver.count() >= 4; }), "four POSTs seen");

    hook.stop();
    receiver.stop();
    return true;
}

bool test_stop_interrupts_backoff() {
    Receiver receiver;
    TEST_ASSERT(receiver.start(http_response(500, "Bad", "{}")), "receiver up");

    AlertWebhookConfig config;
    config.provider = "slack";
    config.url = receiver.base_url();
    config.timeout_ms = 1000;
    config.retry_delay_ms[0] = 10000;  // 长退避：stop 必须可中断
    config.retry_delay_ms[1] = 10000;
    config.retry_delay_ms[2] = 10000;

    AlertWebhook hook(config);
    int fallback_calls = 0;
    hook.set_fallback([&](const AlertEvent&) { ++fallback_calls; });
    hook.start();
    hook.send(sample_event());

    // 等首发失败进入第一次退避等待，再叫停
    TEST_ASSERT(wait_for([&] { return hook.attempts() == 1; }), "first attempt done");
    const auto begin = std::chrono::steady_clock::now();
    hook.stop();  // 排干语义：退避中叫停 → 立即回写收束
    const auto stop_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - begin)
            .count();
    TEST_ASSERT(stop_ms < 5000, "stop interrupts backoff (no hang)");
    TEST_ASSERT(fallback_calls == 1, "stop-time fallback written");
    TEST_ASSERT(hook.failed() == 1, "aborted retry counted failed");
    receiver.stop();
    return true;
}

#endif // !_WIN32

} // namespace

int main() {
#if defined(_WIN32)
    if (!test_render_body()) {
        return 1;
    }
    if (!test_signed_url()) {
        return 1;
    }
    if (!test_queue_overflow()) {
        return 1;
    }
    std::cout << "AlertWebhookTests: windows-scope cases passed" << std::endl;
    return 0;
#else
    const struct {
        const char* name;
        bool (*fn)();
    } cases[] = {
        {"render_body", test_render_body},
        {"signed_url", test_signed_url},
        {"queue_overflow", test_queue_overflow},
        {"env_config_parse", test_env_config_parse},
        {"e2e_delivered", test_e2e_delivered},
        {"retry_then_fallback", test_retry_then_fallback},
        {"stop_interrupts_backoff", test_stop_interrupts_backoff},
    };
    int failed = 0;
    for (const auto& c : cases) {
        if (!c.fn()) {
            std::cerr << "CASE FAILED: " << c.name << std::endl;
            ++failed;
        }
    }
    if (failed == 0) {
        std::cout << "AlertWebhookTests: all " << (sizeof(cases) / sizeof(cases[0]))
                  << " cases passed" << std::endl;
        return 0;
    }
    std::cout << "AlertWebhookTests: " << failed << " case(s) failed" << std::endl;
    return 1;
#endif
}
