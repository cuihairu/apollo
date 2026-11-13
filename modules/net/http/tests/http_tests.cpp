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
// Content-Length 不崩、httpGet url 解析生效；audit C2：巨值/超上限
// Content-Length 与无长度流式灌入 → 413 拒绝（原协议混淆+OOM 面）。
#include "apollo/net/http.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
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


// audit C2 回归：①巨值 Content-Length（u64 回绕面）→ 413 拒绝而非协议混淆；
// ②声明体长超上限 → 413；③无 Content-Length 流式灌入顶缓冲上限 → 413 断开
bool test_server_huge_content_length_rejected() {
    auto server = createHttpServer();
    server->setRouter(std::make_shared<Router>());
    TEST_ASSERT(server->start("127.0.0.1", kPortBase + 8), "server start");

    int fd = connectTcp(kPortBase + 8);
    TEST_ASSERT(fd >= 0, "tcp connect");
    const char* req =
        "POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 18446744073709551615\r\n\r\n";
    sendAll(fd, req, std::strlen(req));

    char buf[512] = {};
    ssize_t n = ::recv(fd, buf, sizeof(buf) - 1, 0);
    TEST_ASSERT(n > 0, "server responded");
    TEST_ASSERT(std::strstr(buf, "413") != nullptr, "huge length gets 413");

    ::close(fd);
    server->stop();
    return true;
}

bool test_server_body_over_cap_rejected() {
    auto server = createHttpServer();
    server->setRouter(std::make_shared<Router>());
    TEST_ASSERT(server->start("127.0.0.1", kPortBase + 9), "server start");

    int fd = connectTcp(kPortBase + 9);
    TEST_ASSERT(fd >= 0, "tcp connect");
    const std::string req =
        "POST / HTTP/1.1\r\nHost: x\r\nContent-Length: " +
        std::to_string(apollo::net::http::RequestParser::kMaxBodySize + 1) +
        "\r\n\r\n";
    sendAll(fd, req.data(), req.size());

    char buf[512] = {};
    ssize_t n = ::recv(fd, buf, sizeof(buf) - 1, 0);
    TEST_ASSERT(n > 0, "server responded");
    TEST_ASSERT(std::strstr(buf, "413") != nullptr, "over-cap body gets 413");

    ::close(fd);
    server->stop();
    return true;
}

bool test_server_header_flood_capped() {
    auto server = createHttpServer();
    server->setRouter(std::make_shared<Router>());
    TEST_ASSERT(server->start("127.0.0.1", kPortBase + 10), "server start");

    int fd = connectTcp(kPortBase + 10);
    TEST_ASSERT(fd >= 0, "tcp connect");
    // 头部洪泛：无 \r\n\r\n 终结、无 Content-Length，灌超缓冲上限
    const std::string junk(64 * 1024, 'x');
    const size_t flood = apollo::net::http::RequestParser::kMaxBodySize + 2 * 1024 * 1024;
    for (size_t off = 0; off < flood; off += junk.size()) {
        sendAll(fd, junk.data(), junk.size());  // 服务端提前断开时静默失败
    }

    char buf[512] = {};
    ssize_t n = ::recv(fd, buf, sizeof(buf) - 1, 0);
    TEST_ASSERT(n > 0, "server responded or closed");
    TEST_ASSERT(std::strstr(buf, "413") != nullptr, "header flood gets 413");

    ::close(fd);
    server->stop();
    return true;
}

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

// audit M2：用户路由抛异常不得穿出连接线程（穿出即 std::terminate 杀宿主
// 进程）——现场渲染 500；keep-alive 连接上后续请求照常服务（连接线程存活
// 证明：修复前第一发即进程死，用例到不了后续断言）。
bool test_server_route_exception_returns_500() {
    auto server = createHttpServer();
    auto router = std::make_shared<Router>();
    router->get("/ok", [](const apollo::net::http::Request&) {
        return apollo::net::http::Response::ok("fine");
    });
    router->get("/boom", [](const apollo::net::http::Request&)
                   -> apollo::net::http::Response {
        throw std::runtime_error("route blew up");
    });
    router->get("/boom2", [](const apollo::net::http::Request&)
                    -> apollo::net::http::Response {
        throw 42;  // 非 std::exception 抛掷物
    });
    server->setRouter(router);
    TEST_ASSERT(server->start("127.0.0.1", kPortBase + 11), "server start");

    auto client = createHttpClient();
    TEST_ASSERT(client->connect("127.0.0.1", kPortBase + 11, 2000), "client connect");
    client->setResponseTimeout(2000);

    const auto boom = client->get("/boom");
    TEST_ASSERT(boom.status == StatusCode::InternalServerError, "throwing route renders 500");
    TEST_ASSERT(boom.body == "Internal Server Error", "500 body rendered");

    const auto boom2 = client->get("/boom2");
    TEST_ASSERT(boom2.status == StatusCode::InternalServerError,
                "non-exception throw renders 500 too");

    const auto ok = client->get("/ok");
    TEST_ASSERT(ok.status == StatusCode::OK, "connection survives route exception");

    client->disconnect();
    server->stop();
    return true;
}

