#include "world_app/cell_register_client.hpp"

#if !defined(_WIN32)

// cell 侧注册真实实现（L101 注册次切片）：LinkManager active 端点拨 World
// M1 受理面 + 契约 wire 960/961。机制零内部线程——泵由宿主（cell 角色
// pre-tick 钩子）驱动，960 发送与 961 记账都发生在宿主泵上下文内。
//
// 锁面：state_/reported_drain_state_ 跨线程读写（state() 观测面），mutex
// 只守这两个字段——不跨 manager_->pump() 持锁（961 handler 在 pump 内同
// 步回调，重入取锁即死锁）；manager_ 生命周期归宿主泵线程（start/stop/
// pump 同线程），免锁访问。

#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/wire/contract_wire.hpp"
#include "apollo_contract.h"

#include <chrono>
#include <fstream>
#include <mutex>
#include <sstream>
#include <utility>
#include <vector>

namespace world_app {

namespace {

namespace link = apollo::net::link;
namespace wire = apollo::net::wire;
using apollo::contract::gen::MsgId;

constexpr std::uint16_t id_of(MsgId id) { return static_cast<std::uint16_t>(id); }
constexpr std::uint16_t kRegisterRequestId = id_of(MsgId::cell_register_request);
constexpr std::uint16_t kRegisterReplyId = id_of(MsgId::cell_register_reply);

std::uint64_t steady_now_ms() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

class CellRegisterClientImpl final : public CellRegisterClient {
public:
    explicit CellRegisterClientImpl(CellRegisterConfig config)
        : config_(std::move(config)) {
        if (config_.start_epoch == 0) {
            config_.start_epoch = steady_now_ms();  // 自铸（单调，重启必增）
        }
    }

    bool start() override {
        if (manager_) {
            return true;  // 已在运行（幂等）
        }
        // 入口校验显形（配置缺位/路由表不可读 = 拒起，不带隐式回退）
        if (config_.world_host.empty() || config_.world_port == 0
            || config_.cell_id == 0 || config_.route_json.empty()) {
            return false;
        }
        route_table_ = std::make_unique<link::RouteTable>();
        std::ifstream in(config_.route_json, std::ios::binary);
        if (!in) {
            route_table_.reset();
            return false;
        }
        std::ostringstream buffer;
        buffer << in.rdbuf();
        if (!link::load_route_table_json(buffer.str(), *route_table_)) {
            route_table_.reset();
            return false;
        }

        link::LinkManagerConfig mc;
        mc.self_name = "cell-" + std::to_string(config_.cell_id);  // peer 自报名
        mc.route_version = link::route_version_from_hash(route_table_->schema_hash());
        mc.compat_route_versions = link::compat_route_versions_from_env();
        mc.authority_epoch = config_.authority_epoch;  // 0 = 进程自铸
        manager_ = std::make_unique<link::LinkManager>(mc);

        link::EndpointConfig ep;
        ep.name = "world";  // 对端组件名（World M1 端点 self_name）
        ep.host = config_.world_host;
        ep.port = config_.world_port;
        ep.role = link::EndpointRole::kActive;
        if (!manager_->add_endpoint(ep)) {
            manager_.reset();
            route_table_.reset();
            return false;
        }
        // active 端点链路对象随 add_endpoint 即在且重连复用同链——分发面一次
        // 装配即可（passive fan-out 的 peer_listener 场景不适用，见
        // link_manager.hpp 头注）。
        auto& dispatcher = manager_->link("world")->dispatcher();
        dispatcher.set_route_table(route_table_.get());
        dispatcher.allow_domain("internal");
        dispatcher.set_handler(
            kRegisterReplyId,
            [this](std::uint16_t, const std::uint8_t* data, std::size_t size) {
                handle_reply(data, size);
            });
        up_seen_ = false;
        return true;
    }

    void stop() override {
        manager_.reset();
        route_table_.reset();
        up_seen_ = false;
    }

