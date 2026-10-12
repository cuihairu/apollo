#pragma once

// 迁移快照拉取客户端（P0-2 路 2 五步②：新 zone → 老 zone，契约 935/936）。
// fleetctl ControlDial 同型一次性拨号：RouteTable + LinkManager active 端点
// + RpcClient 自泵线程——call 同步等待在调用线程（禁泵线程纪律不变）。
// 老 zone 受理面 = M1BaseServer（passive self_name "base"，握手只验
// route_version——拨入者按自报名入对端 peer 表，M1 面 RpcServer 随新链装配）。
// header-only 直编习语（app 域件，不进 game_session 模块）。

#include "base/migration_wire.hpp"

#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/link/route_table.hpp"
#include "apollo/net/linkrpc/rpc_channel.hpp"

#include <atomic>
#include <chrono>
#include <fstream>
#include <sstream>
#include <thread>

#if APOLLO_NET_LINK_HAS_JSON

namespace base {
namespace migration {

namespace link = apollo::net::link;

class MigrationPuller {
public:
    // route_json = 契约路由表路径（与受理面同一份——域内 route_version
    // 一致才握得上）。host/port = 老 zone M1 受理面地址（937 载荷携带）。
    MigrationPuller(const std::string& route_json, const std::string& host,
                    std::uint16_t port) {
        route_table_ = std::make_unique<link::RouteTable>();
        std::ifstream in(route_json, std::ios::binary);
        std::ostringstream buffer;
        if (!in || !link::load_route_table_json(
                       (buffer << in.rdbuf(), buffer.str()), *route_table_)) {
            route_table_.reset();
            return;  // 装配错误显形：ready() false，不带隐式回退
        }

        link::LinkManagerConfig mc;
        mc.self_name = "base-migration";  // 拨入者按自报名入对端表
        mc.route_version = link::route_version_from_hash(route_table_->schema_hash());
        manager_ = std::make_unique<link::LinkManager>(mc);

        link::EndpointConfig ep;
        ep.name = "base";  // 对端 G-1 组件名（M1BaseServer self_name）
        ep.host = host;
        ep.port = port;
        ep.role = link::EndpointRole::kActive;
        if (!manager_->add_endpoint(ep)) {
            manager_.reset();
            route_table_.reset();
            return;
        }

        auto* peer = manager_->link(ep.name);
        peer->dispatcher().set_route_table(route_table_.get());
        peer->dispatcher().allow_domain("internal");  // RPC 信封族入闸
        client_ = std::make_unique<apollo::net::linkrpc::RpcClient>(*peer);

        running_ = true;
        pump_ = std::thread([this]() {
            while (running_.load(std::memory_order_acquire)) {
                manager_->pump(now_ms());
                std::this_thread::sleep_for(kPumpTick);
            }
        });
    }

    ~MigrationPuller() { shutdown(); }

    MigrationPuller(const MigrationPuller&) = delete;
    MigrationPuller& operator=(const MigrationPuller&) = delete;

    bool ready() const { return manager_ != nullptr; }

    bool waitUp() const {
        const auto deadline = Clock::now() + kUpTimeout;
        auto* peer = manager_ ? manager_->link("base") : nullptr;
        while (peer != nullptr && Clock::now() < deadline) {
            if (peer->state() == link::LinkState::kUp) {
                return true;
            }
            std::this_thread::sleep_for(kPumpTick);
        }
        return false;
    }

    // 935 → 936：快照拉取（解码/超时/断链统一收为 success=false）
    SnapshotData pull(std::uint64_t player_id, std::uint64_t request_id) {
        SnapshotData out;
        if (client_ == nullptr) {
            out.error_message = "puller not ready";
            return out;
        }
        SnapshotRequest req;
        req.player_id = player_id;
        req.request_id = request_id;
        auto reply = client_->call(encodeFrame(req), kRpcTimeout);
        if (!reply) {
            out.error_message = "snapshot request timed out";
            return out;
        }
        SnapshotReply msg;
        protocol::SessionID session = 0;
        if (!decodeFrame(*reply, msg, session)) {
            out.error_message = "snapshot reply undecodable";
            return out;
        }
        out.success = msg.success;
        out.error_message = msg.error_message;
        out.profile_json = std::move(msg.profile_json);
        out.session_id = msg.session_id;
        out.gateway_id = msg.gateway_id;
        out.home_zone_id = msg.home_zone_id;
        return out;
    }

    void shutdown() {
        if (!running_.exchange(false, std::memory_order_acq_rel)) {
            return;
        }
        if (pump_.joinable()) {
            pump_.join();
        }
        client_.reset();
        manager_.reset();
        route_table_.reset();
    }

private:
    using Clock = std::chrono::steady_clock;

    static constexpr std::chrono::milliseconds kPumpTick{10};
    static constexpr std::chrono::seconds kUpTimeout{3};
    static constexpr std::chrono::milliseconds kRpcTimeout{3000};

    static std::uint64_t now_ms() {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                Clock::now().time_since_epoch()).count());
    }

    std::unique_ptr<link::RouteTable> route_table_;
    std::unique_ptr<link::LinkManager> manager_;
    std::unique_ptr<apollo::net::linkrpc::RpcClient> client_;
    std::thread pump_;
    std::atomic<bool> running_{false};
};

} // namespace migration
} // namespace base

#endif // APOLLO_NET_LINK_HAS_JSON
