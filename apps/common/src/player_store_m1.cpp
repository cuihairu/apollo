#include "player_store/m1_server.hpp"

#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/link/route_table.hpp"
#include "apollo/net/linkrpc/rpc_channel.hpp"

#include <atomic>
#include <chrono>
#include <fstream>
#include <sstream>
#include <thread>
#include <utility>

namespace player_store {

#if !defined(_WIN32)

// base 受理面真实现（POSIX）：InterServerLink passive 端点 + linkrpc
// RpcServer（批① login 准入面同型）。泵线程单写者——受理/应答全在泵拍
// （handler 链内），宿主（PlayerStore）只管 start/stop 生命周期。
// 命名空间开在平台分支外——Windows 桩分支同属 base（m1_egress 同款）。

namespace {

namespace link = apollo::net::link;

class PlayerStoreLinkServerImpl final : public PlayerStoreLinkServer {
public:
    PlayerStoreLinkServerImpl(PlayerStoreLinkServerConfig config, Handler handler)
        : config_(std::move(config))
        , handler_(std::move(handler)) {}

    ~PlayerStoreLinkServerImpl() override {
        stop();
    }

    bool start() override {
        if (running_.load(std::memory_order_acquire)) {
            return true;
        }
        route_table_ = std::make_unique<link::RouteTable>();
        if (!load_route_table(*route_table_)) {
            route_table_.reset();
            return false;  // 装配错误显形（空路径/坏 JSON = 拒起，不带隐式回退）
        }

        link::LinkManagerConfig mc;
        mc.self_name = "world";
        mc.route_version = link::route_version_from_hash(route_table_->schema_hash());
        mc.compat_route_versions = link::compat_route_versions_from_env();
        manager_ = std::make_unique<link::LinkManager>(mc);

        link::EndpointConfig ep;
        ep.name = "login";  // 拨入方 G-1 组件名（握手身份 = 其 Link self_name）
        ep.host = config_.host;
        ep.port = config_.port;
        ep.role = link::EndpointRole::kPassive;
        if (!manager_->add_endpoint(ep)) {
            manager_.reset();
            route_table_.reset();
            return false;
        }

        // 受理装配随 peer 上线（fan-out 面；重连 = 同名新链重新过此装配）。
        rpc_server_ = std::make_unique<apollo::net::linkrpc::RpcServer>();
        manager_->set_peer_listener([this](link::InterServerLink& peer) {
            peer.dispatcher().set_route_table(route_table_.get());
            peer.dispatcher().allow_domain("internal");  // RPC 信封族入闸
            rpc_server_->install(
                peer, [this](const std::uint8_t* frame, std::size_t size) {
                    return handler_ ? handler_(frame, size) : std::vector<std::uint8_t>{};
                });
        });

        running_.store(true, std::memory_order_release);
        pump_thread_ = std::thread([this]() {
            while (running_.load(std::memory_order_acquire)) {
                manager_->pump(now_ms());
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
        return true;
    }

    void stop() override {
        if (!running_.exchange(false, std::memory_order_acq_rel)) {
            return;
        }
        if (pump_thread_.joinable()) {
            pump_thread_.join();
        }
        // 先摘泵线程再拆链路（RpcServer 的 handler 悬垂面随管理器析构一并
        // 消失；宿主 stop 序保证在途 handler 先归还——dispatchRequest 维护
        // 闸兜底迟到拍）。
        manager_.reset();
        rpc_server_.reset();
        route_table_.reset();
    }

    std::uint16_t listenPort() const override {
        return manager_ ? manager_->endpoint_port("login") : 0;
    }

    bool peerConnected() const override {
        auto* peer = const_cast<PlayerStoreLinkServerImpl*>(this)->login_peer();
        return peer != nullptr && peer->state() == link::LinkState::kUp;
    }

private:
    link::InterServerLink* login_peer() {
        return manager_ ? manager_->link("login") : nullptr;
    }

    bool load_route_table(link::RouteTable& table) {
        if (config_.route_json.empty()) {
            return false;
        }
        std::ifstream in(config_.route_json, std::ios::binary);
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

    static std::uint64_t now_ms() {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());
    }

    PlayerStoreLinkServerConfig config_;
    Handler handler_;
    std::unique_ptr<link::RouteTable> route_table_;
    std::unique_ptr<link::LinkManager> manager_;
    std::unique_ptr<apollo::net::linkrpc::RpcServer> rpc_server_;
    std::thread pump_thread_;
    std::atomic<bool> running_{false};
};

} // namespace

std::unique_ptr<PlayerStoreLinkServer> makePlayerStoreLinkServer(PlayerStoreLinkServerConfig config,
                                               PlayerStoreLinkServer::Handler handler) {
    return std::make_unique<PlayerStoreLinkServerImpl>(std::move(config), std::move(handler));
}

#else // _WIN32

std::unique_ptr<PlayerStoreLinkServer> makePlayerStoreLinkServer(PlayerStoreLinkServerConfig /*config*/,
                                               PlayerStoreLinkServer::Handler /*handler*/) {
    // link TCP 底座不进本批（POSIX only）：Windows 退 null 桩（start false
    // = 面关，PlayerStore 装配面无 #if）。
    class NullPlayerStore final : public PlayerStoreLinkServer {
    public:
        bool start() override { return false; }
        void stop() override {}
        std::uint16_t listenPort() const override { return 0; }
        bool peerConnected() const override { return false; }
    };
    return std::make_unique<NullPlayerStore>();
}

#endif // !_WIN32

} // namespace player_store
