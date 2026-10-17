// Gateway M1 出口测试（M1 上层接线批 · 2b-iii）：
//   1. 五族 legacy 世界帧 → 契约 wire 前递（字段镜像断言 + request_id 配发）
//   2. 929 应答回流 → pending 关联回收（acked 计数 + 账面清空）
//   3. 应答超时清扫（timed_out 回收 + 迟到应答不误账）
//   4. 不可前递族落地（残帧/PING/断连通知——legacy 链路死观察面同型）
//   5. 投递箱顶格溢出落地
//   6. 路由表缺位 = 装配降级显形
//   7. 全链 e2e：真客户端 socket → gateway 入口 → M1 出口 → 真 RoomServer M1
//      接收端 → 受理执行 → 延时应答 929 回流 → 出口关联回收
// 客户端端 = 裸 LinkManager（拆帧工具复用 link 面，帧格式属被测契约）。

#include "gateway/config.hpp"
#include "gateway/gateway_server.hpp"
#include "gateway/m1_egress.hpp"
#include "room_server/room_server.hpp"
#include "apollo/core/log/log_manager.h"
#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/linktrace/envelope.hpp"
#include "apollo/net/linktrace/span_emitter.hpp"
#include "apollo/net/wire/contract_wire.hpp"
#include "apollo_contract.h"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"

#if !defined(_WIN32)

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

namespace {

namespace link = apollo::net::link;
namespace trace = apollo::net::trace;
namespace wire = apollo::net::wire;

using apollo::contract::gen::MsgId;
using gateway::M1Egress;
using gateway::M1EgressConfig;
using gateway::makeM1Egress;

constexpr std::uint16_t id_of(MsgId id) {
    return static_cast<std::uint16_t>(id);
}

const char* kRouteJson = APOLLO_GATEWAY_CONTRACT_ROUTE_JSON;

std::uint64_t now_ms() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

bool load_route(link::RouteTable& table) {
    std::ifstream in(kRouteJson, std::ios::binary);
    if (!in) {
        return false;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
#if APOLLO_NET_LINK_HAS_JSON
    return link::load_route_table_json(buffer.str(), table);
#else
    (void)table;
    return false;
#endif
}

// link 帧包封（客户端流帧格式属被测契约面，拆装工具复用 link 面）
std::vector<std::uint8_t> frame_bytes(std::uint32_t seq, std::uint8_t flags,
                                      const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> out;
    link::append_frame(out, seq, flags, payload);
    return out;
}

// 阻塞式测试客户端（独立实现，不复用被测代码的收发路径；仅发面）
class SendClient {
public:
    ~SendClient() { close(); }

    bool connect(const std::string& host, std::uint16_t port) {
        fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd_ < 0) {
            return false;
        }
        timeval tv{};
        tv.tv_sec = 10;  // 单收上限：CI 挂死守门
        ::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        ::sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1 ||
            ::connect(fd_, reinterpret_cast<::sockaddr*>(&addr), sizeof addr) != 0) {
            ::close(fd_);
            fd_ = -1;
            return false;
        }
        return true;
    }

    void sendAll(const std::vector<std::uint8_t>& bytes) {
        std::size_t off = 0;
        while (off < bytes.size()) {
            const ssize_t n = ::send(fd_, bytes.data() + off, bytes.size() - off, 0);
            if (n <= 0) {
                if (n < 0 && errno == EINTR) {
                    continue;
                }
                return;
            }
            off += static_cast<std::size_t>(n);
        }
    }

    void close() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

private:
    int fd_ = -1;
};

// 桩 RoomServer M1 接收端（裸 LinkManager，被动端点）：记录五族意图 + 按需回 929。
// 932 双受理（P3-3 语义层批）：信封解包 → 内层同族记录 + trace 携带面显形
//（bare 记录 wrapped=false、trace 全零）。
struct IntentRecord {
    std::uint16_t contract_id{0};  // 内层意图 id（932 解包后）
    std::uint64_t request_id{0};
    std::uint64_t session_id{0};
    std::vector<wire::FieldValue> fields;
    bool wrapped{false};
    trace::TraceContext trace{};
};

