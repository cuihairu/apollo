#include "admin/alert_webhook.hpp"

#include "apollo/net/http/rest_client.h"

#include <nlohmann/json.hpp>

#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <utility>

namespace apollo::admin {

namespace {

using apollo::net::http::HttpResponse;
using apollo::net::http::RestTemplate;
using apollo::net::http::RestTemplateBuilder;

// ---- 加签原语（OpenSSL 单一 crypto 源，§5.9）----

std::string hmac_sha256_base64(const std::string& key, const std::string& data) {
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int md_len = 0;
    HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(), md,
         &md_len);
    unsigned char b64[EVP_MAX_MD_SIZE * 2] = {0};
    const int n = EVP_EncodeBlock(b64, md, static_cast<int>(md_len));
    return std::string(reinterpret_cast<char*>(b64), static_cast<std::size_t>(n));
}

std::string provider_lower(const std::string& provider) {
    std::string out = provider;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

std::string append_query(const std::string& url, const std::string& query) {
    return url + (url.find('?') == std::string::npos ? '?' : '&') + query;
}

// 家族 code 面：显式非零 errcode/code 判失败（限频/凭证错），无字段视送达
bool body_code_ok(const std::string& body) {
    try {
        const auto parsed = nlohmann::json::parse(body);
        if (parsed.is_object()) {
            if (parsed.contains("errcode") && parsed["errcode"].is_number_integer() &&
                parsed["errcode"].get<long long>() != 0) {
                return false;
            }
            if (parsed.contains("code") && parsed["code"].is_number_integer() &&
                parsed["code"].get<long long>() != 0) {
                return false;
            }
        }
    } catch (...) {
        // 非 JSON 体：家族外网关中转面——按 2xx 送达口径放行
    }
    return true;
}

std::uint64_t system_now_ms() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

} // namespace

// ---- AlertWebhook ----

AlertWebhook::AlertWebhook(AlertWebhookConfig config) : config_(std::move(config)) {
    // ⑤白名单：目标 = 装配期唯一 URL 的 host，构建期封闭（§5.10 SSRF 同
    // 纪律；hostOf 空 = 表空 = 全拒——fail closed）
    client_ = std::make_unique<RestTemplate>(
        RestTemplateBuilder()
            .allowHost(RestTemplate::hostOf(config_.url))
            .timeout(config_.timeout_ms)
            .build());
    clock_ = system_now_ms;
}

AlertWebhook::~AlertWebhook() {
    stop();
}

void AlertWebhook::set_fallback(Fallback fallback) {
    fallback_ = std::move(fallback);
}

void AlertWebhook::set_clock(Clock clock) {
    clock_ = std::move(clock);
}

void AlertWebhook::start() {
    if (worker_.joinable()) {
        return;
    }
    worker_ = std::thread([this] { worker_loop(); });
}

void AlertWebhook::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void AlertWebhook::send(const AlertEvent& event) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() >= config_.max_queue) {
            ++dropped_;  // ③限频留余量的兜底档：满则丢不积压
            return;
        }
        queue_.push_back(event);
    }
    cv_.notify_one();
}

bool AlertWebhook::wait_stoppable(std::uint32_t ms) {
    std::unique_lock<std::mutex> lock(mutex_);
    return !cv_.wait_for(lock, std::chrono::milliseconds(ms),
                         [this] { return stopping_; });
}

void AlertWebhook::worker_loop() {
    for (;;) {
        AlertEvent event;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
            if (queue_.empty()) {
                return;  // stopping_ 且排干
            }
            event = std::move(queue_.front());
            queue_.pop_front();
        }
        attempt_with_retries(event);
    }
}

bool AlertWebhook::post_once(const AlertEvent& event) {
    ++attempts_;
    const std::uint64_t now = clock_ ? clock_() : 0;
    const std::string url =
        signed_webhook_url(config_.provider, config_.url, config_.secret, now);
    const HttpResponse resp =
        client_->postForJson(url, render_webhook_body(config_.provider, event));
    if (resp.statusCode / 100 != 2) {
        return false;
    }
    return body_code_ok(resp.body);
}

void AlertWebhook::attempt_with_retries(const AlertEvent& event) {
    for (int attempt = 0; attempt <= 3; ++attempt) {  // 首发 + 重试 ≤3（§5.2 ①）
        if (attempt > 0) {
            if (!wait_stoppable(config_.retry_delay_ms[attempt - 1])) {
                ++failed_;
                run_fallback(event);  // stop 中弃重试不弃留证
                return;
            }
        }
        if (post_once(event)) {
            ++delivered_;
            return;
        }
    }
    ++failed_;
    run_fallback(event);
}

void AlertWebhook::run_fallback(const AlertEvent& event) {
    if (fallback_) {
        try {
            fallback_(event);  // 观测面三禁同源：出口抛异常吞掉
        } catch (...) {
        }
        return;
    }
    std::cerr << alert_log_line(event) << std::endl;
}

// ---- 纯函数面 ----

std::string render_webhook_body(const std::string& provider,
                                const AlertEvent& event) {
    // ⑥模板变量封闭集：正文恒为 alert_log_line（结构化行），信封按家族
    const std::string line = alert_log_line(event);
    nlohmann::json body;
    if (provider == "wecom") {
        body["msgtype"] = "markdown";
        body["markdown"]["content"] = line;
    } else if (provider == "dingtalk") {
        body["msgtype"] = "markdown";
        body["markdown"]["title"] = "alert";
        body["markdown"]["text"] = line;
    } else if (provider == "feishu") {
        body["msg_type"] = "text";
        body["content"]["text"] = line;
    } else {  // slack 与未知家族：通用 text 体
        body["text"] = line;
    }
    return body.dump();
}

std::string signed_webhook_url(const std::string& provider, const std::string& url,
                               const std::string& secret, std::uint64_t now_ms) {
    if (secret.empty()) {
        return url;
    }
    if (provider == "dingtalk") {
        // sign = base64(HMAC-SHA256(secret, "<ts>\n<ts>"))，ts = 毫秒；
        // 签名进 query 需 url 编码（base64 含 +/=）
        const std::string ts = std::to_string(now_ms);
        const std::string sign =
            hmac_sha256_base64(secret, ts + "\n" + ts);
        return append_query(url, "timestamp=" + ts + "&sign=" +
                                     RestTemplate::encodeParam(sign));
    }
    if (provider == "feishu") {
        // ts = 秒；sign 原样 base64（家族文档口径）
        const std::string ts = std::to_string(now_ms / 1000);
        const std::string sign = hmac_sha256_base64(secret, ts + "\n" + ts);
        return append_query(url, "timestamp=" + ts + "&sign=" + sign);
    }
    return url;  // 企业微信 key / Slack URL 即凭证——家族无签名
}

AlertWebhookConfig parse_webhook_config() {
    AlertWebhookConfig config;
    if (const char* url = std::getenv("APOLLO_WEBHOOK_URL")) {
        config.url = url;
    }
    if (const char* provider = std::getenv("APOLLO_WEBHOOK_PROVIDER")) {
        config.provider = provider_lower(provider);
    }
    if (const char* secret = std::getenv("APOLLO_WEBHOOK_SECRET")) {
        config.secret = secret;
    }
    return config;
}

} // namespace apollo::admin
