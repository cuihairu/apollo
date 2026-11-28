// 编队控制面探针（D31 最终拓扑演练批）：ops 侧裸帧客户端两mode——
// 954/955 gateway 排空控制（drain）与 963/966 world 缩容命令（cell_retire）。
// 客户端投影同 test_gateway_drain.cpp OpsEnd / test_login_chain_e2e.cpp
// WorldControlClient（active 拨 + 自泵线程 + request_id 收账），独立成
// 可执行供 python 演练外部驱动真进程控制面。
//
// 用法：
//   ops_probe --probe-gateway-drain <port> <op> [deadline_s]
//     打印 GW_DRAIN ok=<0|1> state=<n> active=<n> detail=<s>
//     rc 0 = ok / 2 = 拒或无回执
//   ops_probe --probe-cell-retire <port> <op> <cell_id>
//     打印 CELL_RETIRE accepted=<0|1>
//     rc 0 = accepted / 2 = 拒或无回执
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/wire/contract_wire.hpp"
#include "apollo_contract.h"

#ifndef APOLLO_OPS_PROBE_ROUTE_JSON
#define APOLLO_OPS_PROBE_ROUTE_JSON "contract_route.json"
#endif

namespace netlink = apollo::net::link;
namespace netwire = apollo::net::wire;

using apollo::contract::gen::MsgId;

constexpr std::uint16_t id_of(MsgId id) {
    return static_cast<std::uint16_t>(id);
}

static std::uint64_t now_ms() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

// ops 控制端：active 拨对端（gateway 排空面 / world 管理面），自泵收回执。
struct OpsEnd {
    netlink::RouteTable table;
    std::unique_ptr<netlink::LinkManager> manager;
    bool has_reply = false;
    std::vector<netwire::FieldValue> reply_fields;

    bool start(const char* peer, std::uint16_t port) {
        std::ifstream in(APOLLO_OPS_PROBE_ROUTE_JSON, std::ios::binary);
        if (!in) {
            std::cerr << "route json open failed: "
                      << APOLLO_OPS_PROBE_ROUTE_JSON << "\n";
            return false;
        }
        std::ostringstream buffer;
        buffer << in.rdbuf();
        if (!netlink::load_route_table_json(buffer.str(), table)) {
            std::cerr << "route table load failed\n";
            return false;
        }
        netlink::LinkManagerConfig mc;
        mc.self_name = "ops-probe";
        mc.route_version = netlink::route_version_from_hash(table.schema_hash());
        manager = std::make_unique<netlink::LinkManager>(mc);
        netlink::EndpointConfig ep;
        ep.name = peer;
        ep.host = "127.0.0.1";
        ep.port = port;
        ep.role = netlink::EndpointRole::kActive;
        if (!manager->add_endpoint(ep)) {
            return false;
        }
        auto& dispatcher = manager->link(peer)->dispatcher();
        dispatcher.set_route_table(&table);
        dispatcher.allow_domain("internal");
        return true;
    }

