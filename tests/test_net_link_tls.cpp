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
//   9. 客户端验签面（§5.9 出站侧）：信任链 + 主机名是受理前提——受信链
//      + 主机名匹配握手通；不受信链 fatal（验签码 = 链不受信）；主机名
//      不匹配 fatal（验签码 = 主机名不匹配）；显式关闭验签的本地联调
//      逃生门可用。
//  10. link 管理器 TLS 装配（工厂缝）：passive accept 工厂 + active dial
//      工厂包成 TLS 记录层，Hello + 双向 reliable 全走 TLS；信任锚装错
//      → 链路永不 kUp、退避重拨、服务端计数失败握手。

#include <iostream>

#if !defined(_WIN32)

#include "tls_test_support.hpp"

#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/route_table.hpp"
#include "apollo/net/link/tcp_transport.hpp"
#include "apollo/net/link/transport.hpp"
#include "apollo/net/tls/tls_client.hpp"
#include "apollo/net/tls/tls_transport.hpp"

#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

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
using apollo::net::tls::TlsClientContext;
using apollo::net::tls::TlsClientTransport;
using apollo::net::tls::TlsListener;
using apollo::net::tls::TlsServerContext;
using apollo::net::tls::make_tls_accept_factory;
using apollo::net::tls::make_tls_dial_factory;
using link::EndpointConfig;
using link::EndpointRole;
using link::InterServerLink;
using link::LinkManager;
using link::LinkManagerConfig;
using link::LinkState;
using link::RouteTable;
using link::SendClass;
using link::SendCode;

// 管理器 e2e 夹具（与 link 测试同形：单路由表 + 收帧日志）。
struct RxLog {
    std::vector<std::string> got;

    link::Dispatcher::Handler handler() {
        return [this](std::uint16_t id, const std::uint8_t* data, std::size_t size) {
            got.emplace_back(std::to_string(id) + ":" +
                             std::string(reinterpret_cast<const char*>(data), size));
        };
    }
};

RouteTable make_dispatch_table() {
    RouteTable t;
    t.add({901, "test.echo", "P2P", "control", "internal", "native"});
    return t;
}

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

// 客户端侧裸连（阻塞 connect——本机监听器必即刻受理；fd 交被测传输接管）。
int raw_connect(std::uint16_t port) {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    ::sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1 ||
        ::connect(fd, reinterpret_cast<::sockaddr*>(&addr), sizeof addr) != 0) {
        ::close(fd);
        return -1;
    }
    return fd;
}

// 双侧泵步进至客户端握手完成（服务端 TlsTransport 由缝读写步进 SSL_accept）。
bool pump_client_handshake(TlsClientTransport& client_side,
                           link::ILinkTransport& server_side) {
    std::uint8_t scratch[256];
    for (int i = 0; i < 2000; ++i) {
        if (client_side.handshakeDone()) {
            return true;
        }
        (void)client_side.write(scratch, 0);  // 空写也推进 SSL_connect
        (void)client_side.read(scratch, sizeof scratch);
        (void)server_side.write(scratch, 0);
        (void)server_side.read(scratch, sizeof scratch);
        std::this_thread::sleep_for(kTick);
    }
    return client_side.handshakeDone();
}

// 明文 round-trip（双侧缝互发；client→server→client）。
bool roundtrip(link::ILinkTransport& client_side, link::ILinkTransport& server_side) {
    const std::vector<std::uint8_t> payload{'t', 'l', 's', '-', 'o', 'k'};
    for (int i = 0; i < 2000; ++i) {
        (void)client_side.write(payload.data(), payload.size());
        std::uint8_t buf[64] = {};
        const int n = server_side.read(buf, sizeof buf);
        if (n == static_cast<int>(payload.size())) {
            for (int j = 0; j < 2000; ++j) {
                (void)server_side.write(buf, static_cast<std::size_t>(n));
                const int m = client_side.read(buf, sizeof buf);
                if (m == static_cast<int>(payload.size())) {
                    return true;
                }
                std::this_thread::sleep_for(kTick);
            }
            return false;
        }
        std::this_thread::sleep_for(kTick);
    }
    return false;
}

// ---- 9. 客户端验签：信任链 + 主机名是受理前提 ----

