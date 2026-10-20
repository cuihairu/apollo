#include "login/m1_service_register_client.hpp"

// 服务注册拨号客户端真实现（L123 拨号次切片）：LinkManager active 端点拨
// login 服务注册受理面 + 契约 wire 964/965。机制零内部线程——泵由宿主
// （tick 钩子 / 主循环拍）驱动，964 发送与 965 记账都发生在宿主泵上下文内
// （cell_register_client 同型，镜像它的锁面纪律：mutex 只守账面快照与
// ready 字段，不跨 manager_->pump() 持锁——965 handler 在 pump 内同步
// 回调，重入取锁即死锁）。

#include "apollo/core/log/log_manager.h"
#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/link/route_table.hpp"
#include "apollo/net/wire/contract_wire.hpp"
#include "apollo_contract.h"

#include <chrono>
#include <fstream>
#include <mutex>
#include <sstream>
#include <utility>
#include <vector>

namespace login {

#if !defined(_WIN32)

namespace {

namespace link = apollo::net::link;
namespace wire = apollo::net::wire;
using apollo::contract::gen::MsgId;

constexpr std::uint16_t id_of(MsgId id) { return static_cast<std::uint16_t>(id); }
constexpr std::uint16_t kRegisterRequestId = id_of(MsgId::service_register_request);
constexpr std::uint16_t kRegisterReplyId = id_of(MsgId::service_register_reply);
constexpr char kLoginEndpoint[] = "login";  // 对端组件名（login M1 端点 self_name）

class M1ServiceRegisterClientImpl final : public M1ServiceRegisterClient {
public:
    explicit M1ServiceRegisterClientImpl(M1ServiceRegisterClientConfig config)
        : config_(std::move(config)) {
        if (config_.incarnation == 0) {
            config_.incarnation = steady_now_ms();  // 自铸（单调，重启必增）
        }
        if (config_.self_name.empty()) {
            config_.self_name = "svc-" + std::to_string(config_.service_kind) +
                                "-" + std::to_string(config_.logical_id);
        }
    }

    bool start() override {
        if (manager_) {
            return true;  // 已在运行（幂等）
        }
        // 入口校验显形（配置缺位/路由表不可读 = 拒起，不带隐式回退）
        if (config_.login_host.empty() || config_.login_port == 0
            || config_.logical_id == 0 || config_.route_json.empty()) {
            return false;
        }
        route_table_ = std::make_unique<link::RouteTable>();
        if (!load_route_table(*route_table_)) {
            route_table_.reset();
            return false;
        }

        link::LinkManagerConfig mc;
        mc.self_name = config_.self_name;  // peer 自报名（回执按名寻路）
        mc.route_version = link::route_version_from_hash(route_table_->schema_hash());
        mc.compat_route_versions = link::compat_route_versions_from_env();
        manager_ = std::make_unique<link::LinkManager>(mc);

        link::EndpointConfig ep;
        ep.name = kLoginEndpoint;
        ep.host = config_.login_host;
        ep.port = config_.login_port;
        ep.role = link::EndpointRole::kActive;
        if (!manager_->add_endpoint(ep)) {
            manager_.reset();
            route_table_.reset();
            return false;
        }
        // active 端点链路对象随 add_endpoint 即在且重连复用同链——分发面一次
        // 装配即可。
        auto& dispatcher = manager_->link(kLoginEndpoint)->dispatcher();
        dispatcher.set_route_table(route_table_.get());
        dispatcher.allow_domain("internal");
        dispatcher.set_handler(kRegisterReplyId,
                               [this](std::uint16_t, const std::uint8_t* data,
                                      std::size_t size) { handle_reply(data, size); });
        up_seen_ = false;
        return true;
    }

    void stop() override {
        manager_.reset();
        route_table_.reset();
        up_seen_ = false;
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_ = M1ServiceRegisterState{};  // 宿主意图停机：账面随新生命周期重置
    }

    void pump(std::uint64_t now_ms) override {
        if (!manager_) {
            return;
        }
        manager_->pump(now_ms);  // 965 记账在 handle_reply 内自取锁
        const bool up = link_up();
        // kUp 跃迁（首连/断链重连）即发 964；up 期间周期心跳（heartbeat_ms
        // 间隔）重发——幂等刷新 login 侧 last-heard，兼作 965 回执丢失的
        // 重试（重发裁决在受理面幂等，不重试风暴）。
        if (up && !up_seen_) {
            up_seen_ = true;
            send_register(now_ms);
        } else if (up && config_.heartbeat_ms > 0
                   && now_ms - last_send_ms_ >= config_.heartbeat_ms) {
            send_register(now_ms);
        } else if (!up) {
            up_seen_ = false;
        }
    }

    bool linkUp() const override { return link_up(); }

    M1ServiceRegisterState state() const override {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return state_;
    }

