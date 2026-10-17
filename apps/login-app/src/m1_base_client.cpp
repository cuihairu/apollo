#include "login/m1_base_client.hpp"

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

namespace login {

#if !defined(_WIN32)

// base 客户端面真实现（POSIX）：InterServerLink active 端点 + linkrpc
// RpcClient（批① gateway 准入链路面同型）。泵线程单写者——收发/关联
// 回收全在泵拍；call 的同步等待发生在调用方线程（禁在本类泵线程上调）。
// 命名空间开在平台分支外——Windows 桩分支同属 login（m1_egress 同款）。

namespace {

namespace link = apollo::net::link;

class M1BaseClientImpl final : public M1BaseClient {
public:
    explicit M1BaseClientImpl(M1BaseClientConfig config)
        : config_(std::move(config)) {}

    ~M1BaseClientImpl() override {
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
        mc.self_name = config_.self_name;
        mc.route_version = link::route_version_from_hash(route_table_->schema_hash());
            mc.compat_route_versions = link::compat_route_versions_from_env();
        manager_ = std::make_unique<link::LinkManager>(mc);

        link::EndpointConfig ep;
        ep.name = config_.peer_name;  // 对端 G-1 组件名（握手身份 = 其 Link self_name）
        ep.host = config_.host;
        ep.port = config_.port;
        ep.role = link::EndpointRole::kActive;
        if (!manager_->add_endpoint(ep)) {
            manager_.reset();
            route_table_.reset();
            return false;
        }

        // 链路对象随 add_endpoint 即存在且跨重连稳定——路由表/白名单/
        // 响应 handler 装配一次，重连不复装（rpc_channel.hpp 纪律；
        // 装配必须在 RpcClient 构造前——漏装 = 响应帧 kUnknownId 落地，
        // 调用方永等超时，批① 踩坑注记）。
        auto* peer = manager_->link(ep.name);
        peer->dispatcher().set_route_table(route_table_.get());
        peer->dispatcher().allow_domain("internal");  // RPC 信封族入闸
        client_ = std::make_unique<apollo::net::linkrpc::RpcClient>(*peer);

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
        // 先停泵再拆面：RpcClient 摘响应 handler 后链路随之析构（in-flight
        // call 由调用方线程先于 stop 收口——宿主 stop 序保证）。
        client_.reset();
        manager_.reset();
        route_table_.reset();
    }

    std::optional<std::vector<std::uint8_t>> call(
        const std::vector<std::uint8_t>& frame,
        std::chrono::milliseconds timeout) override {
        if (client_ == nullptr) {
            return std::nullopt;  // 面未起（Windows/装配降级）= 传输缺席
        }
        return client_->call(frame, timeout);
    }

    bool peerUp() const override {
        auto* peer = const_cast<M1BaseClientImpl*>(this)->dialed_peer();
        return peer != nullptr && peer->state() == link::LinkState::kUp;
    }

private:
    link::InterServerLink* dialed_peer() {
        return manager_ ? manager_->link(config_.peer_name) : nullptr;
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

    M1BaseClientConfig config_;
    std::unique_ptr<link::RouteTable> route_table_;
    std::unique_ptr<link::LinkManager> manager_;
    std::unique_ptr<apollo::net::linkrpc::RpcClient> client_;
    std::thread pump_thread_;
    std::atomic<bool> running_{false};
};

} // namespace

std::unique_ptr<M1BaseClient> makeM1BaseClient(M1BaseClientConfig config) {
    auto client = std::make_unique<M1BaseClientImpl>(std::move(config));
    if (!client->start()) {
        return nullptr;  // 装配缺位（路由表缺/Windows）= 调用方退桩传输
    }
    return client;
}

#else // _WIN32

namespace {

class NullBaseClient final : public M1BaseClient {
public:
    bool start() override { return false; }
    void stop() override {}
    std::optional<std::vector<std::uint8_t>> call(const std::vector<std::uint8_t>& /*frame*/,
                                                  std::chrono::milliseconds /*timeout*/) override {
        return std::nullopt;
    }
    bool peerUp() const override { return false; }
};

} // namespace

std::unique_ptr<M1BaseClient> makeM1BaseClient(M1BaseClientConfig /*config*/) {
    // link TCP 底座不进本批（POSIX only）：Windows 退 null 桩（start false
    // = 面关，LoginServer 装配面无 #if）。
    return std::make_unique<NullBaseClient>();
}

#endif // !_WIN32

} // namespace login
