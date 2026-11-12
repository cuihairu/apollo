#include "world_app/m1_projection_client.hpp"

// 执行位置投影发布客户端真实现（L115 位置投影首切片，契约 968）：LinkManager
// active 端点拨 login 服务注册受理面 + 968 裸帧发送。机制零内部线程——泵由
// 宿主（manager_pre_tick 钩子）驱动；publish 由 WorldApp 分配/释放路径在
// 管理泵上下文直调——publish/pump/镜像全同线程串行，无锁面（镜像 m1_projection_
// client.hpp 线程纪律段）。968 无回执面——裁决在目录侧水位幂等，本端不重试
// 不记账回执（断链缺口由 kUp 全量镜像重放收敛）。

#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/link/route_table.hpp"
#include "apollo/net/wire/contract_wire.hpp"
#include "apollo_contract.h"

#include <chrono>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

namespace world_app {

#if !defined(_WIN32)

namespace {

namespace link = apollo::net::link;
namespace wire = apollo::net::wire;
using apollo::contract::gen::MsgId;

constexpr std::uint16_t id_of(MsgId id) { return static_cast<std::uint16_t>(id); }
constexpr std::uint16_t kProjectionId = id_of(MsgId::space_position_projection);
constexpr char kLoginEndpoint[] = "login-proj";  // 对端组件名（login passive
                                                 // 端点按握手自报名寻路，本端
                                                 // 链路名独立于 964 注册链）

// 镜像条目（当前已提交视图；kUp 重放事实源）
struct MirrorEntry {
    std::uint64_t space_id = 0;
    std::uint64_t cell_id = 0;
    std::uint64_t assignment_version = 0;
    std::uint64_t management_epoch = 0;
};

class M1PositionProjectionClientImpl final : public M1PositionProjectionClient {
public:
    explicit M1PositionProjectionClientImpl(M1PositionProjectionConfig config)
        : config_(std::move(config)) {
        if (config_.self_name.empty()) {
            config_.self_name = "world-proj-" + std::to_string(config_.world_id);
        }
    }

    bool start() override {
        if (manager_) {
            return true;  // 已在运行（幂等）
        }
        // 入口校验显形（配置缺位/路由表不可读 = 拒起，不带隐式回退）
        if (config_.login_host.empty() || config_.login_port == 0
            || config_.world_id == 0 || config_.route_json.empty()) {
            return false;
        }
        route_table_ = std::make_unique<link::RouteTable>();
        if (!load_route_table(*route_table_)) {
            route_table_.reset();
            return false;
        }

        link::LinkManagerConfig mc;
        mc.self_name = config_.self_name;
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
        // 968 单向面：无下行 handler 装配（目录无回执；裁决在目录侧水位）。
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
        manager_->pump(now_ms);
        // kUp 跃迁（首连/断链重连）即全量镜像重放（D05 §1.2 目录重启重建
        // ——同版本幂等在目录侧收敛，重放不重复记账）。
        const bool up = link_up();
        if (up && !up_seen_) {
            up_seen_ = true;
            replay_mirror();
        } else if (!up) {
            up_seen_ = false;
        }
    }

    bool linkUp() const override { return link_up(); }

    void publish(std::uint64_t space_id, std::uint64_t cell_id,
                 std::uint64_t assignment_version,
                 std::uint64_t management_epoch, bool ready) override {
        // 镜像先行（重放事实源）：claim 入表 / 墓碑抹除。镜像 = World 当前
        // 已提交视图，version 0 的 claim 是非法发布（962 版本自 1 起）不记账。
        if (ready && cell_id != 0) {
            if (assignment_version == 0) {
                return;
            }
            mirror_[space_id] = MirrorEntry{space_id, cell_id,
                                            assignment_version, management_epoch};
        } else if (!ready && cell_id == 0) {
            mirror_.erase(space_id);
        } else {
            return;  // ready/cell_id 不配对：解耦面违例，不发布不记账
        }
        send_frame(space_id, cell_id, assignment_version, management_epoch, ready);
    }

