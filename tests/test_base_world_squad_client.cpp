// Base→World M1 小队准入链路客户端 e2e（D11 issuer=Base 批①；契约 972-976
// 拨号侧 wire 面——World 侧受理/预留面属批②，本卷用裸被动 LinkManager
// 自扮 "world" 对端（self_name 同 m1_link_server，握手身份即按名）：
//   config_guards：host/port/base_id/路由表缺位 start false；死端口链路
//   永不 up 且出向全拒（哑管道不自动重试）；
//   roundtrip：974 出向（世界端逐字段解）/ 972 入向（回调字段全口径）/
//   975 入向（granted+预留号）/ 973+976 出向（5/11 字段世界端原样解）；
//   计数账面收支对齐；
//   bad_frame：残帧不回调，bad_frames_in 计数。
// 断言事实源：客户端账面 state() + 世界端解码落账（日志不算）。
// Windows 下 link TCP 底座不进本批，用例本体空转。
#include "base/world_squad_client.hpp"

#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/link/route_table.hpp"
#include "apollo/net/wire/contract_wire.hpp"
#include "apollo_contract.h"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifndef APOLLO_TEST_ROUTE_JSON
#define APOLLO_TEST_ROUTE_JSON ""
#endif

using base::SquadCapacityGrant;
using base::SquadValidationRequest;
using base::WorldSquadClient;
using base::WorldSquadClientConfig;
using base::WorldSquadLinkState;

namespace {

namespace link = apollo::net::link;
namespace wire = apollo::net::wire;
using apollo::contract::gen::MsgId;

#if !defined(_WIN32)

constexpr std::uint16_t id_of(MsgId id) { return static_cast<std::uint16_t>(id); }
constexpr std::uint16_t kValidationRequestId = id_of(MsgId::squad_validation_request);
constexpr std::uint16_t kValidationReplyId = id_of(MsgId::squad_validation_reply);
constexpr std::uint16_t kCapacityRequestId = id_of(MsgId::squad_capacity_request);
constexpr std::uint16_t kCapacityGrantId = id_of(MsgId::squad_capacity_grant);
constexpr std::uint16_t kCredentialBundleId = id_of(MsgId::squad_credential_bundle);

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

const char* kRouteJson = APOLLO_TEST_ROUTE_JSON;

std::uint64_t now_ms() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

// 世界端解码落账（观测面）。
struct CapacityRequestRecord {
    std::uint64_t request_id = 0;
    std::uint64_t party_id = 0;
    std::uint64_t instance_id = 0;
    std::uint64_t cell_id = 0;
    std::string roster_hash;
    std::int32_t slots = 0;
};

struct ValidationReplyRecord {
    std::uint64_t request_id = 0;
    bool accepted = false;
    std::string roster_hash;
    std::string failed_members;
    std::int32_t code = 0;
};

struct CredentialRecord {
    std::uint64_t request_id = 0;
    std::uint64_t reservation_id = 0;
    std::uint64_t credential_id = 0;
    std::uint64_t player_id = 0;
    std::uint64_t squad_id = 0;
    std::uint64_t instance_id = 0;
    std::uint64_t cell_id = 0;
    std::string roster_hash;
    std::uint64_t issued_at = 0;
    std::uint64_t expires_at = 0;
    std::string issuer;
};

struct WorldSideState {
    std::mutex mutex;
    std::vector<CapacityRequestRecord> capacity_requests;
    std::vector<ValidationReplyRecord> validation_replies;
    std::vector<CredentialRecord> credentials;
};

// 裸被动 LinkManager 自扮 world（self_name 同 m1_link_server；握手身份按
// 名——批② World 侧受理面接前后，本壳只验 wire 面双向）。
struct RawWorldEnd {
    std::unique_ptr<link::RouteTable> table;
    std::shared_ptr<link::LinkManager> manager;
    WorldSideState state;
    std::uint16_t port = 0;