class FakeCell {
public:
    FakeCell() {
        link::LinkManagerConfig mc;
        mc.self_name = "room-server";
        route_ok_ = load_route(table_);
        if (route_ok_) {
            mc.route_version = link::route_version_from_hash(table_.schema_hash());
        }
        manager_ = std::make_unique<link::LinkManager>(mc);

        link::EndpointConfig ep;
        ep.name = "gateway";
        ep.host = "127.0.0.1";
        ep.port = 0;
        ep.role = link::EndpointRole::kPassive;
        added_ = manager_->add_endpoint(ep);
        if (added_) {
            // 分发装配随 peer 上线（fan-out 面：链路对象回调后地址稳定）
            manager_->set_peer_listener([this](link::InterServerLink& peer) {
                auto& dispatcher = peer.dispatcher();
                if (route_ok_) {
                    dispatcher.set_route_table(&table_);
                }
                dispatcher.allow_domain("internal");
                for (const std::uint16_t id :
                     {id_of(MsgId::cell_create_entity), id_of(MsgId::cell_destroy_entity),
                      id_of(MsgId::cell_entity_move), id_of(MsgId::cell_cross_border),
                      id_of(MsgId::cell_skill_cast)}) {
                    dispatcher.set_handler(id,
                                           [this, id](std::uint16_t, const std::uint8_t* data,
                                                      std::size_t size) {
                                               record(id, data, size);
                                           });
                }
                dispatcher.set_handler(id_of(MsgId::internal_envelope),
                                       [this](std::uint16_t, const std::uint8_t* data,
                                              std::size_t size) {
                                           record_envelope(data, size);
                                       });
            });
        }
    }

    bool healthy() const { return route_ok_ && added_; }

    std::uint16_t listen_port() const {
        return manager_ ? manager_->endpoint_port("gateway") : 0;
    }

    void pump(std::uint64_t now) {
        if (manager_) {
            manager_->pump(now);
        }
    }

    void record(std::uint16_t contract_id, const std::uint8_t* data, std::size_t size) {
        std::vector<wire::FieldValue> fields;
        if (!wire::decode_payload(contract_id, data, size, fields) || fields.size() < 2) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        records_.push_back(IntentRecord{contract_id,
                                        static_cast<std::uint64_t>(fields[0].integer),
                                        static_cast<std::uint64_t>(fields[1].integer),
                                        std::move(fields)});
    }

    void record_envelope(const std::uint8_t* data, std::size_t size) {
        trace::EnvelopeFields env;
        if (!trace::unpack_envelope(data, size, env)) {
            return;
        }
        std::uint16_t inner_id = 0;
        const std::uint8_t* body = nullptr;
        std::size_t body_size = 0;
        if (!trace::split_payload(env.payload, inner_id, body, body_size)) {
            return;
        }
        std::vector<wire::FieldValue> fields;
        if (!wire::decode_payload(inner_id, body, body_size, fields) || fields.size() < 2) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        records_.push_back(IntentRecord{inner_id,
                                        static_cast<std::uint64_t>(fields[0].integer),
                                        static_cast<std::uint64_t>(fields[1].integer),
                                        std::move(fields), true, env.trace});
    }

    std::vector<IntentRecord> records() {
        std::lock_guard<std::mutex> lock(mutex_);
        return records_;
    }

    std::size_t count() {
        std::lock_guard<std::mutex> lock(mutex_);
        return records_.size();
    }

    // 回 929 应答（code 0 / 指定值；裸载荷，send 自封 id 信封）
    bool ack(std::uint64_t request_id, std::int32_t code) {
        auto* gw = manager_ ? manager_->link("gateway") : nullptr;
        if (gw == nullptr) {
            return false;
        }
        std::vector<wire::FieldValue> fields(3);
        fields[0].integer = static_cast<std::int64_t>(request_id);
        fields[1].integer = code;
        fields[2].text = "";
        std::vector<std::uint8_t> payload;
        if (!wire::encode_payload(id_of(MsgId::cell_intent_ack), fields, payload)) {
            return false;
        }
        (void)gw->send(id_of(MsgId::cell_intent_ack), payload.data(), payload.size(),
                       link::SendClass::kReliable);
        return true;
    }

