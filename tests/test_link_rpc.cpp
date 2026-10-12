// Link RPC 关联原语测试（LinkRpcTests，P4-4 批⓪）。
//
// 覆盖（rpc_channel.hpp 纪律投影）：
//   1. rpc_roundtrip：TCP 对端全链路 call_async → poll（body 保真 +
//      corr 单调 + 双侧 stats）；
//   2. rpc_concurrent：两笔在途请求交叉应答不串号（corr 关联隔离）；
//   3. rpc_wait_then_late：无受理面 wait 超时摘表 → 事后补帧 = 迟到弃
//      （late_replies 显形）；
//   4. rpc_not_up：链路非 kUp 发起 = 立即失败（0 哨，不占表）；
//   5. rpc_bad_envelope：请求信封字段级非法 = decode_failures，不产响应。
//
// 全部单线程（测试线程泵两端）+ call_async/poll 非阻塞走——wait 仅
// 超时路径用（无响应可等，无死锁面）。

#include "apollo/net/link/interserver_link.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/link/route_table.hpp"
#include "apollo/net/link/tcp_transport.hpp"
#include "apollo/net/linkrpc/rpc_channel.hpp"
#include "apollo/net/wire/contract_wire.hpp"
#include "apollo_contract.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifndef APOLLO_CONTRACT_ROUTE_JSON
#define APOLLO_CONTRACT_ROUTE_JSON ""
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

namespace link = apollo::net::link;
namespace wire = apollo::net::wire;
using apollo::net::linkrpc::RpcClient;
using apollo::net::linkrpc::RpcServer;

constexpr std::uint16_t id_of(apollo::contract::gen::MsgId id) {
    return static_cast<std::uint16_t>(id);
}

