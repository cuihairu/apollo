// RoomServer M1 接收端批测试（M1 上层接线批 2b-ii；bare 退役批改张）：
// 契约意图族 924-928 真 TCP 上行（客户端 = 裸 LinkManager active 端点，
// 恒走 932 internal_envelope 信封——bare 924-928 双受理已退役）→ 信封解包 →
// 契约 wire 解包 → legacy 帧镜像 → 受理队列 → tick 边界执行 → 延时应答
// 929（request_id 关联，code 0 = 已消费）；拒收面（字段级非法静默计数、
// 域越权/未知 id 不致命不断连、裸五族帧 = no_handler 静默丢弃）与顶格拒收
// 即时回执 2101。
// 无 GTest，plain-assert + main 汇总。
#include "room_server/room_server.hpp"
#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/linktrace/envelope.hpp"
#include "apollo/net/linktrace/span_emitter.hpp"
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

using room_server::RoomServerConfig;
using room_server::RoomServer;
using room_server::IntentEnvelope;
using apollo::contract::gen::MsgId;

constexpr std::uint16_t id_of(MsgId id) {
    return static_cast<std::uint16_t>(id);
}
constexpr std::uint16_t kAckId = id_of(MsgId::cell_intent_ack);
constexpr std::uint16_t kEvalRequestId = id_of(MsgId::debug_eval_request);
constexpr std::uint16_t kEvalResultId = id_of(MsgId::debug_eval_result);

const char* kRouteJson = APOLLO_ROOM_SERVER_CONTRACT_ROUTE_JSON;

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

// 945 eval 回执日志（scripting-lua §7.1 attach transport 契约面测试）。
struct EvalReply {
    std::uint64_t eval_id = 0;
    bool ok = false;
    std::string result;
    std::string error;
};
struct EvalLog {
    std::mutex mutex;
    std::vector<EvalReply> replies;

    void add(std::uint64_t eval_id, bool ok, std::string result, std::string error) {
        std::lock_guard<std::mutex> lock(mutex);
        replies.push_back({eval_id, ok, std::move(result), std::move(error)});
    }
    bool find(std::uint64_t eval_id, EvalReply& out) {
        std::lock_guard<std::mutex> lock(mutex);
        for (const auto& r : replies) {
            if (r.eval_id == eval_id) {
                out = r;
                return true;
            }
        }
        return false;
    }
    std::size_t size() {
        std::lock_guard<std::mutex> lock(mutex);
        return replies.size();
    }
};

EvalLog* g_evals = nullptr;

// 客户端端（网关投影）：LinkManager active 端点拨入 RoomServer M1 端口，同一路由
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
        ep.name = "room-server";
        ep.host = "127.0.0.1";
        ep.port = port;
        ep.role = link::EndpointRole::kActive;
        if (!manager->add_endpoint(ep)) {
            return false;
        }

        auto& dispatcher = manager->link("room-server")->dispatcher();
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
        dispatcher.set_handler(kEvalResultId, [](std::uint16_t, const std::uint8_t* data,
                                                 std::size_t size) {
            std::vector<wire::FieldValue> fields;
            if (wire::decode_payload(kEvalResultId, data, size, fields) && fields.size() == 4
                && g_evals != nullptr) {
                g_evals->add(static_cast<std::uint64_t>(fields[0].integer),
                             fields[1].integer != 0, fields[2].text, fields[3].text);
            }
        });
        return true;
    }

    void pump() { manager->pump(now_ms()); }

    link::InterServerLink* cell_link() { return manager->link("room-server"); }
};

// 客户端泵等待（服务端由 RoomServer gameThread 自泵；客户端由本测试线程泵）。
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

// 裸帧发送（bare 退役批后仅退役探针用：正路上行恒走信封——见
// send_wrapped_intent_fields；裸五族帧应被 no_handler 静默丢弃）。
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

