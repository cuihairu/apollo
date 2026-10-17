// fleetctl — 编队探针（P0-1 路1 演练量测面：backup-revive §8 验收的
// 「更新期间新玩家可登录成功率」量测件）：对 baseappmgr M1 控制面发
// AssignWorld / ResolveRoute / Migrate，单行打印结果 + 按结果给退出码
// （drill 脚本断言面）。客户端装配 = login m1_base_client 同型：
// RouteTable + LinkManager active 端点 + RpcClient 自泵线程——call 同步
// 等待在主线程（禁泵线程纪律不变）。
//
// eval 子命令（scripting-lua §7.1 attach transport 的 admin 入口）：对
// 目标进程（默认 world）发 944 debug_eval_request 裸帧 + 等 945 回执——
// eval 结果晚一拍（Scene tick 边界执行），不可走 rpc 同步面，故 ControlDial
// 另开契约裸帧 handler/send 面。
//
// 三态可观测（演练断言的三种录取面）：
//   exit 0 = ASSIGN_OK / RESOLVE_OK / MIGRATE_OK / EVAL_OK（闸开/迁移成/求值成）
//   exit 2 = *_REJECTED/*_FAILED/EVAL_FAILED （排空闸闭/迁移拒/求值败）
//   exit 3 = UNREACHABLE（链不通/超时）     （进程更新中：拉起窗）
// exit 1 = 用法/装配错误。

#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/link/route_table.hpp"
#include "apollo/net/linkrpc/rpc_channel.hpp"
#include "apollo/net/wire/contract_wire.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"
#include "base/migration_wire.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#if defined(_WIN32)
int main() {
    std::cerr << "fleetctl: link TCP face is POSIX-only" << std::endl;
    return 1;
}
#else

namespace {

// 别名收在匿名命名空间内——::link 是 POSIX 系统调用函数，全局别名同名
// 冲突（m1_base_client 在 login 域内做别名同理）。
namespace link = apollo::net::link;
namespace protocol = apollo::protocol;
namespace migration = base::migration;
namespace wire = apollo::net::wire;
using apollo::contract::gen::MsgId;

constexpr std::uint16_t id_of(MsgId id) {
    return static_cast<std::uint16_t>(id);
}

using namespace std::chrono_literals;
constexpr auto kPumpTick = 10ms;    // 自泵拍（m1_base_client 同款）
constexpr auto kUpTimeout = 5000ms; // 等链路 kUp 上限
constexpr auto kRpcTimeout = 3000ms;

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
    std::string peer;                 // 空 = 按命令取默认（eval→world，余→baseappmgr）
    std::uint64_t scene_id = 0;       // 0 = 默认 scene
    std::string source;               // Lua 源（表达式优先、语句回退）
};

void printUsage() {
    std::cout << "fleetctl - fleet probe against M1 control/debug faces\n"
              << "Usage:\n"
              << "  fleetctl assign  [--host H] [--port P] --player N [--world W] [--map M]\n"
              << "                   [--instance I] [--space S] [--route-version V]\n"
              << "  fleetctl resolve [--host H] [--port P] --player N\n"
              << "  fleetctl migrate [--host H] [--port P] --player N --source-port SP\n"
              << "                   --target-port TP --target-zone Z\n"
              << "                   [--source-host SH] [--target-host TH]\n"
              << "  fleetctl eval    [--host H] --port P [--peer NAME] [--scene S] --source SRC\n"
              << "                   (default peer=world, port 9101 = WorldApp M1 link)\n"
              << "Exit codes: 0 admitted/ok / 2 rejected/failed / 3 unreachable / 1 usage or setup\n";
}