bool load_table(link::RouteTable& table) {
    std::ifstream in(APOLLO_CONTRACT_ROUTE_JSON, std::ios::binary);
    if (!in) {
        std::cerr << "route json not found" << std::endl;
        return false;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return link::load_route_table_json(buffer.str(), table);
}

#if !defined(_WIN32)

// 全链路对端：listener + connector → InterServerLink 握手至 kUp（同
// test_net_link 的 tcp pair 模式；轮询每拍 2ms 让步——macOS CI 口径）。
struct LinkPair {
    link::RouteTable table;
    link::InterServerLink a{link::LinkConfig{"auth", 0}};
    link::InterServerLink b{link::LinkConfig{"game", 0}};

    bool bring_up() {
        if (!load_table(table)) {
            return false;
        }
        // 握手 route_version 两端同缺省（比对只论相等）；分发路由表 +
        // internal 域白名单照实装配（940/941 过闸靠它）。
        for (link::InterServerLink* l : {&a, &b}) {
            l->dispatcher().set_route_table(&table);
            l->dispatcher().allow_domain("internal");
        }

        link::TcpListener listener;
        if (!listener.start("127.0.0.1", 0)) {
            return false;
        }
        link::TcpConnector connector;
        if (!connector.start("127.0.0.1", listener.port(), 3000, 1000)) {
            return false;
        }
        for (int i = 0; i < 200 && a.state() == link::LinkState::kIdle; ++i) {
            const std::uint64_t t = 1000 + static_cast<std::uint64_t>(i) * 5;
            connector.pump(t);
            if (auto t_a = connector.take_transport()) {
                a.attach(std::move(t_a), t);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        for (int i = 0; i < 200 && b.state() == link::LinkState::kIdle; ++i) {
            const std::uint64_t t = 1500 + static_cast<std::uint64_t>(i) * 5;
            if (auto t_b = listener.accept()) {
                b.attach(std::move(t_b), t);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        for (int i = 0;
             i < 200 && (a.state() != link::LinkState::kUp || b.state() != link::LinkState::kUp);
             ++i) {
            const std::uint64_t t = 2000 + static_cast<std::uint64_t>(i) * 5;
            a.pump(t);
            b.pump(t);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return a.state() == link::LinkState::kUp && b.state() == link::LinkState::kUp;
    }

    // 交替泵一拍（业务帧面：flush 出队 + 读入分发）。
    void pump_once(std::uint64_t t) {
        a.pump(t);
        b.pump(t);
    }
};

std::vector<std::uint8_t> to_bytes(const std::string& s) {
    return std::vector<std::uint8_t>(s.begin(), s.end());
}

std::string to_string(const std::vector<std::uint8_t>& v) {
    return std::string(v.begin(), v.end());
}

bool test_rpc_roundtrip() {
    LinkPair pair;
    TEST_ASSERT(pair.bring_up(), "pair up");

    // 受理面：body 前缀回显（响应体 ≠ 请求体原样，保真可判）。
    RpcServer server;
    server.install(pair.b, [](const std::uint8_t* data, std::size_t size) {
        std::vector<std::uint8_t> out{'r', ':'};
        out.insert(out.end(), data, data + size);
        return out;
    });
    RpcClient client(pair.a);

    const auto req = to_bytes("ping-1");
    const std::uint64_t corr1 = client.call_async(req);
    TEST_ASSERT(corr1 != 0, "first call accepted");
    TEST_ASSERT(client.poll(corr1) == std::nullopt, "no response before pump");

    std::optional<std::vector<std::uint8_t>> resp1;
    for (int i = 0; i < 300 && !resp1; ++i) {
        pair.pump_once(3000 + static_cast<std::uint64_t>(i) * 5);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        resp1 = client.poll(corr1);
    }
    TEST_ASSERT(resp1.has_value(), "response arrived");
    TEST_ASSERT(client.poll(corr1) == std::nullopt, "poll consumes entry");

    // 保真 + corr 单调 + stats。
    const std::uint64_t corr2 = client.call_async(to_bytes("ping-2"));
    TEST_ASSERT(corr2 == corr1 + 1, "corr monotonic");
    std::optional<std::vector<std::uint8_t>> resp;
    for (int i = 0; i < 300 && !resp; ++i) {
        pair.pump_once(6000 + static_cast<std::uint64_t>(i) * 5);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        resp = client.poll(corr2);
    }
    TEST_ASSERT(resp.has_value(), "second response arrived");
    TEST_ASSERT(to_string(*resp) == "r:ping-2", "body roundtrip verbatim");

    TEST_ASSERT(server.stats().requests == 2, "server served two");
    TEST_ASSERT(server.stats().decode_failures == 0, "no decode failures");
    TEST_ASSERT(client.stats().responses == 2, "client got two");
    TEST_ASSERT(client.stats().late_replies == 0, "no late replies");
    return true;
}

bool test_rpc_concurrent() {
    LinkPair pair;
    TEST_ASSERT(pair.bring_up(), "pair up");

    RpcServer server;
    server.install(pair.b, [](const std::uint8_t* data, std::size_t size) {
        std::vector<std::uint8_t> out{'#'};
        out.insert(out.end(), data, data + size);
        return out;
    });
    RpcClient client(pair.a);

    const std::uint64_t c1 = client.call_async(to_bytes("one"));
    const std::uint64_t c2 = client.call_async(to_bytes("two"));
    TEST_ASSERT(c1 != 0 && c2 != 0 && c1 != c2, "two outstanding corrs");

    std::optional<std::vector<std::uint8_t>> r1;
    std::optional<std::vector<std::uint8_t>> r2;
    for (int i = 0; i < 300 && (!r1 || !r2); ++i) {
        pair.pump_once(9000 + static_cast<std::uint64_t>(i) * 5);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        if (!r1) r1 = client.poll(c1);
        if (!r2) r2 = client.poll(c2);
    }
    TEST_ASSERT(r1.has_value() && r2.has_value(), "both resolved");
    TEST_ASSERT(to_string(*r1) == "#one" && to_string(*r2) == "#two", "no cross-correlation");
    return true;
}

bool test_rpc_wait_then_late() {
    LinkPair pair;
    TEST_ASSERT(pair.bring_up(), "pair up");

    // 不挂受理面：请求被 kNoHandler 拒，响应永不到 → wait 超时摘表。
    RpcClient client(pair.a);
    const std::uint64_t corr = client.call_async(to_bytes("nobody-home"));
    TEST_ASSERT(corr != 0, "call accepted while up");
    TEST_ASSERT(client.wait(corr, std::chrono::milliseconds(80)) == std::nullopt,
                "wait times out");
    TEST_ASSERT(client.poll(corr) == std::nullopt, "entry removed after timeout");

    // 事后补帧（无等待者）：迟到弃 + 计数显形。
    std::vector<wire::FieldValue> fields(2);
    fields[0].integer = static_cast<std::int64_t>(corr);
    fields[1].bytes = to_bytes("too-late");
    std::vector<std::uint8_t> payload;
    TEST_ASSERT(wire::encode_payload(id_of(apollo::contract::gen::MsgId::rpc_response), fields,
                                     payload),
                "fabricate response");
    TEST_ASSERT(pair.b.send(id_of(apollo::contract::gen::MsgId::rpc_response), payload,
                            link::SendClass::kReliable) == link::SendCode::kAccepted,
                "late frame accepted");
    bool seen = false;
    for (int i = 0; i < 200 && !seen; ++i) {
        pair.pump_once(12000 + static_cast<std::uint64_t>(i) * 5);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        seen = client.stats().late_replies > 0;
    }
    TEST_ASSERT(seen, "late reply counted");
    TEST_ASSERT(client.stats().responses == 0, "no entry ever satisfied");
    return true;
}

bool test_rpc_not_up() {
    LinkPair pair;
    TEST_ASSERT(pair.bring_up(), "pair up");

    // 新链路未挂传输（kIdle）：发起 = 立即失败（0 哨），不占表。
    link::InterServerLink idle(link::LinkConfig{"ghost", 0});
    RpcClient client(idle);
    TEST_ASSERT(client.call_async(to_bytes("nope")) == 0, "idle link fails fast");
    TEST_ASSERT(client.call(to_bytes("nope"), std::chrono::milliseconds(50)) == std::nullopt,
                "call wrapper fails fast");
    return true;
}

bool test_rpc_bad_envelope() {
    LinkPair pair;
    TEST_ASSERT(pair.bring_up(), "pair up");

    RpcServer server;
    server.install(pair.b, [](const std::uint8_t*, std::size_t) {
        return std::vector<std::uint8_t>{'x'};
    });
    RpcClient client(pair.a);

    // 请求信封字段级非法（2 字节喂 int64+bytes 布局）。
    TEST_ASSERT(pair.a.send(id_of(apollo::contract::gen::MsgId::rpc_request), to_bytes("\x01\x02"),
                            link::SendClass::kReliable) == link::SendCode::kAccepted,
                "garbage frame accepted by link");
    bool seen = false;
    for (int i = 0; i < 200 && !seen; ++i) {
        pair.pump_once(15000 + static_cast<std::uint64_t>(i) * 5);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        seen = server.stats().decode_failures > 0;
    }
    TEST_ASSERT(seen, "decode failure counted");
    TEST_ASSERT(server.stats().requests == 0, "no request served");
    TEST_ASSERT(client.stats().responses == 0, "no response produced");
    return true;
}

#else // _WIN32

// link TCP 底座不进 Windows（POSIX only）：全用例踩 TcpListener/Connector，
// 用例本体空转（test_net_link 同门法）。
bool test_rpc_roundtrip() {
    std::cout << "SKIP (windows: link tcp base posix only)" << std::endl;
    return true;
}
bool test_rpc_concurrent() {
    std::cout << "SKIP (windows: link tcp base posix only)" << std::endl;
    return true;
}
bool test_rpc_wait_then_late() {
    std::cout << "SKIP (windows: link tcp base posix only)" << std::endl;
    return true;
}
bool test_rpc_not_up() {
    std::cout << "SKIP (windows: link tcp base posix only)" << std::endl;
    return true;
}
bool test_rpc_bad_envelope() {
    std::cout << "SKIP (windows: link tcp base posix only)" << std::endl;
    return true;
}

#endif // !_WIN32

} // namespace

int main() {
    const struct {
        const char* name;
        bool (*fn)();
    } cases[] = {
        {"rpc_roundtrip", test_rpc_roundtrip},
        {"rpc_concurrent", test_rpc_concurrent},
        {"rpc_wait_then_late", test_rpc_wait_then_late},
        {"rpc_not_up", test_rpc_not_up},
        {"rpc_bad_envelope", test_rpc_bad_envelope},
    };
    int failed = 0;
    for (const auto& c : cases) {
        if (!c.fn()) {
            std::cerr << "CASE FAILED: " << c.name << std::endl;
            ++failed;
        }
    }
    if (failed == 0) {
        std::cout << "LinkRpcTests: all " << (sizeof(cases) / sizeof(cases[0]))
                  << " cases passed" << std::endl;
        return 0;
    }
    std::cout << "LinkRpcTests: " << failed << " case(s) failed" << std::endl;
    return 1;
}
