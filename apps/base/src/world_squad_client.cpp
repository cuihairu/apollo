#include "base/world_squad_client.hpp"

// Base→World M1 小队准入链路客户端真实现（D11 issuer=Base 批①）：LinkManager
// active 端点拨 world M1 管理面 + 契约 wire 972-976。机制零内部线程——泵由
// 宿主（tick 钩子 / 主循环拍）驱动，入向记账与回调都发生在宿主泵上下文内
// （m1_service_register_client 同型，镜像它的锁面纪律：mutex 只守计数账面，
// 不跨 manager_->pump() 持锁——入向 handler 在 pump 内同步回调，宿主回调里
// 直接调 send_* 安全，重入 pump() 即死锁）。

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

namespace base {

#if !defined(_WIN32)

namespace {

namespace link = apollo::net::link;
namespace wire = apollo::net::wire;
using apollo::contract::gen::MsgId;

constexpr std::uint16_t id_of(MsgId id) { return static_cast<std::uint16_t>(id); }
constexpr std::uint16_t kValidationRequestId = id_of(MsgId::squad_validation_request);
constexpr std::uint16_t kValidationReplyId = id_of(MsgId::squad_validation_reply);
constexpr std::uint16_t kCapacityRequestId = id_of(MsgId::squad_capacity_request);
constexpr std::uint16_t kCapacityGrantId = id_of(MsgId::squad_capacity_grant);
constexpr std::uint16_t kCredentialBundleId = id_of(MsgId::squad_credential_bundle);
constexpr char kWorldEndpoint[] = "world";  // 对端组件名（World M1 self_name）

class WorldSquadClientImpl final : public WorldSquadClient {
public:
    explicit WorldSquadClientImpl(WorldSquadClientConfig config)
        : config_(std::move(config)) {
        if (config_.self_name.empty()) {
            config_.self_name = "base-squad-" + std::to_string(config_.base_id);
        }
    }

    bool start() override {
        if (manager_) {
            return true;  // 已在运行（幂等）
        }
        // 入口校验显形（配置缺位/路由表不可读 = 拒起，不带隐式回退）。
        if (config_.world_host.empty() || config_.world_port == 0
            || config_.base_id == 0 || config_.route_json.empty()) {
            return false;
        }
        route_table_ = std::make_unique<link::RouteTable>();
        if (!load_route_table(*route_table_)) {
            route_table_.reset();
            return false;
        }

        link::LinkManagerConfig mc;
        mc.self_name = config_.self_name;  // peer 自报名（World 按名回链路）
        mc.route_version = link::route_version_from_hash(route_table_->schema_hash());
        mc.compat_route_versions = link::compat_route_versions_from_env();
        manager_ = std::make_unique<link::LinkManager>(mc);

        link::EndpointConfig ep;
        ep.name = kWorldEndpoint;
        ep.host = config_.world_host;
        ep.port = config_.world_port;
        ep.role = link::EndpointRole::kActive;
        if (!manager_->add_endpoint(ep)) {
            manager_.reset();
            route_table_.reset();
            return false;
        }
        // active 端点链路对象随 add_endpoint 即在且重连复用同链——分发面一次
        // 装配即可。入向两帧（972 转 cell 受理 / 975 预留裁决）。
        auto& dispatcher = manager_->link(kWorldEndpoint)->dispatcher();
        dispatcher.set_route_table(route_table_.get());
        dispatcher.allow_domain("internal");
        dispatcher.set_handler(kValidationRequestId,
                               [this](std::uint16_t, const std::uint8_t* data,
                                      std::size_t size) {
                                   handle_validation_request(data, size);
                               });
        dispatcher.set_handler(kCapacityGrantId,
                               [this](std::uint16_t, const std::uint8_t* data,
                                      std::size_t size) {
                                   handle_capacity_grant(data, size);
                               });
        return true;
    }

    void stop() override {
        manager_.reset();
        route_table_.reset();
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_ = WorldSquadLinkState{};  // 宿主意图停机：账面随新生命周期重置
    }

    void pump(std::uint64_t now_ms) override {
        (void)now_ms;  // 哑管道无周期发送——出向全由宿主显式发起
        if (manager_) {
            manager_->pump(now_ms);  // 入向记账/回调在 handler 内自取锁
        }
    }

