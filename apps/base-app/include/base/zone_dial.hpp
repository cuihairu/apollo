#pragma once

// Zone 间一次性拨号会话（P0-2 路 2：933 停写 / 935 快照 / 937 交权的拨出
// 底座）。fleetctl ControlDial 同型：RouteTable + LinkManager active 端点
// + RpcClient 自泵线程——call 同步等待在调用线程（禁泵线程纪律不变）。
// 对端受理面 = M1BaseServer（passive self_name "base"，握手只验
// route_version——拨入者按自报名入对端 peer 表，RpcServer 随新链装配）。
// header-only 直编习语（app 域件，不进 game_session 模块）。

#include "base/migration_wire.hpp"

#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/link/route_table.hpp"
#include "apollo/net/linkrpc/rpc_channel.hpp"

#include <atomic>
#include <chrono>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <thread>
#include <vector>

#if APOLLO_NET_LINK_HAS_JSON

namespace base {

namespace link = apollo::net::link;

class ZoneDial {
public:
    // route_json = 契约路由表路径（与受理面同一份——域内 route_version
    // 一致才握得上）。self_name = 拨入者自报（对端按它入 peer 表）。
    // peer_name = 对端 G-1 组件名（zone 受理面 = "base"）。
    ZoneDial(const std::string& route_json, const std::string& self_name,
             const std::string& peer_name, const std::string& host,
             std::uint16_t port)
        : peer_name_(peer_name) {
        route_table_ = std::make_unique<link::RouteTable>();
        std::ifstream in(route_json, std::ios::binary);
        std::ostringstream buffer;
        if (!in || !link::load_route_table_json(
                       (buffer << in.rdbuf(), buffer.str()), *route_table_)) {
            route_table_.reset();
            return;  // 装配错误显形：ready() false，不带隐式回退
        }

        link::LinkManagerConfig mc;
        mc.self_name = self_name;
        mc.route_version = link::route_version_from_hash(route_table_->schema_hash());
        manager_ = std::make_unique<link::LinkManager>(mc);

        link::EndpointConfig ep;
        ep.name = peer_name;
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

    ~ZoneDial() { shutdown(); }

    ZoneDial(const ZoneDial&) = delete;
    ZoneDial& operator=(const ZoneDial&) = delete;

    bool ready() const { return manager_ != nullptr; }

    bool waitUp() const {
        const auto deadline = Clock::now() + kUpTimeout;
        auto* peer = manager_ ? manager_->link(peer_name_) : nullptr;
        while (peer != nullptr && Clock::now() < deadline) {
            if (peer->state() == link::LinkState::kUp) {
                return true;
            }
            std::this_thread::sleep_for(kPumpTick);
        }
        return false;
    }

    std::optional<std::vector<std::uint8_t>> call(std::vector<std::uint8_t> frame) {
        if (client_ == nullptr) {
            return std::nullopt;
        }
        return client_->call(frame, kRpcTimeout);
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

    std::string peer_name_;
    std::unique_ptr<link::RouteTable> route_table_;
    std::unique_ptr<link::LinkManager> manager_;
    std::unique_ptr<apollo::net::linkrpc::RpcClient> client_;
    std::thread pump_;
    std::atomic<bool> running_{false};
};

} // namespace base

#endif // APOLLO_NET_LINK_HAS_JSON
