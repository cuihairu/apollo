#include "apollo/runtime/crash_capture.hpp"
#include "apollo/core/log/log_manager.h"
#include "apollo/net/logpush/log_push_env.hpp"
#include "apollo/net/metricspush/metrics_push_env.hpp"
#include "apollo/net/discovery/discovery.hpp"
#include "apollo/net/discovery/udp_transport.hpp"
#include "world_app/world_app.hpp"
#include "world_app/cell_register_client.hpp"
#include "world_app/role_options.hpp"
#include "login/m1_service_register_client.hpp"
#include <iostream>
#include <csignal>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <vector>

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

// 编队面参数（loadFleetOptions 解析；本循环放行不解析，防未知参数守卫误判）
bool is_fleet_arg(const std::string& arg) {
    return arg == "--machined-host" || arg == "--machined-port" ||
           arg == "--component-id" || arg == "--zone-id";
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
        } else if (arg == "--service-register-host" && i + 1 < argc) {
            config.serviceRegisterHost = argv[++i];
        } else if (arg == "--service-register-port" && i + 1 < argc) {
            config.serviceRegisterPort =
                static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--cell-expiry-ms" && i + 1 < argc) {
            config.cellExpiryMs = std::strtoull(argv[++i], nullptr, 10);
        } else if (arg == "--world-journal" && i + 1 < argc) {
            config.worldJournalPath = argv[++i];
        } else if (arg == "--link-grace-ms" && i + 1 < argc) {
            config.linkGraceMs = std::strtoull(argv[++i], nullptr, 10);
        } else if (arg == "--m1-authority-epoch" && i + 1 < argc) {
            config.m1AuthorityEpoch =
                std::strtoull(argv[++i], nullptr, 10);
        } else if (arg == "--assign-spaces" && i + 1 < argc) {
            std::stringstream ss(argv[++i]);
            std::string item;
            while (std::getline(ss, item, ',')) {
                if (item.empty()) {
                    continue;
                }
                // 条目格式 <id> 或 <id>:<needs>——needs = 该 Space 的
                // 能力位要求（容量申报裁决；十进制/十六进制 0x 皆可）。
                const auto colon = item.find(':');
                const std::string id_text =
                    colon == std::string::npos ? item : item.substr(0, colon);
                const std::uint64_t space_id = std::strtoull(id_text.c_str(), nullptr, 10);
                if (space_id == 0) {
                    std::cerr << "world: --assign-spaces ids must be non-zero"
                              << std::endl;
                    std::exit(2);
                }
                std::int32_t needs = 0;
                if (colon != std::string::npos) {
                    const std::string caps_text = item.substr(colon + 1);
                    char* caps_end = nullptr;
                    const long caps_value =
                        std::strtol(caps_text.c_str(), &caps_end, 0);
                    if (caps_text.empty() || *caps_end != '\0' ||
                        caps_value < 0 ||
                        caps_value > std::numeric_limits<std::int32_t>::max()) {
                        std::cerr << "world: --assign-spaces needs must be a "
                                     "non-negative integer in "
                                  << item << std::endl;
                        std::exit(2);
                    }
                    needs = static_cast<std::int32_t>(caps_value);
                }
                bool duplicate = false;
                for (const auto& slot : config.assignSpaces) {
                    if (slot.space_id == space_id) {
                        duplicate = true;
                        break;
                    }
                }
                if (duplicate) {
                    std::cerr << "world: --assign-spaces duplicate id "
                              << std::to_string(space_id) << std::endl;
                    std::exit(2);
                }
                config.assignSpaces.push_back({space_id, needs});
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
                      << "  --role <manager>           manager = management (default); cell runs via cellServer\n"
                      << "  --world-id <id>            World identity (default: 1)\n"
                      << "  --max-instances <n>        Squad instance capacity (default: 16)\n"
                      << "  --m1-port <port>           M1 link listen port (default: 9101)\n"
                      << "  --base-business-host <h>   Base business face host (default: 127.0.0.1)\n"
                      << "  --base-business-port <p>   Base business face port, 0 = off (default: 0)\n"
                      << "  --settlement-credential-dir <dir>  Settlement credential dir (default: off)\n"
                      << "  --register-host <h>        World M1 host to register with, manager role (default: off)\n"
                      << "  --register-port <p>        World M1 port to register with, 0 = off (default: 0)\n"
                      << "  --cell-id <id>             Logical cell id for registration, manager role (default: 0)\n"
                      << "  --service-register-host <h>  login service register host, manager role (default: off)\n"
                      << "  --service-register-port <p>  login service register port, 0 = off (default: 0)\n"
                      << "  --cell-expiry-ms <ms>     Registry expiry window for cells without heartbeats, 0 = never (default: 30000)\n"
                      << "  --world-journal <file>    Epoch tombstone journal for cross-restart cell isolation (default: off)\n"
                      << "  --link-grace-ms <ms>      Cell-side control-link grace window before freeze, 0 = never (default: 15000)\n"
                      << "  --assign-spaces <ids>      Comma-separated Space pool to assign to registered cells,\n"
                         "                             each entry <id> or <id>:<needs-bits> (default: off)\n"
                      << "  --space <name>              Space name (default: main_world)\n"
                      << "  --add-space <id>:<name>     Declare an extra Space at startup, repeatable (default: none)\n"
                      << "  --size <width> <height>     Space size (default: 1000 1000)\n"
                      << "  --tick-rate <ms>            Tick rate in ms (default: 50)\n"
                      << "  --script-boot <file>        Lua boot source file (default: none)\n"
                      << "  --script-handlers <file>    Lua contract handler module, loaded first (default: none)\n"
                      << "  --script-contract <file>    Lua contract table, loaded after handlers (default: none)\n"
                      << "  --help, -h                 Show this help\n";
            std::exit(0);
        } else if (arg == "--role" || arg.rfind("--role=", 0) == 0) {
            // 角色面由 parse_world_role 处理（--role <v> 与 --role=<v>）；
            // 取值形式吃掉参数，否则被下面的未知参数守卫误判
            if (arg == "--role" && i + 1 < argc) {
                ++i;
            }
        } else if (is_fleet_arg(arg)) {
            // 编队面由 loadFleetOptions 处理（本循环只放行不解析）
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                ++i;
            }
        } else {
            // 入口显形：未知参数过去静默忽略（drill 脚本曾传 --host/
            // --report-to-port 等本进程无解析分支的 flag，整段配置带病无效）
            std::cerr << "world: unknown arg " << arg << " (see --help)"
                      << std::endl;
            std::exit(2);
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
    logger->info("  Role: " + config.role + " (world manager)");
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

        WorldApp server(config);
        g_stop_request = [&server] { server.stop(); };

        logger->info("Starting server...");
        server.start();

        // login 服务注册拨号（L123 拨号次切片，manager 角色）：配置了 login
        // 服务注册地址即登记 World 逻辑身份（kind=World, id=worldId，自报面
        // = M1 实占监听）。泵挂管理侧拍首钩子（内核线程同拍，单写者纪律不
        // 破）；装配失败降级警告不拦启动（同 admission/cell 注册面惯例）。
        std::unique_ptr<login::M1ServiceRegisterClient> service_register;
        if (config.serviceRegisterPort != 0) {
            login::M1ServiceRegisterClientConfig src;
            src.login_host = config.serviceRegisterHost;
            src.login_port = config.serviceRegisterPort;
            src.service_kind = login::kServiceKindWorld;
            src.logical_id = config.worldId;
            src.advertise_host = config.host;
            src.advertise_port = server.m1Link().listenPort();
            src.route_json = config.m1RouteJson;
            src.self_name = "world-" + std::to_string(config.worldId);
            service_register = login::makeM1ServiceRegisterClient(std::move(src));
            if (service_register->start()) {
                server.set_manager_pre_tick(
                    [&service_register](std::uint64_t now_ms) {
                        service_register->pump(now_ms);
                    });
                logger->info("Service register: dialing "
                             + config.serviceRegisterHost + ":"
                             + std::to_string(config.serviceRegisterPort)
                             + " as world-" + std::to_string(config.worldId));
            } else {
                service_register.reset();
                logger->warning(
                    "service register degraded: config or route table invalid");
            }
        }

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
        if (service_register) {
            service_register->stop();
            service_register.reset();
        }

        logger->info("Server stopped.");
        g_stop_request = nullptr;

    } catch (const std::exception& e) {
        logger->error(std::string("Error: ") + e.what());
        return 1;
    }

    return 0;
}