    bool linkUp() const override {
        if (!manager_) {
            return false;
        }
        auto* l = manager_->link(kWorldEndpoint);
        return l != nullptr && l->state() == link::LinkState::kUp;
    }

    WorldSquadLinkState state() const override {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return state_;
    }

    void set_validation_handler(
        std::function<void(const SquadValidationRequest&)> fn) override {
        validation_handler_ = std::move(fn);
    }

    void set_grant_handler(std::function<void(const SquadCapacityGrant&)> fn) override {
        grant_handler_ = std::move(fn);
    }

    bool send_validation_reply(std::uint64_t request_id, bool accepted,
                               const std::string& roster_hash,
                               const std::string& failed_members,
                               std::int32_t code) override {
        // 973 五字段：request_id / accepted / roster_hash / failed_members /
        // code（受理面同序解）。
        std::vector<wire::FieldValue> fields(5);
        fields[0].integer = static_cast<std::int64_t>(request_id);
        fields[1].integer = accepted ? 1 : 0;
        fields[2].text = roster_hash;
        fields[3].text = failed_members;
        fields[4].integer = code;
        if (!send_frame(kValidationReplyId, fields)) {
            return false;
        }
        std::lock_guard<std::mutex> lock(state_mutex_);
        ++state_.validation_replies_out;
        return true;
    }

    bool send_capacity_request(std::uint64_t request_id, std::uint64_t party_id,
                               std::uint64_t instance_id, std::uint64_t cell_id,
                               const std::string& roster_hash,
                               std::int32_t slots) override {
        // 974 六字段：request_id / party_id / instance_id / cell_id /
        // roster_hash / slots。
        std::vector<wire::FieldValue> fields(6);
        fields[0].integer = static_cast<std::int64_t>(request_id);
        fields[1].integer = static_cast<std::int64_t>(party_id);
        fields[2].integer = static_cast<std::int64_t>(instance_id);
        fields[3].integer = static_cast<std::int64_t>(cell_id);
        fields[4].text = roster_hash;
        fields[5].integer = slots;
        if (!send_frame(kCapacityRequestId, fields)) {
            return false;
        }
        std::lock_guard<std::mutex> lock(state_mutex_);
        ++state_.capacity_requests_out;
        return true;
    }

    bool send_credential_bundle(
        std::uint64_t request_id, std::uint64_t reservation_id,
        std::uint64_t credential_id, std::uint64_t player_id,
        std::uint64_t squad_id, std::uint64_t instance_id,
        std::uint64_t cell_id, const std::string& roster_hash,
        std::uint64_t issued_at, std::uint64_t expires_at,
        const std::string& issuer) override {
        // 976 十一字段：request_id / reservation_id / credential_id /
        // player_id / squad_id / instance_id / cell_id / roster_hash /
        // issued_at / expires_at / issuer（字段序全口径同 messages.xml）。
        std::vector<wire::FieldValue> fields(11);
        fields[0].integer = static_cast<std::int64_t>(request_id);
        fields[1].integer = static_cast<std::int64_t>(reservation_id);
        fields[2].integer = static_cast<std::int64_t>(credential_id);
        fields[3].integer = static_cast<std::int64_t>(player_id);
        fields[4].integer = static_cast<std::int64_t>(squad_id);
        fields[5].integer = static_cast<std::int64_t>(instance_id);
        fields[6].integer = static_cast<std::int64_t>(cell_id);
        fields[7].text = roster_hash;
        fields[8].integer = static_cast<std::int64_t>(issued_at);
        fields[9].integer = static_cast<std::int64_t>(expires_at);
        fields[10].text = issuer;
        if (!send_frame(kCredentialBundleId, fields)) {
            return false;
        }
        std::lock_guard<std::mutex> lock(state_mutex_);
        ++state_.credentials_out;
        return true;
    }

private:
    bool send_frame(std::uint16_t msg_id, const std::vector<wire::FieldValue>& fields) {
        auto* l = manager_ ? manager_->link(kWorldEndpoint) : nullptr;
        if (l == nullptr || l->state() != link::LinkState::kUp) {
            return false;  // 链路未 up 拒发（哑管道不自动重试）
        }
        std::vector<std::uint8_t> payload;
        if (!wire::encode_payload(msg_id, fields, payload)) {
            return false;
        }
        return l->send(msg_id, payload.data(), payload.size(),
                       link::SendClass::kReliable) == link::SendCode::kAccepted;
    }

