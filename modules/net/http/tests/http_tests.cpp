// HTTP 回归（audit P1#7 修复 2026-11-12）。
// 服务端：①连接线程原 detach 捕获裸 this，server 析构后 handler 访问
//   router_ 悬空；connections_ 只增不减（removeConnection 无人调用）每连接
//   泄漏 shared_ptr——改为连接线程持 router/registry shared_ptr，线程经共享
//   状态访问，退出统一 remove；stop() 收线：置 shutdown 标志 → select 周期
//   退出 → join 全部连接线程（原阻塞 recv 无法收线）。
// 客户端：②receiveResponse 原无超时无大小上限（对端不发永久阻塞/可 OOM）
//   ——加 select 超时 + 响应大小上限；③std::stoull/stoi 对垃圾 Content-Length
//   抛异常——包 try 按无体处理；④httpGet/httpPost 原忽略 url 恒连
//   localhost:80——按 url 解析 host/port/path（https 拒绝）。
// 测试经回环 TCP 直连：GET/POST 往返、连接关闭后表项移除、stop() 快速收线、
// 静默对端响应超时、垃圾 Content-Length 不抛、超大响应封顶、服务端垃圾
// Content-Length 不崩、httpGet url 解析生效。
#include "apollo/net/http.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

using apollo::net::http::HttpClient;
using apollo::net::http::HttpServer;
using apollo::net::http::Router;
using apollo::net::http::StatusCode;
using apollo::net::http::createHttpClient;
using apollo::net::http::createHttpServer;
using apollo::net::http::httpGet;

#define TEST_ASSERT(cond, msg) \
    do { if (!(cond)) { std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; return false; } } while (0)

// 回环 TCP 客户端连接（POSIX）。各用例独立端口（相邻号），规避前例连接
// TIME_WAIT 残留导致后例 bind/accept 竞争；ctest 串行无冲突
constexpr uint16_t kPortBase = 19241;

#ifndef _WIN32

int connectTcp(uint16_t port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        ::close(fd);
        return -1;
    }
    return fd;
}

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0  // macOS/BSD 无此标志（进程已 ignore SIGPIPE，兜底）
#endif

void sendAll(int fd, const char* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = ::send(fd, data + sent, len - sent, MSG_NOSIGNAL);
        if (n <= 0) return;  // 对端关闭/错误，停止
        sent += static_cast<size_t>(n);
    }
}

