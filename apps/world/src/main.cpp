#include "apollo/runtime/crash_capture.hpp"
#include "apollo/core/log/log_manager.h"
#include "apollo/net/logpush/log_push_env.hpp"
#include "apollo/net/metricspush/metrics_push_env.hpp"
#include "apollo/net/discovery/discovery.hpp"
#include "apollo/net/discovery/udp_transport.hpp"
#include "world_app/world_app.hpp"
#include "world_app/cell_register_client.hpp"
#include "world_app/role_options.hpp"
#include <iostream>
#include <csignal>
#include <cstdint>
#include <functional>
#include <memory>

using namespace world_app;

// 全局停机钩子（L98 双角色：manager/cell 两壳各自装配；信号上下文直调
// stop——同旧 g_server->stop() 语义）
static std::function<void()> g_stop_request;

// 信号处理（信号上下文保持直写 console——logger 面带锁，信号路径不进）
void signalHandler(int signal) {
    if (g_stop_request) {
        std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
        g_stop_request();
    }
}

// 日志接线（ADR-013 L1+L2）：console 人读 + 本地结构化文件真相源并行；
// push 出口 = APOLLO_LOG_PUSH_ADDR 环境接线（logging.md §5；未设 = 关）。
static apollo::core::log::LoggerPtr initLogging() {
    auto& logs = apollo::core::log::global_log_manager();
    apollo::core::log::LogManagerConfig config;
    config.processIdentity = "world";
    config.fileEnabled = true;
    config.fileConfig.directory = "log";
    config.fileConfig.baseName = "world";
    config.fileConfig.structuredOutput = true;  // §5.1 结构化行（六键固定序）
    // L4 push 出口（logging.md §5 双出口）：APOLLO_LOG_PUSH_ADDR 非空才接
    // （collector 缺席 = 客户端有界队列缓冲，本地文件恒真相源）
    if (auto push_client = apollo::net::logpush::make_log_push_from_env("world")) {
        config.pushSink = [push_client](const std::string& line) {
            push_client->push(line);
        };
    }
    logs.initialize(config);
    return logs.createLogger("world");
}

