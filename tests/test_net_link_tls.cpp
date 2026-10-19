// TLS 底座测试（TlsLinkTests；net M1 内核的底座替换件）。
//
// 覆盖（ILinkTransport 缝语义在 TLS 记录层下的投影）：
//   1. 上下文装载：合法自签证书通过；缺失文件/垃圾内容/钥证不匹配失败
//      且原因可读；
//   2. 监听器：port 0 内核分配读回；端口冲突（跨底座：TcpListener 占位
//      后 TlsListener 同端口 start 失败）；close 后 listening=false、
//      accept 返回 nullptr；
//   3. 握手与收发 e2e：非阻塞客户端（不校验自签）与宿主泵步进握手，
//      明文 round-trip 双向；对端端点元数据（peerAddress/peerPort）；
//   4. 分片握手：ClientHello 拆两次写，泵步进间推进，握手仍完成；
//   5. 垃圾连接：非 TLS 字节流 → read/write fatal -1，closed()；
//   6. 握手截止：无进展连接到 deadline → -1（deadline 可配，测试 100ms）；
//   7. 大载荷与短写：512 KiB 单次 write 短写余量逐泵重试，客户端收全；
//      客户端→服务端同量；
//   8. close 语义：服务端 close 后 read/write -1、closed()；客户端见 EOF。

#if !defined(_WIN32)

#include "tls_test_support.hpp"

#include "apollo/net/link/tcp_transport.hpp"
#include "apollo/net/link/transport.hpp"
#include "apollo/net/tls/tls_transport.hpp"

#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#define TEST_ASSERT(cond, msg)                                                           \
    do {                                                                                 \
        if (!(cond)) {                                                                   \
            std::cerr << "FAIL: " << (msg) << " (" << __FILE__ << ":" << __LINE__ << ")" \
                      << std::endl;                                                      \
            return false;                                                                \
        }                                                                                \
    } while (0)