bool parseArgs(int argc, char* argv[], Options& opt) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&]() -> const char* {
            return (i + 1 < argc) ? argv[++i] : nullptr;
        };
        if (arg == "assign" || arg == "resolve" || arg == "migrate" || arg == "eval") {
            opt.command = arg;
        } else if (arg == "--host") {
            if (const auto* v = next()) opt.host = v; else return false;
        } else if (arg == "--port") {
            if (const auto* v = next()) opt.port = static_cast<std::uint16_t>(std::atoi(v));
            else return false;
        } else if (arg == "--source-host") {
            if (const auto* v = next()) opt.source_host = v; else return false;
        } else if (arg == "--source-port") {
            if (const auto* v = next()) opt.source_port = static_cast<std::uint16_t>(std::atoi(v));
            else return false;
        } else if (arg == "--target-host") {
            if (const auto* v = next()) opt.target_host = v; else return false;
        } else if (arg == "--target-port") {
            if (const auto* v = next()) opt.target_port = static_cast<std::uint16_t>(std::atoi(v));
            else return false;
        } else if (arg == "--target-zone") {
            if (const auto* v = next()) opt.target_zone_id = static_cast<std::uint32_t>(std::atoi(v));
            else return false;
        } else if (arg == "--player") {
            if (const auto* v = next()) opt.player_id = std::strtoull(v, nullptr, 10);
            else return false;
        } else if (arg == "--world") {
            if (const auto* v = next()) opt.world_id = static_cast<std::uint32_t>(std::atoi(v));
            else return false;
        } else if (arg == "--map") {
            if (const auto* v = next()) opt.map_id = std::strtoull(v, nullptr, 10);
            else return false;
        } else if (arg == "--instance") {
            if (const auto* v = next()) opt.instance_id = std::strtoull(v, nullptr, 10);
            else return false;
        } else if (arg == "--space") {
            if (const auto* v = next()) opt.space_id = std::strtoull(v, nullptr, 10);
            else return false;
        } else if (arg == "--route-version") {
            if (const auto* v = next()) opt.route_version = std::strtoull(v, nullptr, 10);
            else return false;
        } else if (arg == "--peer") {
            if (const auto* v = next()) opt.peer = v; else return false;
        } else if (arg == "--scene") {
            if (const auto* v = next()) opt.scene_id = std::strtoull(v, nullptr, 10);
            else return false;
        } else if (arg == "--source") {
            if (const auto* v = next()) opt.source = v; else return false;
        } else {
            return false;
        }
    }
    if (opt.command.empty() || opt.port == 0) {
        return false;
    }
    if (opt.peer.empty()) {
        opt.peer = opt.command == "eval" ? "world" : "baseappmgr";
    }
    if (opt.command == "eval") {
        return !opt.source.empty();  // Lua 源必给（0 保留关联键由本进程单调分配）
    }
    if (opt.player_id == 0) {
        return false;
    }
    if (opt.command == "migrate") {
        return opt.source_port != 0 && opt.target_port != 0 && opt.target_zone_id != 0;
    }
    return true;
}

