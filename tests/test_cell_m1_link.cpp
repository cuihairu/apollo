// Cell M1 接收端批测试（M1 上层接线批 2b-ii）：
// 契约意图族 924-928 真 TCP 上行（客户端 = 裸 LinkManager active 端点）→
// 契约 wire 解包 → legacy 帧镜像 → 受理队列 → tick 边界执行 → 延时应答
// 929（request_id 关联，code 0 = 已消费）；拒收面（字段级非法静默计数、
// 域越权/未知 id 不致命不断连）与顶格拒收即时回执 2101。
// 无 GTest，plain-assert + main 汇总。
#include "cell/cell_server.hpp"
#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/linktrace/envelope.hpp"
#include "apollo/net/wire/contract_wire.hpp"
#include "apollo_contract.h"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

namespace {

int g_failures = 0;

#define TEST_ASSERT(cond, msg)                                                     \
    do {                                                                           \
        if (!(cond)) {                                                             \
            std::cout << "FAIL: " << (msg) << " (" << #cond << ") at "             \
                      << __LINE__ << std::endl;                                    \
            ++g_failures;                                                          \
            return false;                                                          \
        }                                                                          \
    } while (0)

namespace link = apollo::net::link;
namespace trace = apollo::net::trace;
namespace wire = apollo::net::wire;
namespace protocol = apollo::protocol;

using cell::CellConfig;
using cell::CellServer;
using cell::IntentEnvelope;
using apollo::contract::gen::MsgId;

constexpr std::uint16_t id_of(MsgId id) {
    return static_cast<std::uint16_t>(id);
}
constexpr std::uint16_t kAckId = id_of(MsgId::cell_intent_ack);

const char* kRouteJson = APOLLO_CELL_CONTRACT_ROUTE_JSON;

std::uint64_t now_ms() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

// 929 回执日志（客户端 dispatcher handler 捕获；测试串行，指针注入足够）。
struct AckLog {
    std::mutex mutex;
    std::vector<std::tuple<std::uint64_t, std::int32_t, std::string>> acks;

    void add(std::uint64_t request_id, std::int32_t code, std::string message) {
        std::lock_guard<std::mutex> lock(mutex);
        acks.emplace_back(request_id, code, std::move(message));
    }
    std::size_t size() {
        std::lock_guard<std::mutex> lock(mutex);
        return acks.size();
    }
    bool has(std::uint64_t request_id, std::int32_t code) {
        std::lock_guard<std::mutex> lock(mutex);
        for (const auto& a : acks) {
            if (std::get<0>(a) == request_id && std::get<1>(a) == code) {
                return true;
            }
        }
        return false;
    }
    bool has_code(std::int32_t code) {
        std::lock_guard<std::mutex> lock(mutex);
        for (const auto& a : acks) {
            if (std::get<1>(a) == code) {
                return true;
            }
        }
        return false;
    }
    int count_code(std::int32_t code) {
        std::lock_guard<std::mutex> lock(mutex);
        int n = 0;
        for (const auto& a : acks) {
            if (std::get<1>(a) == code) {
                ++n;
            }
        }
        return n;
    }
};

AckLog* g_acks = nullptr;

// 客户端端（网关投影）：LinkManager active 端点拨入 cell M1 端口，同一路由
// 表（同 schema_hash → 握手 route_version 一致）。
struct ClientEnd {
    std::unique_ptr<link::RouteTable> table;
    std::unique_ptr<link::LinkManager> manager;