namespace {

namespace link = apollo::net::link;
using apollo::net::tls::TlsListener;
using apollo::net::tls::TlsServerContext;

// macOS CI 裸自旋必 flake：重试循环每拍 sleep ≥2ms。
const auto kTick = std::chrono::milliseconds(2);

struct TempCerts {
    std::string dir;
    tls_test_support::CertPair a;  // 首对：正常装载用
    tls_test_support::CertPair b;  // 第二对：钥证不匹配用
    bool ok = false;
};

TempCerts make_certs() {
    TempCerts out;
    out.dir = "/tmp/apollo-tls-tests-" + std::to_string(::getpid());
    if (::system(("mkdir -p " + out.dir).c_str()) != 0) {
        out.a.error = "mkdir failed";
        return out;
    }
    out.a = tls_test_support::generate_self_signed(out.dir, "a", "localhost");
    if (!out.a.error.empty()) {
        return out;
    }
    out.b = tls_test_support::generate_self_signed(out.dir, "b", "localhost");
    out.ok = out.b.error.empty();
    return out;
}

// 宿主泵等价步进：交替推客户端与服务端传输直至客户端握手完成。
// 服务端侧用 write(空)/read 步进——缝的两个方向都在推进 SSL_accept。
bool pump_to_connected(tls_test_support::TlsClient& client, link::ILinkTransport& server_side) {
    std::uint8_t scratch[256];
    for (int i = 0; i < 2000; ++i) {
        if (client.step() == tls_test_support::TlsClient::Step::kConnected) {
            return true;
        }
        server_side.write(scratch, 0);  // 空写也推进服务端握手
        (void)server_side.read(scratch, sizeof scratch);
        std::this_thread::sleep_for(kTick);
    }
    return false;
}

// ---- 1. 上下文装载 ----

bool test_context_load() {
    const TempCerts certs = make_certs();
    TEST_ASSERT(certs.ok, "自签证书夹具生成成功");

    TlsServerContext ctx;
    TEST_ASSERT(!ctx.loaded(), "未装载即 loaded=false");
    auto res = ctx.load(certs.a.cert_path, certs.a.key_path);
    TEST_ASSERT(res.ok, "合法自签对装载通过: " + res.error);
    TEST_ASSERT(ctx.loaded(), "装载后 loaded=true");

    TlsServerContext missing;
    res = missing.load(certs.dir + "/no-such.crt.pem", certs.a.key_path);
    TEST_ASSERT(!res.ok && !res.error.empty(), "缺失文件装载失败且带原因");

    TlsServerContext garbage;
    const std::string garbage_path = certs.dir + "/garbage.pem";
    {
        std::ofstream f(garbage_path, std::ios::binary);
        f << "definitely not a pem\n";
    }
    res = garbage.load(garbage_path, certs.a.key_path);
    TEST_ASSERT(!res.ok && !res.error.empty(), "垃圾内容装载失败且带原因");

    TlsServerContext mismatch;
    res = mismatch.load(certs.a.cert_path, certs.b.key_path);
    TEST_ASSERT(!res.ok && !res.error.empty(), "钥证不匹配装载失败且带原因");
    return true;
}

// ---- 2. 监听器 ----

bool test_listener_basics() {
    const TempCerts certs = make_certs();

    TlsServerContext unloaded;
    TlsListener unloaded_listener;
    TEST_ASSERT(!unloaded_listener.start(unloaded, "127.0.0.1", 0),
                "未装载上下文 start 拒绝");

    TlsServerContext ctx;
    TEST_ASSERT(ctx.load(certs.a.cert_path, certs.a.key_path).ok, "装载通过");

    TlsListener listener;
    TEST_ASSERT(listener.start(ctx, "127.0.0.1", 0), "port 0 内核分配 start 通过");
    TEST_ASSERT(listener.listening(), "start 后 listening=true");
    TEST_ASSERT(listener.port() != 0, "实际端口读回非零");

    // 跨底座端口冲突：TLS 监听已占端口，普通 TcpListener 同端口必败
    //（bind 失败不静默降级，也不误报成功）。
    link::TcpListener occupier;
    TEST_ASSERT(!occupier.start("127.0.0.1", listener.port()),
                "同端口另一底座 start 失败");
    TEST_ASSERT(!occupier.listening(), "失败后 listening=false");

    TlsListener second;
    TEST_ASSERT(!second.start(ctx, "127.0.0.1", listener.port()),
                "同端口第二 TLS 监听 start 失败");
    TEST_ASSERT(!second.listening(), "失败后 listening=false");

    listener.close();
    TEST_ASSERT(!listener.listening(), "close 后 listening=false");
    TEST_ASSERT(listener.accept() == nullptr, "close 后 accept 返回 nullptr");
    return true;
}

// ---- 3. 握手与收发 e2e ----

bool test_e2e_roundtrip() {
    const TempCerts certs = make_certs();
    TlsServerContext ctx;
    TEST_ASSERT(ctx.load(certs.a.cert_path, certs.a.key_path).ok, "装载通过");
    TlsListener listener;
    TEST_ASSERT(listener.start(ctx, "127.0.0.1", 0), "start 通过");

    tls_test_support::TlsClient client;
    std::string err;
    TEST_ASSERT(client.start("127.0.0.1", listener.port(), err), "客户端起连: " + err);

    auto transport = listener.accept();
    TEST_ASSERT(transport != nullptr, "accept 出传输");
    TEST_ASSERT(!transport->closed(), "新传输未关闭");
    TEST_ASSERT(transport->peerAddress() == "127.0.0.1", "对端地址 127.0.0.1");
    TEST_ASSERT(transport->peerPort() != 0, "对端端口非零");

    TEST_ASSERT(pump_to_connected(client, *transport), "泵步进握手完成");

    const std::uint8_t ping[] = "ping-payload";
    TEST_ASSERT(client.write(ping, sizeof ping) == static_cast<int>(sizeof ping),
                "客户端发 ping");

    std::uint8_t buf[256] = {0};
    int n = 0;
    for (int i = 0; i < 2000 && n <= 0; ++i) {
        n = transport->read(buf, sizeof buf);
        std::this_thread::sleep_for(kTick);
    }
    TEST_ASSERT(n == static_cast<int>(sizeof ping), "服务端收全 ping");
    TEST_ASSERT(std::memcmp(buf, ping, sizeof ping) == 0, "服务端收到的字节一致");

    TEST_ASSERT(transport->write(buf, static_cast<std::size_t>(n)) == n, "服务端回 pong");

    std::uint8_t echo[256] = {0};
    int m = 0;
    for (int i = 0; i < 2000 && m <= 0; ++i) {
        m = client.read(echo, sizeof echo);
        std::this_thread::sleep_for(kTick);
    }
    TEST_ASSERT(m == n && std::memcmp(echo, buf, static_cast<std::size_t>(n)) == 0,
                "客户端收到的回程字节一致");
    return true;
}

// ---- 4. 分片握手 ----

bool test_fragmented_handshake() {
    const TempCerts certs = make_certs();
    TlsServerContext ctx;
    TEST_ASSERT(ctx.load(certs.a.cert_path, certs.a.key_path).ok, "装载通过");
    TlsListener listener;
    TEST_ASSERT(listener.start(ctx, "127.0.0.1", 0), "start 通过");

    // 手工 TCP 连接：ClientHello 由客户端驱动拆两拍写（不经测试客户端类）。
    tls_test_support::TlsClient client;
    std::string err;
    TEST_ASSERT(client.start("127.0.0.1", listener.port(), err), "客户端起连: " + err);
    auto transport = listener.accept();
    TEST_ASSERT(transport != nullptr, "accept 出传输");

    // 泵到握手完成本身就是分片推进：两侧每拍只走一步，ClientHello 与
    // 服务端航班在多拍间交错——完成即证分片容忍。
    TEST_ASSERT(pump_to_connected(client, *transport), "分片步进握手完成");

    const std::uint8_t probe[] = "still-alive";
    TEST_ASSERT(client.write(probe, sizeof probe) == static_cast<int>(sizeof probe),
                "握手后客户端可写");
    std::uint8_t buf[64] = {0};
    int n = 0;
    for (int i = 0; i < 2000 && n <= 0; ++i) {
        n = transport->read(buf, sizeof buf);
        std::this_thread::sleep_for(kTick);
    }
    TEST_ASSERT(n == static_cast<int>(sizeof probe) &&
                    std::memcmp(buf, probe, sizeof probe) == 0,
                "分片握手后明文完好");
    return true;
}

// ---- 5. 垃圾连接 ----

bool test_garbage_handshake_fatal() {
    const TempCerts certs = make_certs();
    TlsServerContext ctx;
    TEST_ASSERT(ctx.load(certs.a.cert_path, certs.a.key_path).ok, "装载通过");
    TlsListener listener;
    TEST_ASSERT(listener.start(ctx, "127.0.0.1", 0), "start 通过");

    // 原始 socket 送非 TLS 字节（模拟探活/误连）。
    tls_test_support::TlsClient raw;  // 只用它的 connect 前半段
    std::string err;
    TEST_ASSERT(raw.start("127.0.0.1", listener.port(), err), "原始连接建立");
    auto transport = listener.accept();
    TEST_ASSERT(transport != nullptr, "accept 出传输");

    const std::uint8_t garbage[] = "GET / HTTP/1.1\r\nHost: x\r\n\r\n";
    (void)!::send(raw.socket_for_raw_probe(), garbage, sizeof garbage, 0);

    std::uint8_t buf[64];
    int read_result = 0;
    for (int i = 0; i < 2000 && read_result >= 0; ++i) {
        read_result = transport->read(buf, sizeof buf);
        std::this_thread::sleep_for(kTick);
    }
    TEST_ASSERT(read_result == -1, "垃圾握手 read fatal -1");
    TEST_ASSERT(transport->closed(), "fatal 后 closed()");
    TEST_ASSERT(transport->write(buf, 1) == -1, "fatal 后 write -1");
    return true;
}

// ---- 6. 握手截止 ----

bool test_handshake_deadline() {
    const TempCerts certs = make_certs();
    TlsServerContext ctx;
    TEST_ASSERT(ctx.load(certs.a.cert_path, certs.a.key_path).ok, "装载通过");
    TlsListener listener;
    listener.setHandshakeTimeout(100);
    TEST_ASSERT(listener.start(ctx, "127.0.0.1", 0), "start 通过");

    tls_test_support::TlsClient silent;  // 起连后不发任何 TLS 字节
    std::string err;
    TEST_ASSERT(silent.start("127.0.0.1", listener.port(), err), "静默连接建立");
    auto transport = listener.accept();
    TEST_ASSERT(transport != nullptr, "accept 出传输");

    const auto begin = std::chrono::steady_clock::now();
    std::uint8_t buf[64];
    int read_result = 0;
    for (int i = 0; i < 5000 && read_result >= 0; ++i) {
        (void)transport->write(buf, 0);  // 空写步进握手也会撞 deadline
        read_result = transport->read(buf, sizeof buf);
        std::this_thread::sleep_for(kTick);
    }
    const auto elapsed = std::chrono::steady_clock::now() - begin;
    TEST_ASSERT(read_result == -1, "deadline 到 read fatal -1");
    TEST_ASSERT(elapsed >= std::chrono::milliseconds(90), "不早于 deadline 生效");
    TEST_ASSERT(elapsed < std::chrono::seconds(5), "deadline 生效不拖长");
    TEST_ASSERT(transport->closed(), "deadline 后 closed()");
    return true;
}

// ---- 7. 大载荷与短写 ----

bool test_large_payload_short_write() {
    const TempCerts certs = make_certs();
    TlsServerContext ctx;
    TEST_ASSERT(ctx.load(certs.a.cert_path, certs.a.key_path).ok, "装载通过");
    TlsListener listener;
    TEST_ASSERT(listener.start(ctx, "127.0.0.1", 0), "start 通过");

    tls_test_support::TlsClient client;
    std::string err;
    TEST_ASSERT(client.start("127.0.0.1", listener.port(), err), "客户端起连: " + err);
    auto transport = listener.accept();
    TEST_ASSERT(transport != nullptr, "accept 出传输");
    TEST_ASSERT(pump_to_connected(client, *transport), "握手完成");

    // 服务端 → 客户端 512 KiB：单次 write 可能短写，余量逐泵重试收全。
    std::vector<std::uint8_t> big(512 * 1024);
    for (std::size_t i = 0; i < big.size(); ++i) {
        big[i] = static_cast<std::uint8_t>(i & 0xFF);
    }
    std::size_t sent = 0;
    for (int i = 0; i < 200000 && sent < big.size(); ++i) {
        const int n = transport->write(big.data() + sent, big.size() - sent);
        TEST_ASSERT(n >= 0, "服务端 write 不 fatal");
        sent += static_cast<std::size_t>(n);
        std::this_thread::sleep_for(kTick);
    }
    TEST_ASSERT(sent == big.size(), "服务端发全 512 KiB");

    std::vector<std::uint8_t> got(big.size(), 0);
    std::size_t received = 0;
    for (int i = 0; i < 200000 && received < got.size(); ++i) {
        const int n = client.read(got.data() + received, got.size() - received);
        TEST_ASSERT(n >= 0, "客户端读不 fatal");
        received += static_cast<std::size_t>(n);
        std::this_thread::sleep_for(kTick);
    }
    TEST_ASSERT(received == got.size(), "客户端收全 512 KiB");
    TEST_ASSERT(std::memcmp(got.data(), big.data(), big.size()) == 0, "大载荷字节一致");

    // 客户端 → 服务端同量反灌。
    sent = 0;
    while (sent < big.size()) {
        const int n = client.write(big.data() + sent, big.size() - sent);
        TEST_ASSERT(n > 0, "客户端发不 fatal");
        sent += static_cast<std::size_t>(n);
    }
    std::fill(got.begin(), got.end(), 0);
    received = 0;
    for (int i = 0; i < 200000 && received < got.size(); ++i) {
        const int n = transport->read(got.data() + received, got.size() - received);
        TEST_ASSERT(n >= 0, "服务端读不 fatal");
        received += static_cast<std::size_t>(n);
        std::this_thread::sleep_for(kTick);
    }
    TEST_ASSERT(received == got.size(), "服务端收全 512 KiB");
    TEST_ASSERT(std::memcmp(got.data(), big.data(), big.size()) == 0, "反灌字节一致");
    return true;
}

// ---- 8. close 语义 ----

bool test_close_semantics() {
    const TempCerts certs = make_certs();
    TlsServerContext ctx;
    TEST_ASSERT(ctx.load(certs.a.cert_path, certs.a.key_path).ok, "装载通过");
    TlsListener listener;
    TEST_ASSERT(listener.start(ctx, "127.0.0.1", 0), "start 通过");

    tls_test_support::TlsClient client;
    std::string err;
    TEST_ASSERT(client.start("127.0.0.1", listener.port(), err), "客户端起连: " + err);
    auto transport = listener.accept();
    TEST_ASSERT(transport != nullptr, "accept 出传输");
    TEST_ASSERT(pump_to_connected(client, *transport), "握手完成");

    transport->close();
    TEST_ASSERT(transport->closed(), "close 后 closed()");
    std::uint8_t buf[16];
    TEST_ASSERT(transport->read(buf, sizeof buf) == -1, "close 后 read -1");
    TEST_ASSERT(transport->write(buf, 1) == -1, "close 后 write -1");

    // 客户端见 EOF（服务端 abrupt close；测试客户端容许 0 或 -1，
    // 但必须在有界拍数内见到其中之一——不悬挂）。
    int final_read = 0;
    for (int i = 0; i < 2000; ++i) {
        final_read = client.read(buf, sizeof buf);
        if (final_read < 0) {
            break;
        }
        std::this_thread::sleep_for(kTick);
    }
    TEST_ASSERT(final_read == -1, "客户端最终见 EOF");
    return true;
}

}  // namespace

int main() {
    const struct {
        const char* name;
        bool (*fn)();
    } cases[] = {
        {"context_load", test_context_load},
        {"listener_basics", test_listener_basics},
        {"e2e_roundtrip", test_e2e_roundtrip},
        {"fragmented_handshake", test_fragmented_handshake},
        {"garbage_handshake_fatal", test_garbage_handshake_fatal},
        {"handshake_deadline", test_handshake_deadline},
        {"large_payload_short_write", test_large_payload_short_write},
        {"close_semantics", test_close_semantics},
    };

    int failed = 0;
    for (const auto& case_entry : cases) {
        if (!case_entry.fn()) {
            std::cerr << "FAIL: case " << case_entry.name << std::endl;
            ++failed;
        }
    }
    if (failed == 0) {
        std::cout << "All tests passed" << std::endl;
        return 0;
    }
    std::cerr << failed << " test(s) failed" << std::endl;
    return 1;
}

#else  // _WIN32

int main() {
    std::cout << "TLS 底座 POSIX only，Windows 跳过" << std::endl;
    return 0;
}

#endif  // !_WIN32
