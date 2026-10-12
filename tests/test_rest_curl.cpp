// rest_client 真实现 + 出站目标白名单测试（RestCurlTests，net-abstraction
// §5.10 出站裁决批）。
//
// 覆盖：
//   1. 白名单匹配面（纯逻辑，Windows 同跑）：精确主机（端口/userinfo 剥
//      离、大小写归一）/ 前导点后缀（仅子域，apex 不中、前缀相似不误中）/
//      IPv4 CIDR 网段（同段中、跨段不中、域名不入网段条目）；
//   2. 缺省拒绝面：未配置 = 全拒（§5.10「目标必须可枚举」），拒绝不发起
//      拨号（时界可证）、statusCode=-1 + error 标记；
//   3. 环回真请求：GET 200 + body 逐位 + 响应头解析 + 请求行/UA 到达面；
//   4. POST 体往返（echo 服务端）+ Content-Type/Length 到达面；
//   5. 重定向守卫：同白名单跳转放行（CURLOPT_PREREQFUNCTION 前置面），
//      跨白名单跳转即断（不产生 200 响应）；
//   6. 连接拒绝：类型化 error 不悬挂（时界）；
//   7. async future 面。
//
// 服务端走 link TCP 底座（TcpListener/TcpTransport，POSIX only——
// Windows 只跑纯白名单逻辑用例，同 metricspush_tests 口径）。

#include "apollo/net/http/rest_client.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#if !defined(_WIN32)
#include "apollo/net/link/tcp_transport.hpp"
#include <atomic>
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

using apollo::net::http::HttpResponse;
using apollo::net::http::RestTemplate;
using apollo::net::http::RestTemplateBuilder;

// ---- 白名单匹配面（纯逻辑，无 IO，Windows 同跑）----

bool test_whitelist_matching() {
    auto rest = RestTemplateBuilder()
                    .allowHost("api.example.com")
                    .allowHost(".corp.example.org")
                    .allowHost("127.0.0.0/8")
                    .allowHost("10.0.0.0/24")
                    .build();
    TEST_ASSERT(rest.destinationAllowed("http://api.example.com/v1"), "exact host");
    TEST_ASSERT(rest.destinationAllowed("https://API.EXAMPLE.COM/v1"),
                "case normalized");
    TEST_ASSERT(rest.destinationAllowed("http://api.example.com:8080/x"),
                "port stripped");
    TEST_ASSERT(rest.destinationAllowed("http://user@api.example.com:80/p"),
                "userinfo stripped");
    TEST_ASSERT(!rest.destinationAllowed("http://evilexample.com/"),
                "exact match does not suffix-hit");
    TEST_ASSERT(!rest.destinationAllowed("http://example.com/"),
                "apex not covered by exact entry");
    TEST_ASSERT(rest.destinationAllowed("http://a.corp.example.org/"),
                "leading-dot suffix covers subdomain");
    TEST_ASSERT(!rest.destinationAllowed("http://corp.example.org/"),
                "apex not covered by suffix entry");
    TEST_ASSERT(!rest.destinationAllowed("http://notcorp.example.org/"),
                "suffix match has label boundary");
    TEST_ASSERT(rest.destinationAllowed("http://127.0.0.1:9/x"), "cidr /8 in-segment");
    TEST_ASSERT(rest.destinationAllowed("http://127.255.0.9/"), "cidr /8 upper bound");
    TEST_ASSERT(!rest.destinationAllowed("http://128.0.0.1/"), "cidr /8 out-segment");
    TEST_ASSERT(rest.destinationAllowed("http://10.0.0.199/"), "cidr /24 in-segment");
    TEST_ASSERT(!rest.destinationAllowed("http://10.0.1.1/"), "cidr /24 out-segment");
    TEST_ASSERT(!rest.destinationAllowed("http://example.com/"),
                "domain never matches cidr entry");
    TEST_ASSERT(!rest.destinationAllowed(""), "empty url denied");
    return true;
}

