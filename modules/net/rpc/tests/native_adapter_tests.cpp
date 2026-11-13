// NativeAdapter 回归（audit P1#5 修复 2026-11-12）。
// ①NativeConnection::handleRecv 长度头原为 *reinterpret_cast<uint32_t*>
//   原生序裸读——非对齐/别名 UB，且小端机上与发送侧（SessionHelper::
//   sendPacket 大端编码）字节序互斥：正常包被判超长直接断连，一个包
//   都收不到。修复为逐字节大端组装（socketpair 对拍：大端帧必须原样
//   送达）。
// ②NativeNetworkManager::processEvents/stop 原无锁迭代 listeners_/
//   connectors_，与多 IO 线程并发及 createListener/removeListener 等
//   持锁变更方构成数据竞争——修复为持锁快照、出锁处理（用例注册真实
//   listener/connector 后 update() 走通快照路径，ASan 树加压）。
#include "apollo/net/adapters/native_adapter.h"

#include <cstring>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#ifndef _WIN32
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

using apollo::net::adapters::NativeConnection;
using apollo::net::adapters::NativeNetworkManager;

#define TEST_ASSERT(cond, msg) \
    do { if (!(cond)) { std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; return false; } } while (0)

// 按发送侧同款大端 4 字节长度头封装一帧
static void appendFrame(std::string& wire, const std::string& payload) {
    const uint32_t n = static_cast<uint32_t>(payload.size());
    wire += static_cast<char>((n >> 24) & 0xFF);
    wire += static_cast<char>((n >> 16) & 0xFF);
    wire += static_cast<char>((n >> 8) & 0xFF);
    wire += static_cast<char>(n & 0xFF);
    wire += payload;
}

#ifndef _WIN32
// 大端帧对拍：小端裸读把头 00 00 00 05 读成 0x05000000 判超长断连，
// 修复前本用例收不到任何包且连接被标记关闭
bool test_recv_framing_big_endian() {
    int fds[2];
    TEST_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0, "socketpair");

    auto conn = std::make_shared<NativeConnection>(fds[0], 1, "127.0.0.1", 1);
    std::vector<std::pair<uint32_t, std::string>> got;
    conn->setPacketHandler([&](NativeConnection*, const char* data, uint32_t len) {
        got.emplace_back(len, std::string(data, len));
    });

    std::string wire;
    appendFrame(wire, "hello");
    appendFrame(wire, "abc");
    const ssize_t wireLen = static_cast<ssize_t>(wire.size());
    TEST_ASSERT(::write(fds[1], wire.data(), wire.size()) == wireLen, "write frames");
    ::close(fds[1]);  // 关写端：缓冲数据照常可读，后续 recv 立即返回不悬挂

    // 对端已关，recv 必有返回；限次重试防意外分片
    for (int i = 0; i < 10 && got.size() < 2; ++i) {
        (void)conn->handleRecv();
    }
    TEST_ASSERT(got.size() == 2, "two packets delivered");
    TEST_ASSERT(got[0].first == 5 && got[0].second == "hello", "frame 1 intact");
    TEST_ASSERT(got[1].first == 3 && got[1].second == "abc", "frame 2 intact");
    TEST_ASSERT(conn->isConnected() && !conn->shouldClose(), "connection stays open");
    return true;
}

// 超长守卫：声明长度 > 64K 必须断连且不派发
bool test_recv_oversize_rejected() {
    int fds[2];
    TEST_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0, "socketpair");

    auto conn = std::make_shared<NativeConnection>(fds[0], 2, "127.0.0.1", 1);
    int delivered = 0;
    conn->setPacketHandler([&](NativeConnection*, const char*, uint32_t) { ++delivered; });

    const unsigned char oversized[4] = {0x00, 0x01, 0x00, 0x01};  // 65537 > 64K
    TEST_ASSERT(::write(fds[1], oversized, sizeof(oversized)) == 4, "write header");
    ::close(fds[1]);

    (void)conn->handleRecv();
    TEST_ASSERT(delivered == 0, "no packet dispatched");
    TEST_ASSERT(conn->shouldClose(), "oversize marks for close");
    return true;
}
#endif

// audit M4 同型残留：LengthPrefixPacketParser（公共 API 面第二份长度头
// 解码）同为原生序裸读——大端帧在小端机上读成字节翻转值（5 → 0x05000000
// 判超长返 -1）。修复为逐字节大端组装 + 减法判据防回绕（与 handleRecv
// 同款）。
bool test_length_prefix_parser_big_endian() {
    using apollo::net::adapters::LengthPrefixPacketParser;
    LengthPrefixPacketParser parser;

    // 大端帧 "hello"（5）：裸读当 0x05000000 超长返 -1
    std::string wire;
    appendFrame(wire, "hello");
    const int32_t r1 = parser.parse(wire.data(), static_cast<uint32_t>(wire.size()));
    TEST_ASSERT(r1 == static_cast<int32_t>(wire.size()), "big-endian frame accepted");

    // 不完整帧（头+1 字节体）：needMore 形态 0
    const int32_t r2 = parser.parse(wire.data(), 6);
    TEST_ASSERT(r2 == 0, "partial frame asks for more");
    // 头都不足：0
    TEST_ASSERT(parser.parse(wire.data(), 3) == 0, "short header asks for more");

    // 超长（65537 > 64K）：返 -1
    const unsigned char oversized[8] = {0x00, 0x01, 0x00, 0x01, 'x', 'x', 'x', 'x'};
    TEST_ASSERT(parser.parse(reinterpret_cast<const char*>(oversized),
                             sizeof(oversized)) == -1,
                "oversize rejected");

    // 两帧连排：首帧消费长度精确
    appendFrame(wire, "abc");
    const int32_t r3 = parser.parse(wire.data(), static_cast<uint32_t>(wire.size()));
    TEST_ASSERT(r3 == static_cast<int32_t>(4 + 5), "first frame length exact");
    return true;
}

// 快照路径：注册 listener/connector 后 update() 持锁快照、出锁轮询，
// 再 stop() 持锁收线——修复前此路径与 IO 线程/变更接口无锁互踩
bool test_manager_snapshot_update_stop() {
    NativeNetworkManager mgr;
    apollo::net::NetworkConfig cfg;
    TEST_ASSERT(mgr.initialize(cfg), "initialize");

    TEST_ASSERT(mgr.createListener("l1") != nullptr, "create listener");
    TEST_ASSERT(mgr.createListener("l1") == nullptr, "duplicate name rejected");
    TEST_ASSERT(mgr.createConnector("c1") != nullptr, "create connector");
    TEST_ASSERT(mgr.getListener("l1") != nullptr, "listener registered");
    TEST_ASSERT(mgr.getConnector("c1") != nullptr, "connector registered");

    mgr.update();
    mgr.update();

    TEST_ASSERT(mgr.removeConnector("c1"), "remove connector");
    TEST_ASSERT(mgr.removeConnector("c1") == false, "double remove rejected");
    mgr.removeAllListeners();
    TEST_ASSERT(mgr.getListener("l1") == nullptr, "all listeners removed");

    mgr.stop();
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
    run("test_recv_framing_big_endian", test_recv_framing_big_endian);
    run("test_recv_oversize_rejected", test_recv_oversize_rejected);
#endif
    run("test_length_prefix_parser_big_endian", test_length_prefix_parser_big_endian);
    run("test_manager_snapshot_update_stop", test_manager_snapshot_update_stop);
    std::cout << "=== Results: " << passed << "/" << total << " passed ===" << std::endl;
    return (passed == total) ? 0 : 1;
}
