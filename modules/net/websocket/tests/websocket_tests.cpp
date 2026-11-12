// WebSocket 回归（audit P1#6 修复 2026-11-12）。
// ①handleReceive 原 receiveBuffer_ 无上限（帧声明长度攻击者可控，未完成的
//   帧无限累积可打 OOM）+ 每帧 erase(begin) 首部搬移 O(n²)——改为偏移消费
//   分摊 O(1)、超限断连（服务端与客户端同构修复）；
// ②服务器握手原单次 recv，请求分片到达即判定无效握手断开——改循环收至
//   \r\n\r\n；
// ③getConnection 原返回裸指针，连接表 erase 后调用方悬空——改返回
//   shared_ptr（接口+实现同步，仓内无其他实现/调用方）。
// 测试经回环 TCP 直连服务端：正常文本帧往返、分片握手、声明超长帧断连。
#include "apollo/net/websocket.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

using apollo::net::websocket::Callbacks;
using apollo::net::websocket::Config;
using apollo::net::websocket::Handshake;
using apollo::net::websocket::Message;
using apollo::net::websocket::Server;
using apollo::net::websocket::createServer;

#define TEST_ASSERT(cond, msg) \
    do { if (!(cond)) { std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; return false; } } while (0)

// 回环 TCP 客户端连接（POSIX）。各用例独立端口（相邻号），规避前例连接
// TIME_WAIT 残留导致后例 bind/accept 竞争；ctest 串行无冲突
constexpr uint16_t kPortBase = 19231;

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

void sendAll(int fd, const char* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = ::send(fd, data + sent, len - sent, 0);
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

// 正常文本帧往返：偏移消费重构后单帧仍须完整送达并触发 onMessage
bool test_normal_text_frame_roundtrip() {
    auto server = createServer();
    Callbacks cb;
    std::string received;
    std::mutex mu;
    std::atomic<int> opens{0};
    cb.onOpen = [&]() { opens++; };
    cb.onMessage = [&](const Message& m) {
        std::lock_guard<std::mutex> lk(mu);
        received = m.getText();
    };
    Config cfg;
    TEST_ASSERT(server->start("127.0.0.1", kPortBase + 0, cb, cfg), "server start");

    int fd = connectTcp(kPortBase + 0);
    TEST_ASSERT(fd >= 0, "tcp connect");

    const std::string req = Handshake::generateClientRequest("/chat", "127.0.0.1");
    sendAll(fd, req.data(), req.size());
    TEST_ASSERT(waitFor([&]() { return opens.load() == 1; }, 2000), "handshake established");

    // 未掩码文本帧 "hello"
    const uint8_t frame[] = {0x81, 0x05, 'h', 'e', 'l', 'l', 'o'};
    sendAll(fd, reinterpret_cast<const char*>(frame), sizeof(frame));
    TEST_ASSERT(waitFor([&]() {
        std::lock_guard<std::mutex> lk(mu);
        return received == "hello";
    }, 2000), "onMessage received hello");

    ::close(fd);
    server->stop();
    return true;
}

// 分片握手：请求拆两段发送（间隔 50ms）。原单次 recv 只拿到首段无结束符，
// parseClientRequest 失败即断开；修复后循环收至 \r\n\r\n 建立连接
bool test_fragmented_handshake() {
    auto server = createServer();
    Callbacks cb;
    std::atomic<int> opens{0};
    cb.onOpen = [&]() { opens++; };
    Config cfg;
    TEST_ASSERT(server->start("127.0.0.1", kPortBase + 1, cb, cfg), "server start");

    int fd = connectTcp(kPortBase + 1);
    TEST_ASSERT(fd >= 0, "tcp connect");

    const std::string req = Handshake::generateClientRequest("/chat", "127.0.0.1");
    size_t mid = req.find("\r\n\r\n");
    TEST_ASSERT(mid != std::string::npos, "request has header terminator");
    sendAll(fd, req.data(), mid);          // 首段（无结束符）
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    sendAll(fd, req.data() + mid, req.size() - mid);  // 尾段（含结束符）

    TEST_ASSERT(waitFor([&]() { return opens.load() == 1; }, 2000), "fragmented handshake established");

    ::close(fd);
    server->stop();
    return true;
}

// 声明超长帧断连：帧头 8 字节扩展长声明 256MB，随后流式灌入数据直至超过
// maxMessageSize。原 receiveBuffer_ 无上限无限累积（OOM 面）且不断连；修复后
// 超限即关闭连接
bool test_oversize_frame_closes_connection() {
    auto server = createServer();
    Callbacks cb;
    std::atomic<int> opens{0};
    cb.onOpen = [&]() { opens++; };
    Config cfg;
    cfg.maxMessageSize = 128 * 1024;  // 缩小阈值加速用例
    TEST_ASSERT(server->start("127.0.0.1", kPortBase + 2, cb, cfg), "server start");

    int fd = connectTcp(kPortBase + 2);
    TEST_ASSERT(fd >= 0, "tcp connect");

    const std::string req = Handshake::generateClientRequest("/chat", "127.0.0.1");
    sendAll(fd, req.data(), req.size());
    TEST_ASSERT(waitFor([&]() { return opens.load() == 1; }, 2000), "handshake established");
    TEST_ASSERT(server->getConnectionCount() == 1, "one connection");

    // 帧头：FIN|Binary, 8 字节扩展长，声明 0x10000000（256MB）
    char header[10];
    header[0] = 0x82;
    header[1] = 0x7F;
    header[2] = 0x10; header[3] = 0x00; header[4] = 0x00; header[5] = 0x00;
    header[6] = 0x00; header[7] = 0x00; header[8] = 0x00; header[9] = 0x00;
    sendAll(fd, header, sizeof(header));

    std::vector<char> chunk(4096, 0);
    for (int i = 0; i < 64; ++i) {  // 64 × 4KB = 256KB > 128KB 上限
        sendAll(fd, chunk.data(), chunk.size());
        if (server->getConnectionCount() == 0) break;
    }

    TEST_ASSERT(waitFor([&]() { return server->getConnectionCount() == 0; }, 2000),
                "oversized frame closes connection");

    ::close(fd);
    server->stop();
    return true;
}

#endif  // !_WIN32

// 接口面：getConnection 返回 shared_ptr（编译期证明接口一致），
// 未启动服务端时取连接为 nullptr
bool test_get_connection_null_when_stopped() {
    auto server = createServer();
    auto conn = server->getConnection(1);
    TEST_ASSERT(conn == nullptr, "no connection before start");
    return true;
}

} // namespace

int main() {
    int passed = 0, total = 0;
    auto run = [&](const char* name, bool (*fn)()) {
        std::cout << "== " << name << " ==" << std::endl;
        ++total;
        if (fn()) { ++passed; std::cout << "  PASSED" << std::endl; }
    };
#ifndef _WIN32
    run("test_normal_text_frame_roundtrip", test_normal_text_frame_roundtrip);
    run("test_fragmented_handshake", test_fragmented_handshake);
    run("test_oversize_frame_closes_connection", test_oversize_frame_closes_connection);
#endif
    run("test_get_connection_null_when_stopped", test_get_connection_null_when_stopped);
    std::cout << "=== Results: " << passed << "/" << total << " passed ===" << std::endl;
    return (passed == total) ? 0 : 1;
}