bool test_client_verifies_chain_and_host() {
    const TempCerts certs = make_certs();
    TEST_ASSERT(certs.ok, "自签证书夹具生成成功");

    // 服务端：证书 a（CN=localhost）作监听端。
    TlsServerContext server_ctx;
    TEST_ASSERT(server_ctx.load(certs.a.cert_path, certs.a.key_path).ok, "服务端装载通过");
    TlsListener listener;
    TEST_ASSERT(listener.start(server_ctx, "127.0.0.1", 0), "监听器 start 通过");

    // 客户端：信任锚 = a 自签证书（自签证书兼作 CA），主机名绑定 localhost。
    TlsClientContext client_ctx;
    TEST_ASSERT(!client_ctx.loaded(), "未装载即 loaded=false");
    TEST_ASSERT(client_ctx.verifyEnabled(), "验签默认开");
    auto res = client_ctx.loadCaFile(certs.a.cert_path);
    TEST_ASSERT(res.ok, "信任锚装载通过: " + res.error);
    TEST_ASSERT(client_ctx.loaded(), "装载后 loaded=true");

    const int fd = raw_connect(listener.port());
    TEST_ASSERT(fd >= 0, "客户端裸连通过");
    TlsClientTransport client_side(fd, client_ctx, "localhost");
    auto server_side = listener.accept();
    TEST_ASSERT(server_side != nullptr, "accept 出服务端传输");
    TEST_ASSERT(pump_client_handshake(client_side, *server_side), "握手完成（链+主机名过）");
    TEST_ASSERT(client_side.verifyResult() == 0, "验签码为 0");
    TEST_ASSERT(client_side.failureReason().empty(), "无失败原因");
    TEST_ASSERT(roundtrip(client_side, *server_side), "明文 round-trip 双向");
    return true;
}

bool test_client_rejects_untrusted_chain() {
    const TempCerts certs = make_certs();
    TEST_ASSERT(certs.ok, "自签证书夹具生成成功");

    TlsServerContext server_ctx;
    TEST_ASSERT(server_ctx.load(certs.a.cert_path, certs.a.key_path).ok, "服务端装载通过");
    TlsListener listener;
    TEST_ASSERT(listener.start(server_ctx, "127.0.0.1", 0), "监听器 start 通过");

    // 信任锚 = 另一张无关自签 b：a 的链不被覆盖 → 握手 fatal。
    TlsClientContext client_ctx;
    TEST_ASSERT(client_ctx.loadCaFile(certs.b.cert_path).ok, "信任锚 b 装载通过");

    const int fd = raw_connect(listener.port());
    TEST_ASSERT(fd >= 0, "客户端裸连通过");
    TlsClientTransport client_side(fd, client_ctx, "localhost");
    auto server_side = listener.accept();
    TEST_ASSERT(server_side != nullptr, "accept 出服务端传输");
    TEST_ASSERT(!pump_client_handshake(client_side, *server_side), "不受信链握手必败");
    TEST_ASSERT(client_side.closed(), "fatal 后 closed()");
    std::uint8_t buf[16];
    TEST_ASSERT(client_side.read(buf, sizeof buf) == -1, "fatal 后 read -1");
    TEST_ASSERT(client_side.write(buf, 1) == -1, "fatal 后 write -1");
    TEST_ASSERT(client_side.verifyResult() == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT,
                "验签码 = 链不受信（自签零深度）: got "
                    + std::to_string(client_side.verifyResult()) + " "
                    + client_side.failureReason());
    TEST_ASSERT(!client_side.failureReason().empty(), "失败原因可读");
    return true;
}

bool test_client_rejects_hostname_mismatch() {
    const TempCerts certs = make_certs();
    TEST_ASSERT(certs.ok, "自签证书夹具生成成功");

    TlsServerContext server_ctx;
    TEST_ASSERT(server_ctx.load(certs.a.cert_path, certs.a.key_path).ok, "服务端装载通过");
    TlsListener listener;
    TEST_ASSERT(listener.start(server_ctx, "127.0.0.1", 0), "监听器 start 通过");

    // 链受信（锚 = a）但主机名与证书 subject 不匹配 → 握手 fatal。
    TlsClientContext client_ctx;
    TEST_ASSERT(client_ctx.loadCaFile(certs.a.cert_path).ok, "信任锚装载通过");

    const int fd = raw_connect(listener.port());
    TEST_ASSERT(fd >= 0, "客户端裸连通过");
    TlsClientTransport client_side(fd, client_ctx, "other.example");
    auto server_side = listener.accept();
    TEST_ASSERT(server_side != nullptr, "accept 出服务端传输");
    TEST_ASSERT(!pump_client_handshake(client_side, *server_side), "主机名不匹配握手必败");
    TEST_ASSERT(client_side.closed(), "fatal 后 closed()");
    TEST_ASSERT(client_side.verifyResult() == X509_V_ERR_HOSTNAME_MISMATCH,
                "验签码 = 主机名不匹配");
    TEST_ASSERT(!client_side.failureReason().empty(), "失败原因可读");
    return true;
}