bool waitFor(const std::function<bool()>& pred, int timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (pred()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return pred();
}

// 起一个 raw TCP 静默服务端：accept 后不发送任何数据（模拟对端不发响应），
// 用于客户端响应超时用例。监听套接字交由后台线程持有
void startSilentServer(uint16_t port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    ::bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    ::listen(fd, 8);
    std::thread([fd]() {
        struct sockaddr_in peer {};
        socklen_t peerLen = sizeof(peer);
        int c = ::accept(fd, reinterpret_cast<struct sockaddr*>(&peer), &peerLen);
        // 保持连接不发送任何数据
        std::this_thread::sleep_for(std::chrono::seconds(3));
        if (c >= 0) ::close(c);
        ::close(fd);
    }).detach();
}

// ①a GET/POST 往返：请求经服务端 handler 处理并回包
bool test_roundtrip() {
    auto server = createHttpServer();
    auto router = std::make_shared<Router>();
    router->get("/hello", [](const apollo::net::http::Request&) {
        return apollo::net::http::Response::ok("hello world");
    });
    server->setRouter(router);
    TEST_ASSERT(server->start("127.0.0.1", kPortBase + 0), "server start");

    auto client = createHttpClient();
    TEST_ASSERT(client->connect("127.0.0.1", kPortBase + 0, 2000), "client connect");
    auto resp = client->get("/hello");
    TEST_ASSERT(resp.status == StatusCode::OK, "status OK");
    TEST_ASSERT(resp.body == "hello world", "body roundtrip");
    client->disconnect();
    server->stop();
    return true;
}

// ①b 连接关闭后表项移除：客户端断开 → 连接线程退出统一 removeConnection，
// 计数归零（原 removeConnection 无人调用，计数只增不减）
bool test_connection_removed_after_close() {
    auto server = createHttpServer();
    server->setRouter(std::make_shared<Router>());
    TEST_ASSERT(server->start("127.0.0.1", kPortBase + 1), "server start");

    int fd = connectTcp(kPortBase + 1);
    TEST_ASSERT(fd >= 0, "tcp connect");
    TEST_ASSERT(waitFor([&]() { return server->getConnectionCount() == 1; }, 2000),
                "one connection");

    sendAll(fd, "GET / HTTP/1.1\r\nHost: x\r\n\r\n", sizeof("GET / HTTP/1.1\r\nHost: x\r\n\r\n") - 1);
    char buf[512];
    ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
    TEST_ASSERT(n > 0, "got response");

    ::close(fd);
    TEST_ASSERT(waitFor([&]() { return server->getConnectionCount() == 0; }, 2000),
                "connection removed after close");

    server->stop();
    return true;
}

// ①c stop() 收线：连接线程静默挂起时 stop() 置 shutdown 并 join，须快速返回
// 且连接表清空（原阻塞 recv 无法收线，析构后线程访问 server 成员悬空）
bool test_stop_collects_threads() {
    auto server = createHttpServer();
    TEST_ASSERT(server->start("127.0.0.1", kPortBase + 2), "server start");

    int fd = connectTcp(kPortBase + 2);
    TEST_ASSERT(fd >= 0, "tcp connect");
    // 不发请求，连接线程挂起在 select
    TEST_ASSERT(waitFor([&]() { return server->getConnectionCount() == 1; }, 2000),
                "one connection");

    const auto t0 = std::chrono::steady_clock::now();
    server->stop();  // 收线：应快速返回（≤1s）
    const auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();
    TEST_ASSERT(dt < 1000, "stop returns promptly");
    TEST_ASSERT(server->getConnectionCount() == 0, "no connections after stop");

    ::close(fd);
    return true;
}

// ②a 客户端响应超时：对端静默不发 → select 超时返回（原阻塞 recv 永久挂起）
bool test_client_response_timeout() {
    startSilentServer(kPortBase + 3);

    auto client = createHttpClient();
    TEST_ASSERT(client->connect("127.0.0.1", kPortBase + 3, 2000), "client connect");
    client->setResponseTimeout(200);

    const auto t0 = std::chrono::steady_clock::now();
    auto resp = client->get("/");  // 对端不发响应
    const auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();
    (void)resp;
    TEST_ASSERT(dt < 1500, "timeout returns promptly (was infinite block)");

    client->disconnect();
    return true;
}

// ③ 客户端垃圾 Content-Length 不抛异常（原 std::stoull 抛异常未捕获）
bool test_client_garbage_content_length_no_throw() {
    auto server = createHttpServer();
    auto router = std::make_shared<Router>();
    router->get("/", [](const apollo::net::http::Request&) {
        apollo::net::http::Response r = apollo::net::http::Response::ok("body");
        r.headers.set("Content-Length", "notanumber");
        return r;
    });
    server->setRouter(router);
    TEST_ASSERT(server->start("127.0.0.1", kPortBase + 4), "server start");

    auto client = createHttpClient();
    TEST_ASSERT(client->connect("127.0.0.1", kPortBase + 4, 2000), "client connect");
    client->setResponseTimeout(1000);

    bool threw = false;
    try {
        auto resp = client->get("/");
        (void)resp;
    } catch (...) {
        threw = true;
    }
    TEST_ASSERT(!threw, "no throw on garbage content-length");

    client->disconnect();
    server->stop();
    return true;
}

// ②b 客户端响应大小上限：对端声明超大 Content-Length 并持续发数据，客户端
// 达上限即停止（原无上限无限累积可 OOM）
bool test_client_oversize_response_capped() {
    auto server = createHttpServer();
    auto router = std::make_shared<Router>();
    router->get("/", [](const apollo::net::http::Request&) {
        std::string big(65536, 'x');  // 64KB 体
        return apollo::net::http::Response::ok(big);
    });
    server->setRouter(router);
    TEST_ASSERT(server->start("127.0.0.1", kPortBase + 5), "server start");

    auto client = createHttpClient();
    TEST_ASSERT(client->connect("127.0.0.1", kPortBase + 5, 2000), "client connect");
    client->setMaxResponseSize(4096);  // 小上限加速用例
    client->setResponseTimeout(3000);

    const auto t0 = std::chrono::steady_clock::now();
    auto resp = client->get("/");
    const auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();
    // 原无上限：收满 64KB（body==65536）；修复后达 4KB 上限即停（body<65536）
    TEST_ASSERT(resp.body.size() < 65536, "response capped below full size");
    TEST_ASSERT(dt < 4000, "capped response returns promptly");

    client->disconnect();
    server->stop();
    return true;
}

// 服务端垃圾 Content-Length 不崩（原 RequestParser std::stoll 抛异常，
// 连接线程未捕获 → std::terminate）
bool test_server_garbage_content_length_no_crash() {
    auto server = createHttpServer();
    server->setRouter(std::make_shared<Router>());
    TEST_ASSERT(server->start("127.0.0.1", kPortBase + 6), "server start");

    int fd = connectTcp(kPortBase + 6);
    TEST_ASSERT(fd >= 0, "tcp connect");
    sendAll(fd, "POST / HTTP/1.1\r\nHost: x\r\nContent-Length: garbage\r\n\r\n",
            sizeof("POST / HTTP/1.1\r\nHost: x\r\nContent-Length: garbage\r\n\r\n") - 1);

    char buf[512];
    ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
    TEST_ASSERT(n > 0, "server responded without crash");

    ::close(fd);
    server->stop();
    return true;
}

// ④ httpGet url 解析生效：非 80 端口 + 路径按 url 解析（原忽略 url 恒连
// localhost:80）
bool test_httpget_url_parsing() {
    auto server = createHttpServer();
    auto router = std::make_shared<Router>();
    std::atomic<bool> hit{false};
    router->get("/hello", [&](const apollo::net::http::Request&) {
        hit = true;
        return apollo::net::http::Response::ok("pong");
    });
    server->setRouter(router);
    TEST_ASSERT(server->start("127.0.0.1", kPortBase + 7), "server start");

    std::string url = "http://127.0.0.1:" + std::to_string(kPortBase + 7) + "/hello";
    auto resp = httpGet(url, 2000);
    TEST_ASSERT(resp.status == StatusCode::OK, "status OK");
    TEST_ASSERT(hit.load(), "route hit via parsed url");
    TEST_ASSERT(resp.body == "pong", "body from parsed url");

    server->stop();
    return true;
}

// ④ https 拒绝：内置客户端仅 TCP 不支持 TLS，返回 serverError（不连接）
bool test_httpget_https_rejected() {
    auto resp = httpGet("https://127.0.0.1:1/x", 200);
    TEST_ASSERT(resp.status == StatusCode::InternalServerError, "https rejected");
    TEST_ASSERT(resp.body == "Invalid URL", "invalid url body");
    return true;
}

#endif  // !_WIN32

// 接口面：createHttpServer/createHttpClient 可构造，setResponseTimeout/
// setMaxResponseSize 接口一致（编译期），未启动服务端 isRunning 为 false
bool test_interface_compiles() {
    auto server = createHttpServer();
    TEST_ASSERT(!server->isRunning(), "not running before start");
    TEST_ASSERT(server->getConnectionCount() == 0, "no connections before start");

    auto client = createHttpClient();
    client->setResponseTimeout(100);
    client->setMaxResponseSize(1024);
    return true;
}

} // namespace

