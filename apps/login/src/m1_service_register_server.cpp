#include "login/m1_service_register_server.hpp"

#include "apollo/core/log/log_manager.h"
#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/link/route_table.hpp"
#include "apollo/net/wire/contract_wire.hpp"
#include "apollo_contract.h"

#include <atomic>
#include <chrono>
#include <fstream>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>

namespace login {

#if !defined(_WIN32)

namespace {

namespace link = apollo::net::link;
namespace wire = apollo::net::wire;
using apollo::contract::gen::MsgId;

constexpr std::uint16_t id_of(MsgId id) { return static_cast<std::uint16_t>(id); }

// 服务注册受理面真实现（POSIX）：InterServerLink passive 端点 + 964 裸帧
// dispatch（同 World 侧 M1LinkServer 960 面）。泵线程单写者——受理/裁决/
// 回执全在泵拍（handler 链内），registry 由宿主持有，本面只受理不拥有。
class M1ServiceRegisterServerImpl final : public M1ServiceRegisterServer {
public:
    M1ServiceRegisterServerImpl(M1ServiceRegisterConfig config,
                                ServiceRegistry* registry)
        : config_(std::move(config))
        , registry_(registry) {}

    ~M1ServiceRegisterServerImpl() override {
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
        mc.self_name = "login";
        mc.route_version = link::route_version_from_hash(route_table_->schema_hash());
        mc.compat_route_versions = link::compat_route_versions_from_env();
        manager_ = std::make_unique<link::LinkManager>(mc);

        link::EndpointConfig ep;
        ep.name = "services";  // 本端把手名（对端按地址拨入，身份 = 握手自报名）
        ep.host = config_.host;
        ep.port = config_.port;
        ep.role = link::EndpointRole::kPassive;
        if (!manager_->add_endpoint(ep)) {
            manager_.reset();
            route_table_.reset();
            return false;
        }

        // 受理装配随 peer 上线（fan-out 面：Base/World 各自拨入；重连 = 同名
        // 新链重新过此装配）。peer 自报名 = 链路名（回执按名查链路）。
        manager_->set_peer_listener([this](link::InterServerLink& peer) {
            auto& dispatcher = peer.dispatcher();
            dispatcher.set_route_table(route_table_.get());
            dispatcher.allow_domain("internal");  // 964 internal 域白名单
            dispatcher.set_handler(id_of(MsgId::service_register_request),
                                   [&peer, this](std::uint16_t,
                                                 const std::uint8_t* data,
                                                 std::size_t size) {
                                       handle_register_frame(peer.peer_name(),
                                                             data, size);
                                   });
        });

        running_.store(true, std::memory_order_release);
        pump_thread_ = std::thread([this]() {
            while (running_.load(std::memory_order_acquire)) {
                const auto now = now_ms();
                manager_->pump(now);
                // 心跳失效窗与受理同泵线程（registry 单写者纪律）；扫拍取
                // 泵后现值（受理在泵内记账，时戳不早于本扫拍）。扫出显形
                // 日志——目录收缩是运维可见事件，静默丢弃不可诊断。
                const auto swept =
                    registry_->sweep_expired(now_ms(), config_.heartbeat_timeout_ms);
                if (swept != 0) {
                    apollo::core::log::global_log_manager()
                        .createLogger("login_server")
                        ->info("service register expired: swept=" +
                               std::to_string(swept));
                }
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
        // 先摘泵线程再拆链路（handler 悬垂面随管理器析构一并消失）。
        manager_.reset();
        route_table_.reset();
    }

    std::uint16_t listenPort() const override {
        return manager_ ? manager_->endpoint_port("services") : 0;
    }

    std::uint64_t rejectedFrames() const override {
        return rejected_.load(std::memory_order_acquire);
    }

private:
    // 964 受理：契约 wire 解包 → registry 同步裁决 → 965 按名回链路。
    // request_id = 0 保留（无回执无登记，同 960 惯例）；残帧拒收计数。
    void handle_register_frame(const std::string& peer, const std::uint8_t* data,
                               std::size_t size) {
        std::vector<wire::FieldValue> fields;
        if (!wire::decode_payload(id_of(MsgId::service_register_request), data, size,
                                  fields)
            || fields.size() != 8) {
            rejected_.fetch_add(1, std::memory_order_acq_rel);
            return;
        }
        const auto request_id = static_cast<std::uint64_t>(fields[0].integer);
        if (request_id == 0) {
            return;  // 保留语义：无回执无登记
        }
        ServiceRegistration reg;
        reg.service_kind = static_cast<std::int32_t>(fields[1].integer);
        reg.logical_id = static_cast<std::uint64_t>(fields[2].integer);
        reg.incarnation = static_cast<std::uint64_t>(fields[3].integer);
        reg.host = fields[4].text;
        reg.port = static_cast<std::int32_t>(fields[5].integer);
        reg.compat = fields[6].integer;
        reg.ready = fields[7].integer != 0;
        reg.peer = peer;
        const auto verdict = registry_->register_service(reg, now_ms());
        if (verdict.accepted) {
            apollo::core::log::global_log_manager()
                .createLogger("login_server")
                ->info("service registered: kind=" +
                       std::to_string(reg.service_kind) + " id=" +
                       std::to_string(reg.logical_id) + " incarnation=" +
                       std::to_string(reg.incarnation) + " peer=" + peer);
        }
        send_reply(peer, request_id, verdict, reg);
    }

    // 965 回执（kReliable 同 961 面）：按 peer 自报名查链路；未知 peer =
    // 尽力面（调用方超时面重发裁决）。
    void send_reply(const std::string& peer, std::uint64_t request_id,
                    const ServiceRegisterVerdict& verdict,
                    const ServiceRegistration& reg) {
        auto* link = manager_ ? manager_->link(peer) : nullptr;
        if (link == nullptr) {
            return;
        }
        std::vector<wire::FieldValue> fields(6);
        fields[0].integer = static_cast<std::int64_t>(request_id);
        fields[1].integer = verdict.accepted ? 1 : 0;
        fields[2].integer = verdict.code;
        fields[3].integer = reg.service_kind;
        fields[4].integer = static_cast<std::int64_t>(reg.logical_id);
        fields[5].text = verdict.detail;
        std::vector<std::uint8_t> payload;
        if (!wire::encode_payload(id_of(MsgId::service_register_reply), fields,
                                  payload)) {
            return;
        }
        (void)link->send(id_of(MsgId::service_register_reply), payload.data(),
                         payload.size(), link::SendClass::kReliable);
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

    M1ServiceRegisterConfig config_;
    ServiceRegistry* registry_ = nullptr;
    std::unique_ptr<link::RouteTable> route_table_;
    std::unique_ptr<link::LinkManager> manager_;
    std::thread pump_thread_;
    std::atomic<bool> running_{false};
    std::atomic<std::uint64_t> rejected_{0};
};

} // namespace

std::unique_ptr<M1ServiceRegisterServer> makeM1ServiceRegisterServer(
    M1ServiceRegisterConfig config, ServiceRegistry* registry) {
    return std::make_unique<M1ServiceRegisterServerImpl>(std::move(config),
                                                         registry);
}

#else // _WIN32

std::unique_ptr<M1ServiceRegisterServer> makeM1ServiceRegisterServer(
    M1ServiceRegisterConfig /*config*/, ServiceRegistry* /*registry*/) {
    // link TCP 底座不进本批（POSIX only）：Windows 退 null 桩（start false
    // = 面关，宿主装配面无 #if）。
    class NullServiceRegisterServer final : public M1ServiceRegisterServer {
    public:
        bool start() override { return false; }
        void stop() override {}
        std::uint16_t listenPort() const override { return 0; }
        std::uint64_t rejectedFrames() const override { return 0; }
    };
    return std::make_unique<NullServiceRegisterServer>();
}

#endif // !_WIN32

} // namespace login