    bool start(std::uint16_t port) {
        table = std::make_unique<link::RouteTable>();
        std::ifstream in(kRouteJson, std::ios::binary);
        if (!in) {
            return false;
        }
        std::ostringstream buffer;
        buffer << in.rdbuf();
        if (!link::load_route_table_json(buffer.str(), *table)) {
            return false;
        }
        link::LinkManagerConfig mc;
        mc.self_name = "gateway";
        mc.route_version = link::route_version_from_hash(table->schema_hash());
        manager = std::make_unique<link::LinkManager>(mc);

        link::EndpointConfig ep;
        ep.name = "cell";
        ep.host = "127.0.0.1";
        ep.port = port;
        ep.role = link::EndpointRole::kActive;
        if (!manager->add_endpoint(ep)) {
            return false;
        }

        auto& dispatcher = manager->link("cell")->dispatcher();
        dispatcher.set_route_table(table.get());
        dispatcher.allow_domain("internal");
        dispatcher.set_handler(kAckId, [](std::uint16_t, const std::uint8_t* data,
                                          std::size_t size) {
            std::vector<wire::FieldValue> fields;
            if (wire::decode_payload(kAckId, data, size, fields) && fields.size() == 3
                && g_acks != nullptr) {
                g_acks->add(static_cast<std::uint64_t>(fields[0].integer),
                            static_cast<std::int32_t>(fields[1].integer), fields[2].text);
            }
        });
        return true;
    }

    void pump() { manager->pump(now_ms()); }