bool test_whitelist_default_deny() {
    RestTemplate rest;  // 未配置 = 全拒（§5.10 SSRF 面）
    const auto begin = std::chrono::steady_clock::now();
    const HttpResponse resp = rest.get("http://127.0.0.1:1/");
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - begin)
                                .count();
    TEST_ASSERT(resp.statusCode == -1, "denied request reports -1");
    TEST_ASSERT(resp.error.find("whitelist") != std::string::npos,
                "denial names the whitelist");
    TEST_ASSERT(elapsed_ms < 2000, "denial does not dial (time bound)");
    TEST_ASSERT(!rest.destinationAllowed("http://127.0.0.1/"),
                "empty whitelist allows nothing");
    return true;
}

// ---- 环回真请求（POSIX）----

#if !defined(_WIN32)

// 极小 HTTP 服务端：逐连接取 responses 队首应答（空则复读末条）；echo
// 模式回显收到的请求体。请求原文留证（last_request）。
struct HttpServer {
    apollo::net::link::TcpListener listener;
    std::thread worker;
    std::atomic<bool> stopping{false};
    std::deque<std::string> responses;
    bool echo = false;
    std::vector<std::string> requests;
    std::mutex requests_mutex;
    int connections = 0;

    ~HttpServer() { stop(); }

    bool start(std::deque<std::string> response_texts, bool echo_mode = false) {
        if (!listener.start("127.0.0.1", 0)) {
            return false;
        }
        responses = std::move(response_texts);
        echo = echo_mode;
        worker = std::thread([this] { serve_loop(); });
        return true;
    }

    std::uint16_t port() const { return listener.port(); }

    std::string base_url() const {
        return "http://127.0.0.1:" + std::to_string(listener.port());
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

    void serve_loop() {
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (!stopping.load() && std::chrono::steady_clock::now() < deadline) {
            auto transport = listener.accept();
            if (transport == nullptr) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }
            ++connections;
            const std::string request = read_request(*transport);
            {
                std::lock_guard<std::mutex> lock(requests_mutex);
                requests.push_back(request);
            }
            // 应答选择：echo 模式回显请求体；否则取队列首条（空则不回）
            std::string out;
            bool have = false;
            if (echo) {
                out = make_response(201, "Created", request_body(request),
                                    "application/json");
                have = true;
            } else if (!responses.empty()) {
                out = responses.front();
                responses.pop_front();
                have = true;
            }
            if (have) {
                write_all(*transport, out);
            }
            transport->close();
        }
    }