int main() {
#ifndef _WIN32
    // macOS Release：对端关闭后仍 send → SIGPIPE 默认杀进程（linux 用
    // MSG_NOSIGNAL，macOS 无该标志）——进程忽略之
    std::signal(SIGPIPE, SIG_IGN);
#endif
    int passed = 0, total = 0;
    auto run = [&](const char* name, bool (*fn)()) {
        std::cout << "== " << name << " ==" << std::endl;
        ++total;
        if (fn()) { ++passed; std::cout << "  PASSED" << std::endl; }
    };
#ifndef _WIN32
    run("test_roundtrip", test_roundtrip);
    run("test_connection_removed_after_close", test_connection_removed_after_close);
    run("test_stop_collects_threads", test_stop_collects_threads);
    run("test_client_response_timeout", test_client_response_timeout);
    run("test_client_garbage_content_length_no_throw", test_client_garbage_content_length_no_throw);
    run("test_client_oversize_response_capped", test_client_oversize_response_capped);
    run("test_server_garbage_content_length_no_crash", test_server_garbage_content_length_no_crash);
    run("test_httpget_url_parsing", test_httpget_url_parsing);
    run("test_httpget_https_rejected", test_httpget_https_rejected);
#endif
    run("test_interface_compiles", test_interface_compiles);
    std::cout << "=== Results: " << passed << "/" << total << " passed ===" << std::endl;
    return (passed == total) ? 0 : 1;
}