// 944 debug_eval_request 裸帧上行（§7.1 attach transport：eval 非意图族，
// 不走 932 信封——RoomServer 受理面恒裸帧登记）。
bool send_debug_eval(ClientEnd& client, std::uint64_t eval_id, std::uint64_t scene_id,
                     const std::string& source) {
    std::vector<wire::FieldValue> f(3);
    f[0].integer = static_cast<std::int64_t>(eval_id);
    f[1].integer = static_cast<std::int64_t>(scene_id);
    f[2].text = source;
    std::vector<std::uint8_t> payload;
    if (!wire::encode_payload(kEvalRequestId, f, payload)) {
        return false;
    }
    return client.cell_link()->send(kEvalRequestId, payload.data(), payload.size(),
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

// 信封上行的通用形（bare 退役批）：内层 id + 契约字段恒包 932——镜像
// gateway m1_egress 发端语义（EnvelopeKind::EntityMethodCall + 内层 u16
// LE id + 契约载荷）；未采样（sampled 缺省 false）= 零 span 发射干扰。
bool send_wrapped_intent_fields(ClientEnd& client, std::uint16_t inner_id,
                                std::uint64_t request_id,
                                std::vector<wire::FieldValue> fields) {
    trace::EnvelopeFields env{};
    env.message_type = trace::EnvelopeKind::EntityMethodCall;
    env.request_id = request_id;
    env.source_app = "gateway";
    env.target_app = "room-server";
    env.payload = wire::append_envelope(inner_id, [&] {
        std::vector<std::uint8_t> p;
        (void)wire::encode_payload(inner_id, fields, p);
        return p;
    }());
    return send_wrapped_intent(client, env);
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
    RoomServerConfig config;
    std::unique_ptr<RoomServer> server;

    bool start(int tick_rate_ms) {
        config.tickRateMs = tick_rate_ms;
        config.m1Port = 0;
        config.m1RouteJson = kRouteJson;
        try {
            server = std::make_unique<RoomServer>(config);
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
    auto server = room_server::makeM1LinkServer(room_server::M1LinkConfig{}, [](IntentEnvelope) {});
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

    // 五族意图各一发（恒走 932 信封，字段序 = 契约表序）；执行语义不崩溃 +
    // 延时应答全量
    auto create = base_fields(1, 77);
    create.push_back(i64(1001));  // entity_id
    create.push_back(i64(1));     // entity_type PLAYER
    create.push_back(i64(1));     // space_id
    create.push_back(f32(1.0f));  // pos
    create.push_back(f32(2.0f));
    create.push_back(f32(3.0f));
    TEST_ASSERT(send_wrapped_intent_fields(c, id_of(MsgId::cell_create_entity), 1,
                                           std::move(create)),
                "create sent");

    auto destroy = base_fields(2, 77);
    destroy.push_back(i64(1001));  // entity_id
    TEST_ASSERT(send_wrapped_intent_fields(c, id_of(MsgId::cell_destroy_entity), 2,
                                           std::move(destroy)),
                "destroy sent");

    auto move = base_fields(3, 77);
    move.push_back(i64(999));      // entity_id（未知实体 = handler 内 no-op 不回错）
    move.push_back(f32(1.0f));     // old pos
    move.push_back(f32(2.0f));
    move.push_back(f32(3.0f));
    move.push_back(f32(4.0f));     // new pos
    move.push_back(f32(5.0f));
    move.push_back(f32(6.0f));
    TEST_ASSERT(send_wrapped_intent_fields(c, id_of(MsgId::cell_entity_move), 3,
                                           std::move(move)),
                "move sent");

    auto border = base_fields(4, 77);
    border.push_back(i64(999));    // entity_id
    border.push_back(i64(1));      // from_space
    border.push_back(i64(1));      // to_space（同场景 = 无路由转移 no-op 面）
    border.push_back(f32(7.0f));   // pos
    border.push_back(f32(8.0f));
    border.push_back(f32(9.0f));
    TEST_ASSERT(send_wrapped_intent_fields(c, id_of(MsgId::cell_cross_border), 4,
                                           std::move(border)),
                "border sent");

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
    TEST_ASSERT(send_wrapped_intent_fields(c, id_of(MsgId::cell_skill_cast), 5,
                                           std::move(skill)),
                "skill sent");

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

    // 信封合法、内层 924 载荷截断（request_id 需 8B，只给 3B）→ 内层字段级
    // 解码失败（bare 退役批：同一拒收语义自裸帧迁入信封内层）
    trace::EnvelopeFields env{};
    env.message_type = trace::EnvelopeKind::EntityMethodCall;
    env.request_id = 70;
    env.source_app = "gateway";
    env.target_app = "room-server";
    const std::vector<std::uint8_t> truncated{0x01, 0x02, 0x03};
    env.payload = wire::append_envelope(id_of(MsgId::cell_create_entity), truncated);
    TEST_ASSERT(send_wrapped_intent(c, env), "truncated inner sent");

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

// ---------- bare 924-928 双受理退役（P3-3 移交余件②） ----------
//
// 消费面清零证据：生产发端唯 gateway m1_egress，5bdca27f 起恒包 932 信封
//（sdk/scripts/examples 零裸发端）。退役后裸五族帧 = dispatcher 无 handler
// → no_handler 静默丢弃：不受理、不回执、不断连、不计 handler 拒收。

bool test_bare_intent_family_retired() {
    std::cout << "Running: test_bare_intent_family_retired..." << std::endl;
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

    // 合法形裸 create（字段序 = 契约表序；双受理若回潮 = 本断言红）
    auto create = base_fields(90, 77);
    create.push_back(i64(1001));  // entity_id
    create.push_back(i64(1));     // entity_type PLAYER
    create.push_back(i64(1));     // space_id
    create.push_back(f32(1.0f));
    create.push_back(f32(2.0f));
    create.push_back(f32(3.0f));
    TEST_ASSERT(send_intent(c, id_of(MsgId::cell_create_entity), create),
                "bare frame accepted at link level");

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(600);
    while (std::chrono::steady_clock::now() < deadline) {
        c.pump();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    TEST_ASSERT(acks.size() == 0, "no ack for bare frame");
    TEST_ASSERT(s.server->m1Link().receivedIntents() == 0, "bare frame not delivered");
    TEST_ASSERT(s.server->m1Link().rejectedFrames() == 0, "no-handler drop is router-level");
    TEST_ASSERT(c.cell_link()->state() == link::LinkState::kUp, "link survives bare frame");

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

// ---------- 932 internal_envelope 唯一受理面（P3-3 语义层批 · trace 子集载体；
// bare 退役批后即唯一上行形） ----------

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

    // span 收集器（RoomServer span 发射面：采样命中 + trace 链关联）
    std::vector<trace::SpanRecord> spans;
    trace::set_span_sink([&spans](const trace::SpanRecord& record) {
        spans.push_back(record);
    });

    TEST_ASSERT(send_wrapped_intent(c, env), "wrapped intent sent");

    TEST_ASSERT(pump_until(c, [&] { return acks.has(60, 0); }, 15000),
                "wrapped intent acked");

    TEST_ASSERT(s.server->m1Link().receivedIntents() >= 1, "wrapped intent received");
    TEST_ASSERT(s.server->m1Link().rejectedFrames() == 0, "no rejects on wrapped intent");
    TEST_ASSERT(c.cell_link()->state() == link::LinkState::kUp, "link still up");

    // RoomServer span：parent = 信封发端（gateway span id），trace_id 同链
    bool cell_span = false;
    for (const auto& record : spans) {
        if (record.name == "room_server_intent_execute") {
            cell_span = true;
            TEST_ASSERT(record.ctx.trace_id == env.trace.trace_id, "RoomServer span same chain");
            TEST_ASSERT(record.ctx.parent_span_id == env.trace.parent_span_id,
                        "RoomServer span chains under gateway span");
            TEST_ASSERT(record.start_ms <= record.end_ms, "RoomServer span timing ordered");
        }
    }
    TEST_ASSERT(cell_span, "RoomServer span emitted for sampled intent");
    trace::set_span_sink(nullptr);

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
        // 全量本地入队（Reliable 队列 < 4MiB 水位），再单拍刷线（恒走信封）
        for (int i = 0; i < kBurst; ++i) {
            auto f = base_fields(base_req + static_cast<std::uint64_t>(i),
                                 static_cast<std::uint64_t>(500 + i));
            f.push_back(i64(7000 + i));  // entity_id
            f.push_back(i64(3));         // entity_type MONSTER
            f.push_back(i64(1));         // space_id
            f.push_back(f32(0.0f));
            f.push_back(f32(0.0f));
            f.push_back(f32(0.0f));
            TEST_ASSERT(send_wrapped_intent_fields(c, id_of(MsgId::cell_create_entity),
                                                   base_req + static_cast<std::uint64_t>(i),
                                                   std::move(f)),
                        "burst intent enqueued");
        }
        c.pump();

        // 全量回执（受理 0 + 拒收 2101；恰一次 = 链路恰一次投影）
        TEST_ASSERT(pump_until(c, [&] { return acks.size() >= static_cast<std::size_t>(kBurst); },
                               20000),
                    "all burst acks returned");
        rejected = acks.count_code(room_server::kIntentAckBackpressure) > 0;
    }
    if (!rejected) {
        // 迟到回执兜底等待（2101 为受理点即时回执，正常一拍内到）
        pump_until(c, [&] { return acks.count_code(room_server::kIntentAckBackpressure) > 0; }, 5000);
        rejected = acks.count_code(room_server::kIntentAckBackpressure) > 0;
    }

    TEST_ASSERT(rejected, "at least one 2101 backpressure ack across rounds");
    TEST_ASSERT(s.server->m1Link().receivedIntents() > 0, "accepted intents delivered");
    TEST_ASSERT(c.cell_link()->state() == link::LinkState::kUp, "link survives overflow");

    const auto accepted = s.server->m1Link().receivedIntents();
    const auto rejected_acks = acks.count_code(room_server::kIntentAckBackpressure);
    s.stop();
    c.manager.reset();
    g_acks = nullptr;
    std::cout << "  PASSED (accepted=" << accepted
              << " rejected_acks=" << rejected_acks << ")" << std::endl;
    return true;
}

// ---------- §7.1 attach transport（944/945 debug eval 契约面） ----------

// 全链 e2e：944 裸帧上行 → RoomServer 受理（默认 scene 定位 + enqueue）→ Scene
// tick 首拍固定调试点执行 → 945 回执（eval_id 关联，ok/result/error 契约字序）。
bool test_debug_eval_roundtrip() {
    std::cout << "Running: test_debug_eval_roundtrip..." << std::endl;
    EvalLog evals;
    g_evals = &evals;
    ServerEnd s;
    TEST_ASSERT(s.start(10), "server started");
    ClientEnd c;
    TEST_ASSERT(c.start(s.server->m1Link().listenPort()), "client dialed");
    TEST_ASSERT(pump_until(c, [&] {
                    return c.cell_link()->state() == link::LinkState::kUp;
                }, 5000), "link up");

    TEST_ASSERT(send_debug_eval(c, 77, 0, "1+1"), "eval request sent");
    EvalReply r;
    TEST_ASSERT(pump_until(c, [&] { return evals.find(77, r); }, 5000),
                "eval reply arrived");
    TEST_ASSERT(r.ok, "eval ok");
    TEST_ASSERT(r.result == "2", "eval result value");
    TEST_ASSERT(r.error.empty(), "no error field on success");

    s.stop();
    c.manager.reset();
    g_evals = nullptr;
    std::cout << "  PASSED" << std::endl;
    return true;
}

// 拒收面：未知 scene = 即时 ok=0 回执（错误原文非空）；eval_id 0 = 关联键
// 缺位静默（0 保留——连回执都不给，与 sendM1Ack 的 request_id=0 同型）。
bool test_debug_eval_reject_face() {
    std::cout << "Running: test_debug_eval_reject_face..." << std::endl;
    EvalLog evals;
    g_evals = &evals;
    ServerEnd s;
    TEST_ASSERT(s.start(10), "server started");
    ClientEnd c;
    TEST_ASSERT(c.start(s.server->m1Link().listenPort()), "client dialed");
    TEST_ASSERT(pump_until(c, [&] {
                    return c.cell_link()->state() == link::LinkState::kUp;
                }, 5000), "link up");

    // 未知 scene：即时 945 ok=0 + 错误原文
    TEST_ASSERT(send_debug_eval(c, 78, 9999, "1"), "unknown scene request sent");
    EvalReply r;
    TEST_ASSERT(pump_until(c, [&] { return evals.find(78, r); }, 5000),
                "reject reply arrived");
    TEST_ASSERT(!r.ok, "unknown scene not ok");
    TEST_ASSERT(!r.error.empty(), "reject carries error text");
    TEST_ASSERT(r.result.empty(), "no result field on failure");

    // eval_id 0：静默（若干拍后无任何回执）
    TEST_ASSERT(send_debug_eval(c, 0, 0, "1"), "zero id request sent");
    const auto before = evals.size();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    pump_until(c, [] { return false; }, 50);  // 末拍收尾（谓词恒 false）
    TEST_ASSERT(evals.size() == before, "zero eval_id stays silent");

    s.stop();
    c.manager.reset();
    g_evals = nullptr;
    std::cout << "  PASSED" << std::endl;
    return true;
}

// 失败面 + 宿主存活：运行错源 → ok=0 + lua 错误原文（eval_debug 失败面
// result = 错误原文的契约投影）；失败后宿主继续服务（后续 eval 正常回执）。
bool test_debug_eval_failure_surface() {
    std::cout << "Running: test_debug_eval_failure_surface..." << std::endl;
    EvalLog evals;
    g_evals = &evals;
    ServerEnd s;
    TEST_ASSERT(s.start(10), "server started");
    ClientEnd c;
    TEST_ASSERT(c.start(s.server->m1Link().listenPort()), "client dialed");
    TEST_ASSERT(pump_until(c, [&] {
                    return c.cell_link()->state() == link::LinkState::kUp;
                }, 5000), "link up");

    TEST_ASSERT(send_debug_eval(c, 79, 0, "error('boom')"), "failing request sent");
    EvalReply bad;
    TEST_ASSERT(pump_until(c, [&] { return evals.find(79, bad); }, 5000),
                "failing eval reply arrived");
    TEST_ASSERT(!bad.ok, "failing eval not ok");
    TEST_ASSERT(bad.error.find("boom") != std::string::npos,
                "error carries lua error text");

    // 宿主仍服务（失败不打断：后续 eval 正常回执）
    TEST_ASSERT(send_debug_eval(c, 80, 0, "'alive'"), "follow-up request sent");
    EvalReply after;
    TEST_ASSERT(pump_until(c, [&] { return evals.find(80, after); }, 5000),
                "follow-up reply arrived");
    TEST_ASSERT(after.ok && after.result == "alive", "host still serving");
    TEST_ASSERT(s.server->isRunning(), "cell alive after eval failure");

    s.stop();
    c.manager.reset();
    g_evals = nullptr;
    std::cout << "  PASSED" << std::endl;
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
    if (!test_bare_intent_family_retired()) ++failed;
    if (!test_forbidden_and_unknown_ids_silent()) ++failed;
    if (!test_queue_full_backpressure_ack()) ++failed;
    if (!test_wrapped_envelope_intent()) ++failed;
    if (!test_wrapped_non_intent_inner_rejected()) ++failed;
    if (!test_debug_eval_roundtrip()) ++failed;
    if (!test_debug_eval_reject_face()) ++failed;
    if (!test_debug_eval_failure_surface()) ++failed;
#endif
    if (failed == 0) {
        std::cout << "ALL CELL M1 LINK TESTS PASSED" << std::endl;
        return 0;
    }
    std::cerr << failed << " cell m1 test(s) failed" << std::endl;
    return 1;
}
