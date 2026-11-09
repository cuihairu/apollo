// fleetctl 拆分共享内面（LAY-005）：别名/常量/Options/ControlDial 与各
// 子命令入口声明。实现单元——main.cpp（入口与用法解析）/rpc_cmds
// (assign/resolve/migrate/eval)/module/drain/retire。
#pragma once

#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/link/route_table.hpp"
#include "apollo/net/linkrpc/rpc_channel.hpp"
#include "apollo/net/wire/contract_wire.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"
#include "player_store/migration_wire.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>

namespace fleetctl {

// 别名收在匿名命名空间内——::link 是 POSIX 系统调用函数，全局别名同名
// 冲突（m1_base_client 在 login 域内做别名同理）。
namespace link = apollo::net::link;
namespace protocol = apollo::protocol;
namespace migration = player_store::migration;
namespace wire = apollo::net::wire;
using apollo::contract::gen::MsgId;

constexpr std::uint16_t id_of(MsgId id) {
    return static_cast<std::uint16_t>(id);
}

using namespace std::chrono_literals;
inline constexpr auto kPumpTick = 10ms;    // 自泵拍（m1_base_client 同款）
inline constexpr auto kUpTimeout = 5000ms; // 等链路 kUp 上限
inline constexpr auto kRpcTimeout = 3000ms;

using Clock = std::chrono::steady_clock;

struct Options {
    std::string command;              // assign / resolve / migrate / eval
    std::string host = "127.0.0.1";
    std::uint16_t port = 9105;        // dev 编队 baseappmgr M1 control（dev_fleet 布局）
    protocol::PlayerID player_id = 0;
    std::uint32_t world_id = 1;
    std::uint64_t map_id = 1;
    std::uint64_t instance_id = 0;
    std::uint64_t space_id = 0;
    std::uint64_t route_version = 1;
    // migrate 面（P0-2 路 2 编排命令；源/目标 zone M1 地址 + 落点 zone）
    std::string source_host = "127.0.0.1";
    std::uint16_t source_port = 0;
    std::string target_host = "127.0.0.1";
    std::uint16_t target_port = 0;
    std::uint32_t target_zone_id = 0;
    // eval 面（§7.1 attach transport；目标 peer 自报名 + 目标 scene + Lua 源）
    std::string peer;                 // 空 = 按命令取默认（eval/module→world，余→baseappmgr）
    std::uint64_t scene_id = 0;       // 0 = 默认 scene
    std::string source;               // Lua 源（表达式优先、语句回退）
    // module 面（P4 仅代码更新；跨 World 模块编排）：目标集 + 灰度子集 +
    // 模块源（--source 内联或 --source-file 文件）
    std::vector<std::string> targets;     // H:P 列表（全量目标）
    std::vector<std::string> canaries;    // H:P 列表（灰度子集；空 = 首个 target）
    std::string module_name;
    std::uint64_t module_version = 0;
    std::string module_deps;              // 逗号分隔依赖名
    std::string source_file;              // 模块源文件（优先于 --source）
    std::uint64_t observe_ticks = 100;    // 世界侧灰度观察窗（tick 计）
    std::uint64_t fault_threshold = 0;    // 窗内 fault 上限（超阈早失败）
    std::uint32_t rollout_timeout_s = 60; // 每目标终态轮询上限（秒）
    // drain 面（P4 排空批）：begin 期限（0 = 无期限自然排空；终态轮询复用
    // --timeout）
    std::uint64_t drain_deadline_s = 0;
    // retire 面（L104 缩容管理命令面，world 契约 963/966）：目标 cell id +
    // --cancel 走 cancel 语义
    std::uint64_t cell_id = 0;
    bool retire_cancel = false;
};

inline std::uint64_t now_ms() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now().time_since_epoch()).count());
}

// 一次性拨号会话：装配（kUp 前装齐路由表/白名单/客户端——rpc_channel
// 纪律）→ 自泵 → 等 kUp → call → 收线（先停泵再拆面）。peer 名参数化
// （baseappmgr 控制面 / World 调试面共用）；契约裸帧面（944/945 族）与
// rpc 面并存——eval 回执晚一拍，handler 捕获 + 主线程轮询关账。
class ControlDial {
public:
    ControlDial(const std::string& route_json, const std::string& peer_name,
                const std::string& host, std::uint16_t port)
        : peer_name_(peer_name) {
        route_table_ = std::make_unique<link::RouteTable>();
#if APOLLO_NET_LINK_HAS_JSON
        std::ifstream in(route_json, std::ios::binary);
        std::ostringstream buffer;
        if (!in || !link::load_route_table_json(
                       (buffer << in.rdbuf(), buffer.str()), *route_table_)) {
            route_table_.reset();
            return;  // 装配错误显形：ready() false，不带隐式回退
        }
#else
        (void)route_json;
        route_table_.reset();
        return;
#endif

        link::LinkManagerConfig mc;
        mc.self_name = "fleetctl";  // 拨入者按自报名入对端表（fan-out 命名）
        mc.route_version = link::route_version_from_hash(route_table_->schema_hash());
        mc.compat_route_versions = link::compat_route_versions_from_env();
        manager_ = std::make_unique<link::LinkManager>(mc);

        link::EndpointConfig ep;
        ep.name = peer_name_;  // 对端 G-1 组件名（握手身份 = 其 Link self_name）
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
        peer->dispatcher().allow_domain("internal");  // RPC 信封族 + eval 裸帧入闸
        client_ = std::make_unique<apollo::net::linkrpc::RpcClient>(*peer);

        running_ = true;
        pump_ = std::thread([this]() {
            while (running_.load(std::memory_order_acquire)) {
                manager_->pump(now_ms());
                std::this_thread::sleep_for(kPumpTick);
            }
        });
    }

    ~ControlDial() { shutdown(); }

    ControlDial(const ControlDial&) = delete;
    ControlDial& operator=(const ControlDial&) = delete;

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

    // 契约裸帧受理面（泵线程分发；kUp 前挂——对端只会在收到本端 944 后
    // 才可能回 945，先挂后发无竞速窗）。
    void set_frame_handler(
        std::uint16_t msg_id,
        std::function<void(std::uint16_t, const std::uint8_t*, std::size_t)> fn) {
        auto* peer = manager_ ? manager_->link(peer_name_) : nullptr;
        if (peer != nullptr) {
            peer->dispatcher().set_handler(msg_id, std::move(fn));
        }
    }

    // 契约裸帧发送（kReliable——944 请求不可丢；InterServerLink::send 自封
    // u16 id 信封，传裸载荷）。
    bool send_frame(std::uint16_t msg_id, const std::vector<std::uint8_t>& payload) {
        auto* peer = manager_ ? manager_->link(peer_name_) : nullptr;
        if (peer == nullptr) {
            return false;
        }
        (void)peer->send(msg_id, payload.data(), payload.size(),
                         link::SendClass::kReliable);
        return true;
    }

private:
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

    std::string peer_name_;
    std::unique_ptr<link::RouteTable> route_table_;
    std::unique_ptr<link::LinkManager> manager_;
    std::unique_ptr<apollo::net::linkrpc::RpcClient> client_;
    std::thread pump_;
    std::atomic<bool> running_{false};
};

int runAssign(const Options& opt, ControlDial& dial);
int runResolve(const Options& opt, ControlDial& dial);
int runMigrate(const Options& opt, ControlDial& dial);
int runEval(const Options& opt, ControlDial& dial, std::uint64_t eval_id);
int runModule(const Options& opt);
int runDrain(const Options& opt);
int runRetire(const Options& opt);

} // namespace fleetctl
