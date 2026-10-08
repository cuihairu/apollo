#include "base/base_server.hpp"
#include "apollo/game/session/directory_mirror.hpp"
#include "apollo/game/session/fleet_recovery.hpp"
#include "apollo/net/discovery/udp_transport.hpp"
#include <iostream>
#include <csignal>
#include <cstdlib>
#include <memory>

using namespace base;

namespace session = apollo::game::session;
namespace disco = apollo::net::discovery;

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
                      << "  --mirror-port <port>       Directory mirror listen port; 0 disables mirror (default: 0)\n"
                      << "  --mirror-owner-host <ipv4> Directory owner host for snapshot requests (default: 127.0.0.1)\n"
                      << "  --mirror-owner-port <port> Directory owner port; 0 disables requests (default: 0)\n"
                      << "  --report-to-host <ipv4>    Fleet recovery full-report target host (default: 127.0.0.1)\n"
                      << "  --report-to-port <port>    Fleet recovery full-report target port; 0 disables (default: 0)\n"
                      << "  --component-id <n>         This component's fleet id for full reports (required with --report-to-port)\n"
                      << "  --zone-id <n>              This component's home zone id (default: 0)\n"
                      << "  --report-interval-ms <ms>  Full report refresh period; 0 = startup only (default: 30000)\n"
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

    // 在线目录跨进程镜像消费面（G-1 收尾批增量②）：bind 收包口 → 目录
    // 本地只读投影（seq 续传 + 快照重组）。--mirror-port 0 = 关闭。
    uint16_t mirror_port = 0;
    std::string mirror_owner_host = "127.0.0.1";
    uint16_t mirror_owner_port = 0;
    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        if (arg == "--mirror-port" && i + 1 < argc) {
            mirror_port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--mirror-owner-host" && i + 1 < argc) {
            mirror_owner_host = argv[++i];
        } else if (arg == "--mirror-owner-port" && i + 1 < argc) {
            mirror_owner_port = static_cast<uint16_t>(std::atoi(argv[++i]));
        }
    }
    disco::UdpFeed mirror_feed;
    disco::UdpBeaconTransport mirror_request_transport;
    std::unique_ptr<session::DirectoryMirror> mirror;
    if (mirror_port != 0) {
        if (!mirror_feed.open(mirror_port, "0.0.0.0")) {
            std::cerr << "mirror feed bind failed on port " << mirror_port
                      << "; running without directory mirror" << std::endl;
        } else {
            mirror = std::make_unique<session::DirectoryMirror>();
            if (mirror_owner_port != 0 && mirror_request_transport.open()) {
                const std::string owner_host = mirror_owner_host;
                const uint16_t owner_port = mirror_owner_port;
                mirror->set_snapshot_requester(
                    [owner_host, owner_port, &mirror_request_transport]() {
                        std::uint8_t req[session::kDirectoryWireHeaderSize];
                        if (session::encode_snapshot_request(req, sizeof(req)) != 0) {
                            (void)mirror_request_transport.send_bytes(
                                owner_host, owner_port, req, sizeof(req));
                        }
                    });
            }
        }
    }

    // 恢复相位全量重报面（G-1 收尾批增量③）：本进程现存会话（AnchorManager
    // 快照）→ FullReport 分片发 manager——启动即报 + 周期刷新（幂等 intake）。
    // --report-to-port 0 = 关闭；开着则 --component-id 必填（0 拒发）。
    std::string report_to_host = "127.0.0.1";
    uint16_t report_to_port = 0;
    uint64_t component_id = 0;
    uint32_t zone_id = 0;
    uint32_t report_interval_ms = 30000;
    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        if (arg == "--report-to-host" && i + 1 < argc) {
            report_to_host = argv[++i];
        } else if (arg == "--report-to-port" && i + 1 < argc) {
            report_to_port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--component-id" && i + 1 < argc) {
            component_id = static_cast<uint64_t>(std::strtoull(argv[++i], nullptr, 10));
        } else if (arg == "--zone-id" && i + 1 < argc) {
            zone_id = static_cast<uint32_t>(std::atoi(argv[++i]));
        } else if (arg == "--report-interval-ms" && i + 1 < argc) {
            report_interval_ms = static_cast<uint32_t>(std::atoi(argv[++i]));
        }
    }
    disco::UdpBeaconTransport report_transport;
    std::unique_ptr<session::FleetReporter> reporter;
    if (report_to_port != 0) {
        if (component_id == 0) {
            std::cerr << "report-to-port set without --component-id; full report "
                      << "disabled" << std::endl;
        } else if (!report_transport.open()) {
            std::cerr << "report transport open failed; full report disabled"
                      << std::endl;
        } else {
            const std::string target_host = report_to_host;
            const uint16_t target_port = report_to_port;
            reporter = std::make_unique<session::FleetReporter>(
                component_id, zone_id,
                [target_host, target_port, &report_transport](
                    const std::uint8_t* data, std::size_t len) {
                    return report_transport.send_bytes(target_host, target_port,
                                                       data, len);
                });
        }
    }

    std::cout << "Configuration:" << std::endl;
    std::cout << "  Listen: " << config.host << ":" << config.port << std::endl;
    std::cout << "  Database: " << config.dbHost << ":" << config.dbPort << "/" << config.dbName << std::endl;
    if (mirror) {
        std::cout << "  Directory mirror: listen 0.0.0.0:" << mirror_feed.port();
        if (mirror_owner_port != 0) {
            std::cout << " (snapshot requests to " << mirror_owner_host << ":"
                      << mirror_owner_port << ")";
        }
        std::cout << std::endl;
    }
    if (reporter) {
        std::cout << "  Fleet report: component=" << component_id << " zone="
                  << zone_id << " -> " << report_to_host << ":" << report_to_port
                  << " (interval " << report_interval_ms << "ms)" << std::endl;
    }

    // 注册信号处理
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    try {
        BaseServer server(config);
        g_server = &server;

        std::cout << "\nStarting server..." << std::endl;
        server.start();

        std::cout << "Server is running. Press Ctrl+C to stop." << std::endl;

        // 全量重报收集（增量③报告方供数面）：AnchorManager 快照 → 投影条目
        //（epoch 列恒 0——裁决键归 manager，intake restore-not-kick）
        const auto collect_sessions = [&server]() {
            std::vector<session::MirrorEntry> sessions;
            if (const auto anchors = server.anchor_manager()) {
                sessions.reserve(anchors->anchor_count());
                for (const auto& anchor : anchors->snapshot()) {
                    if (!anchor || anchor->player_id() == 0) {
                        continue;
                    }
                    session::MirrorEntry e;
                    e.player_id = anchor->player_id();
                    e.session_id = anchor->session_binding().session_id;
                    e.gateway_id = anchor->session_binding().gateway_id;
                    e.zone_id = anchor->home_zone_id();
                    e.state = session::PlayerDirectory::EntryState::Online;
                    e.assignment = anchor->world_assignment();
                    sessions.push_back(e);
                }
            }
            return sessions;
        };
        if (reporter) {
            const auto sent = reporter->report_full(collect_sessions());
            std::cout << "[report] startup full report sent: parts=" << sent
                      << std::endl;
        }

        auto since_report = std::chrono::milliseconds(0);
        // 镜像状态去噪观测面：投影行数/seq/断档态/收包数变化才落一行日志
        std::size_t last_logged_size = static_cast<std::size_t>(-1);
        std::uint32_t last_logged_seq = 0;
        bool last_logged_stale = false;
        std::uint64_t mirror_rx = 0;
        std::uint64_t last_logged_rx = static_cast<std::uint64_t>(-1);
        std::uint8_t mirror_buf[disco::kMaxDatagramSize];
        while (server.isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            // 周期全量重报刷新（增量③：幂等 intake——manager 侧 restore-
            // not-kick，重放无害；0 = 仅启动期一报）
            if (reporter && report_interval_ms != 0) {
                since_report += std::chrono::seconds(1);
                if (since_report.count() >= report_interval_ms) {
                    since_report = std::chrono::milliseconds(0);
                    const auto sent = reporter->report_full(collect_sessions());
                    std::cout << "[report] periodic full report sent: parts=" << sent
                              << std::endl;
                }
            }
            if (!mirror) {
                continue;
            }
            // 目录镜像收包（G-1 编队事件进轮询源——§7 控制面进程间延伸）
            std::size_t len = 0;
            while (mirror_feed.try_receive(mirror_buf, sizeof(mirror_buf), len)) {
                mirror->on_datagram(mirror_buf, len);
                ++mirror_rx;
            }
            const std::size_t cur_size = mirror->size();
            const std::uint32_t cur_seq = mirror->last_seq();
            const bool cur_stale = mirror->stale();
            if (cur_size != last_logged_size || cur_seq != last_logged_seq ||
                cur_stale != last_logged_stale || mirror_rx != last_logged_rx) {
                last_logged_size = cur_size;
                last_logged_seq = cur_seq;
                last_logged_stale = cur_stale;
                last_logged_rx = mirror_rx;
                std::cout << "[mirror] rx=" << mirror_rx << " entries=" << cur_size
                          << " seq=" << cur_seq
                          << (cur_stale ? " STALE(等待快照)" : "") << std::endl;
            }
        }

        std::cout << "Server stopped." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    g_server = nullptr;
    return 0;
}
