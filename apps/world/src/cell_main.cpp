#include "world_app/cell_main.hpp"
#include "world_app/cell_app.hpp"
#include "world_app/cell_register_client.hpp"
#include "world_app/m1_link_server.hpp"
#include "world_app/dashboard.hpp"
#include "world_app/dashboard_data.hpp"
#include "world_app/role_options.hpp"
#include "world_app/config.hpp"
#include "apollo/runtime/crash_capture.hpp"
#include "apollo/core/log/log_manager.h"
#include "apollo/script/module_rollout.hpp"
#include "apollo/net/logpush/log_push_env.hpp"
#include "apollo/net/metricspush/metrics_push_env.hpp"
#include "apollo/net/discovery/discovery.hpp"
#include "apollo/net/discovery/udp_transport.hpp"
#include "world_app/world_types.hpp"
#include <chrono>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace world_app {

namespace {

// 执行端独立入口的停机钩子与信号处理（world 共用二进制的 manager 壳各自
// 持有自己的 g_stop_request——cellapp 二进制不编 WorldApp，此面独立）。
std::function<void()> g_stop_request;

void signal_handler(int signal) {
    if (g_stop_request) {
        std::cout << "\nReceived signal " << signal << ", shutting down..."
                  << std::endl;
        g_stop_request();
    }
}

// 编队面参数（cellServer 独立入口就地解析，避免把私有辅助暴露给共享库层）。
bool is_fleet_arg(const std::string& arg) {
    return arg == "--machined-host" || arg == "--machined-port" ||
           arg == "--component-id" || arg == "--zone-id";
}

// 逗号分隔依赖名 → 列表（952 编排请求；空段跳过，全空 = 无依赖）。
std::vector<std::string> split_dep_list(const std::string& text) {
    std::vector<std::string> out;
    std::string token;
    auto flush = [&] {
        if (!token.empty()) {
            out.push_back(std::move(token));
        }
        token.clear();
    };
    for (const char c : text) {
        if (c == ',') {
            flush();
        } else if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            token.push_back(c);
        }
    }
    flush();
    return out;
}

// 错误行拼合（953 detail 多行面）。
std::string join_lines(const std::vector<std::string>& lines) {
    std::string out;
    for (const auto& l : lines) {
        if (!out.empty()) {
            out.push_back(';');
            out.push_back(' ');
        }
        out += l;
    }
    return out;
}

// 执行端出口适配（IM1Egress → M1LinkServer；与 WorldApp 内嵌适配同型；
// 链路未装配 = 尽力面落地）
struct CellLinkEgress final : public IM1Egress {
    M1LinkServer* link = nullptr;
    CellRegisterClient* register_client = nullptr;

    void sendClientDeliver(std::uint64_t session_id, std::uint32_t gateway_id,
                           const std::vector<std::uint8_t>& payload) override {
        // 切片3 独立 cell 下行回程：注册连接优先（gateway 连 WorldApp 的
        // M1，cell 的下行须经 World 转发 gateway）；直连 gateway 面保留
        //（独立 cell 直服 gateway 的未来形态/本进程自测面）。
        if (register_client) {
            register_client->sendClientDeliver(session_id, gateway_id, payload);
            return;
        }
        if (link) {
            link->sendClientDeliver(session_id, gateway_id, payload);
        }
    }
    void sendIntentAck(std::uint64_t request_id, std::int32_t code,
                       const std::string& message) override {
        // 切片2 意图转发回程：注册连接优先（World→cell 意图经注册连接进入，
        // ack 也走回程）；直连 gateway 面保留（独立 cell 直服 gateway 的
        // 未来形态/本进程自测面）。
        if (register_client && request_id != 0) {
            register_client->sendIntentAck(request_id, code, message);
            return;
        }
        if (link) {
            link->sendIntentAck(request_id, code, message);
        }
    }
    std::uint64_t receivedIntents() const override {
        return link ? link->receivedIntents() : 0;
    }
    std::uint64_t rejectedFrames() const override {
        return link ? link->rejectedFrames() : 0;
    }
};