    // 全部已记录意图回 code 0
    std::size_t ack_all() {
        const auto snapshot = records();
        std::size_t n = 0;
        for (const auto& r : snapshot) {
            if (ack(r.request_id, 0)) {
                ++n;
            }
        }
        return n;
    }

private:
    link::RouteTable table_;
    bool route_ok_{false};
    bool added_{false};
    std::unique_ptr<link::LinkManager> manager_;
    std::mutex mutex_;
    std::vector<IntentRecord> records_;
};

// 双端泵驱等待（egress 与桩 cell 每 10ms 一拍，条件满足即回）
template <typename Pred>
bool pump_both(M1Egress& egress, FakeCell& cell, Pred&& done, int timeout_ms) {
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        const auto now = now_ms();
        egress.pump(now);
        cell.pump(now);
        if (done()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    egress.pump(now_ms());
    cell.pump(now_ms());
    return done();
}

// 五族 legacy 世界帧样本（session 77）
std::vector<std::vector<std::uint8_t>> five_family_frames() {
    namespace protocol = apollo::protocol;
    std::vector<std::vector<std::uint8_t>> frames;

    protocol::CellCreateEntity create;
    create.entityId = 42;
    create.entityType = static_cast<protocol::EntityType>(3);
    create.spaceId = 7;
    create.position = protocol::Position(1.5f, 2.5f, 3.5f);
    frames.push_back(protocol::MessageCodec::encode(create, 77));

    protocol::CellDestroyEntity destroy;
    destroy.entityId = 99;
    frames.push_back(protocol::MessageCodec::encode(destroy, 77));

    protocol::CellEntityMove move;
    move.entityId = 5;
    move.oldPos = protocol::Position(1.0f, 2.0f, 3.0f);
    move.newPos = protocol::Position(4.0f, 5.0f, 6.0f);
    frames.push_back(protocol::MessageCodec::encode(move, 77));

    protocol::CellCrossBorder border;
    border.entityId = 8;
    border.fromSpace = 1;
    border.toSpace = 2;
    border.position = protocol::Position(9.0f, 9.0f, 9.0f);
    frames.push_back(protocol::MessageCodec::encode(border, 77));

    protocol::CombatSkillCast skill;
    skill.casterId = 10;
    skill.targetId = 11;
    skill.skillId = 77;
    skill.casterPos = protocol::Position(1.0f, 1.0f, 1.0f);
    skill.targetPos = protocol::Position(2.0f, 2.0f, 2.0f);
    frames.push_back(protocol::MessageCodec::encode(skill, 77));

    return frames;
}

bool expect_field(const wire::FieldValue& f, std::int64_t v, const char* what) {
    if (f.integer != v) {
        std::cerr << "  FAILED: field " << what << " = " << f.integer << " want " << v
                  << std::endl;
        return false;
    }
    return true;
}

bool expect_real(const wire::FieldValue& f, double v, const char* what) {
    if (f.real != v) {
        std::cerr << "  FAILED: field " << what << " = " << f.real << " want " << v
                  << std::endl;
        return false;
    }
    return true;
}

//------------------------------------------------------------------------------

bool test_egress_forwards_five_families() {
    std::cout << "Running: test_egress_forwards_five_families..." << std::endl;

    FakeCell cell;
    TEST_ASSERT(cell.healthy(), "fake cell assembled");
    TEST_ASSERT(cell.listen_port() != 0, "fake cell listening");

    M1EgressConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = cell.listen_port();
    cfg.route_json = kRouteJson;
    auto egress = makeM1Egress(cfg);
    TEST_ASSERT(egress->start(), "egress started");
    TEST_ASSERT(egress->forwardIntent(77, {1, 2, 3}) == false, "short frame rejected");

    const auto frames = five_family_frames();
    for (const auto& f : frames) {
        TEST_ASSERT(egress->forwardIntent(77, f), "family frame accepted");
    }
    TEST_ASSERT(egress->unforwardableFrames() == 1, "short frame counted");

    TEST_ASSERT(pump_both(*egress, cell, [&] { return cell.count() == 5; }, 5000),
                "all five intents reached fake cell");
    TEST_ASSERT(egress->sentIntents() == 5, "five sent");
    TEST_ASSERT(egress->pendingAcks() == 5, "five pending");
    TEST_ASSERT(egress->roomServerConnected(), "link up");

    const auto records = cell.records();
    TEST_ASSERT(records.size() == 5, "five recorded");
    for (std::size_t i = 0; i < 5; ++i) {
        TEST_ASSERT(records[i].session_id == 77, "session passthrough");
        TEST_ASSERT(records[i].request_id == i + 1, "request_id monotonic from 1");
    }
    TEST_ASSERT(records[0].contract_id == id_of(MsgId::cell_create_entity), "create id");
    if (!expect_field(records[0].fields[2], 42, "entity") ||
        !expect_field(records[0].fields[3], 3, "entity_type") ||
        !expect_field(records[0].fields[4], 7, "space") ||
        !expect_real(records[0].fields[5], 1.5, "pos_x") ||
        !expect_real(records[0].fields[6], 2.5, "pos_y") ||
        !expect_real(records[0].fields[7], 3.5, "pos_z")) {
        return false;
    }
    TEST_ASSERT(records[1].contract_id == id_of(MsgId::cell_destroy_entity), "destroy id");
    if (!expect_field(records[1].fields[2], 99, "entity")) {
        return false;
    }
    TEST_ASSERT(records[2].contract_id == id_of(MsgId::cell_entity_move), "move id");
    if (!expect_field(records[2].fields[2], 5, "entity") ||
        !expect_real(records[2].fields[3], 1.0, "old_x") ||
        !expect_real(records[2].fields[8], 6.0, "new_z")) {
        return false;
    }
    TEST_ASSERT(records[3].contract_id == id_of(MsgId::cell_cross_border), "border id");
    if (!expect_field(records[3].fields[2], 8, "entity") ||
        !expect_field(records[3].fields[3], 1, "from") ||
        !expect_field(records[3].fields[4], 2, "to")) {
        return false;
    }
    TEST_ASSERT(records[4].contract_id == id_of(MsgId::cell_skill_cast), "skill id");
    if (!expect_field(records[4].fields[2], 10, "caster") ||
        !expect_field(records[4].fields[3], 11, "target") ||
        !expect_field(records[4].fields[4], 77, "skill")) {
        return false;
    }

    // 929 回流 → 关联回收
    TEST_ASSERT(cell.ack_all() == 5, "five acks sent");
    TEST_ASSERT(pump_both(*egress, cell, [&] { return egress->ackedIntents() == 5; }, 5000),
                "all acks resolved");
    TEST_ASSERT(egress->pendingAcks() == 0, "pending drained");
    TEST_ASSERT(egress->failedAcks() == 0, "no failed acks");

    egress->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_egress_timeout_sweep() {
    std::cout << "Running: test_egress_timeout_sweep..." << std::endl;

    FakeCell cell;
    TEST_ASSERT(cell.healthy(), "fake cell assembled");

    M1EgressConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = cell.listen_port();
    cfg.route_json = kRouteJson;
    cfg.ack_timeout_ms = 300;
    auto egress = makeM1Egress(cfg);
    TEST_ASSERT(egress->start(), "egress started");

    const auto frames = five_family_frames();
    TEST_ASSERT(egress->forwardIntent(9, frames[0]), "first accepted");
    TEST_ASSERT(egress->forwardIntent(9, frames[1]), "second accepted");
    // 送达面等真到达（链路未连时 reliable 也会受理入队，pred 只看 sent
    // 会在无连接时假绿——迟到应答用例需要真实在册 peer 回投）
    TEST_ASSERT(pump_both(*egress, cell, [&] { return cell.count() == 2; }, 5000),
                "two delivered");
    TEST_ASSERT(cell.count() == 2, "two at cell");

    // 时钟快进越过清扫线：pending 回收 + 计数
    egress->pump(now_ms() + 400);
    TEST_ASSERT(egress->timedOutIntents() == 2, "both timed out");
    TEST_ASSERT(egress->pendingAcks() == 0, "pending swept");
    TEST_ASSERT(egress->ackedIntents() == 0, "nothing acked yet");

    // 迟到应答：无挂账可消，不误账不崩
    TEST_ASSERT(cell.ack(1, 0), "late ack sent");
    pump_both(*egress, cell, [] { return false; }, 100);
    TEST_ASSERT(egress->ackedIntents() == 0, "late ack not counted");

    egress->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_egress_unforwardable_frames() {
    std::cout << "Running: test_egress_unforwardable_frames..." << std::endl;

    FakeCell cell;
    TEST_ASSERT(cell.healthy(), "fake cell assembled");

    M1EgressConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = cell.listen_port();
    cfg.route_json = kRouteJson;
    auto egress = makeM1Egress(cfg);
    TEST_ASSERT(egress->start(), "egress started");

    // 残帧 / PING / 断连通知：都不在 M1 前递面（五族白名单外）
    TEST_ASSERT(egress->forwardIntent(1, {0x01, 0x02, 0x03}) == false, "short frame");
    apollo::protocol::Ping ping;
    ping.timestamp = 1;
    TEST_ASSERT(egress->forwardIntent(1, apollo::protocol::MessageCodec::encode(ping, 1)) ==
                    false,
                "ping not forwardable");
    apollo::protocol::GatewayClientDisconnect disconnect;
    disconnect.sessionId = 1;
    disconnect.playerId = 1;
    disconnect.normalClose = true;
    TEST_ASSERT(egress->forwardIntent(
                    1, apollo::protocol::MessageCodec::encode(disconnect)) == false,
                "disconnect notify not forwardable");
    TEST_ASSERT(egress->unforwardableFrames() == 3, "three counted");

    // 对照：意图族照常前递
    const auto frames = five_family_frames();
    TEST_ASSERT(egress->forwardIntent(1, frames[0]), "family frame accepted");
    TEST_ASSERT(pump_both(*egress, cell, [&] { return cell.count() == 1; }, 5000),
                "only family frame reached cell");
    TEST_ASSERT(egress->sentIntents() == 1, "one sent");

    egress->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_egress_inbox_overflow() {
    std::cout << "Running: test_egress_inbox_overflow..." << std::endl;

    FakeCell cell;
    TEST_ASSERT(cell.healthy(), "fake cell assembled");

    M1EgressConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = cell.listen_port();
    cfg.route_json = kRouteJson;
    cfg.inbox_cap = 4;
    auto egress = makeM1Egress(cfg);
    TEST_ASSERT(egress->start(), "egress started");

    const auto frames = five_family_frames();
    // 不泵拍连续投递 6 件：4 入箱，2 溢出落地（legacy 链路死观察面同型）
    for (int i = 0; i < 6; ++i) {
        const auto accepted = egress->forwardIntent(1, frames[static_cast<std::size_t>(i % 5)]);
        TEST_ASSERT(accepted == (i < 4), "inbox cap enforced");
    }
    TEST_ASSERT(egress->unforwardableFrames() == 2, "overflow counted");

    TEST_ASSERT(pump_both(*egress, cell, [&] { return cell.count() == 4; }, 5000),
                "inbox drained");
    TEST_ASSERT(egress->sentIntents() == 4, "four sent");

    egress->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_egress_route_table_missing() {
    std::cout << "Running: test_egress_route_table_missing..." << std::endl;

    M1EgressConfig cfg;
    cfg.route_json = "";  // 缺位 = 装配降级显形（起进程不拦由调用方裁断）
    auto egress = makeM1Egress(cfg);
    TEST_ASSERT(egress->start() == false, "start fails on empty route json");
    TEST_ASSERT(egress->forwardIntent(1, {1, 2, 3}) == false, "degraded egress drops");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//------------------------------------------------------------------------------

bool test_gateway_cell_e2e() {
    std::cout << "Running: test_gateway_cell_e2e..." << std::endl;

    // 真 cell：M1 接收端（被动）+ 受理队列 + tick 执行 + 延时应答
    room_server::RoomServerConfig roomServerConfig;
    roomServerConfig.host = "127.0.0.1";
    roomServerConfig.tickRateMs = 20;
    roomServerConfig.m1Port = 0;
    roomServerConfig.m1RouteJson = kRouteJson;
    room_server::RoomServer roomServer(roomServerConfig);
    roomServer.start();
    TEST_ASSERT(roomServer.m1Link().listenPort() != 0, "RoomServer M1 listening");

    // 真 gateway：M1 出口指向 cell，入口真 TCP 监听
    gateway::GatewayConfig gwCfg;
    gwCfg.host = "127.0.0.1";
    gwCfg.port = 0;
    gwCfg.roomServerM1Host = "127.0.0.1";
    gwCfg.roomServerM1Port = roomServer.m1Link().listenPort();
    gwCfg.roomServerM1RouteJson = kRouteJson;
    gwCfg.sessionTimeoutMs = 60000;
    gwCfg.heartbeatCheckIntervalMs = 50;
    gateway::GatewayServer gw(gwCfg);
    gw.start();
    TEST_ASSERT(gw.listenPort() != 0, "gateway listening");

    SendClient client;
    TEST_ASSERT(client.connect("127.0.0.1", gw.listenPort()), "client connected");

    // 真入口会话号（registry 挂靠面读回；仅此一条连接）
    gateway::SessionID sid = 0;
    for (int i = 0; i < 100 && sid == 0; ++i) {
        for (gateway::SessionID s = 1; s <= 16; ++s) {
            if (gw.findClientEndpoint(s).has_value()) {
                sid = s;
                break;
            }
        }
        if (sid == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    TEST_ASSERT(sid != 0, "client session registered");
    TEST_ASSERT(gw.authenticateSession(sid, 42, "ticket"), "session authenticated");

    // 客户端世界帧 → link 帧 → 入口 → 分发（World 族）→ M1 出口 → cell
    apollo::protocol::CellCreateEntity create;
    create.entityId = 777;
    create.entityType = static_cast<apollo::protocol::EntityType>(3);
    create.spaceId = 1;
    create.position = apollo::protocol::Position(1.5f, 2.5f, 3.5f);
    const auto packet = apollo::protocol::MessageCodec::encode(create, sid);
    client.sendAll(frame_bytes(1, 0, packet));

    // 全链收敛：RoomServer 受理执行 → 929 回流 → 出口关联回收
    auto* egress = gw.messageRouter() ? gw.messageRouter()->egress() : nullptr;
    TEST_ASSERT(egress != nullptr, "egress wired");
    bool converged = false;
    for (int i = 0; i < 500; ++i) {
        if (roomServer.m1Link().receivedIntents() == 1 && egress->ackedIntents() == 1) {
            converged = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    TEST_ASSERT(converged, "intent executed and ack resolved");

    TEST_ASSERT(roomServer.m1Link().rejectedFrames() == 0, "no rejections");
    TEST_ASSERT(egress->sentIntents() == 1, "one sent");
    TEST_ASSERT(egress->failedAcks() == 0, "no failed acks");
    TEST_ASSERT(egress->timedOutIntents() == 0, "no timeouts");

    client.close();
    gw.stop();
    roomServer.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

} // namespace

// ---------- 932 信封 trace 传播（P3-3 语义层批） ----------

bool test_egress_wrapped_trace() {
    std::cout << "Running: test_egress_wrapped_trace..." << std::endl;

    // span 收集器（采样命中面；rate=1 由 main setenv 保证全采）
    std::mutex span_mutex;
    std::vector<trace::SpanRecord> spans;
    trace::set_span_sink([&](const trace::SpanRecord& record) {
        std::lock_guard<std::mutex> lock(span_mutex);
        spans.push_back(record);
    });

    FakeCell cell;
    TEST_ASSERT(cell.healthy(), "fake cell assembled");

    M1EgressConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = cell.listen_port();
    cfg.route_json = kRouteJson;
    auto egress = makeM1Egress(cfg);
    TEST_ASSERT(egress->start(), "egress started");

    const auto frames = five_family_frames();
    TEST_ASSERT(egress->forwardIntent(77, frames[0]), "create frame accepted");
    TEST_ASSERT(pump_both(*egress, cell, [&] { return cell.count() == 1; }, 5000),
                "wrapped intent reached fake cell");

    const auto records = cell.records();
    TEST_ASSERT(records.size() == 1, "one recorded");
    TEST_ASSERT(records[0].wrapped, "arrived as internal envelope");
    TEST_ASSERT(records[0].contract_id == id_of(MsgId::cell_create_entity), "inner family id");
    TEST_ASSERT(records[0].request_id == 1, "request_id关联不变");
    TEST_ASSERT(records[0].session_id == 77, "session passthrough");
    TEST_ASSERT(records[0].trace.has_trace(), "trace id carried");
    TEST_ASSERT(records[0].trace.sampled, "sampled flag propagated (rate=1)");

    // gateway span（根，采样命中即发射）：span_id = 信封 parent 位（发端
    // id），parent 全零，受理→发出计时有序
    std::vector<trace::SpanRecord> snapshot;
    {
        std::lock_guard<std::mutex> lock(span_mutex);
        snapshot = spans;
    }
    bool gw_span = false;
    for (const auto& s : snapshot) {
        if (s.name == "gateway_intent_forward") {
            gw_span = true;
            TEST_ASSERT(s.span_id == records[0].trace.parent_span_id,
                        "gateway span id is envelope parent");
            bool parent_zero = true;
            for (std::uint8_t b : s.ctx.parent_span_id) {
                parent_zero = parent_zero && b == 0;
            }
            TEST_ASSERT(parent_zero, "gateway span is root");
            TEST_ASSERT(s.start_ms <= s.end_ms, "span timing ordered");
        }
    }
    TEST_ASSERT(gw_span, "gateway span emitted");

    trace::set_span_sink(nullptr);
    egress->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

int main() {
    std::cout << "=== Apollo Gateway M1 Egress Test Suite ===" << std::endl;

    // 头采样 dev 覆写（logging.md §5.1 APOLLO_TRACE_SAMPLE_RATE）：rate=1
    // 全采——trace 传播断言确定性；head_sampler 首用即读
    setenv("APOLLO_TRACE_SAMPLE_RATE", "1", 1);

    int failed = 0;
    if (!test_egress_forwards_five_families()) ++failed;
    if (!test_egress_timeout_sweep()) ++failed;
    if (!test_egress_unforwardable_frames()) ++failed;
    if (!test_egress_inbox_overflow()) ++failed;
    if (!test_egress_route_table_missing()) ++failed;
    if (!test_egress_wrapped_trace()) ++failed;
    if (!test_gateway_cell_e2e()) ++failed;

    if (failed == 0) {
        std::cout << "ALL GATEWAY M1 EGRESS TESTS PASSED" << std::endl;
        return 0;
    }
    std::cout << failed << " GATEWAY M1 EGRESS TESTS FAILED" << std::endl;
    return 1;
}

#else // _WIN32

// link TCP 底座不进 M1 批（POSIX only）——Windows 下用例本体空转
int main() {
    std::cout << "=== Apollo Gateway M1 Egress Test Suite (windows no-op) ===" << std::endl;
    return 0;
}

#endif // !_WIN32
