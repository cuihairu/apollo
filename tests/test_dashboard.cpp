// 内部状态 Dashboard responder 测试（DASH-1，用户令 2026-11-02）。
// DashboardResponder 真端口往返：GET / → 200 text/html、GET /api/state →
// 200 application/json（回调产出）、其余路径 404、token 面（未设放行/设置
// 后 401）、连接超时弃连、应答后连接表清空。Windows 只跑空件面（link TCP
// 底座 POSIX only，同 metricspush_tests 口径）。

#include "world_app/dashboard.hpp"
#include "world_app/dashboard_data.hpp"
#include "apollo/core/metrics/metric_registry.h"

#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#ifndef APOLLO_CONTRACT_ROUTE_JSON
#define APOLLO_CONTRACT_ROUTE_JSON ""
#endif

#define TEST_ASSERT(cond, msg)                                                           \
    do {                                                                                 \
        if (!(cond)) {                                                                   \
            std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")"   \
                      << std::endl;                                                      \
            return false;                                                                \
        }                                                                                \
    } while (0)

namespace {

namespace link = apollo::net::link;
namespace wapp = world_app;

std::uint64_t now_ms() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

#if !defined(_WIN32)

struct HttpReply {
    bool connected = false;
    std::string body;
};

// 拨号（宿主泵驱动）+ 全量写出 + 读到 EOF（服务端 Connection: close）。
// 与 test_metrics_push.cpp http_fetch 同型——dashboard 与 metrics 同为
// 宿主泵驱动的单拍 responder。
HttpReply http_fetch(wapp::DashboardResponder& http, std::uint16_t port,
                     const std::string& request) {
    HttpReply reply;
    link::TcpConnector connector;
    if (!connector.start("127.0.0.1", port, 3000, now_ms())) {
        return reply;
    }
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(5000);
    while (connector.state() == link::ConnectState::kConnecting) {
        if (connector.pump(now_ms()) != link::TcpConnector::PumpResult::kInProgress) {
            break;
        }
        if (std::chrono::steady_clock::now() > deadline) {
            return reply;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    if (connector.state() != link::ConnectState::kConnected) {
        return reply;
    }
    auto transport = connector.take_transport();
    if (transport == nullptr) {
        return reply;
    }
    reply.connected = true;

    std::size_t sent = 0;
    while (sent < request.size()) {
        const int n = transport->write(
            reinterpret_cast<const std::uint8_t*>(request.data()) + sent,
            request.size() - sent);
        if (n < 0) {
            return reply;
        }
        if (n > 0) {
            sent += static_cast<std::size_t>(n);
        } else if (std::chrono::steady_clock::now() > deadline) {
            return reply;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }

    std::uint8_t buf[4096];
    while (std::chrono::steady_clock::now() < deadline) {
        http.pump(now_ms());
        const int n = transport->read(buf, sizeof buf);
        if (n < 0) {
            break;  // 对端关闭（Connection: close 后 EOF）
        }
        if (n > 0) {
            reply.body.append(reinterpret_cast<const char*>(buf),
                              static_cast<std::size_t>(n));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
    return reply;
}

bool test_routes() {
    // GET / → 200 text/html 内嵌单页；GET /api/state → 200 application/json
    // 回调产出；其余路径 404。
    wapp::DashboardConfig cfg;
    cfg.port = 0;
    wapp::DashboardResponder http(
        cfg, [] { return std::string("{\"process\":{\"pid\":7}}"); });
    TEST_ASSERT(http.start(), "http bound");
    const std::uint16_t port = http.port();
    TEST_ASSERT(port != 0, "ephemeral port observed");

    const HttpReply root = http_fetch(http, port, "GET / HTTP/1.1\r\nHost: x\r\n\r\n");
    TEST_ASSERT(root.connected, "root client connected");
    TEST_ASSERT(root.body.rfind("HTTP/1.1 200 OK\r\n", 0) == 0, "root 200");
    TEST_ASSERT(root.body.find("Content-Type: text/html; charset=utf-8\r\n")
                    != std::string::npos,
                "root html content type");
    TEST_ASSERT(root.body.find("Apollo 状态") != std::string::npos,
                "root html body");

    const HttpReply state = http_fetch(http, port, "GET /api/state HTTP/1.1\r\n\r\n");
    TEST_ASSERT(state.connected, "state client connected");
    TEST_ASSERT(state.body.rfind("HTTP/1.1 200 OK\r\n", 0) == 0, "state 200");
    TEST_ASSERT(state.body.find("Content-Type: application/json\r\n")
                    != std::string::npos,
                "state json content type");
    TEST_ASSERT(state.body.find("{\"process\":{\"pid\":7}}") != std::string::npos,
                "state callback body");

    const HttpReply nf = http_fetch(http, port, "GET /nope HTTP/1.1\r\n\r\n");
    TEST_ASSERT(nf.connected, "nf client connected");
    TEST_ASSERT(nf.body.rfind("HTTP/1.1 404 Not Found\r\n", 0) == 0,
                "404 on other path");

    // 应答完成后连接表清空
    for (int i = 0; i < 5; ++i) {
        http.pump(now_ms());
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    TEST_ASSERT(http.served() == 3, "three connections served");
    TEST_ASSERT(http.connections() == 0, "connections drained");
    TEST_ASSERT(http.aborted() == 0, "nothing aborted");
    TEST_ASSERT(http.refused() == 0, "nothing refused");
    TEST_ASSERT(http.unauthorized() == 0, "nothing unauthorized");
    http.stop();
    return true;
}

bool test_token() {
    // token 设置后：无 Authorization / 错 token → 401；正确 → 200。未设 →
    // 直接放行（上面 test_routes 已证）。
    wapp::DashboardConfig cfg;
    cfg.port = 0;
    cfg.token = "s3cret";
    wapp::DashboardResponder http(cfg, [] { return std::string("{}"); });
    TEST_ASSERT(http.start(), "http bound");
    const std::uint16_t port = http.port();

    const HttpReply noauth =
        http_fetch(http, port, "GET /api/state HTTP/1.1\r\n\r\n");
    TEST_ASSERT(noauth.connected, "noauth client connected");
    TEST_ASSERT(noauth.body.rfind("HTTP/1.1 401 Unauthorized\r\n", 0) == 0,
                "no token → 401");

    const HttpReply badauth =
        http_fetch(http, port,
                   "GET /api/state HTTP/1.1\r\nAuthorization: Bearer wrong\r\n\r\n");
    TEST_ASSERT(badauth.connected, "badauth client connected");
    TEST_ASSERT(badauth.body.rfind("HTTP/1.1 401 Unauthorized\r\n", 0) == 0,
                "wrong token → 401");

    const HttpReply ok =
        http_fetch(http, port,
                   "GET /api/state HTTP/1.1\r\nAuthorization: Bearer s3cret\r\n\r\n");
    TEST_ASSERT(ok.connected, "ok client connected");
    TEST_ASSERT(ok.body.rfind("HTTP/1.1 200 OK\r\n", 0) == 0,
                "correct token → 200");

    // 未带 token 的 GET / 也 401（全路由生效）
    const HttpReply root_401 =
        http_fetch(http, port, "GET / HTTP/1.1\r\n\r\n");
    TEST_ASSERT(root_401.connected, "root_401 connected");
    TEST_ASSERT(root_401.body.rfind("HTTP/1.1 401 Unauthorized\r\n", 0) == 0,
                "root no token → 401");

    TEST_ASSERT(http.unauthorized() == 3, "three unauthorized");
    http.stop();
    return true;
}

bool test_abort() {
    // 连接占用超时：只发半个请求头不关 → 超时弃连。
    wapp::DashboardConfig cfg;
    cfg.port = 0;
    cfg.write_timeout_ms = 100;
    wapp::DashboardResponder http(cfg, [] { return std::string("{}"); });
    TEST_ASSERT(http.start(), "http bound");
    const std::uint16_t port = http.port();

    link::TcpConnector connector;
    TEST_ASSERT(connector.start("127.0.0.1", port, 3000, now_ms()), "dial");
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(3000);
    while (connector.state() == link::ConnectState::kConnecting) {
        if (connector.pump(now_ms()) != link::TcpConnector::PumpResult::kInProgress) {
            break;
        }
        if (std::chrono::steady_clock::now() > deadline) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    TEST_ASSERT(connector.state() == link::ConnectState::kConnected, "connected");
    auto transport = connector.take_transport();
    TEST_ASSERT(transport != nullptr, "transport taken");
    const char partial[] = "GET /api/state HTTP/1.1\r\nHost: x\r\n";  // 无空行
    transport->write(reinterpret_cast<const std::uint8_t*>(partial),
                     sizeof(partial) - 1);

    // 驱动超过超时窗（连接在表内未排空）
    for (int i = 0; i < 30; ++i) {
        http.pump(now_ms());
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    TEST_ASSERT(http.aborted() >= 1, "timeout aborted");
    TEST_ASSERT(http.connections() == 0, "connection drained after abort");
    http.stop();
    return true;
}

bool test_data_collector() {
    // DashboardDataCollector：collect_links/collect_script/collect_metrics →
    // render JSON 形状（process/links/script/load/orders/fleet 六键），空态
    // 与填充态。
    wapp::DashboardProcessInfo info;
    info.role = "world";
    info.started_ms = 1000;
    wapp::DashboardDataCollector collector(info);

    // 空态：六键都在，fleet 空（fleet_dir 空 = 不读文件）
    {
        const auto text = collector.render();
        TEST_ASSERT(text.find("\"process\"") != std::string::npos, "process key");
        TEST_ASSERT(text.find("\"links\":[]") != std::string::npos, "links empty");
        TEST_ASSERT(text.find("\"orders\"") != std::string::npos, "orders key");
        TEST_ASSERT(text.find("\"script\":{\"modules\":[]}") != std::string::npos,
                    "script empty");
        TEST_ASSERT(text.find("\"load\"") != std::string::npos, "load key");
        TEST_ASSERT(text.find("\"fleet\":[]") != std::string::npos, "fleet empty");
        TEST_ASSERT(text.find("\"role\":\"world\"") != std::string::npos, "role");
    }

    // 填充态：links + script + metrics
    std::vector<apollo::net::link::LinkSnapshot> links;
    apollo::net::link::LinkSnapshot l;
    l.peer = "cell-21";
    l.state = "up";
    l.tls = true;
    l.tx = 42;
    l.rx = 7;
    links.push_back(l);
    collector.collect_links(links);

    std::map<std::string, std::uint64_t> mods;
    mods["prodgreet"] = 5;
    collector.collect_script(mods);

    apollo::core::metrics::MetricRegistry::Sample s;
    s.name = "world_ticks_total";
    s.is_counter = true;
    s.counter_value = 100;
    std::vector<apollo::core::metrics::MetricRegistry::Sample> samples;
    samples.push_back(s);
    collector.collect_metrics(samples);

    const auto text = collector.render();
    TEST_ASSERT(text.find("\"peer\":\"cell-21\"") != std::string::npos, "link peer");
    TEST_ASSERT(text.find("\"state\":\"up\"") != std::string::npos, "link state");
    TEST_ASSERT(text.find("\"tls\":true") != std::string::npos, "link tls");
    TEST_ASSERT(text.find("\"tx\":42") != std::string::npos, "link tx");
    TEST_ASSERT(text.find("\"rx\":7") != std::string::npos, "link rx");
    TEST_ASSERT(text.find("\"name\":\"prodgreet\",\"version\":5") != std::string::npos,
                "script module");
    TEST_ASSERT(text.find("\"ticks_total\":100") != std::string::npos, "ticks total");
    return true;
}

}  // namespace

bool test_html_override() {
    // 首页 HTML 覆写（gateway 维度页复用面）：config.html 非空 = GET /
    // 服覆写页；空 = 内建 world 页（test_routes 已证）。
    wapp::DashboardConfig cfg;
    cfg.port = 0;
    cfg.html = "<!DOCTYPE html><html><body>gateway-page</body></html>";
    wapp::DashboardResponder http(
        cfg, [] { return std::string("{\"role\":\"gateway\"}"); });
    TEST_ASSERT(http.start(), "http bound");
    const std::uint16_t port = http.port();

    const HttpReply root = http_fetch(http, port, "GET / HTTP/1.1\r\nHost: x\r\n\r\n");
    TEST_ASSERT(root.connected, "root client connected");
    TEST_ASSERT(root.body.find("gateway-page") != std::string::npos,
                "override html served");
    TEST_ASSERT(root.body.find("Apollo 状态") == std::string::npos,
                "builtin html not served");

    const HttpReply state = http_fetch(http, port, "GET /api/state HTTP/1.1\r\n\r\n");
    TEST_ASSERT(state.connected, "state client connected");
    TEST_ASSERT(state.body.find("{\"role\":\"gateway\"}") != std::string::npos,
                "state callback still wired");
    http.stop();
    return true;
}

bool test_recv_cap() {
    // read_and_maybe_answer recv 无上限：发送超过 kMaxRequestBytes 的请求
    // 头应被拒弃（aborted++），不无界增长。
    wapp::DashboardConfig cfg;
    cfg.port = 0;
    cfg.write_timeout_ms = 500;
    wapp::DashboardResponder http(cfg, [] { return std::string("{}"); });
    TEST_ASSERT(http.start(), "http bound");
    const std::uint16_t port = http.port();

    link::TcpConnector connector;
    TEST_ASSERT(connector.start("127.0.0.1", port, 3000, now_ms()), "dial");
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(3000);
    while (connector.state() == link::ConnectState::kConnecting) {
        if (connector.pump(now_ms()) != link::TcpConnector::PumpResult::kInProgress) {
            break;
        }
        if (std::chrono::steady_clock::now() > deadline) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    TEST_ASSERT(connector.state() == link::ConnectState::kConnected, "connected");
    auto transport = connector.take_transport();
    TEST_ASSERT(transport != nullptr, "transport taken");

    // 发送超过上限的请求（不含空行），无边界增长。
    const std::string big(wapp::DashboardResponder::kMaxRequestBytes + 4096, 'a');
    transport->write(reinterpret_cast<const std::uint8_t*>(big.data()), big.size());

    for (int i = 0; i < 30; ++i) {
        http.pump(now_ms());
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    // 双计回归：read_and_maybe_answer 超限路径不再自行 ++aborted_，pump()
    // 对同一 return false 只计一次。若回归为双计，此处 ==1 会失败。
    TEST_ASSERT(http.aborted() == 1, "oversized request counted exactly once");
    TEST_ASSERT(http.connections() == 0, "oversized connection drained");
    http.stop();
    return true;
}

bool test_route_boundary() {
    // /api/state 前缀匹配带路径边界："/api/stateXYZ" 等带后缀不命中
    // /api/state，应 404 而非 200。
    wapp::DashboardConfig cfg;
    cfg.port = 0;
    wapp::DashboardResponder http(
        cfg, [] { return std::string("{\"process\":{\"pid\":7}}"); });
    TEST_ASSERT(http.start(), "http bound");
    const std::uint16_t port = http.port();

    const HttpReply suffix = http_fetch(
        http, port, "GET /api/stateXYZ HTTP/1.1\r\nHost: x\r\n\r\n");
    TEST_ASSERT(suffix.connected, "suffix client connected");
    TEST_ASSERT(suffix.body.rfind("HTTP/1.1 404 Not Found\r\n", 0) == 0,
                "suffix path 404");

    const HttpReply query = http_fetch(
        http, port, "GET /api/state?x=1 HTTP/1.1\r\nHost: x\r\n\r\n");
    TEST_ASSERT(query.connected, "query client connected");
    TEST_ASSERT(query.body.rfind("HTTP/1.1 200 OK\r\n", 0) == 0,
                "query path 200");

    http.stop();
    return true;
}

int main() {
#if defined(_WIN32)
    std::cout << "DashboardTests: windows-scope passed" << std::endl;
    return 0;
#else
    if (!test_routes()) {
        return 1;
    }
    if (!test_html_override()) {
        return 1;
    }
    if (!test_token()) {
        return 1;
    }
    if (!test_abort()) {
        return 1;
    }
    if (!test_recv_cap()) {
        return 1;
    }
    if (!test_route_boundary()) {
        return 1;
    }
    if (!test_data_collector()) {
        return 1;
    }
    std::cout << "DashboardTests: passed" << std::endl;
    return 0;
#endif
}

#endif  // !_WIN32
