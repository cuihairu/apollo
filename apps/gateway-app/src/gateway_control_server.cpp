#include "gateway/gateway_control_server.hpp"

#if !defined(_WIN32)

// Gateway 排空控制受理面真实现（P4 排空批）：InterServerLink passive 端点 +
// 契约 wire 解包 + 955 现场回带。机制零内部线程——泵由宿主（GatewayServer
// 泵线程）驱动，on_request 投递与应答都发生在宿主泵线程内（排空状态机
// 单线程独占，无锁）。
//
// 受理面纪律：954 裸帧受理（952 同型——控制编排族不走 932 信封）；
// request_id = 0 保留（回执关联键缺位即静默丢弃，不计 handler 拒收面）。

#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/wire/contract_wire.hpp"
#include "apollo_contract.h"

#include <atomic>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

#endif // !_WIN32

namespace gateway {

#if !defined(_WIN32)

namespace {

namespace link = apollo::net::link;
namespace wire = apollo::net::wire;

using apollo::contract::gen::MsgId;

constexpr std::uint16_t id_of(MsgId id) {
    return static_cast<std::uint16_t>(id);
}

class GatewayControlServerImpl final : public GatewayControlServer {
public:
    GatewayControlServerImpl(
        GatewayControlConfig config,
        std::function<DrainStatus(const DrainControlRequest&)> on_request)
        : config_(std::move(config)), on_request_(std::move(on_request)) {}

    bool start() override {
        if (started_.load(std::memory_order_acquire)) {
            return true;
        }
        route_table_ = std::make_unique<link::RouteTable>();
        if (config_.route_json.empty()) {
            return false;  // 装配错误显形（空路径 = 拒起，不带隐式回退）
        }
        std::ifstream in(config_.route_json, std::ios::binary);
        if (!in) {
            return false;
        }
        std::ostringstream buffer;
        buffer << in.rdbuf();
#if APOLLO_NET_LINK_HAS_JSON
        if (!link::load_route_table_json(buffer.str(), *route_table_)) {
            return false;
        }
#else
        return false;
#endif

        link::LinkManagerConfig mc;
        mc.self_name = "gateway";
        mc.route_version = link::route_version_from_hash(route_table_->schema_hash());
        mc.compat_route_versions = link::compat_route_versions_from_env();
        manager_ = std::make_unique<link::LinkManager>(mc);

        link::EndpointConfig ep;
        ep.name = "ops";  // 端点槽名（被动面链路按拨入方 self_name 建键）
        ep.host = config_.host;
        ep.port = config_.port;
        ep.role = link::EndpointRole::kPassive;
        if (!manager_->add_endpoint(ep)) {
            manager_.reset();
            route_table_.reset();
            return false;
        }

        // handler/表/白名单随新 peer 上线装配（链路对象在 peer_listener
        // 回调后地址稳定，重连 = 同名新链重新过此装配）。
        manager_->set_peer_listener([this](link::InterServerLink& peer) {
            install_dispatching(peer);
        });

        started_.store(true, std::memory_order_release);
        return true;
    }

    void stop() override {
        started_.store(false, std::memory_order_release);
        manager_.reset();  // 监听器与链路随管理器析构关闭
    }

    void pump(std::uint64_t now_ms) override {
        if (manager_) {
            manager_->pump(now_ms);
        }
    }

    std::uint16_t listenPort() const override {
        return manager_ ? manager_->endpoint_port("ops") : 0;
    }

private:
    void install_dispatching(link::InterServerLink& peer) {
        auto& dispatcher = peer.dispatcher();
        dispatcher.set_route_table(route_table_.get());
        dispatcher.allow_domain("internal");  // §11.3 上行白名单：控制编排族入闸
        // 954 裸帧受理 + 955 现场回带（受理点同步执行——排空状态机归宿主
        // 泵线程，快照即查即答，无延时面）。
        dispatcher.set_handler(
            id_of(MsgId::gateway_drain_request),
            [&peer, this](std::uint16_t, const std::uint8_t* data, std::size_t size) {
                handle_request_frame(peer, data, size);
            });
    }

    void handle_request_frame(link::InterServerLink& peer, const std::uint8_t* data,
                              std::size_t size) {
        std::vector<wire::FieldValue> fields;
        if (!wire::decode_payload(id_of(MsgId::gateway_drain_request), data, size, fields)
            || fields.size() != 3 || on_request_ == nullptr) {
            return;
        }
        DrainControlRequest req;
        req.request_id = static_cast<std::uint64_t>(fields[0].integer);
        req.op = static_cast<std::int32_t>(fields[1].integer);
        req.deadline_s = static_cast<std::uint64_t>(fields[2].integer);
        if (req.request_id == 0) {
            return;  // 0 保留（回执关联键缺位即静默）
        }
        const DrainStatus st = on_request_(req);

        std::vector<wire::FieldValue> out(6);
        out[0].integer = static_cast<std::int64_t>(req.request_id);
        out[1].integer = req.op;
        out[2].integer = st.ok ? 1 : 0;
        out[3].integer = st.state;
        out[4].integer = static_cast<std::int64_t>(st.active_connections);
        out[5].text = st.detail;
        std::vector<std::uint8_t> payload;
        if (!wire::encode_payload(id_of(MsgId::gateway_drain_reply), out, payload)) {
            return;
        }
        // Reliable：控制回执不可丢；链路断连期间入队，重连恰一次补投。
        (void)peer.send(id_of(MsgId::gateway_drain_reply), payload.data(), payload.size(),
                        link::SendClass::kReliable);
    }

    GatewayControlConfig config_;
    std::function<DrainStatus(const DrainControlRequest&)> on_request_;
    std::unique_ptr<link::RouteTable> route_table_;
    std::unique_ptr<link::LinkManager> manager_;
    std::atomic<bool> started_{false};
};

} // namespace

std::unique_ptr<GatewayControlServer> makeGatewayControlServer(
    GatewayControlConfig config,
    std::function<DrainStatus(const DrainControlRequest&)> on_request) {
    return std::make_unique<GatewayControlServerImpl>(std::move(config),
                                                      std::move(on_request));
}

#else // _WIN32

std::unique_ptr<GatewayControlServer> makeGatewayControlServer(
    GatewayControlConfig /*config*/,
    std::function<DrainStatus(const DrainControlRequest&)> /*on_request*/) {
    // link TCP 底座不进 M1 批（POSIX only）——Windows 退 null 桩（start
    // true / 泵空转 / 端口 0，头注同口径），GatewayServer 装配面无 #if。
    return std::make_unique<GatewayControlServer>();
}

#endif // !_WIN32

} // namespace gateway
