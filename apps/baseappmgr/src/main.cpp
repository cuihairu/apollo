#include "baseappmgr/baseappmgr.hpp"
#include <iostream>
#include <csignal>
#include <cstdlib>
#include <thread>
#include <chrono>

using baseappmgr::BaseAppMgr;

// 全局服务器指针
static BaseAppMgr* g_mgr = nullptr;

// 信号处理
void signalHandler(int signal) {
    if (g_mgr) {
        std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
        g_mgr->stop();
    }
}

// 加载配置
uint16_t loadPort(int argc, char* argv[]) {
    uint16_t port = 9003;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --port <port>              Listen port (default: 9003)\n"
                      << "  --help, -h                 Show this help\n";
            std::exit(0);
        }
    }

    return port;
}

int main(int argc, char* argv[]) {
    std::cout << "======================================" << std::endl;
    std::cout << "       Apollo BaseAppMgr             " << std::endl;
    std::cout << "======================================" << std::endl;

    auto port = loadPort(argc, argv);

    std::cout << "Configuration:" << std::endl;
    std::cout << "  Listen: 0.0.0.0:" << port << std::endl;

    // 注册信号处理
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    try {
        BaseAppMgr mgr(port);
        g_mgr = &mgr;

        std::cout << "\nStarting BaseAppMgr..." << std::endl;
        mgr.start();

        std::cout << "BaseAppMgr is running. Press Ctrl+C to stop." << std::endl;

        while (mgr.isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        std::cout << "BaseAppMgr stopped." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    g_mgr = nullptr;
    return 0;
}
