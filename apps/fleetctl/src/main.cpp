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
#include "fleetctl_internal.hpp"
#include <atomic>
#include <cstdlib>

#if defined(_WIN32)
int main() {
    std::cerr << "fleetctl: link TCP face is POSIX-only" << std::endl;
    return 1;
}
#else
namespace fleetctl {

namespace {

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
              << "  fleetctl module  --target H:P [--target H:P ...] [--canary H:P ...]\n"
              << "                   --name NAME --version V (--source-file F | --source SRC)\n"
              << "                   [--deps A,B] [--observe-ticks T] [--fault-threshold X]\n"
              << "                   [--timeout S] [--peer NAME]\n"
              << "                   (跨 World 模块编排：canary 先行，观察窗全绿再推余下)\n"
              << "  fleetctl drain   --target H:P [--deadline S] [--timeout S] [--peer NAME]\n"
              << "                   (排空：gateway 停新连接 / world(--peer world) 停分新副本；\n"
              << "                    既有面服务至自然退出或期限强停；default peer=gateway)\n"
              << "  fleetctl retire  --target H:P --cell N [--cancel] [--peer NAME]\n"
              << "                   (缩容：向 world 置该 cell 的缩容意图——停止新认领、\n"
              << "                    名下 Space 回池，在途自然排空；--cancel 只撤意图\n"
              << "                    不取回已接管者；default peer=world)\n"
              << "Exit codes: 0 admitted/ok / 2 rejected/failed / 3 unreachable / 1 usage or setup\n";
}

bool parseArgs(int argc, char* argv[], Options& opt) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&]() -> const char* {
            return (i + 1 < argc) ? argv[++i] : nullptr;
        };
        if (arg == "assign" || arg == "resolve" || arg == "migrate" || arg == "eval"
            || arg == "module" || arg == "drain" || arg == "retire") {
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
        } else if (arg == "--target" || arg == "--canary") {
            if (const auto* v = next()) {
                std::string item = v;
                std::string part;
                for (std::size_t i = 0; i <= item.size(); ++i) {
                    if (i == item.size() || item[i] == ',') {
                        if (!part.empty()) {
                            (arg == "--target" ? opt.targets : opt.canaries)
                                .push_back(part);
                        }
                        part.clear();
                    } else {
                        part.push_back(item[i]);
                    }
                }
            } else {
                return false;
            }
        } else if (arg == "--name") {
            if (const auto* v = next()) opt.module_name = v; else return false;
        } else if (arg == "--version") {
            if (const auto* v = next()) opt.module_version = std::strtoull(v, nullptr, 10);
            else return false;
        } else if (arg == "--deps") {
            if (const auto* v = next()) opt.module_deps = v; else return false;
        } else if (arg == "--source-file") {
            if (const auto* v = next()) opt.source_file = v; else return false;
        } else if (arg == "--observe-ticks") {
            if (const auto* v = next()) opt.observe_ticks = std::strtoull(v, nullptr, 10);
            else return false;
        } else if (arg == "--fault-threshold") {
            if (const auto* v = next()) opt.fault_threshold = std::strtoull(v, nullptr, 10);
            else return false;
        } else if (arg == "--timeout") {
            if (const auto* v = next())
                opt.rollout_timeout_s = static_cast<std::uint32_t>(std::atoi(v));
            else return false;
        } else if (arg == "--deadline") {
            if (const auto* v = next()) opt.drain_deadline_s = std::strtoull(v, nullptr, 10);
            else return false;
        } else if (arg == "--cell") {
            if (const auto* v = next()) opt.cell_id = std::strtoull(v, nullptr, 10);
            else return false;
        } else if (arg == "--cancel") {
            opt.retire_cancel = true;
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
        opt.peer = (opt.command == "eval" || opt.command == "module"
                     || opt.command == "retire")
                       ? "world"
                       : (opt.command == "drain" ? "gateway" : "login");
    }
    if (opt.command == "eval") {
        return !opt.source.empty();  // Lua 源必给（0 保留关联键由本进程单调分配）
    }
    if (opt.command == "drain") {
        // 编排面：目标必给（单 target H:P = 控制受理面；--peer 选组件——
        // gateway 缺省 954/955，world 956/957）
        return !opt.targets.empty();
    }
    if (opt.command == "retire") {
        // 编排面：目标必给（单 target H:P = World M1 管理面）+ cell id 非 0
        //（0 = 非法，对端亦拒）
        return !opt.targets.empty() && opt.cell_id != 0;
    }
    if (opt.command == "module") {
        // 编排面：目标集 + 模块名/版本 + 源（文件或内联）必给；canary 缺省
        // = 首个 target（port 不参本命令——各 target 自带 H:P）
        return !opt.targets.empty() && !opt.module_name.empty()
            && opt.module_version != 0
            && !(opt.source_file.empty() && opt.source.empty());
    }
    if (opt.player_id == 0) {
        return false;
    }
    if (opt.command == "migrate") {
        return opt.source_port != 0 && opt.target_port != 0 && opt.target_zone_id != 0;
    }
    return true;
}
} // namespace
} // namespace fleetctl

using fleetctl::ControlDial;
using fleetctl::parseArgs;
using fleetctl::printUsage;
using fleetctl::Options;
using fleetctl::runAssign;
using fleetctl::runDrain;
using fleetctl::runEval;
using fleetctl::runMigrate;
using fleetctl::runModule;
using fleetctl::runResolve;
using fleetctl::runRetire;

int main(int argc, char* argv[]) {
    Options opt;
    if (!parseArgs(argc, argv, opt)) {
        printUsage();
        return 1;
    }

    // module / drain 编排自管拨号（一 target 一条 ControlDial；共享单拨号
    // 只服单对端命令），先行分流。
    if (opt.command == "module") {
        return runModule(opt);
    }
    if (opt.command == "drain") {
        return runDrain(opt);
    }
    if (opt.command == "retire") {
        return runRetire(opt);
    }

    // 契约代次可注入（APOLLO_FLEETCTL_ROUTE_JSON）：滚动发布演练以旧代
    // 路由表拨新代服务面（兼容窗验收；sdk-contract §6）。缺省仍用编译期
    // 契约（与全端点同源）。
    const char* route_override = std::getenv("APOLLO_FLEETCTL_ROUTE_JSON");
    ControlDial dial(route_override && *route_override != 0
                         ? route_override
                         : APOLLO_FLEETCTL_CONTRACT_ROUTE_JSON,
                     opt.peer, opt.host, opt.port);
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
