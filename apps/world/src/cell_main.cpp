#include "world_app/cell_main.hpp"
#include "world_app/cell_app.hpp"
#include "world_app/cell_register_client.hpp"
#include "world_app/m1_link_server.hpp"
#include "world_app/role_options.hpp"
#include "world_app/config.hpp"
#include "apollo/runtime/crash_capture.hpp"
#include "apollo/core/log/log_manager.h"
#include "apollo/net/logpush/log_push_env.hpp"
#include "apollo/net/metricspush/metrics_push_env.hpp"
#include "apollo/net/discovery/discovery.hpp"
#include "apollo/net/discovery/udp_transport.hpp"
#include "world_app/world_types.hpp"
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
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

// 编队面参数（cellapp 与 world --role=cell 共用解析面；此处为独立入口
// 就地解析，避免把 exec_role 私有辅助暴露给共享库层）。
bool is_fleet_arg(const std::string& arg) {
    return arg == "--machined-host" || arg == "--machined-port" ||
           arg == "--component-id" || arg == "--zone-id";
}

// 执行端出口适配（IM1Egress → M1LinkServer；与 WorldApp 内嵌适配同型；
// 链路未装配 = 尽力面落地）
struct CellLinkEgress final : public IM1Egress {
    M1LinkServer* link = nullptr;

    void sendClientDeliver(std::uint64_t session_id, std::uint32_t gateway_id,
                           const std::vector<std::uint8_t>& payload) override {
        if (link) {
            link->sendClientDeliver(session_id, gateway_id, payload);
        }
    }
    void sendIntentAck(std::uint64_t request_id, std::int32_t code,
                       const std::string& message) override {
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
            config.worldId = static_cast<std::uint32_t>(std::stoul(argv[++i]));
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
            config.tickRateMs = std::atoi(argv[++i]);
        } else if (arg == "--script-boot" && i + 1 < argc) {
            config.scriptBootFile = argv[++i];
        } else if (arg == "--script-handlers" && i + 1 < argc) {
            config.scriptHandersFile = argv[++i];
        } else if (arg == "--script-contract" && i + 1 < argc) {
            config.scriptContractFile = argv[++i];
        } else if (arg == "--m1-authority-epoch" && i + 1 < argc) {
            config.m1AuthorityEpoch =
                std::strtoull(argv[++i], nullptr, 10);
        } else if (arg == "--role" || arg.rfind("--role=", 0) == 0) {
            // 执行端只接受 cell：委托路径由 world 入口 parse_world_role 裁决
            // 且 role==cell 才进入本运行体，独立 cellServer 入口缺省即 cell。
            // 执行端不接受 manager 委托，值校验在本面显式完成（此前 cellServer
            // 传 --role=worker 会静默按 cell 启动，违反入口显形口径）。
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
                      << "  --world-id <id>            World identity (default: 1)\n"
                      << "  --link-grace-ms <ms>       Control-link grace window before freeze, 0 = never (default: 15000)\n"
                      << "  --space <name>              Space name (default: main_world)\n"
                      << "  --add-space <id>:<name>     Declare an extra Space at startup, repeatable (default: none)\n"
                      << "  --size <width> <height>     Space size (default: 1000 1000)\n"
                      << "  --tick-rate <ms>            Tick rate in ms (default: 50)\n"
                      << "  --script-boot <file>        Lua boot source file (default: none)\n"
                      << "  --script-handlers <file>    Lua contract handler module, loaded first (default: none)\n"
                      << "  --script-contract <file>    Lua contract table, loaded after handlers (default: none)\n"
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

    // 信号与停机面在本函数内装配：cellServer 独立入口与 world --role=cell
    // 委托路径共用同一停机语义（signal_handler → g_stop_request → cell.stop）。
    // 委托路径在此覆盖调用方（world manager）已注册的 signalHandler——后者
    // 不持有 cell 停机面，覆盖是无害的单线程注册（装配前无并发信号面）。
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

        std::unique_ptr<CellRegisterClient> register_client;
        cell.set_pre_tick(
            [&egress, &register_client, &cell](std::uint64_t now_ms) {
                if (egress.link) {
                    egress.link->pump(now_ms);
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
        auto link = makeM1LinkServer(
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

        apollo::net::discovery::FleetMember* fleet_raw = nullptr;
        if (fleet.machined_port != 0 && fleet.component_id == 0) {
            logger->warning(
                "machined-port set without --component-id; fleet "
                "registration disabled");
        }
        std::unique_ptr<apollo::net::discovery::FleetMember> cell_fleet_member;
        if (fleet.machined_port != 0 && fleet.component_id != 0) {
            apollo::net::discovery::UdpBeaconTransport cell_transport;
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
