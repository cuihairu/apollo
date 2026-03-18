#include "cell/cell_server.hpp"
#include <iostream>
#include <csignal>

using namespace cell;

// 全局服务器指针
static CellServer* g_server = nullptr;

// 信号处理
void signalHandler(int signal) {
    if (g_server) {
        std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
        g_server->stop();
    }
}

// 加载配置
CellConfig loadConfig(int argc, char* argv[]) {
    CellConfig config;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--port" && i + 1 < argc) {
            config.port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--space" && i + 1 < argc) {
            config.spaceName = argv[++i];
        } else if (arg == "--size" && i + 2 < argc) {
            config.spaceWidth = static_cast<float>(std::atof(argv[++i]));
            config.spaceHeight = static_cast<float>(std::atof(argv[++i]));
        } else if (arg == "--tick-rate" && i + 1 < argc) {
            config.tickRateMs = std::atoi(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --port <port>              Listen port (default: 9100)\n"
                      << "  --space <name>              Space name (default: main_world)\n"
                      << "  --size <width> <height>     Space size (default: 1000 1000)\n"
                      << "  --tick-rate <ms>            Tick rate in ms (default: 50)\n"
                      << "  --help, -h                 Show this help\n";
            std::exit(0);
        }
    }

    return config;
}

int main(int argc, char* argv[]) {
    std::cout << "======================================" << std::endl;
    std::cout << "       Apollo Cell Server           " << std::endl;
    std::cout << "======================================" << std::endl;

    auto config = loadConfig(argc, argv);

    std::cout << "Configuration:" << std::endl;
    std::cout << "  Listen: " << config.host << ":" << config.port << std::endl;
    std::cout << "  Space: " << config.spaceName << " (" << config.spaceWidth
              << "x" << config.spaceHeight << ")" << std::endl;
    std::cout << "  Tick rate: " << config.tickRateMs << "ms (" << (1000 / config.tickRateMs) << " Hz)" << std::endl;

    // 注册信号处理
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    try {
        CellServer server(config);
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