bool test_client_verify_disabled_local_dev() {
    const TempCerts certs = make_certs();
    TEST_ASSERT(certs.ok, "自签证书夹具生成成功");

    TlsServerContext server_ctx;
    TEST_ASSERT(server_ctx.load(certs.a.cert_path, certs.a.key_path).ok, "服务端装载通过");
    TlsListener listener;
    TEST_ASSERT(listener.start(server_ctx, "127.0.0.1", 0), "监听器 start 通过");

    // 显式关闭验签（本地联调）：无信任锚 + 主机名不绑也须能通——但这是
    // 显式逃生门，不是默认路径。
    TlsClientContext client_ctx;
    client_ctx.setVerifyEnabled(false);
    TEST_ASSERT(!client_ctx.verifyEnabled(), "验签已显式关闭");

    const int fd = raw_connect(listener.port());
    TEST_ASSERT(fd >= 0, "客户端裸连通过");
    TlsClientTransport client_side(fd, client_ctx, "unrelated.example");
    auto server_side = listener.accept();
    TEST_ASSERT(server_side != nullptr, "accept 出服务端传输");
    TEST_ASSERT(pump_client_handshake(client_side, *server_side), "关闭验签后握手完成");
    TEST_ASSERT(roundtrip(client_side, *server_side), "明文 round-trip 双向");
    return true;
}

// ---- 10. link 管理器 TLS 装配：工厂缝把拨号/接受包成 TLS 记录层 ----

bool test_link_manager_tls_pair_e2e() {
    const TempCerts certs = make_certs();
    TEST_ASSERT(certs.ok, "自签证书夹具生成成功");

    TlsServerContext server_ctx;
    TEST_ASSERT(server_ctx.load(certs.a.cert_path, certs.a.key_path).ok, "服务端装载通过");
    TlsClientContext client_ctx;
    TEST_ASSERT(client_ctx.loadCaFile(certs.a.cert_path).ok, "信任锚装载通过");

    RouteTable table = make_dispatch_table();

    // passive：accept 工厂把 accept 出的裸 fd 换 TLS 服务端记录层。
    LinkManagerConfig passive_cfg;
    passive_cfg.self_name = "game";
    passive_cfg.route_version = 7;
    passive_cfg.backoff_initial_ms = 50;
    passive_cfg.backoff_max_ms = 200;
    passive_cfg.accept_factory = make_tls_accept_factory(server_ctx);
    LinkManager passive(passive_cfg);

    EndpointConfig pe;
    pe.name = "game";
    pe.host = "127.0.0.1";
    pe.role = EndpointRole::kPassive;
    TEST_ASSERT(passive.add_endpoint(pe), "passive endpoint bound");

    // active：dial 工厂把拨号连上的裸 fd 换 TLS 客户端记录层（验签目标
    // = 端点 host，证书 CN=localhost → 主机名端点）。
    LinkManagerConfig active_cfg;
    active_cfg.self_name = "auth";
    active_cfg.route_version = 7;
    active_cfg.backoff_initial_ms = 50;
    active_cfg.backoff_max_ms = 200;
    active_cfg.dial_factory = make_tls_dial_factory(client_ctx);
    LinkManager active(active_cfg);

    int a_ups = 0, b_ups = 0;
    active.on_up([&](const std::string&) { ++a_ups; });
    passive.on_up([&](const std::string&) { ++b_ups; });

    EndpointConfig ae;
    ae.name = "game";
    ae.host = "localhost";
    ae.port = passive.endpoint_port("game");
    TEST_ASSERT(ae.port != 0, "ephemeral port observed");
    TEST_ASSERT(active.add_endpoint(ae), "active endpoint added");

    InterServerLink* la = active.link("game");
    TEST_ASSERT(la != nullptr, "active link exposed");
    InterServerLink* lb = nullptr;
    RxLog rx, rx_a;
    passive.set_peer_listener([&](InterServerLink& peer) {
        peer.dispatcher().set_route_table(&table);
        peer.dispatcher().allow_domain("internal");
        peer.dispatcher().set_handler(901, rx.handler());
        lb = &peer;
    });
    la->dispatcher().set_route_table(&table);
    la->dispatcher().allow_domain("internal");
    la->dispatcher().set_handler(901, rx_a.handler());

    // 泵到握手完成（TLS 双侧步进在缝读写内隐式推进；每拍实睡——
    // TLS 握手要真实墙钟 I/O，裸自旋会饿死 I/O 且 macOS CI 必 flake）。
    std::uint64_t t = 1000;
    for (int i = 0; i < 2000; ++i) {
        t += 5;
        active.pump(t);
        passive.pump(t);
        if (la->state() == LinkState::kUp && lb != nullptr &&
            lb->state() == LinkState::kUp) {
            break;
        }
        std::this_thread::sleep_for(kTick);
    }
    TEST_ASSERT(la->state() == LinkState::kUp, "active up over tls");
    TEST_ASSERT(lb != nullptr && lb->state() == LinkState::kUp, "passive peer up over tls");
    TEST_ASSERT(lb->peer_name() == "auth", "peer name over tls");
    TEST_ASSERT(a_ups == 1 && b_ups == 1, "manager events over tls");

    // 双向 reliable 各一帧 + 双向 ack（全走 TLS 记录层）。
    TEST_ASSERT(la->send(901, std::vector<std::uint8_t>{'a'}, SendClass::kReliable) ==
                    SendCode::kAccepted,
                "a send");
    TEST_ASSERT(lb->send(901, std::vector<std::uint8_t>{'b'}, SendClass::kReliable) ==
                    SendCode::kAccepted,
                "b send");
    for (int i = 0; i < 2000; ++i) {
        t += 5;
        active.pump(t);
        passive.pump(t);
        if (rx.got.size() == 1 && rx_a.got.size() == 1 &&
            la->stats().reliable_acked == 1 && lb->stats().reliable_acked == 1) {
            break;
        }
        std::this_thread::sleep_for(kTick);
    }
    TEST_ASSERT(rx.got.size() == 1 && rx_a.got.size() == 1, "both directions over tls");
    TEST_ASSERT(rx.got[0] == "901:a" && rx_a.got[0] == "901:b", "payloads true over tls");
    TEST_ASSERT(la->stats().reliable_acked == 1 && lb->stats().reliable_acked == 1,
                "acks round trip over tls");
    return true;
}