// 加载配置
WorldAppConfig loadConfig(int argc, char* argv[]) {
    WorldAppConfig config;

    // 分角色（L27）：缺省 manager；cell/manager 之外与缺值在入口显式拒绝
    // （退出非零），不静默回退默认角色。
    std::string role_err;
    if (!parse_world_role(argc, argv, config.role, role_err)) {
        std::cerr << "world: " << role_err << std::endl;
        std::exit(2);
    }

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--world-id" && i + 1 < argc) {
            config.worldId = static_cast<std::uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--max-instances" && i + 1 < argc) {
            config.maxInstances = static_cast<std::size_t>(std::stoull(argv[++i]));
        } else if (arg == "--m1-port" && i + 1 < argc) {
            config.m1Port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--base-business-host" && i + 1 < argc) {
            config.baseBusinessHost = argv[++i];
        } else if (arg == "--base-business-port" && i + 1 < argc) {
            config.baseBusinessPort =
                static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--settlement-credential-dir" && i + 1 < argc) {
            config.settlementCredentialDir = argv[++i];
        } else if (arg == "--register-host" && i + 1 < argc) {
            config.registerHost = argv[++i];
        } else if (arg == "--register-port" && i + 1 < argc) {
            config.registerPort = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--cell-id" && i + 1 < argc) {
            config.cellId = std::strtoull(argv[++i], nullptr, 10);
        } else if (arg == "--cell-expiry-ms" && i + 1 < argc) {
            config.cellExpiryMs = std::strtoull(argv[++i], nullptr, 10);
        } else if (arg == "--link-grace-ms" && i + 1 < argc) {
            config.linkGraceMs = std::strtoull(argv[++i], nullptr, 10);
        } else if (arg == "--assign-spaces" && i + 1 < argc) {
            std::stringstream ss(argv[++i]);
            std::string item;
            while (std::getline(ss, item, ',')) {
                if (item.empty()) {
                    continue;
                }
                const std::uint64_t space_id = std::strtoull(item.c_str(), nullptr, 10);
                if (space_id == 0) {
                    std::cerr << "world: --assign-spaces ids must be non-zero"
                              << std::endl;
                    std::exit(2);
                }
                if (std::find(config.assignSpaces.begin(), config.assignSpaces.end(),
                              space_id)
                    != config.assignSpaces.end()) {
                    std::cerr << "world: --assign-spaces duplicate id "
                              << std::to_string(space_id) << std::endl;
                    std::exit(2);
                }
                config.assignSpaces.push_back(space_id);
            }
        } else if (arg == "--space" && i + 1 < argc) {
            config.spaceName = argv[++i];
        } else if (arg == "--add-space" && i + 1 < argc) {
            const std::string spec = argv[++i];
            const auto colon = spec.find(':');
            if (colon == std::string::npos || colon == 0 || colon + 1 == spec.size()) {
                std::cerr << "world: --add-space expects <id>:<name>" << std::endl;
                std::exit(2);
            }
            const std::uint64_t space_id =
                std::strtoull(spec.substr(0, colon).c_str(), nullptr, 10);
            if (space_id == 0) {
                std::cerr << "world: --add-space id must be non-zero" << std::endl;
                std::exit(2);
            }
            for (const auto& s : config.spaces) {
                if (s.space_id == space_id) {
                    std::cerr << "world: --add-space duplicate id "
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
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --role <manager|cell>      manager = management (default), cell = execution\n"
                      << "  --world-id <id>            World identity (default: 1)\n"
                      << "  --max-instances <n>        Squad instance capacity (default: 16)\n"
                      << "  --m1-port <port>           M1 link listen port (default: 9101)\n"
                      << "  --base-business-host <h>   Base business face host (default: 127.0.0.1)\n"
                      << "  --base-business-port <p>   Base business face port, 0 = off (default: 0)\n"
                      << "  --settlement-credential-dir <dir>  Settlement credential dir (default: off)\n"
                      << "  --register-host <h>        World M1 host to register with, cell role (default: off)\n"
                      << "  --register-port <p>        World M1 port to register with, 0 = off (default: 0)\n"
                      << "  --cell-id <id>             Logical cell id for registration, cell role (default: 0)\n"
                      << "  --cell-expiry-ms <ms>     Registry expiry window for cells without heartbeats, 0 = never (default: 30000)\n"
                      << "  --link-grace-ms <ms>      Cell-side control-link grace window before freeze, 0 = never (default: 15000)\n"
                      << "  --assign-spaces <ids>      Comma-separated Space pool to assign to registered cells (default: off)\n"
                      << "  --space <name>              Space name (default: main_world)\n"
                      << "  --add-space <id>:<name>     Declare an extra Space at startup, repeatable (default: none)\n"
                      << "  --size <width> <height>     Space size (default: 1000 1000)\n"
                      << "  --tick-rate <ms>            Tick rate in ms (default: 50)\n"
                      << "  --script-boot <file>        Lua boot source file (default: none)\n"
                      << "  --script-handlers <file>    Lua contract handler module, loaded first (default: none)\n"
                      << "  --script-contract <file>    Lua contract table, loaded after handlers (default: none)\n"
                      << "  --help, -h                 Show this help\n";
            std::exit(0);
        }
    }

    return config;
}

// 编队成员注册（P3-4 收尾批）：--machined-port 0 = 关（standalone 跑不依赖
// 编队）；--component-id 0 = 未入编队（仅跑服务不注册）。身份 service_port
// = M1 受理端口（消费方拨号键）；zone_id 进编队名册（死亡事件携带，目录
// owner 按 zone 行反查死亡处置——不进名册 = 死亡不带 zone）。
struct FleetOptions {
    std::string machined_host = "127.0.0.1";
    std::uint16_t machined_port = 0;
    std::uint64_t component_id = 0;
    std::uint32_t zone_id = 0;
};

static FleetOptions loadFleetOptions(int argc, char* argv[]) {
    FleetOptions opts;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--machined-host" && i + 1 < argc) {
            opts.machined_host = argv[++i];
        } else if (arg == "--machined-port" && i + 1 < argc) {
            opts.machined_port =
                static_cast<std::uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--component-id" && i + 1 < argc) {
            opts.component_id =
                std::strtoull(argv[++i], nullptr, 10);
        } else if (arg == "--zone-id" && i + 1 < argc) {
            opts.zone_id =
                static_cast<std::uint32_t>(std::atoi(argv[++i]));
        }
    }
    return opts;
}

