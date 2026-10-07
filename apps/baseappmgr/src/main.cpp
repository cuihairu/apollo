#include "baseappmgr/baseappmgr.hpp"
#include "apollo/game/session/directory_mirror.hpp"
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
namespace session = apollo::game::session;

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
                      << "  --mirror-host <ipv4>       Directory mirror target host (default: 127.0.0.1)\n"
                      << "  --mirror-port <port>       Directory mirror target port; 0 disables mirror (default: 0)\n"
                      << "  --mirror-snapshot-ms <ms>  Full snapshot publish period (default: 10000)\n"
                      << "  --help, -h                 Show this help\n";
            std::exit(0);
        }
    }

    return port;
}

// 目录全量快照导出（P3-1 增量② owner 侧）：在线目录 Online 条目 → 镜像
// 投影条目（gateway_addr 不上 wire——镜像是定位面不是连接面）。
std::vector<session::MirrorEntry> collect_mirror_entries(const BaseAppMgr& mgr) {
    std::vector<session::MirrorEntry> entries;
    const auto& directory = mgr.directory();
    entries.reserve(directory.size());
    for (const auto player_id : directory.online_ids()) {
        const auto* entry = directory.find(player_id);
        if (entry == nullptr) {
            continue;
        }
        session::MirrorEntry me;
        me.player_id = player_id;
        me.anchor_epoch = entry->anchor_epoch;
        me.session_id = entry->binding.session_id;
        me.gateway_id = entry->binding.gateway_id;
        me.zone_id = entry->zone_id;
        me.state = entry->state;
        me.assignment = entry->assignment;
        entries.push_back(me);
    }
    return entries;
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

    // 在线目录跨进程镜像 owner 面（G-1 收尾批增量②）：目录事件 → Delta
    // 广播 + 周期全量快照（失配 healing）。--mirror-port 0 = 关闭。
    std::string mirror_host = "127.0.0.1";
    uint16_t mirror_port = 0;
    uint32_t mirror_snapshot_ms = 10000;
    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        if (arg == "--mirror-host" && i + 1 < argc) {
            mirror_host = argv[++i];
        } else if (arg == "--mirror-port" && i + 1 < argc) {
            mirror_port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--mirror-snapshot-ms" && i + 1 < argc) {
            mirror_snapshot_ms = static_cast<uint32_t>(std::atoi(argv[++i]));
        }
    }
    disco::UdpBeaconTransport mirror_transport;
    std::unique_ptr<session::DirectoryPublisher> mirror_publisher;
    if (mirror_port != 0) {
        if (!mirror_transport.open()) {
            std::cerr << "mirror transport open failed; running without directory "
                      << "mirror publishing" << std::endl;
        } else {
            const std::string target_host = mirror_host;
            const uint16_t target_port = mirror_port;
            mirror_publisher = std::make_unique<session::DirectoryPublisher>(
                [target_host, target_port, &mirror_transport](
                    const std::uint8_t* data, std::size_t len) {
                    return mirror_transport.send_bytes(target_host, target_port,
                                                       data, len);
                });
        }
    }

    std::cout << "Configuration:" << std::endl;
    std::cout << "  Listen: 0.0.0.0:" << port << std::endl;
    if (death_listener) {
        std::cout << "  Death feed: machined " << machined_host << ":"
                  << machined_port << " (listen port " << death_feed.port() << ")"
                  << std::endl;
    }
    if (mirror_publisher) {
        std::cout << "  Directory mirror: " << mirror_host << ":" << mirror_port
                  << " (snapshot every " << mirror_snapshot_ms << "ms)" << std::endl;
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

        // 目录事件链入镜像 publisher（增量② owner 面：PlayerDirectory 事件
        // → Delta wire；快照在主循环按周期发布）
        if (mirror_publisher) {
            mgr.set_directory_event_listener(
                [&publisher = *mirror_publisher](
                    const apollo::game::session::PlayerDirectory::Event& event) {
                    publisher.publish(event);
                });
            // 启动即发一轮全量（晚加入镜像不用等首周期）
            const auto initial = collect_mirror_entries(mgr);
            mirror_publisher->publish_snapshot(initial);
            std::cout << "Directory mirror: initial snapshot sent ("
                      << initial.size() << " entries)" << std::endl;
        }

        std::cout << "BaseAppMgr is running. Press Ctrl+C to stop." << std::endl;

        auto since_snapshot = std::chrono::milliseconds(0);
        while (mgr.isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            since_snapshot += std::chrono::seconds(1);

            // 周期全量快照（增量②对账 healing 面：delta 丢失由快照收敛）
            if (mirror_publisher && since_snapshot.count() >= mirror_snapshot_ms) {
                since_snapshot = std::chrono::milliseconds(0);
                const auto entries = collect_mirror_entries(mgr);
                const auto parts = mirror_publisher->publish_snapshot(entries);
                std::cout << "[mirror] snapshot published: entries=" << entries.size()
                          << " parts=" << parts << " seq="
                          << mirror_publisher->seq() << std::endl;
            }

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