std::uint64_t now_ms() {
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

int runAssign(const Options& opt, ControlDial& dial) {
    protocol::PlayerAssignWorldRequest req;
    req.playerId = opt.player_id;
    req.worldId = opt.world_id;
    req.mapId = opt.map_id;
    req.instanceId = opt.instance_id;
    req.spaceId = opt.space_id;
    req.routeVersion = opt.route_version;

    const auto reply = dial.call(protocol::MessageCodec::encode(req));
    if (!reply) {
        std::cout << "ASSIGN_UNREACHABLE player=" << opt.player_id << std::endl;
        return 3;
    }
    auto header = protocol::MessageCodec::parseHeader(*reply);
    if (static_cast<protocol::MessageType>(header.type)
        != protocol::MessageType::PLAYER_ASSIGN_WORLD_RESPONSE) {
        std::cout << "ASSIGN_UNREACHABLE player=" << opt.player_id
                  << " (bad reply type " << header.type << ")" << std::endl;
        return 3;
    }
    const auto res = protocol::MessageCodec::decodeBody<protocol::PlayerAssignWorldResponse>(
        std::vector<std::uint8_t>(reply->begin() + sizeof(protocol::MessageHeader),
                                  reply->end()));
    if (res.success) {
        std::cout << "ASSIGN_OK player=" << opt.player_id << " world=" << opt.world_id
                  << " route_version=" << res.routeVersion << std::endl;
        return 0;
    }
    std::cout << "ASSIGN_REJECTED player=" << opt.player_id
              << " error=" << res.errorMessage << std::endl;
    return 2;
}

int runResolve(const Options& opt, ControlDial& dial) {
    protocol::PlayerResolveRouteRequest req;
    req.playerId = opt.player_id;
    req.sessionId = 0;  // 按 playerId 反查

    const auto reply = dial.call(protocol::MessageCodec::encode(req));
    if (!reply) {
        std::cout << "RESOLVE_UNREACHABLE player=" << opt.player_id << std::endl;
        return 3;
    }
    auto header = protocol::MessageCodec::parseHeader(*reply);
    if (static_cast<protocol::MessageType>(header.type)
        != protocol::MessageType::PLAYER_RESOLVE_ROUTE_RESPONSE) {
        std::cout << "RESOLVE_UNREACHABLE player=" << opt.player_id
                  << " (bad reply type " << header.type << ")" << std::endl;
        return 3;
    }
    const auto res = protocol::MessageCodec::decodeBody<protocol::PlayerResolveRouteResponse>(
        std::vector<std::uint8_t>(reply->begin() + sizeof(protocol::MessageHeader),
                                  reply->end()));
    if (res.success) {
        std::cout << "RESOLVE_OK player=" << res.playerId
                  << " gateway=" << res.gatewayId << " addr=" << res.gatewayAddr
                  << " world=" << res.worldId << " map=" << res.mapId
                  << " instance=" << res.instanceId
                  << " route_version=" << res.routeVersion << std::endl;
        return 0;
    }
    std::cout << "RESOLVE_FAILED player=" << opt.player_id
              << " error=" << res.errorMessage << std::endl;
    return 2;
}

// 编排命令（P0-2 路 2 五步①③④ 入口；同步等 943——耗时字段即 §9.8 实测
// 出数面）。REQ/REPLY wire 面 = migration 手写族（TYPE 钉生成器 MsgId）。
int runMigrate(const Options& opt, ControlDial& dial) {
    migration::MigrateRequest req;
    req.player_id = opt.player_id;
    req.source_host = opt.source_host;
    req.source_port = opt.source_port;
    req.target_host = opt.target_host;
    req.target_port = opt.target_port;
    req.target_zone_id = opt.target_zone_id;

    const auto reply = dial.call(migration::encodeFrame(req));
    if (!reply) {
        std::cout << "MIGRATE_UNREACHABLE player=" << opt.player_id << std::endl;
        return 3;
    }
    migration::MigrateReply res;
    protocol::SessionID session = 0;
    if (!migration::decodeFrame(*reply, res, session)) {
        std::cout << "MIGRATE_UNREACHABLE player=" << opt.player_id
                  << " (bad reply type "
                  << protocol::MessageCodec::parseHeader(*reply).type << ")"
                  << std::endl;
        return 3;
    }
    if (res.ok) {
        std::cout << "MIGRATE_OK player=" << res.player_id
                  << " zone=" << opt.target_zone_id
                  << " epoch=" << res.new_epoch
                  << " freeze_ms=" << res.freeze_ms
                  << " handoff_ms=" << res.handoff_ms
                  << " total_ms=" << res.total_ms << std::endl;
        return 0;
    }
    std::cout << "MIGRATE_FAILED player=" << res.player_id
              << " error=" << res.error_message
              << " total_ms=" << res.total_ms << std::endl;
    return 2;
}

// 945 回执捕获（泵线程写 / 主线程读；eval_id 关联——stale 回执忽略）。
struct EvalCapture {
    std::mutex m;
    std::uint64_t eval_id = 0;
    bool ok = false;
    std::string text;  // ok=1 → result；ok=0 → error 原文
    bool got = false;
};

// 脚本调试 eval（§7.1 attach transport 契约面）：944 裸帧直发，945 按
// eval_id 异步回传（Scene tick 边界执行——结果晚一拍，等回执而非 rpc 同步）。
int runEval(const Options& opt, ControlDial& dial, std::uint64_t eval_id) {
    EvalCapture cap;
    dial.set_frame_handler(
        id_of(MsgId::debug_eval_result),
        [&cap](std::uint16_t, const std::uint8_t* data, std::size_t size) {
            std::vector<wire::FieldValue> fields;
            if (!wire::decode_payload(id_of(MsgId::debug_eval_result), data, size, fields)
                || fields.size() != 4) {
                return;
            }
            std::lock_guard<std::mutex> lock(cap.m);
            cap.eval_id = static_cast<std::uint64_t>(fields[0].integer);
            cap.ok = fields[1].integer != 0;
            cap.text = fields[1].integer != 0 ? fields[2].text : fields[3].text;
            cap.got = true;
        });

    std::vector<wire::FieldValue> fields(3);
    fields[0].integer = static_cast<std::int64_t>(eval_id);
    fields[1].integer = static_cast<std::int64_t>(opt.scene_id);
    fields[2].text = opt.source;
    std::vector<std::uint8_t> payload;
    if (!wire::encode_payload(id_of(MsgId::debug_eval_request), fields, payload)
        || !dial.send_frame(id_of(MsgId::debug_eval_request), payload)) {
        std::cout << "EVAL_UNREACHABLE scene=" << opt.scene_id
                  << " (send failed)" << std::endl;
        return 3;
    }

    const auto deadline = Clock::now() + kRpcTimeout;  // 含 Scene tick 窗口的一拍
    while (Clock::now() < deadline) {
        {
            std::lock_guard<std::mutex> lock(cap.m);
            if (cap.got && cap.eval_id == eval_id) {
                if (cap.ok) {
                    std::cout << "EVAL_OK scene=" << opt.scene_id
                              << " result=" << cap.text << std::endl;
                    return 0;
                }
                std::cout << "EVAL_FAILED scene=" << opt.scene_id
                          << " error=" << cap.text << std::endl;
                return 2;
            }
        }
        std::this_thread::sleep_for(kPumpTick);
    }
    std::cout << "EVAL_UNREACHABLE scene=" << opt.scene_id
              << " (no reply within timeout)" << std::endl;
    return 3;
}

} // namespace

int main(int argc, char* argv[]) {
    Options opt;
    if (!parseArgs(argc, argv, opt)) {
        printUsage();
        return 1;
    }

    ControlDial dial(APOLLO_FLEETCTL_CONTRACT_ROUTE_JSON, opt.peer, opt.host, opt.port);
    if (!dial.ready()) {
        std::cerr << "fleetctl: control face setup failed (route table missing)"
                  << std::endl;
        return 1;
    }
    if (!dial.waitUp()) {
        std::cout << "UNREACHABLE host=" << opt.host << " port=" << opt.port << std::endl;
        return 3;
    }

    if (opt.command == "assign") {
        return runAssign(opt, dial);
    }
    if (opt.command == "migrate") {
        return runMigrate(opt, dial);
    }
    if (opt.command == "eval") {
        // 关联键本进程单调分配（0 保留；单发进程恒 1 也满足对端关联语义）
        static std::atomic<std::uint64_t> next_eval_id{1};
        return runEval(opt, dial, next_eval_id.fetch_add(1, std::memory_order_relaxed));
    }
    return runResolve(opt, dial);
}

#endif // !_WIN32