    bool start() {
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
        mc.self_name = "world";
        mc.route_version = link::route_version_from_hash(table->schema_hash());
        manager = std::make_shared<link::LinkManager>(mc);

        link::EndpointConfig ep;
        ep.name = "gateway";  // 本壳端点名无寻路义（被动端按对端自报名回链）
        ep.host = "127.0.0.1";
        ep.port = 0;  // 内核分配，endpoint_port 读回
        ep.role = link::EndpointRole::kPassive;
        if (!manager->add_endpoint(ep)) {
            return false;
        }
        manager->set_peer_listener([this](link::InterServerLink& peer) {
            auto& dispatcher = peer.dispatcher();
            dispatcher.set_route_table(table.get());
            dispatcher.allow_domain("internal");
            dispatcher.set_handler(kCapacityRequestId,
                                   [this](std::uint16_t, const std::uint8_t* data,
                                          std::size_t size) {
                                       handle_capacity_request(data, size);
                                   });
            dispatcher.set_handler(kValidationReplyId,
                                   [this](std::uint16_t, const std::uint8_t* data,
                                          std::size_t size) {
                                       handle_validation_reply(data, size);
                                   });
            dispatcher.set_handler(kCredentialBundleId,
                                   [this](std::uint16_t, const std::uint8_t* data,
                                          std::size_t size) {
                                       handle_credential(data, size);
                                   });
        });
        port = manager->endpoint_port("gateway");
        return port != 0;
    }

    void pump() {
        if (manager) {
            manager->pump(now_ms());
        }
    }

    // 客户端侧 linkUp 先于世界端快照显 up（握手收尾不对称）——世界端发帧
    // 前等自己的快照出现 up 对端。
    bool wait_peer_up() {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (std::chrono::steady_clock::now() < deadline) {
            for (const auto& snapshot : manager->snapshot_links()) {
                if (snapshot.state == "up") {
                    return true;
                }
            }
            pump();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return false;
    }

    bool send_to_client(std::uint16_t msg_id,
                        const std::vector<wire::FieldValue>& fields) {
        // 遍历在线对端（passive 侧 snapshot.peer = 对端自报名），按名取链。
        for (const auto& snapshot : manager->snapshot_links()) {
            if (snapshot.state != "up") {
                continue;
            }
            auto* l = manager->link(snapshot.peer);
            if (l == nullptr || l->state() != link::LinkState::kUp) {
                continue;
            }
            std::vector<std::uint8_t> payload;
            if (!wire::encode_payload(msg_id, fields, payload)) {
                return false;
            }
            if (l->send(msg_id, payload.data(), payload.size(),
                        link::SendClass::kReliable) == link::SendCode::kAccepted) {
                return true;
            }
        }
        return false;
    }

    void handle_capacity_request(const std::uint8_t* data, std::size_t size) {
        std::vector<wire::FieldValue> fields;
        if (!wire::decode_payload(kCapacityRequestId, data, size, fields)
            || fields.size() != 6) {
            return;
        }
        CapacityRequestRecord record;
        record.request_id = static_cast<std::uint64_t>(fields[0].integer);
        record.party_id = static_cast<std::uint64_t>(fields[1].integer);
        record.instance_id = static_cast<std::uint64_t>(fields[2].integer);
        record.cell_id = static_cast<std::uint64_t>(fields[3].integer);
        record.roster_hash = fields[4].text;
        record.slots = static_cast<std::int32_t>(fields[5].integer);
        std::lock_guard<std::mutex> lock(state.mutex);
        state.capacity_requests.push_back(record);
    }

    void handle_validation_reply(const std::uint8_t* data, std::size_t size) {
        std::vector<wire::FieldValue> fields;
        if (!wire::decode_payload(kValidationReplyId, data, size, fields)
            || fields.size() != 5) {
            return;
        }
        ValidationReplyRecord record;
        record.request_id = static_cast<std::uint64_t>(fields[0].integer);
        record.accepted = fields[1].integer != 0;
        record.roster_hash = fields[2].text;
        record.failed_members = fields[3].text;
        record.code = static_cast<std::int32_t>(fields[4].integer);
        std::lock_guard<std::mutex> lock(state.mutex);
        state.validation_replies.push_back(record);
    }

    void handle_credential(const std::uint8_t* data, std::size_t size) {
        std::vector<wire::FieldValue> fields;
        if (!wire::decode_payload(kCredentialBundleId, data, size, fields)
            || fields.size() != 11) {
            return;
        }
        CredentialRecord record;
        record.request_id = static_cast<std::uint64_t>(fields[0].integer);
        record.reservation_id = static_cast<std::uint64_t>(fields[1].integer);
        record.credential_id = static_cast<std::uint64_t>(fields[2].integer);
        record.player_id = static_cast<std::uint64_t>(fields[3].integer);
        record.squad_id = static_cast<std::uint64_t>(fields[4].integer);
        record.instance_id = static_cast<std::uint64_t>(fields[5].integer);
        record.cell_id = static_cast<std::uint64_t>(fields[6].integer);
        record.roster_hash = fields[7].text;
        record.issued_at = static_cast<std::uint64_t>(fields[8].integer);
        record.expires_at = static_cast<std::uint64_t>(fields[9].integer);
        record.issuer = fields[10].text;
        std::lock_guard<std::mutex> lock(state.mutex);
        state.credentials.push_back(record);
    }
};

bool wait_link_up(WorldSquadClient& client, RawWorldEnd* world = nullptr) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!client.linkUp()) {
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }
        client.pump(now_ms());
        if (world != nullptr) {
            world->pump();  // 被动端壳无自泵线程（m1_link_server 内置，裸壳须宿主拍）
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return true;
}

bool wait_true(const std::function<bool()>& predicate, WorldSquadClient* client = nullptr,
               RawWorldEnd* world = nullptr) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!predicate()) {
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }
        if (client != nullptr) {
            client->pump(now_ms());  // 入向分派在泵内回调
        }
        if (world != nullptr) {
            world->pump();  // 被动端收发同需泵拍
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return true;
}