    void handle_validation_request(const std::uint8_t* data, std::size_t size) {
        std::vector<wire::FieldValue> fields;
        // 972 五字段：request_id / party_id / instance_id / cell_id / members。
        if (!wire::decode_payload(kValidationRequestId, data, size, fields)
            || fields.size() != 5) {
            std::lock_guard<std::mutex> lock(state_mutex_);
            ++state_.bad_frames_in;
            return;  // 残帧不回调（对面拒收计数在 World 受理面）
        }
        SquadValidationRequest request;
        request.request_id = static_cast<std::uint64_t>(fields[0].integer);
        request.party_id = static_cast<std::uint64_t>(fields[1].integer);
        request.instance_id = static_cast<std::uint64_t>(fields[2].integer);
        request.cell_id = static_cast<std::uint64_t>(fields[3].integer);
        request.members = fields[4].text;
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            ++state_.validation_requests_in;
        }
        if (validation_handler_) {
            validation_handler_(request);  // 泵上下文同步回调
        }
    }

    void handle_capacity_grant(const std::uint8_t* data, std::size_t size) {
        std::vector<wire::FieldValue> fields;
        // 975 五字段：request_id / granted / reservation_id / expires_at / code。
        if (!wire::decode_payload(kCapacityGrantId, data, size, fields)
            || fields.size() != 5) {
            std::lock_guard<std::mutex> lock(state_mutex_);
            ++state_.bad_frames_in;
            return;
        }
        SquadCapacityGrant grant;
        grant.request_id = static_cast<std::uint64_t>(fields[0].integer);
        grant.granted = fields[1].integer != 0;
        grant.reservation_id = static_cast<std::uint64_t>(fields[2].integer);
        grant.expires_at = static_cast<std::uint64_t>(fields[3].integer);
        grant.code = static_cast<std::int32_t>(fields[4].integer);
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            ++state_.capacity_grants_in;
        }
        if (grant_handler_) {
            grant_handler_(grant);
        }
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

    WorldSquadClientConfig config_;
    std::unique_ptr<link::RouteTable> route_table_;
    std::unique_ptr<link::LinkManager> manager_;
    std::function<void(const SquadValidationRequest&)> validation_handler_;
    std::function<void(const SquadCapacityGrant&)> grant_handler_;
    mutable std::mutex state_mutex_;
    WorldSquadLinkState state_;
};

} // namespace

std::unique_ptr<WorldSquadClient> makeWorldSquadClient(
    WorldSquadClientConfig config) {
    return std::make_unique<WorldSquadClientImpl>(std::move(config));
}

#else // _WIN32

std::unique_ptr<WorldSquadClient> makeWorldSquadClient(
    WorldSquadClientConfig /*config*/) {
    // link TCP 底座不进本批（POSIX only）：Windows 退 null 桩（start false
    // = 不拨，宿主装配面无 #if）。
    class NullWorldSquadClient final : public WorldSquadClient {
    public:
        bool start() override { return false; }
        void stop() override {}
        void pump(std::uint64_t) override {}
        bool linkUp() const override { return false; }
        WorldSquadLinkState state() const override { return {}; }
        void set_validation_handler(std::function<void(const SquadValidationRequest&)>) override {}
        void set_grant_handler(std::function<void(const SquadCapacityGrant&)>) override {}
        bool send_validation_reply(std::uint64_t, bool, const std::string&,
                                   const std::string&, std::int32_t) override { return false; }
        bool send_capacity_request(std::uint64_t, std::uint64_t, std::uint64_t,
                                   std::uint64_t, const std::string&,
                                   std::int32_t) override { return false; }
        bool send_credential_bundle(std::uint64_t, std::uint64_t, std::uint64_t,
                                    std::uint64_t, std::uint64_t, std::uint64_t,
                                    std::uint64_t, const std::string&, std::uint64_t,
                                    std::uint64_t, const std::string&) override { return false; }
    };
    return std::make_unique<NullWorldSquadClient>();
}

#endif // !_WIN32

} // namespace base
