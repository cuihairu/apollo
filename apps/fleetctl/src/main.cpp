// fleetctl — 编队探针（P0-1 路1 演练量测面：backup-revive §8 验收的
// 「更新期间新玩家可登录成功率」量测件）：对 baseappmgr M1 控制面发
// AssignWorld / ResolveRoute，单行打印结果 + 按结果给退出码（drill 脚本
// 断言面）。客户端装配 = login m1_base_client 同型：RouteTable + LinkManager
// active 端点 + RpcClient 自泵线程——call 同步等待在主线程（禁泵线程
// 纪律不变）。
//
// 三态可观测（演练断言的三种录取面）：
//   exit 0 = ASSIGN_OK / RESOLVE_OK            （闸开：新玩家可进）
//   exit 2 = ASSIGN_REJECTED（success=false）   （排空闸闭：拒新不拒链）
//   exit 3 = UNREACHABLE（链不通/超时）          （进程更新中：拉起窗）
// exit 1 = 用法/装配错误。

#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/link/route_table.hpp"
#include "apollo/net/linkrpc/rpc_channel.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
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

using namespace std::chrono_literals;
constexpr auto kPumpTick = 10ms;    // 自泵拍（m1_base_client 同款）
constexpr auto kUpTimeout = 5000ms; // 等链路 kUp 上限
constexpr auto kRpcTimeout = 3000ms;

using Clock = std::chrono::steady_clock;

struct Options {
    std::string command;              // assign / resolve
    std::string host = "127.0.0.1";
    std::uint16_t port = 9105;        // dev 编队 baseappmgr M1 control（dev_fleet 布局）
    protocol::PlayerID player_id = 0;
    std::uint32_t world_id = 1;
    std::uint64_t map_id = 1;
    std::uint64_t instance_id = 0;
    std::uint64_t space_id = 0;
    std::uint64_t route_version = 1;
};

void printUsage() {
    std::cout << "fleetctl - fleet probe against baseappmgr M1 control face\n"
              << "Usage:\n"
              << "  fleetctl assign  [--host H] [--port P] --player N [--world W] [--map M]\n"
              << "                   [--instance I] [--space S] [--route-version V]\n"
              << "  fleetctl resolve [--host H] [--port P] --player N\n"
              << "Exit codes: 0 admitted / 2 rejected / 3 unreachable / 1 usage or setup\n";
}

bool parseArgs(int argc, char* argv[], Options& opt) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&]() -> const char* {
            return (i + 1 < argc) ? argv[++i] : nullptr;
        };
        if (arg == "assign" || arg == "resolve") {
            opt.command = arg;
        } else if (arg == "--host") {
            if (const auto* v = next()) opt.host = v; else return false;
        } else if (arg == "--port") {
            if (const auto* v = next()) opt.port = static_cast<std::uint16_t>(std::atoi(v));
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
        } else {
            return false;
        }
    }
    return !opt.command.empty() && opt.player_id != 0 && opt.port != 0;
}

std::uint64_t now_ms() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now().time_since_epoch()).count());
}

// 一次性拨号会话：装配（kUp 前装齐路由表/白名单/客户端——rpc_channel
// 纪律）→ 自泵 → 等 kUp → call → 收线（先停泵再拆面）。
class ControlDial {
public:
    ControlDial(const std::string& route_json, const std::string& host, std::uint16_t port) {
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
        ep.name = "baseappmgr";  // 对端 G-1 组件名（握手身份 = 其 Link self_name）
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
        peer->dispatcher().allow_domain("internal");  // RPC 信封族入闸
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
        auto* peer = manager_ ? manager_->link("baseappmgr") : nullptr;
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

} // namespace

int main(int argc, char* argv[]) {
    Options opt;
    if (!parseArgs(argc, argv, opt)) {
        printUsage();
        return 1;
    }

    ControlDial dial(APOLLO_FLEETCTL_CONTRACT_ROUTE_JSON, opt.host, opt.port);
    if (!dial.ready()) {
        std::cerr << "fleetctl: control face setup failed (route table missing)"
                  << std::endl;
        return 1;
    }
    if (!dial.waitUp()) {
        std::cout << "UNREACHABLE host=" << opt.host << " port=" << opt.port << std::endl;
        return 3;
    }

    return opt.command == "assign" ? runAssign(opt, dial) : runResolve(opt, dial);
}

#endif // !_WIN32
