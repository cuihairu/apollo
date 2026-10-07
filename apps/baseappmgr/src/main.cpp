#include "baseappmgr/baseappmgr.hpp"
#include "apollo/net/discovery/discovery.hpp"
#include "apollo/net/discovery/udp_transport.hpp"
#include <iostream>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <chrono>

using baseappmgr::BaseAppMgr;

namespace disco = apollo::net::discovery;

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
                      << "  --machined-host <ipv4>     Machined host for death feed (default: 127.0.0.1)\n"
                      << "  --machined-port <port>     Machined port; 0 disables death feed (default: 9600)\n"
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

    // 死亡事件跨进程订阅面（G-1 收尾批增量①）：向 machined 注册死亡监听
    //（BW registerDeathListener 同型），主循环收 DeathNotify 落日志。
    std::string machined_host = "127.0.0.1";
    uint16_t machined_port = 9600;
    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        if (arg == "--machined-host" && i + 1 < argc) {
            machined_host = argv[++i];
        } else if (arg == "--machined-port" && i + 1 < argc) {
            machined_port = static_cast<uint16_t>(std::atoi(argv[++i]));
        }
    }
    const bool death_feed_on = machined_port != 0;
    disco::UdpFeed death_feed;
    disco::UdpBeaconTransport death_transport;
    std::unique_ptr<disco::DeathListener> death_listener;
    if (death_feed_on) {
        if (!death_feed.open(0, "0.0.0.0") || !death_transport.open()) {
            std::cerr << "death feed open failed; running without death reporting"
                      << std::endl;
        } else {
            disco::MemberId self;
            self.component_id = disco::kManagerComponentId;
            self.service_port = death_feed.port();
            death_listener = std::make_unique<disco::DeathListener>(
                self, machined_host, machined_port, death_transport);
        }
    }

    std::cout << "Configuration:" << std::endl;
    std::cout << "  Listen: 0.0.0.0:" << port << std::endl;
    if (death_listener) {
        std::cout << "  Death feed: machined " << machined_host << ":"
                  << machined_port << " (listen port " << death_feed.port() << ")"
                  << std::endl;
    }

    // 注册信号处理
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    try {
        BaseAppMgr mgr(port);
        g_mgr = &mgr;

        std::cout << "\nStarting BaseAppMgr..." << std::endl;
        mgr.start();

        if (death_listener && death_listener->subscribe()) {
            std::cout << "Death feed subscribed to machined " << machined_host
                      << ":" << machined_port << std::endl;
        }

        std::cout << "BaseAppMgr is running. Press Ctrl+C to stop." << std::endl;

        while (mgr.isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (!death_listener) {
                continue;
            }
            // 订阅刷新（幂等——machined 侧 TTL = 3 × 其心跳周期，漏刷即停推）
            death_listener->subscribe();
            // 死亡事件收包（G-1 编队事件进轮询源——§7 控制面进程间延伸）
            std::uint8_t buf[disco::kWireSize];
            std::string sender_host;
            std::uint16_t sender_port = 0;
            while (death_feed.try_receive(buf, sender_host, sender_port)) {
                disco::WirePacket p;
                if (!disco::WirePacket::decode_from(buf, p)) {
                    continue;
                }
                disco::DeathEvent death;
                if (!disco::death_event_from(p, death)) {
                    continue;
                }
                const char* kind = "unknown";
                switch (death.kind) {
                case disco::DeathKind::TtlExpired: kind = "ttl_expired"; break;
                case disco::DeathKind::ChildDied: kind = "child_died"; break;
                case disco::DeathKind::GaveUp: kind = "gave_up"; break;
                }
                std::cout << "[death] component=" << death.member.component_id
                          << " zone=" << death.member.zone_id
                          << " port=" << death.member.service_port
                          << " kind=" << kind
                          << " exit=" << death.exit_code << std::endl;
            }
        }

        std::cout << "BaseAppMgr stopped." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    g_mgr = nullptr;
    return 0;
}