// 原始假服务端：accept 一连后回吐预置字节，短歇后收线（字节级控制——
// 服务端真实 Router 无法产出畸形状态行/巨值 CL 的原始字节）
static int startRawResponder(uint16_t port, const std::string& responseBytes) {
    const int listenFd = ::socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    ::setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (listenFd < 0 || ::bind(listenFd, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0 ||
        ::listen(listenFd, 1) != 0) {
        if (listenFd >= 0) ::close(listenFd);
        return -1;
    }
    std::thread([listenFd, responseBytes]() {
        const int fd = ::accept(listenFd, nullptr, nullptr);
        if (fd >= 0) {
            size_t off = 0;
            while (off < responseBytes.size()) {
#ifdef MSG_NOSIGNAL
                const int flags = MSG_NOSIGNAL;
#else
                const int flags = 0;
#endif
                const ssize_t n =
                    ::send(fd, responseBytes.data() + off, responseBytes.size() - off, flags);
                if (n <= 0) break;
                off += static_cast<size_t>(n);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            ::close(fd);
        }
        ::close(listenFd);
    }).detach();
    return 0;
}

// audit M5①：对端声明巨值 Content-Length（u64 max）不得经
// headerBytesReceived+contentLength 加法回绕把残帧当完整成功包——超上限
// 声明按协议错回 500（修复前 1 字节残体 + 200 OK 直接放行）。
bool test_client_giant_content_length_rejected() {
    TEST_ASSERT(startRawResponder(kPortBase + 12,
                                  "HTTP/1.1 200 OK\r\n"
                                  "Content-Length: 18446744073709551615\r\n"
                                  "\r\n"
                                  "x") == 0,
                "raw responder up");

    auto client = createHttpClient();
    TEST_ASSERT(client->connect("127.0.0.1", kPortBase + 12, 2000), "client connect");
    client->setMaxResponseSize(4096);
    client->setResponseTimeout(2000);

    const auto resp = client->get("/");
    TEST_ASSERT(resp.status == StatusCode::InternalServerError,
                "giant declared length rejected as protocol error");

    client->disconnect();
    return true;
}

// audit M5②：垃圾状态行（stoi 抛）与越界码（99999）不得静默保持默认
// 200 OK（调用面会当成功包消费）——无效状态按 500 上抛失败语义。
bool test_client_invalid_status_rejected() {
    TEST_ASSERT(startRawResponder(kPortBase + 13,
                                  "HTTP/1.1 abc def\r\n"
                                  "Content-Length: 2\r\n"
                                  "\r\n"
                                  "hi") == 0,
                "garbage status responder up");
    TEST_ASSERT(startRawResponder(kPortBase + 14,
                                  "HTTP/1.1 99999 out-of-range\r\n"
                                  "Content-Length: 2\r\n"
                                  "\r\n"
                                  "hi") == 0,
                "out-of-range status responder up");

    auto client = createHttpClient();
    client->setResponseTimeout(2000);

    TEST_ASSERT(client->connect("127.0.0.1", kPortBase + 13, 2000), "client connect");
    const auto garbage = client->get("/");
    TEST_ASSERT(garbage.status == StatusCode::InternalServerError,
                "garbage status line rejected");
    client->disconnect();

    TEST_ASSERT(client->connect("127.0.0.1", kPortBase + 14, 2000), "client reconnect");
    const auto outOfRange = client->get("/");
    TEST_ASSERT(outOfRange.status == StatusCode::InternalServerError,
                "out-of-range status code rejected");
    client->disconnect();

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
    run("test_server_huge_content_length_rejected", test_server_huge_content_length_rejected);
    run("test_server_body_over_cap_rejected", test_server_body_over_cap_rejected);
    run("test_server_header_flood_capped", test_server_header_flood_capped);
    run("test_server_route_exception_returns_500", test_server_route_exception_returns_500);
    run("test_client_giant_content_length_rejected", test_client_giant_content_length_rejected);
    run("test_client_invalid_status_rejected", test_client_invalid_status_rejected);
#endif
    run("test_interface_compiles", test_interface_compiles);
    std::cout << "=== Results: " << passed << "/" << total << " passed ===" << std::endl;
    return (passed == total) ? 0 : 1;
}
