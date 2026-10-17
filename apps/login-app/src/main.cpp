#include "apollo/runtime/crash_capture.hpp"
#include "apollo/core/log/log_manager.h"
#include "apollo/net/logpush/log_push_env.hpp"
#include "apollo/net/metricspush/metrics_push_env.hpp"
#include "apollo/net/discovery/discovery.hpp"
#include "apollo/net/discovery/udp_transport.hpp"
#include "login/login_server.hpp"
#include <iostream>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <memory>

using namespace login;

// 全局服务器指针
static LoginServer* g_server = nullptr;

// 信号处理（信号上下文保持直写 console——logger 面带锁，信号路径不进）
void signalHandler(int signal) {
    if (g_server) {
        std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
        g_server->stop();
    }
}

// 日志接线（ADR-013 L1+L2）：console 人读 + 本地结构化文件真相源并行；
// push 出口 = APOLLO_LOG_PUSH_ADDR 环境接线（logging.md §5；未设 = 关）。
static apollo::core::log::LoggerPtr initLogging() {
    auto& logs = apollo::core::log::global_log_manager();
    apollo::core::log::LogManagerConfig config;
    config.processIdentity = "login";
    config.fileEnabled = true;
    config.fileConfig.directory = "log";
    config.fileConfig.baseName = "login";
    config.fileConfig.structuredOutput = true;  // §5.1 结构化行（六键固定序）
    // L4 push 出口（logging.md §5 双出口）：APOLLO_LOG_PUSH_ADDR 非空才接
    // （collector 缺席 = 客户端有界队列缓冲，本地文件恒真相源）
    if (auto push_client = apollo::net::logpush::make_log_push_from_env("login")) {
        config.pushSink = [push_client](const std::string& line) {
            push_client->push(line);
        };
    }
    logs.initialize(config);
    return logs.createLogger("login");
}

// 加载配置
LoginConfig loadConfig(int argc, char* argv[]) {
    LoginConfig config;
    config.directoryEnabled = true;
    config.directoryPort = 9105;
    config.directoryEpochPath = "data/login/epochs.json";
    config.worldStoreM1Port = 9104;
    config.m1Port = 9103;
    bool gatewaysConfigured = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--directory-port" && i + 1 < argc) {
            config.directoryPort = static_cast<std::uint16_t>(std::stoul(argv[++i]));
        } else if (arg == "--directory-epochs" && i + 1 < argc) {
            config.directoryEpochPath = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            config.port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--host" && i + 1 < argc) {
            config.host = argv[++i];

        } else if (arg == "--gateway" && i + 1 < argc) {
            if (!gatewaysConfigured) {
                config.gatewayUrls.clear();
                gatewaysConfigured = true;
            }
            config.gatewayUrls.push_back(argv[++i]);
        } else if (arg == "--m1-port" && i + 1 < argc) {
            config.m1Port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--world-store-m1-port" && i + 1 < argc) {
            config.worldStoreM1Port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--directory-client-port" && i + 1 < argc) {
            config.m1OnlineDirectoryPort = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--mirror-host" && i + 1 < argc) {
            config.mirrorHost = argv[++i];
        } else if (arg == "--mirror-port" && i + 1 < argc) {
            config.mirrorPort = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--mirror-snapshot-ms" && i + 1 < argc) {
            config.mirrorSnapshotMs = static_cast<std::uint32_t>(std::atoi(argv[++i]));
        } else if (arg == "--recovery-port" && i + 1 < argc) {
            config.recoveryPort = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--recovery-expected" && i + 1 < argc) {
            config.recoveryExpected = static_cast<std::uint32_t>(std::atoi(argv[++i]));
        } else if (arg == "--recovery-timeout-ms" && i + 1 < argc) {
            config.recoveryTimeoutMs = static_cast<std::uint32_t>(std::atoi(argv[++i]));
        } else if (arg == "--suspend-window-ticks" && i + 1 < argc) {
            config.suspendWindowTicks = static_cast<std::uint32_t>(std::atoi(argv[++i]));
        } else if (arg == "--gateway-component" && i + 1 < argc) {
            // 死亡 wire 不带 gateway 身份：component → gateway 显式映射
            const std::string spec = argv[++i];
            const auto colon = spec.find(':');
            if (colon != std::string::npos) {
                const std::uint64_t cid =
                    std::strtoull(spec.substr(0, colon).c_str(), nullptr, 10);
                const std::uint32_t gid = static_cast<std::uint32_t>(
                    std::strtoul(spec.substr(colon + 1).c_str(), nullptr, 10));
                if (cid != 0 && gid != 0) {
                    config.gatewayByComponent[cid] = gid;
                }
            }
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --directory-port <port>    Hosted directory port (default: 9105)\n"
                      << "  --directory-epochs <path>  Directory epoch storage\n"
                      << "  --port <port>              Listen port (default: 9001)\n"
                      << "  --host <host>              Listen address (default: 0.0.0.0)\n"
                      << "  --gateway <url>             Gateway URL (can be multiple)\n"
                      << "  --m1-port <port>           M1 admission listen port (0 = off, default: 0)\n"
                      << "  --world-store-m1-port <port>  World player data endpoint (default: 9104)\n"
                      << "  --directory-client-port <port>   OnlineDirectory M1 control dial port (0 = off, default: 0)\n"
                      << "  --mirror-host <ipv4>       Directory mirror target host (default: 127.0.0.1)\n"
                      << "  --mirror-port <port>       Directory mirror target port; 0 disables (default: 0)\n"
                      << "  --mirror-snapshot-ms <ms>  Full snapshot publish period (default: 10000)\n"
                      << "  --recovery-port <port>     Fleet recovery intake port; 0 disables (default: 0; 0-port bind = kernel assigned)\n"
                      << "  --recovery-expected <n>    Expected full-report senders (default: 0)\n"
                      << "  --recovery-timeout-ms <ms> Recovery phase timeout window (default: 5000)\n"
                      << "  --machined-host <ipv4>     Machined host for death feed (default: 127.0.0.1)\n"
                      << "  --machined-port <port>     Machined port; 0 disables death feed (default: 0)\n"
                      << "  --component-id <n>         This component's fleet id (death feed identity)\n"
                      << "  --suspend-window-ticks <n> Death-row keepalive window ticks, tick = 1s (default: 30)\n"
                      << "  --gateway-component <cid:gid>  Death component -> gateway id mapping, repeatable\n"
                      << "  --help, -h                 Show this help\n";
            std::exit(0);
        }
    }

    return config;
}