    static std::string read_request(apollo::net::link::ILinkTransport& transport) {
        std::string request;
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (std::chrono::steady_clock::now() < deadline) {
            std::uint8_t buf[4096];
            const int n = transport.read(buf, sizeof buf);
            if (n < 0) {
                break;
            }
            if (n > 0) {
                request.append(reinterpret_cast<const char*>(buf),
                               static_cast<std::size_t>(n));
                const std::size_t split = request.find("\r\n\r\n");
                if (split != std::string::npos &&
                    request.size() - (split + 4) >= content_length(request)) {
                    break;  // 头 + 全体收齐
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }
        return request;
    }

    static std::size_t content_length(const std::string& request) {
        const std::size_t at = request.find("Content-Length: ");
        if (at == std::string::npos) {
            const std::size_t lower_at = request.find("content-length: ");
            if (lower_at == std::string::npos) {
                return 0;
            }
            return parse_len(request.substr(lower_at));
        }
        return parse_len(request.substr(at));
    }

    static std::size_t parse_len(const std::string& header) {
        const std::size_t colon = header.find(':');
        std::size_t begin = colon + 1;
        while (begin < header.size() && header[begin] == ' ') {
            ++begin;
        }
        std::size_t value = 0;
        while (begin < header.size() && header[begin] >= '0' &&
               header[begin] <= '9') {
            value = value * 10 + static_cast<std::size_t>(header[begin] - '0');
            ++begin;
        }
        return value;
    }

    static std::string request_body(const std::string& request) {
        const std::size_t split = request.find("\r\n\r\n");
        return split == std::string::npos
                   ? std::string()
                   : request.substr(split + 4);
    }

    static void write_all(apollo::net::link::ILinkTransport& transport,
                          const std::string& text) {
        std::size_t sent = 0;
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (sent < text.size() && std::chrono::steady_clock::now() < deadline) {
            const int n = transport.write(
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

    static std::string make_response(int code, const char* status,
                                     const std::string& body,
                                     const char* content_type,
                                     const char* extra_headers = "") {
        std::ostringstream out;
        out << "HTTP/1.1 " << code << " " << status << "\r\n"
            << "Content-Type: " << content_type << "\r\n"
            << "Content-Length: " << body.size() << "\r\n"
            << extra_headers << "\r\n"
            << body;
        return out.str();
    }
};

bool test_real_get_roundtrip() {
    HttpServer server;
    TEST_ASSERT(server.start({HttpServer::make_response(200, "OK", "hello apollo",
                                                        "text/plain")}),
                "server up");
    auto rest = RestTemplateBuilder().allowHost("127.0.0.1").build();
    const HttpResponse resp = rest.get(server.base_url() + "/probe");
    TEST_ASSERT(resp.statusCode == 200, "200 observed");
    TEST_ASSERT(resp.statusMessage == "OK", "status message parsed");
    TEST_ASSERT(resp.body == "hello apollo", "body verbatim");
    TEST_ASSERT(resp.getHeader("Content-Type") == "text/plain",
                "response header parsed");
    TEST_ASSERT(resp.contentLength == 12, "content length parsed");
    const std::string request = server.last_request();
    TEST_ASSERT(request.rfind("GET /probe HTTP/1.1\r\n", 0) == 0, "request line");
    TEST_ASSERT(request.find("User-Agent: ApolloHttpClient/1.0") != std::string::npos,
                "default user agent");
    server.stop();
    return true;
}

bool test_real_post_echo() {
    HttpServer server;
    TEST_ASSERT(server.start({}, true), "echo server up");
    const std::string payload = R"({"rule":"queue_depth","value":11})";
    auto rest = RestTemplateBuilder().allowHost("127.0.0.1").build();
    const HttpResponse resp =
        rest.postForJson(server.base_url() + "/hook", payload);
    TEST_ASSERT(resp.statusCode == 201, "created observed");
    TEST_ASSERT(resp.body == payload, "body echoed back verbatim");
    const std::string request = server.last_request();
    TEST_ASSERT(request.rfind("POST /hook HTTP/1.1\r\n", 0) == 0, "post request line");
    TEST_ASSERT(request.find("Content-Type: application/json") != std::string::npos,
                "json content type sent");
    TEST_ASSERT(request.find("Content-Length: " + std::to_string(payload.size())) !=
                    std::string::npos,
                "content length sent");
    server.stop();
    return true;
}

bool test_redirect_guard() {
    // 同白名单跳转放行：/old → 302 Location 同 host /new → 200
    {
        HttpServer server;
        // 先起 listener 拿端口，再拼跳转 URL，再进服务循环
        TEST_ASSERT(server.listener.start("127.0.0.1", 0), "listener bound");
        const std::string target = server.base_url() + "/new";
        server.responses = {
            HttpServer::make_response(302, "Found", "", "text/plain",
                                      ("Location: " + target + "\r\n").c_str()),
            HttpServer::make_response(200, "OK", "moved", "text/plain")};
        server.worker = std::thread([&server] { server.serve_loop(); });

        auto rest = RestTemplateBuilder().allowHost("127.0.0.1").build();
        const HttpResponse resp = rest.get(server.base_url() + "/old");
        TEST_ASSERT(resp.statusCode == 200, "same-host redirect followed");
        TEST_ASSERT(resp.body == "moved", "redirect landed body");
        server.stop();
    }
    // 跨白名单跳转即断：目标 203.0.113.9（TEST-NET-3）不在表内
    {
        HttpServer server;
        TEST_ASSERT(server.listener.start("127.0.0.1", 0), "listener bound");
        server.responses = {HttpServer::make_response(
            302, "Found", "", "text/plain",
            "Location: http://203.0.113.9/steal\r\n")};
        server.worker = std::thread([&server] { server.serve_loop(); });

        auto rest = RestTemplateBuilder().allowHost("127.0.0.1").build();
        const HttpResponse resp = rest.get(server.base_url() + "/old");
        TEST_ASSERT(!resp.isOk(), "cross-whitelist redirect blocked");
        TEST_ASSERT(resp.statusCode != 200, "no 200 from hijacked hop");
        server.stop();
    }
    return true;
}

bool test_refused_typed_error() {
    // 听口即拆：拨号期无人受理（ECONNREFUSED）→ 类型化 error 不悬挂
    std::uint16_t port = 0;
    {
        apollo::net::link::TcpListener placeholder;
        TEST_ASSERT(placeholder.start("127.0.0.1", 0), "placeholder bound");
        port = placeholder.port();
        placeholder.close();
    }
    auto rest = RestTemplateBuilder()
                    .allowHost("127.0.0.1")
                    .connectTimeout(3000)
                    .timeout(5000)
                    .build();
    const auto begin = std::chrono::steady_clock::now();
    const HttpResponse resp = rest.get("http://127.0.0.1:" + std::to_string(port) + "/");
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - begin)
                                .count();
    TEST_ASSERT(resp.statusCode == -1, "refused reports -1");
    TEST_ASSERT(!resp.error.empty(), "typed error present");
    TEST_ASSERT(resp.body.empty(), "no body on refusal");
    TEST_ASSERT(elapsed_ms < 8000, "refusal returns promptly");
    return true;
}

bool test_async_future() {
    HttpServer server;
    TEST_ASSERT(server.start({HttpServer::make_response(200, "OK", "async-body",
                                                        "text/plain")}),
                "server up");
    auto rest = RestTemplateBuilder().allowHost("127.0.0.1").build();
    auto future = rest.getAsync(server.base_url() + "/async");
    TEST_ASSERT(future.wait_for(std::chrono::seconds(5)) ==
                    std::future_status::ready,
                "async completed in budget");
    const HttpResponse resp = future.get();
    TEST_ASSERT(resp.statusCode == 200 && resp.body == "async-body",
                "async body verbatim");
    server.stop();
    return true;
}

#endif // !_WIN32

} // namespace

int main() {
#if defined(_WIN32)
    if (!test_whitelist_matching()) {
        return 1;
    }
    if (!test_whitelist_default_deny()) {
        return 1;
    }
    std::cout << "RestCurlTests: windows-scope cases passed" << std::endl;
    return 0;
#else
    const struct {
        const char* name;
        bool (*fn)();
    } cases[] = {
        {"whitelist_matching", test_whitelist_matching},
        {"whitelist_default_deny", test_whitelist_default_deny},
        {"real_get_roundtrip", test_real_get_roundtrip},
        {"real_post_echo", test_real_post_echo},
        {"redirect_guard", test_redirect_guard},
        {"refused_typed_error", test_refused_typed_error},
        {"async_future", test_async_future},
    };
    int failed = 0;
    for (const auto& c : cases) {
        if (!c.fn()) {
            std::cerr << "CASE FAILED: " << c.name << std::endl;
            ++failed;
        }
    }
    if (failed == 0) {
        std::cout << "RestCurlTests: all " << (sizeof(cases) / sizeof(cases[0]))
                  << " cases passed" << std::endl;
        return 0;
    }
    std::cout << "RestCurlTests: " << failed << " case(s) failed" << std::endl;
    return 1;
#endif
}