    bool wait_up(const char* peer, int timeout_ms) {
        const auto deadline = std::chrono::steady_clock::now()
                              + std::chrono::milliseconds(timeout_ms);
        while (std::chrono::steady_clock::now() < deadline) {
            manager->pump(now_ms());
            if (manager->link(peer)->state() == netlink::LinkState::kUp) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return false;
    }

    bool send(const char* peer, std::uint16_t msg_id,
              const std::vector<netwire::FieldValue>& fields) {
        std::vector<std::uint8_t> payload;
        if (!netwire::encode_payload(msg_id, fields, payload)) {
            return false;
        }
        auto* peer_link = manager->link(peer);
        return peer_link->send(msg_id, payload.data(), payload.size(),
                               netlink::SendClass::kReliable)
               == netlink::SendCode::kAccepted;
    }

    // 收账面先于 send 挂（send 后同线程泵驱动接收，无窗内丢帧面）。
    void arm_reply(const char* peer, std::uint16_t msg_id) {
        has_reply = false;
        auto& dispatcher = manager->link(peer)->dispatcher();
        dispatcher.set_handler(
            msg_id, [this, msg_id](std::uint16_t, const std::uint8_t* data,
                                   std::size_t size) {
                std::vector<netwire::FieldValue> fields;
                if (!netwire::decode_payload(msg_id, data, size, fields)) {
                    return;
                }
                reply_fields = std::move(fields);
                has_reply = true;
            });
    }

    // 泵等一帧回执（fields 存 reply_fields）。
    bool wait_reply(const char* peer, int timeout_ms) {
        const auto deadline = std::chrono::steady_clock::now()
                              + std::chrono::milliseconds(timeout_ms);
        while (std::chrono::steady_clock::now() < deadline) {
            manager->pump(now_ms());
            if (has_reply) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        manager->pump(now_ms());
        return has_reply;
    }
};

int main(int argc, char** argv) {
    if (argc >= 5 && std::string(argv[1]) == "--probe-gateway-drain") {
        // 954 gateway_drain_request（裸帧——控制编排族不走 932 信封）→
        // 955 回执 fields [request_id, ?, ok, state, active, detail]。
        const std::uint16_t port =
            static_cast<std::uint16_t>(std::stoul(argv[2]));
        const std::int32_t op = std::stoi(argv[3]);
        const std::uint64_t deadline_s =
            argc >= 6 ? std::stoull(argv[5]) : 0;
        OpsEnd ops;
        if (!ops.start("gateway", port) || !ops.wait_up("gateway", 5000)) {
            std::cout << "GW_DRAIN ok=0 state=0 active=0 detail=dial failed\n";
            return 2;
        }
        std::vector<netwire::FieldValue> f(3);
        f[0].integer = 1;
        f[1].integer = op;
        f[2].integer = static_cast<std::int64_t>(deadline_s);
        ops.arm_reply("gateway", id_of(MsgId::gateway_drain_reply));
        if (!ops.send("gateway", id_of(MsgId::gateway_drain_request), f)
            || !ops.wait_reply("gateway", 5000)
            || ops.reply_fields.size() < 6) {
            std::cout << "GW_DRAIN ok=0 state=0 active=0 detail=no reply\n";
            return 2;
        }
        const bool ok = ops.reply_fields[2].integer != 0;
        std::cout << "GW_DRAIN ok=" << (ok ? 1 : 0)
                  << " state=" << ops.reply_fields[3].integer
                  << " active=" << ops.reply_fields[4].integer << " detail="
                  << ops.reply_fields[5].text << "\n";
        return ok ? 0 : 2;
    }
    if (argc == 5 && std::string(argv[1]) == "--probe-cell-retire") {
        // 963 cell_retire_request → 966 回执 fields [request_id, accepted,
        // ...]（test_login_chain_e2e WorldControlClient 同口径）。
        const std::uint16_t port =
            static_cast<std::uint16_t>(std::stoul(argv[2]));
        const std::int32_t op = std::stoi(argv[3]);
        const std::uint64_t cell_id = std::stoull(argv[4]);
        OpsEnd ops;
        if (!ops.start("world", port) || !ops.wait_up("world", 5000)) {
            std::cout << "CELL_RETIRE accepted=0\n";
            return 2;
        }
        std::vector<netwire::FieldValue> f(3);
        f[0].integer = 1;
        f[1].integer = op;
        f[2].integer = static_cast<std::int64_t>(cell_id);
        ops.arm_reply("world", id_of(MsgId::cell_retire_reply));
        if (!ops.send("world", id_of(MsgId::cell_retire_request), f)
            || !ops.wait_reply("world", 5000)
            || ops.reply_fields.size() < 2) {
            std::cout << "CELL_RETIRE accepted=0\n";
            return 2;
        }
        const bool accepted = ops.reply_fields[1].integer != 0;
        std::cout << "CELL_RETIRE accepted=" << (accepted ? 1 : 0)
                  << " code=" << ops.reply_fields[2].integer << " detail="
                  << (ops.reply_fields.size() >= 6 ? ops.reply_fields[5].text
                                                   : "")
                  << "\n";
        return accepted ? 0 : 2;
    }
    std::cerr
        << "usage: ops_probe --probe-gateway-drain <port> <op> [deadline_s]\n"
           "                 ops_probe --probe-cell-retire <port> <op> "
           "<cell_id>\n";
    return 1;
}
