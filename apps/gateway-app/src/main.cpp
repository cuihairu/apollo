#include "apollo/runtime/crash_capture.hpp"
#include "apollo/core/log/log_manager.h"
#include "gateway/gateway_server.hpp"
#include "gateway/config.hpp"
#include <iostream>
#include <csignal>
#include <cstdlib>

using namespace gateway;

// 全局服务器指针
static GatewayServer* g_server = nullptr;

// 信号处理（信号上下文保持直写 console——logger 面带锁，信号路径不进）
void signalHandler(int signal) {
    if (g_server) {
        std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
        g_server->stop();
    }
}

// 日志接线（ADR-013 L1+L2）：console 人读 + 本地结构化文件真相源并行；
// push 出口留接口不实现（collector 面 M1 后，logging.md §5）
static apollo::core::log::LoggerPtr initLogging() {
    auto& logs = apollo::core::log::global_log_manager();
    apollo::core::log::LogManagerConfig config;
    config.processIdentity = "gateway";
    config.fileEnabled = true;
    config.fileConfig.directory = "log";
    config.fileConfig.baseName = "gateway";
    config.fileConfig.structuredOutput = true;  // §5.1 结构化行（六键固定序）
    logs.initialize(config);
    return logs.createLogger("gateway");
}

// 加载配置
GatewayConfig loadConfig(int argc, char* argv[]) {
    GatewayConfig config;

    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--port" && i + 1 < argc) {
            config.port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--host" && i + 1 < argc) {
            config.host = argv[++i];
        } else if (arg == "--max-connections" && i + 1 < argc) {
            config.maxConnections = std::atoi(argv[++i]);
        } else if (arg == "--login-app" && i + 1 < argc) {
            config.loginAppUrl = argv[++i];
        } else if (arg == "--base-app" && i + 1 < argc) {
            config.baseAppUrl = argv[++i];
        } else if (arg == "--cell-app" && i + 1 < argc) {
            config.cellAppUrl = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --port <port>              Listen port (default: 8888)\n"
                      << "  --host <host>              Listen address (default: 0.0.0.0)\n"
                      << "  --max-connections <num>     Max connections (default: 10000)\n"
                      << "  --login-app <url>          LoginApp URL\n"
                      << "  --base-app <url>           BaseApp URL\n"
                      << "  --cell-app <url>           CellApp URL\n"
                      << "  --help, -h                 Show this help\n";
            std::exit(0);
        }
    }

    return config;
}

int main(int argc, char* argv[]) {
    // 先于 crash capture：write() 在未初始化时会惰性按默认配置建管理器（仅 console），
    // 顺序颠倒会使本进程的文件面配置被默认初始化顶掉（initialize 幂等早退）
    const auto logger = initLogging();
    apollo::runtime::init_crash_capture(argc, argv, "gateway-app");
    std::cout << "======================================" << std::endl;
    std::cout << "       Apollo Gateway Server        " << std::endl;
    std::cout << "======================================" << std::endl;

    // 加载配置
    auto config = loadConfig(argc, argv);

    logger->info("Configuration:");
    logger->info("  Listen: " + config.host + ":" + std::to_string(config.port));
    logger->info("  Max connections: " + std::to_string(config.maxConnections));
    logger->info("  LoginApp: " + config.loginAppUrl);
    logger->info("  BaseApp: " + config.baseAppUrl);
    logger->info("  CellApp: " + config.cellAppUrl);

    // 注册信号处理
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

#ifdef _WIN32
    std::signal(SIGBREAK, signalHandler);
#else
    std::signal(SIGHUP, signalHandler);
#endif

    try {
        // 创建服务器
        GatewayServer server(config);
        g_server = &server;

        // 启动服务器
        logger->info("Starting server...");
        server.start();

        logger->info("Server is running. Press Ctrl+C to stop.");

        // 主循环
        while (server.isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        logger->info("Server stopped.");

    } catch (const std::exception& e) {
        logger->error(std::string("Error: ") + e.what());
        return 1;
    }

    g_server = nullptr;
    return 0;
}