// 从 argv 提取 WorldAppConfig（执行端只取 cell 相关字段；未知项显式拒绝，
// 与 world 共用入口同口径——本执行端为独立进程，不复用 manager 的
// loadConfig 静态面）。
WorldAppConfig load_cell_config(int argc, char* argv[], const char* prog) {
    WorldAppConfig config;
    config.role = "cell";
    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        if (arg == "--m1-port" && i + 1 < argc) {
            config.m1Port = static_cast<std::uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--register-host" && i + 1 < argc) {
            config.registerHost = argv[++i];
        } else if (arg == "--register-port" && i + 1 < argc) {
            config.registerPort = static_cast<std::uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--cell-id" && i + 1 < argc) {
            config.cellId = std::strtoull(argv[++i], nullptr, 10);
        } else if (arg == "--world-id" && i + 1 < argc) {
            const char* value_text = argv[++i];
            errno = 0;
            char* end = nullptr;
            const unsigned long long value =
                std::strtoull(value_text, &end, 10);
            if (errno != 0 || end == value_text || *end != '\0'
                || value == 0 || value > 0xFFFFFFFFULL) {
                std::cerr << prog << ": --world-id expects 1..4294967295"
                          << std::endl;
                std::exit(2);
            }
            config.worldId = static_cast<std::uint32_t>(value);
        } else if (arg == "--link-grace-ms" && i + 1 < argc) {
            config.linkGraceMs = std::strtoull(argv[++i], nullptr, 10);
        } else if (arg == "--space" && i + 1 < argc) {
            config.spaceName = argv[++i];
        } else if (arg == "--add-space" && i + 1 < argc) {
            const std::string spec = argv[++i];
            const auto colon = spec.find(':');
            if (colon == std::string::npos || colon == 0 ||
                colon + 1 == spec.size()) {
                std::cerr << prog << ": --add-space expects <id>:<name>"
                          << std::endl;
                std::exit(2);
            }
            const std::uint64_t space_id =
                std::strtoull(spec.substr(0, colon).c_str(), nullptr, 10);
            if (space_id == 0) {
                std::cerr << prog << ": --add-space id must be non-zero"
                          << std::endl;
                std::exit(2);
            }
            for (const auto& s : config.spaces) {
                if (s.space_id == space_id) {
                    std::cerr << prog << ": --add-space duplicate id "
                              << std::to_string(space_id) << std::endl;
                    std::exit(2);
                }
            }
            config.spaces.push_back({space_id, spec.substr(colon + 1)});
        } else if (arg == "--size" && i + 2 < argc) {
            config.spaceWidth = static_cast<float>(std::atof(argv[++i]));
            config.spaceHeight = static_cast<float>(std::atof(argv[++i]));
        } else if (arg == "--tick-rate" && i + 1 < argc) {
            const char* value_text = argv[++i];
            errno = 0;
            char* end = nullptr;
            const unsigned long long value =
                std::strtoull(value_text, &end, 10);
            if (errno != 0 || end == value_text || *end != '\0' || value == 0
                || value > 10000) {
                std::cerr << prog
                          << ": --tick-rate expects 1..10000 (ms)"
                          << std::endl;
                std::exit(2);
            }
            config.tickRateMs = static_cast<int>(value);
        } else if (arg == "--script-boot" && i + 1 < argc) {
            config.scriptBootFile = argv[++i];
        } else if (arg == "--script-handlers" && i + 1 < argc) {
            config.scriptHandersFile = argv[++i];
        } else if (arg == "--script-contract" && i + 1 < argc) {
            config.scriptContractFile = argv[++i];
        } else if (arg == "--dashboard-host" && i + 1 < argc) {
            config.dashboardHost = argv[++i];
        } else if (arg == "--dashboard-port" && i + 1 < argc) {
            char* parseEnd = nullptr;
            const long port = std::strtol(argv[++i], &parseEnd, 10);
            if (parseEnd == argv[i] || *parseEnd != '\0' || port < 0 ||
                port > 65535) {
                std::cerr << "cellServer: --dashboard-port must be 0..65535"
                          << std::endl;
                std::exit(2);
            }
            config.dashboardPort = static_cast<std::uint16_t>(port);
        } else if (arg == "--dashboard-token" && i + 1 < argc) {
            config.dashboardToken = argv[++i];
        } else if (arg == "--m1-authority-epoch" && i + 1 < argc) {
            config.m1AuthorityEpoch =
                std::strtoull(argv[++i], nullptr, 10);
        } else if (arg == "--role" || arg.rfind("--role=", 0) == 0) {
            // 执行端只接受 cell：cellServer 独立入口缺省即 cell。值校验在本
            // 面显式完成（此前传 --role=worker 会静默按 cell 启动，违反入口
            // 显形口径）；manager 不属于执行端，显式拒绝。
            std::string role_value = "cell";
            if (arg == "--role") {
                if (i + 1 >= argc) {
                    std::cerr << prog << ": --role requires a value (cell)"
                              << std::endl;
                    std::exit(2);
                }
                role_value = argv[++i];
            } else {
                role_value = arg.substr(7);
            }
            if (role_value != "cell") {
                std::cerr << prog << ": unknown role '" << role_value
                          << "' (expected cell)" << std::endl;
                std::exit(2);
            }
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --m1-port <port>           M1 link listen port (default: 9101)\n"
                      << "  --register-host <h>        World M1 host to register with (default: off)\n"
                      << "  --register-port <p>        World M1 port to register with, 0 = off (default: 0)\n"
                      << "  --cell-id <id>             Logical cell id for registration (default: 0)\n"
                      << "  (env APOLLO_REGISTER_TOKEN)  Startup credential self-assertion (default: empty)\n"
                      << "  --world-id <id>            World identity (default: 1)\n"
                      << "  --link-grace-ms <ms>       Control-link grace window before freeze, 0 = never (default: 15000)\n"
                      << "  --space <name>              Space name (default: main_world)\n"
                      << "  --add-space <id>:<name>     Declare an extra Space at startup, repeatable (default: none)\n"
                      << "  --size <width> <height>     Space size (default: 1000 1000)\n"
                      << "  --tick-rate <ms>            Tick rate in ms (default: 50)\n"
                      << "  --script-boot <file>        Lua boot source file (default: none)\n"
                      << "  --script-handlers <file>    Lua contract handler module, loaded first (default: none)\n"
                      << "  --script-contract <file>    Lua contract table, loaded after handlers (default: none)\n"
                      << "  --dashboard-host <h>        Dashboard listen host (default: 127.0.0.1)\n"
                      << "  --dashboard-port <p>        Dashboard HTTP port, 0 = off (default: 0)\n"
                      << "  --dashboard-token <t>       Dashboard Bearer token (default: off)\n"
                      << "  --help, -h                 Show this help\n";
            std::exit(0);
        } else if (is_fleet_arg(arg)) {
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                ++i;
            }
        } else {
            std::cerr << prog << ": unknown arg " << arg
                      << " (see --help)" << std::endl;
            std::exit(2);
        }
    }
    // 启动凭据经环境注入（L109 认证凭据面声明通道）：cell 自证凭据源与
    // World 侧同源——APOLLO_REGISTER_TOKEN，不进命令行 /proc cmdline（详见
    // main.cpp 同注）。World spawn 子进程时注入该环境变量。
    if (const char* token = std::getenv("APOLLO_REGISTER_TOKEN")) {
        config.registerToken = token;
    }
    return config;
}