bool test_config_guards() {
    WorldSquadClientConfig config;
    config.route_json = kRouteJson;
    config.base_id = 7;
    // host 缺位拒起。
    {
        auto client = base::makeWorldSquadClient(config);
        TEST_ASSERT(!client->start(), "host 缺位 start 须 false");
    }
    // port 缺位拒起。
    {
        WorldSquadClientConfig c = config;
        c.world_host = "127.0.0.1";
        TEST_ASSERT(!c.world_port, "缺省 port 须为 0");
        auto client = base::makeWorldSquadClient(c);
        TEST_ASSERT(!client->start(), "port 缺位 start 须 false");
    }
    // base_id 缺位拒起（无身份不拨）。
    {
        WorldSquadClientConfig c = config;
        c.world_host = "127.0.0.1";
        c.world_port = 1;  // 回环 port 1 恒拒连（死端口）
        c.base_id = 0;
        auto client = base::makeWorldSquadClient(c);
        TEST_ASSERT(!client->start(), "base_id 缺位 start 须 false");
    }
    // 路由表不可读拒起。
    {
        WorldSquadClientConfig c = config;
        c.world_host = "127.0.0.1";
        c.world_port = 1;
        c.base_id = 7;
        c.route_json = "/nonexistent/apollo_route_test.json";
        auto client = base::makeWorldSquadClient(c);
        TEST_ASSERT(!client->start(), "路由表不可读 start 须 false");
    }
    // 死端口：start true 但链路永不 up；出向全拒、账面零收支。
    {
        WorldSquadClientConfig c = config;
        c.world_host = "127.0.0.1";
        c.world_port = 1;
        c.base_id = 7;
        auto client = base::makeWorldSquadClient(c);
        TEST_ASSERT(client->start(), "死端口 start 须 true（拨装成功，连不通是链路态）");
        for (int i = 0; i < 10; ++i) {
            client->pump(now_ms());
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        TEST_ASSERT(!client->linkUp(), "死端口链路须永不 up");
        TEST_ASSERT(!client->send_capacity_request(1, 2, 3, 4, "h", 2),
                    "链路未 up 出向拒发");
        TEST_ASSERT(!client->send_validation_reply(1, true, "h", "", 0),
                    "链路未 up 回执拒发");
        TEST_ASSERT(!client->send_credential_bundle(1, 2, 3, 4, 5, 6, 7, "h", 8, 9, "i"),
                    "链路未 up 凭证拒发");
        const auto state = client->state();
        TEST_ASSERT(state.capacity_requests_out == 0, "拒发不得记账");
        TEST_ASSERT(state.validation_requests_in == 0, "死端口无入向");
    }
    std::cout << "  config_guards ok" << std::endl;
    return true;
}

bool test_roundtrip() {
    RawWorldEnd world;
    TEST_ASSERT(world.start(), "裸 world 壳须起（内核分配端口）");

    WorldSquadClientConfig config;
    config.world_host = "127.0.0.1";
    config.world_port = world.port;
    config.base_id = 42;
    config.route_json = kRouteJson;
    auto client = base::makeWorldSquadClient(config);

    SquadValidationRequest seen_request;
    bool request_seen = false;
    client->set_validation_handler([&](const SquadValidationRequest& request) {
        seen_request = request;
        request_seen = true;
    });
    SquadCapacityGrant seen_grant;
    bool grant_seen = false;
    client->set_grant_handler([&](const SquadCapacityGrant& grant) {
        seen_grant = grant;
        grant_seen = true;
    });

    TEST_ASSERT(client->start(), "合法配置 start 须 true");
    TEST_ASSERT(wait_link_up(*client, &world), "回环拨装须 up");

    // 出向 974：世界端逐字段解。
    TEST_ASSERT(client->send_capacity_request(11, 500, 9001, 21,
                                              "roster-hash-abc", 3),
                "链路 up 后 974 须受理");
    TEST_ASSERT(wait_true([&] {
        std::lock_guard<std::mutex> lock(world.state.mutex);
        return !world.state.capacity_requests.empty();
    }, client.get(), &world), "世界端须收到 974");
    {
        std::lock_guard<std::mutex> lock(world.state.mutex);
        const auto& record = world.state.capacity_requests.back();
        TEST_ASSERT(record.request_id == 11, "974 request_id 保真");
        TEST_ASSERT(record.party_id == 500, "974 party_id 保真");
        TEST_ASSERT(record.instance_id == 9001, "974 instance_id 保真");
        TEST_ASSERT(record.cell_id == 21, "974 cell_id 保真");
        TEST_ASSERT(record.roster_hash == "roster-hash-abc", "974 roster_hash 保真");
        TEST_ASSERT(record.slots == 3, "974 slots 保真");
    }

    // 入向 972：回调字段全口径。
    {
        std::vector<wire::FieldValue> fields(5);
        fields[0].integer = 77;
        fields[1].integer = 500;
        fields[2].integer = 9001;
        fields[3].integer = 21;
        fields[4].text = "1001,1002,1003";
        TEST_ASSERT(world.send_to_client(kValidationRequestId, fields),
                    "世界端 972 下发须受理");
    }
    TEST_ASSERT(wait_true([&] { return request_seen; }, client.get(), &world), "972 回调须显形");
    TEST_ASSERT(seen_request.request_id == 77, "972 request_id 保真");
    TEST_ASSERT(seen_request.party_id == 500, "972 party_id 保真");
    TEST_ASSERT(seen_request.instance_id == 9001, "972 instance_id 保真");
    TEST_ASSERT(seen_request.cell_id == 21, "972 cell_id 保真");
    TEST_ASSERT(seen_request.members == "1001,1002,1003", "972 members 保真");

    // 入向 975：granted + 预留号 + 过期窗。
    {
        std::vector<wire::FieldValue> fields(5);
        fields[0].integer = 11;
        fields[1].integer = 1;
        fields[2].integer = 4242;
        fields[3].integer = 123456;
        fields[4].integer = 0;
        TEST_ASSERT(world.send_to_client(kCapacityGrantId, fields),
                    "世界端 975 下发须受理");
    }
    TEST_ASSERT(wait_true([&] { return grant_seen; }, client.get(), &world), "975 回调须显形");
    TEST_ASSERT(seen_grant.granted, "975 granted 保真");
    TEST_ASSERT(seen_grant.reservation_id == 4242, "975 reservation_id 保真");
    TEST_ASSERT(seen_grant.expires_at == 123456, "975 expires_at 保真");

    // 出向 973 + 976：世界端原样解。
    TEST_ASSERT(client->send_validation_reply(77, true, "roster-hash-abc", "", 0),
                "973 须受理");
    TEST_ASSERT(wait_true([&] {
        std::lock_guard<std::mutex> lock(world.state.mutex);
        return !world.state.validation_replies.empty();
    }, client.get(), &world), "世界端须收到 973");
    {
        std::lock_guard<std::mutex> lock(world.state.mutex);
        const auto& record = world.state.validation_replies.back();
        TEST_ASSERT(record.request_id == 77, "973 request_id 保真");
        TEST_ASSERT(record.accepted, "973 accepted 保真");
        TEST_ASSERT(record.roster_hash == "roster-hash-abc", "973 roster_hash 保真");
    }
    TEST_ASSERT(client->send_credential_bundle(11, 4242, 90001, 1001, 500, 9001,
                                               21, "roster-hash-abc", 1000, 2000,
                                               "base:42"),
                "976 须受理");
    TEST_ASSERT(wait_true([&] {
        std::lock_guard<std::mutex> lock(world.state.mutex);
        return !world.state.credentials.empty();
    }, client.get(), &world), "世界端须收到 976");
    {
        std::lock_guard<std::mutex> lock(world.state.mutex);
        const auto& record = world.state.credentials.back();
        TEST_ASSERT(record.request_id == 11, "976 request_id 保真");
        TEST_ASSERT(record.reservation_id == 4242, "976 reservation_id 保真");
        TEST_ASSERT(record.credential_id == 90001, "976 credential_id 保真");
        TEST_ASSERT(record.player_id == 1001, "976 player_id 保真");
        TEST_ASSERT(record.squad_id == 500, "976 squad_id 保真");
        TEST_ASSERT(record.instance_id == 9001, "976 instance_id 保真");
        TEST_ASSERT(record.cell_id == 21, "976 cell_id 保真");
        TEST_ASSERT(record.roster_hash == "roster-hash-abc", "976 roster_hash 保真");
        TEST_ASSERT(record.issued_at == 1000, "976 issued_at 保真");
        TEST_ASSERT(record.expires_at == 2000, "976 expires_at 保真");
        TEST_ASSERT(record.issuer == "base:42", "976 issuer 保真");
    }

    // 账面收支对齐。
    const auto state = client->state();
    TEST_ASSERT(state.validation_requests_in == 1, "972 收支记账 1");
    TEST_ASSERT(state.capacity_grants_in == 1, "975 收支记账 1");
    TEST_ASSERT(state.capacity_requests_out == 1, "974 发送记账 1");
    TEST_ASSERT(state.validation_replies_out == 1, "973 发送记账 1");
    TEST_ASSERT(state.credentials_out == 1, "976 发送记账 1");
    TEST_ASSERT(state.bad_frames_in == 0, "完好帧不得计残帧");

    client->stop();
    std::cout << "  roundtrip ok" << std::endl;
    return true;
}

bool test_bad_frame() {
    RawWorldEnd world;
    TEST_ASSERT(world.start(), "裸 world 壳须起");

    WorldSquadClientConfig config;
    config.world_host = "127.0.0.1";
    config.world_port = world.port;
    config.base_id = 9;
    config.route_json = kRouteJson;
    auto client = base::makeWorldSquadClient(config);

    bool grant_seen = false;
    client->set_grant_handler([&](const SquadCapacityGrant&) { grant_seen = true; });
    TEST_ASSERT(client->start(), "start 须 true");
    TEST_ASSERT(wait_link_up(*client, &world), "回环拨装须 up");

    // 残帧（975 截去尾字段）不回调、计残帧——encode 按契约 schema 校验
    // 字段数，残帧须先合法编码再手工截尾。
    {
        TEST_ASSERT(world.wait_peer_up(), "世界端快照须显 up 对端");
        std::vector<wire::FieldValue> fields(5);
        fields[0].integer = 1;
        fields[1].integer = 0;
        fields[2].integer = 7;
        fields[3].integer = 8;
        fields[4].integer = 9;
        auto* l = world.manager->link("base-squad-9");
        TEST_ASSERT(l != nullptr && l->state() == link::LinkState::kUp, "对端链须 up");
        std::vector<std::uint8_t> payload;
        TEST_ASSERT(wire::encode_payload(kCapacityGrantId, fields, payload),
                    "完整 975 编码须过");
        TEST_ASSERT(payload.size() > 4, "载荷须有可截余量");
        payload.resize(payload.size() - 4);  // 截去尾字段字节（合法帧 → 残帧）
        TEST_ASSERT(l->send(kCapacityGrantId, payload.data(), payload.size(),
                            link::SendClass::kReliable)
                        == link::SendCode::kAccepted,
                    "残帧下发须受理");
    }
    // 泵几拍让残帧到端（两侧同泵：世界端出队、客户端入向分派）。
    for (int i = 0; i < 20 && !grant_seen; ++i) {
        client->pump(now_ms());
        world.pump();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    TEST_ASSERT(!grant_seen, "残帧不得触发回调");
    TEST_ASSERT(client->state().bad_frames_in == 1, "残帧须计 bad_frames_in");
    TEST_ASSERT(client->state().capacity_grants_in == 0, "残帧不得计有效收帧");

    client->stop();
    std::cout << "  bad_frame ok" << std::endl;
    return true;
}

} // namespace

int main() {
    std::cout << "test_base_world_squad_client:" << std::endl;
    if (!test_config_guards()) ++g_failures;
    if (!test_roundtrip()) ++g_failures;
    if (!test_bad_frame()) ++g_failures;
    if (g_failures == 0) {
        std::cout << "ALL PASS" << std::endl;
        return 0;
    }
    std::cout << g_failures << " case(s) FAILED" << std::endl;
    return 1;
}

#else // _WIN32

int main() {
    std::cout << "test_base_world_squad_client: skipped (link TCP 底座 POSIX only)"
              << std::endl;
    return 0;
}

#endif // !_WIN32
