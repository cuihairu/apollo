#include "apollo/runtime/crash_capture.hpp"
#include "apollo/core/log/log_manager.h"
#include "apollo/net/logpush/log_push_env.hpp"
#include "apollo/net/metricspush/metrics_push_env.hpp"
#include "apollo/net/discovery/discovery.hpp"
#include "apollo/net/discovery/udp_transport.hpp"
#include "world_app/world_app.hpp"
#include <iostream>
#include <csignal>
#include <cstdint>
#include <memory>

using namespace world_app;

// 全局服务器指针
static WorldApp* g_server = nullptr;

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

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--world-id" && i + 1 < argc) {
            config.worldId = static_cast<std::uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--max-instances" && i + 1 < argc) {
            config.maxInstances = static_cast<std::size_t>(std::stoull(argv[++i]));
        } else if (arg == "--m1-port" && i + 1 < argc) {
            config.m1Port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--space" && i + 1 < argc) {
            config.spaceName = argv[++i];
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
                      << "  --world-id <id>            World identity (default: 1)\n"
                      << "  --max-instances <n>        Squad instance capacity (default: 16)\n"
                      << "  --m1-port <port>           M1 link listen port (default: 9101)\n"
                      << "  --space <name>              Space name (default: main_world)\n"
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

    } catch (const std::exception& e) {
        logger->error(std::string("Error: ") + e.what());
        return 1;
    }

    g_server = nullptr;
    return 0;
}
