#include "base/base_server.hpp"
#include <iostream>
#include <csignal>

using namespace base;

// 全局服务器指针
static BaseServer* g_server = nullptr;

// 信号处理
void signalHandler(int signal) {
    if (g_server) {
        std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
        g_server->stop();
    }
}

// 加载配置
BaseConfig loadConfig(int argc, char* argv[]) {
    BaseConfig config;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--port" && i + 1 < argc) {
            config.port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--db" && i + 1 < argc) {
            config.dbName = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --port <port>              Listen port (default: 9002)\n"
                      << "  --db <name>                Database name\n"
                      << "  --help, -h                 Show this help\n";
            std::exit(0);
        }
    }

    return config;
}

int main(int argc, char* argv[]) {
    std::cout << "======================================" << std::endl;
    std::cout << "       Apollo Base Server           " << std::endl;
    std::cout << "======================================" << std::endl;

    auto config = loadConfig(argc, argv);

    std::cout << "Configuration:" << std::endl;
    std::cout << "  Listen: " << config.host << ":" << config.port << std::endl;
    std::cout << "  Database: " << config.dbHost << ":" << config.dbPort << "/" << config.dbName << std::endl;

    // 注册信号处理
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    try {
        BaseServer server(config);
        g_server = &server;

        std::cout << "\nStarting server..." << std::endl;
        server.start();

        std::cout << "Server is running. Press Ctrl+C to stop." << std::endl;

        while (server.isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        std::cout << "Server stopped." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    g_server = nullptr;
    return 0;
}