    link::InterServerLink* cell_link() { return manager->link("cell"); }
};

// 客户端泵等待（服务端由 CellServer gameThread 自泵；客户端由本测试线程泵）。
bool pump_until(ClientEnd& client, const std::function<bool()>& cond, int timeout_ms) {
    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        client.pump();
        if (cond()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    client.pump();
    return cond();
}

bool send_intent(ClientEnd& client, std::uint16_t id, std::vector<wire::FieldValue> fields) {
    std::vector<std::uint8_t> payload;
    if (!wire::encode_payload(id, fields, payload)) {
        return false;
    }
    // InterServerLink::send 自封 u16 id 信封——传裸载荷，勿再包一层
    return client.cell_link()->send(id, payload.data(), payload.size(),
                                    link::SendClass::kReliable)
        == link::SendCode::kAccepted;
}

// 932 internal_envelope 上行（trace 子集载体，P3-3 语义层批）：pack 产物 =
// 字段载荷（u16 信封由 send 自封）。
bool send_wrapped_intent(ClientEnd& client, const trace::EnvelopeFields& env) {
    std::vector<std::uint8_t> packed;
    if (!trace::pack_envelope(env, packed)) {
        return false;
    }
    return client.cell_link()->send(id_of(MsgId::internal_envelope), packed.data(),
                                    packed.size(), link::SendClass::kReliable)
        == link::SendCode::kAccepted;
}

std::vector<wire::FieldValue> base_fields(std::uint64_t request_id, std::uint64_t session_id) {
    std::vector<wire::FieldValue> f(2);
    f[0].integer = static_cast<std::int64_t>(request_id);
    f[1].integer = static_cast<std::int64_t>(session_id);
    return f;
}

wire::FieldValue i64(std::int64_t v) {
    wire::FieldValue f;
    f.integer = v;
    return f;
}

wire::FieldValue f32(float v) {
    wire::FieldValue f;
    f.real = v;
    return f;
}

// 服务端：tick 速率按需；m1Port 0 = 内核分配（listenPort 查实占）。
struct ServerEnd {
    CellConfig config;
    std::unique_ptr<CellServer> server;

    bool start(int tick_rate_ms) {
        config.tickRateMs = tick_rate_ms;
        config.m1Port = 0;
        config.m1RouteJson = kRouteJson;
        try {
            server = std::make_unique<CellServer>(config);
            server->start();
        } catch (const std::exception& e) {
            std::cout << "  server start failed: " << e.what() << std::endl;
            return false;
        }
        return server->m1Link().listenPort() != 0;
    }
    void stop() {
        if (server) {
            server->stop();
            server.reset();
        }
    }
};

// ---------- 工厂烟囱（双平台） ----------

bool test_factory_smoke() {
    std::cout << "Running: test_factory_smoke..." << std::endl;
    auto server = cell::makeM1LinkServer(cell::M1LinkConfig{}, [](IntentEnvelope) {});
    TEST_ASSERT(server != nullptr, "factory non-null");
    TEST_ASSERT(!server->gatewayConnected(), "seam default disconnected");
#if !defined(_WIN32)
    // POSIX：空路由路径 = 装配失败显形（不带隐式回退）
    TEST_ASSERT(!server->start(), "empty route json rejected at assembly");
#endif
    std::cout << "  PASSED" << std::endl;
    return true;
}

#if !defined(_WIN32)

// ---------- 意图族全量 e2e：上行 → 镜像 → 执行 → 延时应答 ----------

bool test_intent_family_deferred_ack() {
    std::cout << "Running: test_intent_family_deferred_ack..." << std::endl;
    AckLog acks;
    g_acks = &acks;

    ServerEnd s;
    TEST_ASSERT(s.start(50), "server started");
    ClientEnd c;
    TEST_ASSERT(c.start(s.server->m1Link().listenPort()), "client endpoint added");

    // 握手完成（客户端 kUp + 服务端见连接）
    TEST_ASSERT(pump_until(c, [&] {
        return c.cell_link()->state() == link::LinkState::kUp
            && s.server->m1Link().gatewayConnected();
    }, 10000), "handshake complete");

    // 五族意图各一发（字段序 = 契约表序）；执行语义不崩溃 + 延时应答全量
    auto create = base_fields(1, 77);
    create.push_back(i64(1001));  // entity_id
    create.push_back(i64(1));     // entity_type PLAYER
    create.push_back(i64(1));     // space_id
    create.push_back(f32(1.0f));  // pos
    create.push_back(f32(2.0f));
    create.push_back(f32(3.0f));
    TEST_ASSERT(send_intent(c, id_of(MsgId::cell_create_entity), create), "create sent");

    auto destroy = base_fields(2, 77);
    destroy.push_back(i64(1001));  // entity_id
    TEST_ASSERT(send_intent(c, id_of(MsgId::cell_destroy_entity), destroy), "destroy sent");

    auto move = base_fields(3, 77);
    move.push_back(i64(999));      // entity_id（未知实体 = handler 内 no-op 不回错）
    move.push_back(f32(1.0f));     // old pos
    move.push_back(f32(2.0f));
    move.push_back(f32(3.0f));
    move.push_back(f32(4.0f));     // new pos
    move.push_back(f32(5.0f));
    move.push_back(f32(6.0f));
    TEST_ASSERT(send_intent(c, id_of(MsgId::cell_entity_move), move), "move sent");

    auto border = base_fields(4, 77);
    border.push_back(i64(999));    // entity_id
    border.push_back(i64(1));      // from_space
    border.push_back(i64(1));      // to_space（同场景 = 无路由转移 no-op 面）
    border.push_back(f32(7.0f));   // pos
    border.push_back(f32(8.0f));
    border.push_back(f32(9.0f));
    TEST_ASSERT(send_intent(c, id_of(MsgId::cell_cross_border), border), "border sent");

    auto skill = base_fields(5, 77);
    skill.push_back(i64(1001));    // caster_id
    skill.push_back(i64(1002));    // target_id（不在场 = 不结算）
    skill.push_back(i64(7));       // skill_id int32
    skill.push_back(f32(1.0f));    // caster pos
    skill.push_back(f32(2.0f));
    skill.push_back(f32(3.0f));
    skill.push_back(f32(4.0f));    // target pos
    skill.push_back(f32(5.0f));
    skill.push_back(f32(6.0f));
    TEST_ASSERT(send_intent(c, id_of(MsgId::cell_skill_cast), skill), "skill sent");

    // 延时应答：五意图逐一回 code 0（执行发生在 tick 边界，含下一拍上线）
    const bool all_acked = pump_until(c, [&] {
        return acks.size() >= 5 && acks.has(1, 0) && acks.has(2, 0) && acks.has(3, 0)
            && acks.has(4, 0) && acks.has(5, 0);
    }, 15000);
    if (!all_acked) {
        std::cout << "  DEBUG: acks=" << acks.size()
                  << " received=" << s.server->m1Link().receivedIntents()
                  << " rejected=" << s.server->m1Link().rejectedFrames()
                  << " link=" << static_cast<int>(c.cell_link()->state())
                  << " gw=" << s.server->m1Link().gatewayConnected() << std::endl;
    }
    TEST_ASSERT(all_acked, "five deferred acks received");

    TEST_ASSERT(s.server->m1Link().receivedIntents() == 5, "five intents received");
    TEST_ASSERT(s.server->m1Link().rejectedFrames() == 0, "no rejects on valid stream");
    TEST_ASSERT(c.cell_link()->state() == link::LinkState::kUp, "link still up");

    s.stop();
    c.manager.reset();
    g_acks = nullptr;
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- 字段级非法：拒收计数、无回执、不断连 ----------

bool test_malformed_intent_rejected() {
    std::cout << "Running: test_malformed_intent_rejected..." << std::endl;
    AckLog acks;
    g_acks = &acks;

    ServerEnd s;
    TEST_ASSERT(s.start(50), "server started");
    ClientEnd c;
    TEST_ASSERT(c.start(s.server->m1Link().listenPort()), "client endpoint added");
    TEST_ASSERT(pump_until(c, [&] {
        return c.cell_link()->state() == link::LinkState::kUp
            && s.server->m1Link().gatewayConnected();
    }, 10000), "handshake complete");

    // id 924 但载荷截断（request_id 需 8B，只给 3B）→ 字段级解码失败
    const std::vector<std::uint8_t> truncated{0x01, 0x02, 0x03};
    TEST_ASSERT(c.cell_link()->send(id_of(MsgId::cell_create_entity), truncated,
                                    link::SendClass::kReliable)
                    == link::SendCode::kAccepted,
                "truncated frame accepted at link level");

    // 无回执面（decode 失败即丢弃，不产生 929）；链路不因字段非法断
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(600);
    while (std::chrono::steady_clock::now() < deadline) {
        c.pump();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    TEST_ASSERT(acks.size() == 0, "no ack for malformed intent");
    TEST_ASSERT(s.server->m1Link().receivedIntents() == 0, "no intent delivered");
    TEST_ASSERT(s.server->m1Link().rejectedFrames() == 1, "one reject counted");
    TEST_ASSERT(c.cell_link()->state() == link::LinkState::kUp, "link survives field error");

    s.stop();
    c.manager.reset();
    g_acks = nullptr;
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- 域越权 / 未知 id：dispatcher 拒收、不致命不断连 ----------

bool test_forbidden_and_unknown_ids_silent() {
    std::cout << "Running: test_forbidden_and_unknown_ids_silent..." << std::endl;
    AckLog acks;
    g_acks = &acks;

    ServerEnd s;
    TEST_ASSERT(s.start(50), "server started");
    ClientEnd c;
    TEST_ASSERT(c.start(s.server->m1Link().listenPort()), "client endpoint added");
    TEST_ASSERT(pump_until(c, [&] {
        return c.cell_link()->state() == link::LinkState::kUp
            && s.server->m1Link().gatewayConnected();
    }, 10000), "handshake complete");

    // 201 login_request：契约内但 domain=client（上行白名单越权）
    const std::vector<std::uint8_t> junk{0x00};
    TEST_ASSERT(c.cell_link()->send(201, junk, link::SendClass::kReliable)
                    == link::SendCode::kAccepted, "client-domain frame sent");
    // 0xDEAD：契约外 id
    TEST_ASSERT(c.cell_link()->send(0xDEAD, junk, link::SendClass::kReliable)
                    == link::SendCode::kAccepted, "unknown-id frame sent");

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(600);
    while (std::chrono::steady_clock::now() < deadline) {
        c.pump();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    TEST_ASSERT(acks.size() == 0, "no ack for out-of-scope frames");
    TEST_ASSERT(s.server->m1Link().receivedIntents() == 0, "no intent delivered");
    TEST_ASSERT(s.server->m1Link().rejectedFrames() == 0, "router-level rejects not handler rejects");
    TEST_ASSERT(c.cell_link()->state() == link::LinkState::kUp, "link survives dispatch rejects");

    s.stop();
    c.manager.reset();
    g_acks = nullptr;
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- 顶格拒收：单拍批量 > cap → 即时回执 2101 ----------

// ---------- 932 internal_envelope 双受理（P3-3 语义层批 · trace 子集载体） ----------

bool test_wrapped_envelope_intent() {
    std::cout << "Running: test_wrapped_envelope_intent..." << std::endl;
    AckLog acks;
    g_acks = &acks;

    ServerEnd s;
    TEST_ASSERT(s.start(50), "server started");
    ClientEnd c;
    TEST_ASSERT(c.start(s.server->m1Link().listenPort()), "client endpoint added");

    TEST_ASSERT(pump_until(c, [&] {
        return c.cell_link()->state() == link::LinkState::kUp
            && s.server->m1Link().gatewayConnected();
    }, 10000), "handshake complete");

    // 信封全量形：trace 采样命中 + 内层 create entity（与 bare 同族字段序）
    trace::EnvelopeFields env{};
    env.message_type = trace::EnvelopeKind::EntityMethodCall;
    env.request_id = 60;
    env.source_app = "gateway#1";
    env.target_app = "cell#1";
    env.entity_id = 1001;
    env.method_alias = "create";
    env.route_version = 7;
    env.authority_epoch = 3;
    TEST_ASSERT(trace::from_hex("11223344556677889900aabbccddeeff", env.trace.trace_id.data(),
                                16),
                "trace id hex");
    TEST_ASSERT(trace::from_hex("0102030405060708", env.trace.parent_span_id.data(), 8),
                "parent span hex");
    env.trace.sampled = true;

    std::vector<wire::FieldValue> inner = base_fields(60, 88);
    inner.push_back(i64(1001));  // entity_id
    inner.push_back(i64(1));     // entity_type PLAYER
    inner.push_back(i64(1));     // space_id
    inner.push_back(f32(1.0f));
    inner.push_back(f32(2.0f));
    inner.push_back(f32(3.0f));
    env.payload = wire::append_envelope(id_of(MsgId::cell_create_entity),
                                        [&] {
                                            std::vector<std::uint8_t> p;
                                            (void)wire::encode_payload(
                                                id_of(MsgId::cell_create_entity), inner, p);
                                            return p;
                                        }());

    TEST_ASSERT(send_wrapped_intent(c, env), "wrapped intent sent");

    TEST_ASSERT(pump_until(c, [&] { return acks.has(60, 0); }, 15000),
                "wrapped intent acked");

    TEST_ASSERT(s.server->m1Link().receivedIntents() >= 1, "wrapped intent received");
    TEST_ASSERT(s.server->m1Link().rejectedFrames() == 0, "no rejects on wrapped intent");
    TEST_ASSERT(c.cell_link()->state() == link::LinkState::kUp, "link still up");

    s.stop();
    c.manager.reset();
    g_acks = nullptr;
    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_wrapped_non_intent_inner_rejected() {
    std::cout << "Running: test_wrapped_non_intent_inner_rejected..." << std::endl;
    AckLog acks;
    g_acks = &acks;

    ServerEnd s;
    TEST_ASSERT(s.start(50), "server started");
    ClientEnd c;
    TEST_ASSERT(c.start(s.server->m1Link().listenPort()), "client endpoint added");

    TEST_ASSERT(pump_until(c, [&] {
        return c.cell_link()->state() == link::LinkState::kUp
            && s.server->m1Link().gatewayConnected();
    }, 10000), "handshake complete");

    // 内层 = 非意图族（931 metrics_snapshot）：信封形合法、内层不在本接收
    // 面装配 → 拒收计数、无回执、不断连
    trace::EnvelopeFields env{};
    env.message_type = trace::EnvelopeKind::RouteControl;
    env.request_id = 61;
    env.source_app = "gateway#1";
    const std::uint16_t inner_id = id_of(MsgId::metrics_snapshot);
    env.payload = wire::append_envelope(inner_id, [&] {
        std::vector<wire::FieldValue> f(2);
        f[0].text = "gateway#1";
        f[1].text = "x";
        std::vector<std::uint8_t> p;
        (void)wire::encode_payload(inner_id, f, p);
        return p;
    }());

    const auto rejected_before = s.server->m1Link().rejectedFrames();
    TEST_ASSERT(send_wrapped_intent(c, env), "wrapped non-intent sent");
    TEST_ASSERT(pump_until(c, [&] {
        return s.server->m1Link().rejectedFrames() > rejected_before;
    }, 15000), "non-intent inner counted as reject");

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    c.pump();
    TEST_ASSERT(!acks.has(61, 0) && !acks.has(61, 1) && !acks.has(61, 2),
                "no ack for rejected inner");
    TEST_ASSERT(c.cell_link()->state() == link::LinkState::kUp, "reject is not fatal");

    s.stop();
    c.manager.reset();
    g_acks = nullptr;
    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_queue_full_backpressure_ack() {
    std::cout << "Running: test_queue_full_backpressure_ack..." << std::endl;
    AckLog acks;
    g_acks = &acks;

    ServerEnd s;
    // 慢拍（1s）：整批在同一拍内到齐（单 pump 排空读侧）→ 顶格拒收确定面；
    // 偶发拆拍（写读交叠）则本轮全受理，重试轮再压（重试至多 4 轮）。
    TEST_ASSERT(s.start(1000), "server started");
    ClientEnd c;
    TEST_ASSERT(c.start(s.server->m1Link().listenPort()), "client endpoint added");
    TEST_ASSERT(pump_until(c, [&] {
        return c.cell_link()->state() == link::LinkState::kUp
            && s.server->m1Link().gatewayConnected();
    }, 10000), "handshake complete");
    // 首拍 settle：空队列 drain 已过，突发窗口干净
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    c.pump();

    constexpr int kBurst = 2560;  // 2.5× cap（1024）
    bool rejected = false;
    for (int round = 0; round < 4 && !rejected; ++round) {
        const auto base_req = static_cast<std::uint64_t>(100000 + round * 100000);
        // 全量本地入队（ Reliable 队列 < 4MiB 水位），再单拍刷线
        for (int i = 0; i < kBurst; ++i) {
            auto f = base_fields(base_req + static_cast<std::uint64_t>(i),
                                 static_cast<std::uint64_t>(500 + i));
            f.push_back(i64(7000 + i));  // entity_id
            f.push_back(i64(3));         // entity_type MONSTER
            f.push_back(i64(1));         // space_id
            f.push_back(f32(0.0f));
            f.push_back(f32(0.0f));
            f.push_back(f32(0.0f));
            TEST_ASSERT(send_intent(c, id_of(MsgId::cell_create_entity), f),
                        "burst intent enqueued");
        }
        c.pump();

        // 全量回执（受理 0 + 拒收 2101；恰一次 = 链路恰一次投影）
        TEST_ASSERT(pump_until(c, [&] { return acks.size() >= static_cast<std::size_t>(kBurst); },
                               20000),
                    "all burst acks returned");
        rejected = acks.count_code(cell::kIntentAckBackpressure) > 0;
    }
    if (!rejected) {
        // 迟到回执兜底等待（2101 为受理点即时回执，正常一拍内到）
        pump_until(c, [&] { return acks.count_code(cell::kIntentAckBackpressure) > 0; }, 5000);
        rejected = acks.count_code(cell::kIntentAckBackpressure) > 0;
    }

    TEST_ASSERT(rejected, "at least one 2101 backpressure ack across rounds");
    TEST_ASSERT(s.server->m1Link().receivedIntents() > 0, "accepted intents delivered");
    TEST_ASSERT(c.cell_link()->state() == link::LinkState::kUp, "link survives overflow");

    const auto accepted = s.server->m1Link().receivedIntents();
    const auto rejected_acks = acks.count_code(cell::kIntentAckBackpressure);
    s.stop();
    c.manager.reset();
    g_acks = nullptr;
    std::cout << "  PASSED (accepted=" << accepted
              << " rejected_acks=" << rejected_acks << ")" << std::endl;
    return true;
}

#endif // !_WIN32

} // namespace

int main() {
    int failed = 0;
    if (!test_factory_smoke()) ++failed;
#if !defined(_WIN32)
    if (!test_intent_family_deferred_ack()) ++failed;
    if (!test_malformed_intent_rejected()) ++failed;
    if (!test_forbidden_and_unknown_ids_silent()) ++failed;
    if (!test_queue_full_backpressure_ack()) ++failed;
    if (!test_wrapped_envelope_intent()) ++failed;
    if (!test_wrapped_non_intent_inner_rejected()) ++failed;
#endif
    if (failed == 0) {
        std::cout << "ALL CELL M1 LINK TESTS PASSED" << std::endl;
        return 0;
    }
    std::cerr << failed << " cell m1 test(s) failed" << std::endl;
    return 1;
}