// cell 角色出口适配（L98）：IM1Egress → M1LinkServer（与 WorldApp 内嵌
// 适配同型；链路未装配 = 尽力面落地）
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

int main(int argc, char* argv[]) {
    // 先于 crash capture：write() 在未初始化时会惰性按默认配置建管理器（仅 console），
    // 顺序颠倒会使本进程的文件面配置被默认初始化顶掉（initialize 幂等早退）
    const auto logger = initLogging();
    // L4 指标 push 出口（logging.md §5.1 采样流）：APOLLO_METRICS_PUSH_ADDR
    // 环境接线（未设 = 关；exporter 缺席 = 丢拍不积压最新者胜）。句柄进程期
    // 存活，注册表快照渲染在客户端泵线程自拉。
    auto metrics_push = apollo::net::metricspush::make_metrics_push_from_env("world");
    (void)metrics_push;
    apollo::runtime::init_crash_capture(argc, argv, "world");
    std::cout << "======================================" << std::endl;
    std::cout << "       Apollo World           " << std::endl;
    std::cout << "======================================" << std::endl;

    auto config = loadConfig(argc, argv);

    logger->info("Configuration:");
    logger->info("  Role: " + config.role + " (shared binary, L27)");
    logger->info("  M1 link: " + config.host + ":" + std::to_string(config.m1Port));
    logger->info("  Space: " + config.spaceName + " (" + std::to_string(config.spaceWidth)
                 + "x" + std::to_string(config.spaceHeight) + ")");
    logger->info("  Tick rate: " + std::to_string(config.tickRateMs) + "ms ("
                 + std::to_string(1000 / config.tickRateMs) + " Hz)");

    // 注册信号处理
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    try {
        const FleetOptions fleet = loadFleetOptions(argc, argv);

        // cell 角色（L98 首切片）：执行内核 + 仅意图 M1 面。管理受理（944
        // eval / 952 编排 / 956 排空）不装配——M1LinkServer 分发面对缺席回调
        // 自守门（判空拒收），注册/拓扑/容量编排归 manager 侧随后续条目。
        if (config.role == "cell") {
            CellApp cell(config);
            CellLinkEgress egress;
            cell.set_egress(&egress);
            // 注册拨号客户端（L101 次切片；960/961）：配置了 World 地址即
            // 主动注册。对象先声明（pre-tick 捕获引用），装配在 M1 端点
            // start 之后——自报端口 = 本进程受理端口实占值。
            std::unique_ptr<CellRegisterClient> register_client;
            // 拍首钩子 = M1 泵 + 注册泵（链路对象先行，监听在闸开之后）；
            // post-tick 缺席——cell 角色无管理推进
            cell.set_pre_tick([&egress, &register_client, &cell](std::uint64_t now_ms) {
                if (egress.link) {
                    egress.link->pump(now_ms);
                }
                if (register_client) {
                    auto* squads = cell.squadInstanceService();
                    register_client->setReportedDrainState(
                        squads != nullptr && squads->draining() ? 1 : 0);
                    register_client->pump(now_ms);
                }
            });
            auto link = makeM1LinkServer(
                M1LinkConfig{config.host, config.m1Port, config.m1RouteJson,
                             config.m1AuthorityEpoch},
                [&cell](IntentEnvelope intent) { cell.onIntent(std::move(intent)); });
            egress.link = link.get();
            g_stop_request = [&cell] { cell.stop(); };
            // 注册拨号客户端（L101 次切片；960/961）：配置了 World 地址即主
            // 动注册；装配面在此（link 已在）——自报端口 = 本进程受理端口
            // 实占值，调用在 M1 端点 start 之后。
            auto build_register_client = [&]() {
                if (config.registerPort == 0) {
                    return;  // 未配置 = 注册关（缺省单机形态）
                }
                if (config.registerHost.empty() || config.cellId == 0) {
                    throw std::runtime_error(
                        "--register-port requires --register-host and --cell-id");
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
                // 962 分配应用面（L101 首切片 + L108 回收/占用联动切片）：监听
                // 在 pre-tick 泵内同步回调 = 内核线程上下文（add_space 注册窗含
                // 此面），按版本前进触发（客户端去重）。ready=1 且归属本 cell →
                // 取消陈旧待撤（最新 World 视图优先）+ 幂等登记进内核路由表；
                // ready=0 或归属它方（撤销标记：World 释放/重分配）→ 入待撤表
                // （占用中被拒每拍重试至腾空成功，见 CellApp::drainPendingRevokes）。
                register_client->setSpaceAssignListener(
                    [&cell, &config, &logger](const SpaceAssignRecord& rec) {
                        if (!rec.ready || rec.assigned_cell_id != config.cellId) {
                            if (cell.request_space_revoke(rec.space_id)) {
                                logger->info("space revoke requested: id="
                                             + std::to_string(rec.space_id)
                                             + " version="
                                             + std::to_string(rec.version));
                            }
                            return;
                        }
                        cell.cancel_space_revoke(rec.space_id);
                        const auto scene_id = cell.add_space(
                            rec.space_id,
                            space_descriptor_for(config, rec.space_id, ""));
                        logger->info("space assigned: id=" + std::to_string(rec.space_id)
                                     + " version=" + std::to_string(rec.version)
                                     + " scene=" + std::to_string(scene_id));
                    });
                // 失联冻结面（L101 失联策略切片）：宽限窗到期 / 裁决拒（code 3
                // 旧执行端隔离）→ 冻结内核（停模拟 + 关闸，泵照跑听复核）；
                // 961 复核受理 → 解冻。监听在 pre-tick 泵内 = 内核线程上下文。
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
                    "failed to start cell M1 link endpoint (route table load or bind failed)");
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
                logger->info("Cell registration: dialing " + config.registerHost + ":"
                             + std::to_string(config.registerPort) + " as cell-"
                             + std::to_string(config.cellId));
            }

            // 编队成员注册（P3-4 收尾批）：与 manager 同面（service_port =
            // M1 受理端口，消费方按角色拨号）
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
                    self.zone_id = fleet.zone_id != 0 ? fleet.zone_id : config.worldId;
                    self.service_port = config.m1Port;
                    cell_fleet_member =
                        std::make_unique<apollo::net::discovery::FleetMember>(
                            self, fleet.machined_host, fleet.machined_port,
                            cell_transport);
                    fleet_raw = cell_fleet_member.get();
                    logger->info("fleet member: component="
                                 + std::to_string(fleet.component_id)
                                 + " zone=" + std::to_string(self.zone_id)
                                 + " service_port=" + std::to_string(config.m1Port)
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
        }

        WorldApp server(config);
        g_stop_request = [&server] { server.stop(); };

        logger->info("Starting server...");
        server.start();

        // 编队成员注册（P3-4 收尾批）：machined 在位才上 wire；beat 交给
        // 主循环秒拍，退出路径优雅注销（TTL 兜底覆盖注销包丢失）
        apollo::net::discovery::UdpBeaconTransport fleet_transport;
        std::unique_ptr<apollo::net::discovery::FleetMember> fleet_member;
        if (fleet.machined_port != 0) {
            if (fleet.component_id == 0) {
                logger->warning(
                    "machined-port set without --component-id; fleet "
                    "registration disabled");
            } else if (fleet_transport.open()) {
                apollo::net::discovery::MemberId self;
                self.component_id = fleet.component_id;
                self.zone_id = fleet.zone_id != 0 ? fleet.zone_id : config.worldId;
                self.service_port = config.m1Port;
                fleet_member =
                    std::make_unique<apollo::net::discovery::FleetMember>(
                        self, fleet.machined_host, fleet.machined_port,
                        fleet_transport);
                logger->info("fleet member: component="
                             + std::to_string(fleet.component_id)
                             + " zone=" + std::to_string(self.zone_id)
                             + " service_port=" + std::to_string(config.m1Port)
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

        while (server.isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (fleet_member) {
                fleet_member->tick(fleet_now_ms());
            }
        }
        if (fleet_member) {
            fleet_member->stop();
        }

        logger->info("Server stopped.");
        g_stop_request = nullptr;

    } catch (const std::exception& e) {
        logger->error(std::string("Error: ") + e.what());
        return 1;
    }

    return 0;
}