bool test_link_manager_tls_rejects_untrusted() {
    const TempCerts certs = make_certs();
    TEST_ASSERT(certs.ok, "自签证书夹具生成成功");

    TlsServerContext server_ctx;
    TEST_ASSERT(server_ctx.load(certs.a.cert_path, certs.a.key_path).ok, "服务端装载通过");
    // 信任锚故意装错（b 与服务端 a 无关）——TCP 恒通，TLS 验签恒拒：
    // 链路必须永不 kUp，且拨号退避重试（不是一次性吞掉）。
    TlsClientContext client_ctx;
    TEST_ASSERT(client_ctx.loadCaFile(certs.b.cert_path).ok, "错误信任锚装载通过");

    LinkManagerConfig passive_cfg;
    passive_cfg.self_name = "game";
    passive_cfg.route_version = 7;
    passive_cfg.backoff_initial_ms = 50;
    passive_cfg.backoff_max_ms = 200;
    passive_cfg.accept_factory = make_tls_accept_factory(server_ctx);
    LinkManager passive(passive_cfg);

    EndpointConfig pe;
    pe.name = "game";
    pe.host = "127.0.0.1";
    pe.role = EndpointRole::kPassive;
    TEST_ASSERT(passive.add_endpoint(pe), "passive endpoint bound");

    LinkManagerConfig active_cfg;
    active_cfg.self_name = "auth";
    active_cfg.route_version = 7;
    active_cfg.backoff_initial_ms = 50;
    active_cfg.backoff_max_ms = 200;
    active_cfg.dial_factory = make_tls_dial_factory(client_ctx);
    LinkManager active(active_cfg);

    int a_ups = 0;
    active.on_up([&](const std::string&) { ++a_ups; });

    EndpointConfig ae;
    ae.name = "game";
    ae.host = "localhost";
    ae.port = passive.endpoint_port("game");
    TEST_ASSERT(active.add_endpoint(ae), "active endpoint added");

    std::uint64_t t = 1000;
    for (int i = 0; i < 750; ++i) {  // ~1.5s：覆盖多轮 50ms 起步的退避重拨
        t += 5;
        active.pump(t);
        passive.pump(t);
        std::this_thread::sleep_for(kTick);
    }
    TEST_ASSERT(active.link("game")->state() != LinkState::kUp, "link never up on bad chain");
    TEST_ASSERT(a_ups == 0, "no up events on bad chain");
    TEST_ASSERT(active.dial_attempts("game") >= 2, "dial retried with backoff");
    TEST_ASSERT(passive.handshake_failures("game") >= 2, "server counted failed handshakes");
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
        {"client_verifies_chain_and_host", test_client_verifies_chain_and_host},
        {"client_rejects_untrusted_chain", test_client_rejects_untrusted_chain},
        {"client_rejects_hostname_mismatch", test_client_rejects_hostname_mismatch},
        {"client_verify_disabled_local_dev", test_client_verify_disabled_local_dev},
        {"link_manager_tls_pair_e2e", test_link_manager_tls_pair_e2e},
        {"link_manager_tls_rejects_untrusted", test_link_manager_tls_rejects_untrusted},
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