    M1PositionProjectionState state() const override {
        M1PositionProjectionState s;
        s.sent = state_.sent;
        s.replays = state_.replays;
        s.mirrored = mirror_.size();
        return s;
    }

private:
    bool link_up() const {
        if (!manager_) {
            return false;
        }
        auto* l = manager_->link(kLoginEndpoint);
        return l != nullptr && l->state() == link::LinkState::kUp;
    }

    void replay_mirror() {
        ++state_.replays;
        for (const auto& kv : mirror_) {
            const auto& e = kv.second;
            send_frame(e.space_id, e.cell_id, e.assignment_version,
                       e.management_epoch, true);
        }
    }

    void send_frame(std::uint64_t space_id, std::uint64_t cell_id,
                    std::uint64_t assignment_version,
                    std::uint64_t management_epoch, bool ready) {
        auto* l = manager_ ? manager_->link(kLoginEndpoint) : nullptr;
        if (l == nullptr) {
            return;  // 未启动/链路缺席：镜像先行，kUp 重放补投
        }
        // 968 六字段：world_id / space_id / cell_id / assignment_version /
        // management_epoch / ready（受理面同序解）。
        std::vector<wire::FieldValue> fields(6);
        fields[0].integer = config_.world_id;
        fields[1].integer = static_cast<std::int64_t>(space_id);
        fields[2].integer = static_cast<std::int64_t>(cell_id);
        fields[3].integer = static_cast<std::int64_t>(assignment_version);
        fields[4].integer = static_cast<std::int64_t>(management_epoch);
        fields[5].integer = ready ? 1 : 0;
        std::vector<std::uint8_t> payload;
        if (!wire::encode_payload(kProjectionId, fields, payload)) {
            return;
        }
        if (l->send(kProjectionId, payload.data(), payload.size(),
                    link::SendClass::kReliable) == link::SendCode::kAccepted) {
            ++state_.sent;
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

    M1PositionProjectionConfig config_;
    std::unique_ptr<link::RouteTable> route_table_;
    std::unique_ptr<link::LinkManager> manager_;
    bool up_seen_ = false;
    std::unordered_map<std::uint64_t, MirrorEntry> mirror_;
    M1PositionProjectionState state_;  // 泵上下文同线程写；state() 同线程读
};

} // namespace

std::unique_ptr<M1PositionProjectionClient> makeM1PositionProjectionClient(
    M1PositionProjectionConfig config) {
    return std::make_unique<M1PositionProjectionClientImpl>(std::move(config));
}

#else // _WIN32

namespace {

// Windows：link TCP 底座不进本批（POSIX only）——镜像记账照常（重放事实源
// 不因面关失真），发送面恒缺席（kUp 永不成立 = 零发送）。
class NullPositionProjectionClient final : public M1PositionProjectionClient {
public:
    bool start() override { return false; }
    void stop() override {}
    void pump(std::uint64_t) override {}
    bool linkUp() const override { return false; }
    void publish(std::uint64_t space_id, std::uint64_t cell_id,
                 std::uint64_t assignment_version, std::uint64_t, bool ready) override {
        if (ready && cell_id != 0 && assignment_version != 0) {
            mirror_[space_id] = assignment_version;
        } else if (!ready && cell_id == 0) {
            mirror_.erase(space_id);
        }
    }
    M1PositionProjectionState state() const override {
        M1PositionProjectionState s;
        s.mirrored = mirror_.size();
        return s;
    }

private:
    std::unordered_map<std::uint64_t, std::uint64_t> mirror_;
};

} // namespace

std::unique_ptr<M1PositionProjectionClient> makeM1PositionProjectionClient(
    M1PositionProjectionConfig /*config*/) {
    return std::make_unique<NullPositionProjectionClient>();
}

#endif // !_WIN32

} // namespace world_app