    void set_ready(bool ready) override {
        std::lock_guard<std::mutex> lock(state_mutex_);
        ready_ = ready;
    }

private:
    bool link_up() const {
        if (!manager_) {
            return false;
        }
        auto* l = manager_->link(kLoginEndpoint);
        return l != nullptr && l->state() == link::LinkState::kUp;
    }

    void send_register(std::uint64_t now_ms) {
        auto* l = manager_->link(kLoginEndpoint);
        if (l == nullptr) {
            return;
        }
        bool ready = true;
        std::uint64_t request_id = 0;
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            ready = ready_;
            request_id = ++next_request_id_;  // 1 起——0 是保留语义
        }
        // 964 八字段：request_id / kind / logical_id / incarnation / host /
        // port / compat / ready（受理面同序解）。
        std::vector<wire::FieldValue> fields(8);
        fields[0].integer = static_cast<std::int64_t>(request_id);
        fields[1].integer = config_.service_kind;
        fields[2].integer = static_cast<std::int64_t>(config_.logical_id);
        fields[3].integer = static_cast<std::int64_t>(config_.incarnation);
        fields[4].text = config_.advertise_host;
        fields[5].integer = config_.advertise_port;
        fields[6].integer = config_.compat;
        fields[7].integer = ready ? 1 : 0;
        std::vector<std::uint8_t> payload;
        if (!wire::encode_payload(kRegisterRequestId, fields, payload)) {
            return;
        }
        if (l->send(kRegisterRequestId, payload.data(), payload.size(),
                    link::SendClass::kReliable) == link::SendCode::kAccepted) {
            last_send_ms_ = now_ms;
            std::lock_guard<std::mutex> lock(state_mutex_);
            state_.sent = true;
            ++state_.attempts;
        }
    }

    void handle_reply(const std::uint8_t* data, std::size_t size) {
        std::vector<wire::FieldValue> fields;
        if (!wire::decode_payload(kRegisterReplyId, data, size, fields)
            || fields.size() != 6) {
            return;  // 残帧不记账（受理面拒收计数在对面）
        }
        M1ServiceRegisterState update;
        update.sent = true;
        update.replied = true;
        update.accepted = fields[1].integer != 0;
        update.code = static_cast<std::int32_t>(fields[2].integer);
        update.service_kind = static_cast<std::int32_t>(fields[3].integer);
        update.logical_id = static_cast<std::uint64_t>(fields[4].integer);
        update.detail = fields[5].text;
        std::uint64_t attempts;
        std::uint64_t replies;
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            attempts = state_.attempts;  // 发送/回执计数是累计账——快照整体
            replies = state_.replies + 1;  // 替换不回零（同 attempts 面）
            state_ = update;
            state_.attempts = attempts;
            state_.replies = replies;
        }
        // 裁决原样显形（宿主/运维读日志不翻表）——不重裁不停机自裁。
        apollo::core::log::global_log_manager()
            .createLogger("service_register")
            ->info(std::string(update.accepted ? "service register accepted"
                                               : "service register rejected")
                   + ": code=" + std::to_string(update.code) + " kind="
                   + std::to_string(update.service_kind) + " id="
                   + std::to_string(update.logical_id) + " attempts="
                   + std::to_string(attempts));
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

    static std::uint64_t steady_now_ms() {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());
    }

    M1ServiceRegisterClientConfig config_;
    std::unique_ptr<link::RouteTable> route_table_;
    std::unique_ptr<link::LinkManager> manager_;
    bool up_seen_ = false;
    std::uint64_t last_send_ms_ = 0;
    mutable std::mutex state_mutex_;
    M1ServiceRegisterState state_;
    bool ready_ = true;
    std::uint64_t next_request_id_ = 0;  // 泵上下文单写者（发送全在 pump 内）
};

} // namespace

std::unique_ptr<M1ServiceRegisterClient> makeM1ServiceRegisterClient(
    M1ServiceRegisterClientConfig config) {
    return std::make_unique<M1ServiceRegisterClientImpl>(std::move(config));
}

#else // _WIN32

std::unique_ptr<M1ServiceRegisterClient> makeM1ServiceRegisterClient(
    M1ServiceRegisterClientConfig /*config*/) {
    // link TCP 底座不进本批（POSIX only）：Windows 退 null 桩（start false
    // = 不注册，宿主装配面无 #if）。
    class NullServiceRegisterClient final : public M1ServiceRegisterClient {
    public:
        bool start() override { return false; }
        void stop() override {}
        void pump(std::uint64_t) override {}
        bool linkUp() const override { return false; }
        M1ServiceRegisterState state() const override { return {}; }
        void set_ready(bool) override {}
    };
    return std::make_unique<NullServiceRegisterClient>();
}

#endif // !_WIN32

} // namespace login