    void pump(std::uint64_t now_ms) override {
        if (!manager_) {
            return;
        }
        manager_->pump(now_ms);  // 961 记账在 handle_reply 内自取锁
        // kUp 跃迁（首连/断链重连）即发 960；up 期间周期心跳（heartbeat_ms
        // 间隔）重发 960——幂等刷新 World 侧 last-heard，兼作 961 回执丢失的
        // 重试（重发裁决在受理面幂等，不重试风暴）。
        const bool up = link_up();
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

    CellRegisterState state() const override {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return state_;
    }

    void setReportedDrainState(std::int32_t drain_state) override {
        std::lock_guard<std::mutex> lock(state_mutex_);
        reported_drain_state_ = drain_state;
    }

private:
    bool link_up() const {
        if (!manager_) {
            return false;
        }
        auto* l = manager_->link("world");
        return l != nullptr && l->state() == link::LinkState::kUp;
    }

    void send_register(std::uint64_t now_ms) {
        auto* l = manager_ ? manager_->link("world") : nullptr;
        if (l == nullptr) {
            return;
        }
        std::vector<wire::FieldValue> f(9);
        f[0].integer = static_cast<std::int64_t>(++next_request_id_);  // 0 保留
        f[1].integer = static_cast<std::int64_t>(config_.cell_id);
        f[2].integer = static_cast<std::int64_t>(config_.start_epoch);
        f[3].text = config_.advertise_host;
        f[4].integer = config_.advertise_port;
        f[5].integer = 0;  // capabilities 未声明（0——能力裁决随 L102 容量批）
        f[6].integer = 0;  // load 未上报（0——容量申报随 L102）
        f[7].integer = reported_drain_state();
        f[8].integer = 1;  // ready：内核在跑即可分配（就绪判据细化随容量批）
        std::vector<std::uint8_t> payload;
        if (!wire::encode_payload(kRegisterRequestId, f, payload)) {
            return;
        }
        if (l->send(kRegisterRequestId, payload.data(), payload.size(),
                    link::SendClass::kReliable)
            != link::SendCode::kAccepted) {
            return;  // 队列满等瞬态——下个跃迁/心跳拍再发
        }
        last_send_ms_ = now_ms;  // 心跳节拍基准（发送成功才推进）
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_.sent = true;
        ++state_.attempts;
    }

    void handle_reply(const std::uint8_t* data, std::size_t size) {
        std::vector<wire::FieldValue> fields;
        if (!wire::decode_payload(kRegisterReplyId, data, size, fields)
            || fields.size() != 6) {
            return;  // 残帧 = 尽力面丢弃（账不动；受理面已计拒收）
        }
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_.replied = true;
        state_.accepted = fields[1].integer != 0;
        state_.code = static_cast<std::int32_t>(fields[2].integer);
        state_.management_epoch = static_cast<std::uint64_t>(fields[3].integer);
        state_.assigned_cell_id = static_cast<std::uint64_t>(fields[4].integer);
        state_.detail = fields[5].text;
        ++state_.replies;
    }

    std::int32_t reported_drain_state() const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return reported_drain_state_;
    }

    CellRegisterConfig config_;
    std::unique_ptr<link::RouteTable> route_table_;
    std::unique_ptr<link::LinkManager> manager_;  // 宿主泵线程生命周期
    bool up_seen_ = false;
    std::uint64_t last_send_ms_ = 0;  // 最近一次 960 发送成功时刻（心跳节拍基准）
    std::uint64_t next_request_id_ = 0;  // 0 保留，从 1 起单调
    mutable std::mutex state_mutex_;
    CellRegisterState state_;        // 泵上下文写 / 任意线程读
    std::int32_t reported_drain_state_ = 0;  // 宿主写 / 泵上下文读
};

} // namespace

std::unique_ptr<CellRegisterClient> makeCellRegisterClient(CellRegisterConfig config) {
    return std::make_unique<CellRegisterClientImpl>(std::move(config));
}

#else // _WIN32

std::unique_ptr<CellRegisterClient> makeCellRegisterClient(CellRegisterConfig /*config*/) {
    // link TCP 底座不进 M1 批（POSIX only）——Windows 退 null 桩（缝缺省
    // 实现：start true / 泵空转 / 账面恒零），消费面无 #if。
    return std::make_unique<CellRegisterClient>();
}

#endif // !_WIN32

} // namespace world_app