// 编队选项（执行端独立入口的本地解析拷贝；与 world 共用管理入口同口径）。
struct FleetOptions {
    std::string machined_host = "127.0.0.1";
    std::uint16_t machined_port = 0;
    std::uint64_t component_id = 0;
    std::uint32_t zone_id = 0;
};

FleetOptions load_fleet_options(int argc, char* argv[]) {
    FleetOptions opts;
    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        if (arg == "--machined-host" && i + 1 < argc) {
            opts.machined_host = argv[++i];
        } else if (arg == "--machined-port" && i + 1 < argc) {
            opts.machined_port =
                static_cast<std::uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--component-id" && i + 1 < argc) {
            opts.component_id = std::strtoull(argv[++i], nullptr, 10);
        } else if (arg == "--zone-id" && i + 1 < argc) {
            opts.zone_id = static_cast<std::uint32_t>(std::atoi(argv[++i]));
        }
    }
    return opts;
}

} // namespace

int run_cell_entry_body(int argc, char* argv[],
                        const std::shared_ptr<apollo::core::log::Logger>& logger) {
    const char* prog = argc > 0 ? argv[0] : "cellapp";
    auto config = load_cell_config(argc, argv, prog);
    const auto fleet = load_fleet_options(argc, argv);

    // 信号与停机面在本函数内装配：cellServer 独立入口停机语义（signal_handler
    // → g_stop_request → cell.stop）。
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    logger->info("Configuration:");
    logger->info("  Role: cell (executor entry)");
    logger->info("  M1 link: " + config.host + ":"
                 + std::to_string(config.m1Port));
    logger->info("  Space: " + config.spaceName + " ("
                 + std::to_string(config.spaceWidth) + "x"
                 + std::to_string(config.spaceHeight) + ")");
    logger->info("  Tick rate: " + std::to_string(config.tickRateMs) + "ms ("
                 + std::to_string(1000 / config.tickRateMs) + " Hz)");

    try {
        config.bootAtMs = steady_now_ms();
        CellApp cell(config);
        CellLinkEgress egress;
        cell.set_egress(&egress);

        // §3.2 故障观测面（诊断/测试）：ScriptRuntime 冷路径 fault 事件
        // 落日志（模块名/类型/消息/宿主 tick），供运维定位与回归观测。
        // 不改变任何行为，只读旁路。
        cell.set_fault_sink([&logger](const apollo::script::ScriptFaultEvent& e) {
            logger->warning(
                "script fault: module=" + e.chunk
                + " kind=" + std::to_string(static_cast<int>(e.kind))
                + " tick=" + std::to_string(e.tick)
                + " msg=" + e.message);
        });

        std::unique_ptr<CellRegisterClient> register_client;
        // cell M1 接收端（passive 端点；pre_tick 泵引用它在位——先声明后装配）
        std::unique_ptr<M1LinkServer> link;
        // Dashboard 数据聚合器（DASH-1 阶段2）：pre_tick 内核线程采集、
        // dashboard HTTP 泵读——两向经 collector 内部锁交换。
        DashboardProcessInfo dash_info;
        dash_info.role = "cell";
        dash_info.started_ms = steady_now_ms();
        if (const char* fleet_dir = std::getenv("APOLLO_FLEET_DIR")) {
            dash_info.fleet_dir = fleet_dir;
        }
        auto dashboard_data = std::make_shared<DashboardDataCollector>(dash_info);
        // 管理命令状态（切片2 World→cell 转发；cell 宿主独占——监听器在
        // 注册泵上下文同步回调，与 cell pre_tick 同线程单写者）。
        std::unique_ptr<apollo::script::ModuleRollout> cell_module_rollout;
        std::string cell_last_rollout_name;
        std::unordered_map<std::uint64_t, std::unordered_set<std::uint64_t>>
            cell_scene_pending_evals;
        std::int32_t cell_drain_state = 0;  // 0=serving 1=draining 2=drained
        std::uint64_t cell_drain_deadline_ms = 0;
        std::string cell_drain_detail;
        cell.set_pre_tick(
            [&egress, &register_client, &cell, &dashboard_data, &link](
                std::uint64_t now_ms) {
                if (egress.link) {
                    egress.link->pump(now_ms);
                }
                if (dashboard_data) {
                    std::vector<apollo::net::link::LinkSnapshot> links;
                    if (link) {
                        auto l = link->snapshot_links();
                        links.insert(links.end(), l.begin(), l.end());
                    }
                    if (register_client) {
                        auto l = register_client->snapshot_links();
                        links.insert(links.end(), l.begin(), l.end());
                    }
                    dashboard_data->collect_links(links);
                    if (auto* script = cell.script_runtime()) {
                        dashboard_data->collect_script(script->active_modules());
                    }
                    dashboard_data->collect_metrics(
                        apollo::core::metrics::MetricRegistry::instance().snapshot());
                }
                if (register_client) {
                    auto* squads = cell.squadInstanceService();
                    register_client->setReportedDrainState(
                        squads != nullptr && squads->draining() ? 1 : 0);
                    const auto queue_cap = cell.intentQueueCap();
                    const auto pending = cell.pendingIntentCount();
                    register_client->setReportedLoad(
                        queue_cap == 0
                            ? 0
                            : static_cast<std::int32_t>(std::min(
                                  std::size_t{100}, pending * 100 / queue_cap)));
                    register_client->setReportedReady(!cell.frozen());
                    register_client->pump(now_ms);
                }
            });
        cell.set_post_tick(
            [&register_client, &cell, &cell_scene_pending_evals,
             &cell_module_rollout, &cell_last_rollout_name, &cell_drain_state,
             &cell_drain_deadline_ms, &cell_drain_detail](
                double) {
                // eval 收账（切片2）：Scene::tick 的固定调试点已执行在途 eval，
                // 取回传结果经注册连接反向发回 World（World 按 eval_id 关联原
                // fleetctl peer 回传 945）。
                if (register_client) {
                    for (auto it = cell_scene_pending_evals.begin();
                         it != cell_scene_pending_evals.end();) {
                        auto* scene = cell.find_scene(it->first);
                        if (scene == nullptr) {
                            it = cell_scene_pending_evals.erase(it);
                            continue;
                        }
                        for (auto& r : scene->drain_debug_eval_results()) {
                            CellDebugEvalResult res;
                            res.eval_id = r.id;
                            res.ok = r.ok;
                            res.result = r.ok ? r.result : "";
                            res.error = r.ok ? "" : r.result;
                            register_client->sendDebugEvalResult(res);
                            it->second.erase(r.id);
                        }
                        if (it->second.empty()) {
                            it = cell_scene_pending_evals.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
                // module rollout 观察窗推进（切片2 管理命令转发）：在装模块
                // onTick（fault 归因源）+ 观察窗/推广/回滚队列。WorldApp 内嵌
                // 内核 driveModuleRollout 同型——独立 cell 宿主自驱。
                if (cell_module_rollout) {
                    auto* script = cell.script_runtime();
                    if (script) {
                        if (!cell_last_rollout_name.empty()) {
                            (void)script->call_module(cell_last_rollout_name,
                                                      "onTick");
                        }
                        const auto st = cell_module_rollout->state();
                        if (st == apollo::script::RolloutState::kObserving) {
                            cell_module_rollout->observe_target("cell");
                        } else if (st == apollo::script::RolloutState::kPromoting) {
                            for (const auto& k : cell_module_rollout->pending_promote()) {
                                cell_module_rollout->commit_target(k);
                            }
                        } else if (st == apollo::script::RolloutState::kRollingBack) {
                            for (const auto& k : cell_module_rollout->pending_rollback()) {
                                cell_module_rollout->rollback_target(k);
                            }
                        }
                    }
                }
                // drain 推进（切片2）：排空期 active 归零落 drained。
                if (cell_drain_state == 1) {
                    auto* squads = cell.squadInstanceService();
                    if (squads != nullptr && squads->active_count() == 0) {
                        cell_drain_state = 2;
                        cell_drain_detail = "drained: all instances settled";
                    } else if (squads != nullptr && cell_drain_deadline_ms != 0
                               && steady_now_ms() >= cell_drain_deadline_ms) {
                        // 期限到未清强停（与 WorldApp 内嵌内核 driveWorldDrain
                        // 同语义）：快照遍历 abort/dismiss 同步释放绑定。
                        std::size_t force_stopped = 0;
                        for (const auto& binding : squads->activeBindings()) {
                            if (squads->abort(binding.party_id)
                                || squads->dismiss(binding.party_id)) {
                                ++force_stopped;
                            }
                        }
                        if (squads->active_count() == 0) {
                            cell_drain_state = 2;
                            cell_drain_detail =
                                "drained: deadline reached, "
                                + std::to_string(force_stopped)
                                + " instances force-aborted";
                        }
                    }
                }
            });
        link = makeM1LinkServer(
            M1LinkConfig{config.host, config.m1Port, config.m1RouteJson,
                         config.m1AuthorityEpoch},
            [&cell](IntentEnvelope intent) { cell.onIntent(std::move(intent)); });
        egress.link = link.get();
        g_stop_request = [&cell] { cell.stop(); };

        auto build_register_client = [&]() {
            if (config.registerPort == 0) {
                return;
            }
            if (config.registerHost.empty() || config.cellId == 0) {
                throw std::runtime_error(
                    "--register-port needs --register-host and --cell-id");
            }
            CellRegisterConfig rc;
            rc.world_host = config.registerHost;
            rc.world_port = config.registerPort;
            rc.cell_id = config.cellId;
            rc.advertise_host = config.host;
            rc.advertise_port = link->listenPort();
            rc.route_json = config.m1RouteJson;
            rc.authority_epoch = config.m1AuthorityEpoch;
            rc.link_grace_ms = config.linkGraceMs;
            rc.credential = config.registerToken;
            register_client = makeCellRegisterClient(std::move(rc));
            register_client->setSpaceAssignListener(
                [&cell, &config, &logger](const SpaceAssignRecord& rec) {
                    if (!rec.ready ||
                        rec.assigned_cell_id != config.cellId) {
                        if (!cell.hosts_space(rec.space_id)) {
                            return;
                        }
                        cell.request_space_revoke(rec.space_id);
                        return;
                    }
                    cell.cancel_space_revoke(rec.space_id);
                    const auto scene_id = cell.add_space(
                        rec.space_id,
                        space_descriptor_for(config, rec.space_id,
                                             declared_space_name(
                                                 config, rec.space_id)));
                    logger->info("space assigned: id="
                                 + std::to_string(rec.space_id)
                                 + " version=" + std::to_string(rec.version)
                                 + " scene=" + std::to_string(scene_id));
                });
            register_client->set_link_freeze_listener(
                [&cell, &logger](LinkFreezeReason reason) {
                    cell.set_frozen(true);
                    logger->warning(
                        std::string(reason == LinkFreezeReason::kGraceExpired
                                        ? "control link grace expired"
                                        : "adjudication rejected (stale epoch)")
                        + ": kernel frozen");
                });
            register_client->set_readmit_listener([&cell, &logger]() {
                cell.set_frozen(false);
                logger->info("re-admitted by world: kernel resumed");
            });

            // 管理命令受理（切片2 World→cell 转发）：World 端经注册连接反向
            // 发送 944/952/956，cell 宿主在此处理并回执 945/953/957（经本
            // 连接反向发回 World，World 再按 eval_id/request_id 关联原
            // fleetctl/admin peer 回传）。监听器在注册泵上下文同步回调，
            // 与 cell pre_tick 同线程单写者。
            register_client->set_debug_eval_listener(
                [&cell, &register_client, &logger,
                 &cell_scene_pending_evals](const CellDebugEvalRequest& req) {
                    if (req.eval_id == 0) {
                        return;  // 0 保留
                    }
                    auto* scene = cell.find_scene(
                        req.scene_id != 0 ? req.scene_id : cell.default_scene_id());
                    if (scene == nullptr || !scene->script_attached()) {
                        CellDebugEvalResult res;
                        res.eval_id = req.eval_id;
                        res.ok = false;
                        res.error = "scene not attached";
                        register_client->sendDebugEvalResult(res);
                        return;
                    }
                    if (!scene->enqueue_debug_eval(req.eval_id, req.source)) {
                        CellDebugEvalResult res;
                        res.eval_id = req.eval_id;
                        res.ok = false;
                        res.error = "eval queue full";
                        register_client->sendDebugEvalResult(res);
                        return;
                    }
                    cell_scene_pending_evals[req.scene_id != 0
                                                ? req.scene_id
                                                : cell.default_scene_id()]
                        .insert(req.eval_id);
                });
            register_client->set_module_rollout_listener(
                [&cell, &register_client, &logger, &cell_module_rollout,
                 &cell_last_rollout_name](const CellModuleRolloutRequest& req) {
                    if (req.request_id == 0) {
                        return;
                    }
                    auto* script = cell.script_runtime();
                    auto reply = [&](bool ok, std::int32_t state,
                                     std::uint64_t faults, std::uint64_t version,
                                     const std::string& detail) {
                        CellModuleRolloutReply rep;
                        rep.request_id = req.request_id;
                        rep.op = req.op;
                        rep.ok = ok;
                        rep.state = state;
                        rep.window_faults = faults;
                        rep.module_version = version;
                        rep.detail = detail;
                        register_client->sendModuleRolloutReply(rep);
                    };
                    if (script == nullptr) {
                        reply(false, -1, 0, 0, "script runtime not ready");
                        return;
                    }
                    const auto state_of = [&]() -> std::int32_t {
                        return cell_module_rollout
                                   ? static_cast<std::int32_t>(
                                         cell_module_rollout->state())
                                   : 0;
                    };
                    const auto detail_of = [&]() -> std::string {
                        if (!cell_module_rollout) {
                            return std::string("no rollout in flight");
                        }
                        const auto errs = cell_module_rollout->errors();
                        return errs.empty() ? std::string("no errors")
                                            : join_lines(errs);
                    };
                    switch (req.op) {
                        case 1: {  // prepare
                            if (cell_module_rollout == nullptr) {
                                cell_module_rollout =
                                    std::make_unique<apollo::script::ModuleRollout>(
                                        std::vector<apollo::script::RolloutTarget>{
                                            {"cell", script}});
                            }
                            apollo::script::RolloutModule mod;
                            mod.name = req.name;
                            mod.version = req.version;
                            mod.deps = split_dep_list(req.deps);
                            mod.source = req.source;
                            const bool ok = cell_module_rollout->prepare(
                                {mod}, {"cell"},
                                apollo::script::CanaryParams{req.observe_ticks,
                                                             req.fault_threshold});
                            if (ok) {
                                logger->info("cell module rollout prepared: "
                                             + req.name + " v"
                                             + std::to_string(req.version));
                            }
                            reply(ok, state_of(), 0,
                                  script->module_version(req.name), detail_of());
                            return;
                        }
                        case 2: {  // commit
                            if (cell_module_rollout == nullptr) {
                                reply(false, 0, 0, script->module_version(req.name),
                                      "no rollout in flight");
                                return;
                            }
                            if (cell_module_rollout->state() ==
                                apollo::script::RolloutState::kCanaryPending) {
                                cell_module_rollout->commit_target("cell");
                            }
                            const bool ok = cell_module_rollout->state() ==
                                            apollo::script::RolloutState::kObserving;
                            if (ok) {
                                cell_last_rollout_name = req.name;
                                logger->info("cell module rollout committed: "
                                             + req.name + " v"
                                             + std::to_string(req.version));
                            }
                            reply(ok, state_of(), 0,
                                  script->module_version(req.name), detail_of());
                            return;
                        }
                        case 3: {  // rollback/abort
                            if (cell_module_rollout == nullptr) {
                                reply(true, 0, 0, script->module_version(req.name),
                                      "no rollout in flight");
                                return;
                            }
                            if (cell_module_rollout->state() ==
                                apollo::script::RolloutState::kCanaryPending) {
                                cell_module_rollout.reset();
                                reply(true, 0, 0, script->module_version(req.name),
                                      "aborted: nothing committed");
                                return;
                            }
                            cell_module_rollout->rollback_target("cell");
                            const bool rb_ok = cell_module_rollout->state() ==
                                               apollo::script::RolloutState::kRolledBack;
                            reply(rb_ok, state_of(), 0,
                                  script->module_version(req.name), detail_of());
                            return;
                        }
                        default: {  // status
                            const auto statuses =
                                cell_module_rollout ? cell_module_rollout->statuses()
                                                    : std::vector<apollo::script::TargetStatus>{};
                            std::uint64_t faults = 0;
                            for (const auto& s : statuses) {
                                faults += s.window_faults;
                            }
                            reply(true, state_of(), faults,
                                  script->module_version(req.name), detail_of());
                            return;
                        }
                    }
                });
            register_client->set_world_drain_listener(
                [&cell, &register_client, &logger, &cell_drain_state,
                 &cell_drain_deadline_ms, &cell_drain_detail](
                    const CellWorldDrainRequest& req) {
                    if (req.request_id == 0) {
                        return;
                    }
                    auto* squads = cell.squadInstanceService();
                    const auto active_of = [&]() -> std::uint64_t {
                        return squads ? squads->active_count() : 0;
                    };
                    const auto detail_of = [&]() -> std::string {
                        switch (cell_drain_state) {
                            case 2:
                                return cell_drain_detail;
                            case 1:
                                return "draining: "
                                       + std::to_string(active_of())
                                       + " active instances settling";
                            default:
                                return "serving";
                        }
                    };
                    auto reply = [&](bool ok, const std::string& detail) {
                        CellWorldDrainReply rep;
                        rep.request_id = req.request_id;
                        rep.op = req.op;
                        rep.ok = ok;
                        rep.state = cell_drain_state;
                        rep.active_instances = active_of();
                        rep.detail = detail;
                        register_client->sendWorldDrainReply(rep);
                    };
                    if (squads == nullptr) {
                        reply(false, "squad service not ready");
                        return;
                    }
                    switch (req.op) {
                        case 1: {  // begin
                            if (cell_drain_state != 2) {
                                squads->set_drain(true);
                                if (cell_drain_state == 0) {
                                    cell_drain_state = 1;
                                    logger->info("cell drain begun");
                                }
                                // 重复 begin 幂等，期限只紧不松（与 WorldApp
                                // 内嵌 driveWorldDrain 同语义）：新期限更早
                                // 才收紧，0 = 不限时不清已有期限。
                                if (req.deadline_s > 0) {
                                    const auto deadline =
                                        steady_now_ms()
                                        + static_cast<std::uint64_t>(
                                              req.deadline_s) * 1000U;
                                    if (cell_drain_deadline_ms == 0
                                        || deadline < cell_drain_deadline_ms) {
                                        cell_drain_deadline_ms = deadline;
                                    }
                                }
                            }
                            reply(true, detail_of());
                            return;
                        }
                        case 2: {  // status
                            reply(true, detail_of());
                            return;
                        }
                        default:
                            reply(false, "unknown op "
                                            + std::to_string(req.op));
                            return;
                    }
                });

            // 意图族受理（切片2 World→cell 意图转发）：World 把 gateway 的
            // 932 原始 envelope 经注册连接反向发送，cell 在此解包投递
            // CellApp（入队；tick 边界执行）。延时应答经 CellLinkEgress 的
            // register_client 回程（World 再 gateway_link() 转发 gateway）。
            egress.register_client = register_client.get();
            register_client->set_intent_listener(
                [&cell](const std::vector<std::uint8_t>& payload) {
                    IntentEnvelope intent;
                    if (!decodeEnvelopeIntent(payload, intent)) {
                        return;  // 残帧/非意图族 = 尽力面丢弃（World 已拒收计）
                    }
                    cell.onIntent(std::move(intent));
                });
        };

        logger->info("Starting server...");
        cell.start();
        if (!link->start()) {
            throw std::runtime_error(
                "failed to start cell M1 link endpoint (route table load or "
                "bind failed)");
        }
        logger->info("Cell M1 link listening on " + config.host + ":"
                     + std::to_string(link->listenPort()));
        build_register_client();
        if (register_client) {
            if (!register_client->start()) {
                throw std::runtime_error(
                    "failed to start cell registration client (config or route "
                    "table invalid)");
            }
            logger->info("Cell registration: dialing " + config.registerHost
                         + ":" + std::to_string(config.registerPort)
                         + " as cell-" + std::to_string(config.cellId));
        }

        // 内部状态 Dashboard（DASH-1）：配置了 --dashboard-port 即装配嵌入
        // HTTP 状态页。数据源经 dashboard_data 聚合器（pre_tick 采集）。
        std::unique_ptr<DashboardResponder> dashboard;
        if (config.dashboardPort != 0) {
            DashboardConfig dc;
            dc.host = config.dashboardHost;
            dc.port = config.dashboardPort;
            dc.token = config.dashboardToken;
            dashboard = std::make_unique<DashboardResponder>(
                std::move(dc), [dashboard_data]() {
                    return dashboard_data->render();
                });
            if (!dashboard->start()) {
                logger->error("Dashboard bind failed on " + config.dashboardHost
                              + ":" + std::to_string(config.dashboardPort));
                throw std::runtime_error("dashboard bind failed");
            }
            logger->info("Dashboard listening on " + config.dashboardHost + ":"
                         + std::to_string(dashboard->port())
                         + (config.dashboardToken.empty()
                                ? " (no token)"
                                : " (token auth)"));
        }

        apollo::net::discovery::FleetMember* fleet_raw = nullptr;
        if (fleet.machined_port != 0 && fleet.component_id == 0) {
            logger->warning(
                "machined-port set without --component-id; fleet "
                "registration disabled");
        }
        std::unique_ptr<apollo::net::discovery::FleetMember> cell_fleet_member;
        // 传输底座须与 FleetMember 同级存活（FleetMember 持引用——块内局部
        // 出域即析构，tick 打悬空 transport = segfault）
        apollo::net::discovery::UdpBeaconTransport cell_transport;
        if (fleet.machined_port != 0 && fleet.component_id != 0) {
            if (cell_transport.open()) {
                apollo::net::discovery::MemberId self;
                self.component_id = fleet.component_id;
                self.zone_id =
                    fleet.zone_id != 0 ? fleet.zone_id : config.worldId;
                self.service_port = config.m1Port;
                cell_fleet_member =
                    std::make_unique<apollo::net::discovery::FleetMember>(
                        self, fleet.machined_host, fleet.machined_port,
                        cell_transport);
                fleet_raw = cell_fleet_member.get();
                logger->info("fleet member: component="
                             + std::to_string(fleet.component_id)
                             + " zone=" + std::to_string(self.zone_id)
                             + " service_port="
                             + std::to_string(config.m1Port)
                             + " -> machined " + fleet.machined_host + ":"
                             + std::to_string(fleet.machined_port));
            } else {
                logger->warning(
                    "fleet transport open failed; running without fleet "
                    "registration");
            }
        }

        auto fleet_now_ms = [] {
            return static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count());
        };

        logger->info("Server is running. Press Ctrl+C to stop.");
        while (cell.isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (dashboard) {
                dashboard->pump(fleet_now_ms());
            }
            if (fleet_raw) {
                fleet_raw->tick(fleet_now_ms());
            }
        }
        if (fleet_raw) {
            fleet_raw->stop();
        }
        logger->info("Server stopped.");
        g_stop_request = nullptr;
        return 0;
    } catch (const std::exception& e) {
        logger->error(std::string("Error: ") + e.what());
        return 1;
    }
}

int run_cell_entry(int argc, char* argv[]) {
    // 执行端独立日志接线（ADR-013）：identity=cell，文件面 cell
    auto& logs = apollo::core::log::global_log_manager();
    apollo::core::log::LogManagerConfig log_config;
    log_config.processIdentity = "cell";
    log_config.fileEnabled = true;
    log_config.fileConfig.directory = "log";
    log_config.fileConfig.baseName = "cell";
    log_config.fileConfig.structuredOutput = true;
    if (auto push_client =
            apollo::net::logpush::make_log_push_from_env("cell")) {
        log_config.pushSink = [push_client](const std::string& line) {
            push_client->push(line);
        };
    }
    logs.initialize(log_config);
    const auto logger = logs.createLogger("cell");

    auto metrics_push =
        apollo::net::metricspush::make_metrics_push_from_env("cell");
    (void)metrics_push;
    apollo::runtime::init_crash_capture(argc, argv, "cell");

    return run_cell_entry_body(argc, argv, logger);
}

} // namespace world_app
