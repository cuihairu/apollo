#include "gateway/ingress/session_admission_service.hpp"

#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/link/route_table.hpp"
#include "apollo/net/linkrpc/rpc_channel.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"

#include <atomic>
#include <chrono>
#include <exception>
#include <fstream>
#include <sstream>
#include <thread>
#include <utility>

namespace gateway {

namespace {

// 退化容忍面（P4-4 批⑤：nng 桩准入退役）：仅拒空票，非空票放行——桩
// 时代同缝（桩传输恒 throw → catch 容忍），ingress 主线全接线前保持
// admit 容忍（P3-2 收口）。
class DefaultSessionAdmissionService final : public SessionAdmissionService {
public:
    AdmissionResult admit(const AdmissionRequest& request) override {
        if (request.loginTicket.empty()) {
            return {false, "login ticket is empty"};
        }

        // Non-empty tickets remain admissible until the ingress mainline is
        // fully wired (P3-2).
        return {true, {}};
    }
};

// Link M1 准入面（P4-4 批①）：active 端点拨 login 准入受理面 + linkrpc
// 关联原语。自带泵线程（LogPushClient 同型）——准入是同步等待语义，泵
// 必须独立于调用方存活（gateway 的准入调用点在 ingress 宿主泵线程上，
// 自泵自等 = 死锁）。
//
// 降级口径与桩时代同缝：传输不可达/信封残缺不放大成拒绝（准入面缺席
// ≠ 会话非法，ingress 主线全接线前保持 admit 容忍）；仅 login 明确
// success=false 才拒。
class LinkSessionAdmissionService final : public SessionAdmissionService {
public:
    LinkSessionAdmissionService(std::string host, std::uint16_t port, std::string route_json)
        : host_(std::move(host))
        , port_(port)
        , route_json_(std::move(route_json)) {}

    ~LinkSessionAdmissionService() override {
        shutdown();
    }

    bool start() {
#if !defined(_WIN32)
        if (running_.load(std::memory_order_acquire)) {
            return true;
        }
        route_table_ = std::make_unique<apollo::net::link::RouteTable>();
        if (!load_route_table(*route_table_)) {
            route_table_.reset();
            return false;  // 装配错误显形（空路径/坏 JSON = 拒起，调用方退桩）
        }

        apollo::net::link::LinkManagerConfig mc;
        mc.self_name = "gateway";
        mc.route_version =
            apollo::net::link::route_version_from_hash(route_table_->schema_hash());
        manager_ = std::make_unique<apollo::net::link::LinkManager>(mc);

        apollo::net::link::EndpointConfig ep;
        ep.name = "login";  // 对端 G-1 组件名（握手身份 = 其 Link self_name）
        ep.host = host_;
        ep.port = port_;
        ep.role = apollo::net::link::EndpointRole::kActive;
        if (!manager_->add_endpoint(ep)) {
            manager_.reset();
            route_table_.reset();
            return false;
        }

        // 链路对象随 add_endpoint 即存在且跨重连稳定——路由表/白名单/
        // 响应 handler 装配一次，重连不复装（rpc_channel.hpp 纪律）。
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
#else
        return false;  // link TCP 底座 POSIX only——Windows 退桩传输
#endif
    }

    void shutdown() override {
#if !defined(_WIN32)
        if (!running_.exchange(false, std::memory_order_acq_rel)) {
            return;
        }
        if (pump_thread_.joinable()) {
            pump_thread_.join();
        }
        // 先停泵再拆面：RpcClient 摘响应 handler 后链路随之析构（in-flight
        // admit 由调用方线程先于 shutdown 收口——宿主 stop 序保证）。
        client_.reset();
        manager_.reset();
        route_table_.reset();
#endif
    }

    AdmissionResult admit(const AdmissionRequest& request) override {
        if (request.loginTicket.empty()) {
            return {false, "login ticket is empty"};
        }
        if (client_ == nullptr) {
            return {false, "login service unavailable"};
        }

        apollo::protocol::GatewayAssignRequest rpc_request;
        rpc_request.playerId = request.playerId;
        rpc_request.sessionId = request.sessionId;
        rpc_request.loginTicket = request.loginTicket;
        const auto frame =
            apollo::protocol::MessageCodec::encode(rpc_request, request.sessionId);

        // 同步等待（本类泵线程之外调用——rpc_channel.hpp 纪律）。
        const auto response =
            client_->call(frame, std::chrono::milliseconds(kTimeoutMs));
        if (!response) {
            // 无权威确认不准入。
            return {false, "login service unavailable"};
        }

        if (response->size() < sizeof(apollo::protocol::MessageHeader)) {
            return {false, "invalid login response"};
        }
        const auto header = apollo::protocol::MessageCodec::parseHeader(*response);
        if (static_cast<apollo::protocol::MessageType>(header.type) !=
            apollo::protocol::MessageType::GATEWAY_ASSIGN_RESPONSE) {
            return {false, "invalid login response"};
        }
        try {
            apollo::protocol::GatewayAssignResponse parsed;
            const std::vector<std::uint8_t> body(response->begin() + sizeof(apollo::protocol::MessageHeader),
                                                 response->end());
            apollo::protocol::MessageCodec::decodeBody(body, parsed);
            if (!parsed.success) {
                return {false, "gateway admission rejected"};
            }
            return {true, {}};
        } catch (const std::exception&) {
            return {false, "invalid login response"};
        }
    }

private:
    static constexpr std::uint32_t kTimeoutMs = 500;  // 准入同步等待上限

    bool load_route_table(apollo::net::link::RouteTable& table) {
        if (route_json_.empty()) {
            return false;
        }
        std::ifstream in(route_json_, std::ios::binary);
        if (!in) {
            return false;
        }
        std::ostringstream buffer;
        buffer << in.rdbuf();
#if APOLLO_NET_LINK_HAS_JSON
        return apollo::net::link::load_route_table_json(buffer.str(), table);
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

    std::string host_;
    std::uint16_t port_ = 0;
    std::string route_json_;
#if !defined(_WIN32)
    // link 传输底座 POSIX only（Windows 工厂 start false 退桩，成员同收守卫）
    std::unique_ptr<apollo::net::link::RouteTable> route_table_;
    std::unique_ptr<apollo::net::link::LinkManager> manager_;
    std::thread pump_thread_;
#endif
    std::unique_ptr<apollo::net::linkrpc::RpcClient> client_;
    std::atomic<bool> running_{false};
};

} // namespace

std::unique_ptr<SessionAdmissionService> makeDefaultSessionAdmissionService() {
    return std::make_unique<DefaultSessionAdmissionService>();
}

std::unique_ptr<SessionAdmissionService> makeLinkSessionAdmissionService(
    const std::string& host, std::uint16_t port, const std::string& route_json
) {
    // 工厂即装配：start false = 装配缺位（路由表缺/Windows），返回 null
    // 由调用方退桩传输（起进程不拦，处方①同口径）。
    auto service = std::make_unique<LinkSessionAdmissionService>(host, port, route_json);
    if (!service->start()) {
        return nullptr;
    }
    return service;
}

} // namespace gateway