// 编队成员注册（P3-4 收尾批）：--machined-port 0 = 关；service_port = 客户端
// 入口端口（消费方拨号键）。
struct FleetOptions {
    std::string machined_host = "127.0.0.1";
    std::uint16_t machined_port = 0;
    std::uint64_t component_id = 0;
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
            opts.component_id = std::strtoull(argv[++i], nullptr, 10);
        }
    }
    return opts;
}

int main(int argc, char* argv[]) {
    // 先于 crash capture：write() 在未初始化时会惰性按默认配置建管理器（仅 console），
    // 顺序颠倒会使本进程的文件面配置被默认初始化顶掉（initialize 幂等早退）
    const auto logger = initLogging();
    // L4 指标 push 出口（logging.md §5.1 采样流）：liveness-only（SessionManager
    // 无计数 accessor——空快照保 proc_up 存活面）
    auto metrics_push = apollo::net::metricspush::make_metrics_push_from_env("login");
    (void)metrics_push;
    apollo::runtime::init_crash_capture(argc, argv, "login-app");
    std::cout << "======================================" << std::endl;
    std::cout << "       Apollo Login Server          " << std::endl;
    std::cout << "======================================" << std::endl;

    auto config = loadConfig(argc, argv);
    // 死亡订阅面身份（P3-1 re-home）：machined 地址复用编队注册的组件号口径
    const FleetOptions fleet = loadFleetOptions(argc, argv);
    config.machinedHost = fleet.machined_host;
    config.machinedPort = fleet.machined_port;
    config.machinedComponentId = fleet.component_id;

    logger->info("Configuration:");
    logger->info("  Listen: " + config.host + ":" + std::to_string(config.port));
    logger->info("  Gateways: " + std::to_string(config.gatewayUrls.size()));

    // 注册信号处理
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

#ifdef _WIN32
    std::signal(SIGBREAK, signalHandler);
#else
    std::signal(SIGHUP, signalHandler);
#endif

    try {
        LoginServer server(config);
        g_server = &server;

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
                self.service_port = config.port;
                fleet_member =
                    std::make_unique<apollo::net::discovery::FleetMember>(
                        self, fleet.machined_host, fleet.machined_port,
                        fleet_transport);
                logger->info("fleet member: component="
                             + std::to_string(fleet.component_id)
                             + " service_port=" + std::to_string(config.port)
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

    } catch (const std::exception& e) {
        logger->error(std::string("Error: ") + e.what());
        return 1;
    }

    g_server = nullptr;
    return 0;
}
