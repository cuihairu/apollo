#include "base/base_server.hpp"
#include "apollo/core/log/log_manager.h"
#include "apollo/game/session/directory_mirror.hpp"
#include "apollo/game/session/fleet_recovery.hpp"
#include "apollo/net/discovery/udp_transport.hpp"
#include "apollo/runtime/crash_capture.hpp"
#include <iostream>
#include <csignal>
#include <cstdlib>
#include <memory>

using namespace base;

namespace session = apollo::game::session;
namespace disco = apollo::net::discovery;

// 全局服务器指针
static BaseServer* g_server = nullptr;

// 信号处理（信号上下文保持直写 console——logger 面带锁，信号路径不进）
void signalHandler(int signal) {
    if (g_server) {
        std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
        g_server->stop();
    }
}

// 日志接线（ADR-013 L1+L2）：console 人读 + 本地结构化文件真相源并行；
// push 出口留接口不实现（collector 面 M1 后，logging.md §5）。
// 领域子日志（report/mirror/demo）= §5.1 cat= 检索键
static apollo::core::log::LoggerPtr initLogging() {
    auto& logs = apollo::core::log::global_log_manager();
    apollo::core::log::LogManagerConfig config;
    config.processIdentity = "zone-app";
    config.fileEnabled = true;
    config.fileConfig.directory = "log";
    config.fileConfig.baseName = "zone-app";
    config.fileConfig.structuredOutput = true;  // §5.1 结构化行（六键固定序）
    logs.initialize(config);
    return logs.createLogger("zone-app");
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
                      << "  --demo-anchors <n>         Seed n smoke anchors (player 1..n, gateway=1, zone=--zone-id); smoke driver only\n"
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
    // 崩溃采集面（crash-capture 批②）：最早段立采集，失败 fail-open 不阻断。
    apollo::runtime::init_crash_capture(argc, argv, "zone-app");

    std::cout << "======================================" << std::endl;
    std::cout << "       Apollo Base Server           " << std::endl;
    std::cout << "======================================" << std::endl;

    auto& logs = apollo::core::log::global_log_manager();
    const auto reportLog = logs.createLogger("report");
    const auto mirrorLog = logs.createLogger("mirror");
    const auto demoLog = logs.createLogger("demo");

    // 验收面故意崩溃开关（crash-capture 设计 §3 步骤 2；smoke driver 同
    // --demo-anchors 先例——真实用途验证后即弃用注记）
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--crash-test") {
            const std::string kind = (i + 1 < argc) ? argv[i + 1] : "null";
            apollo::runtime::crash_test(kind);
        }
    }

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
            logger->warning("mirror feed bind failed on port "
                            + std::to_string(mirror_port)
                            + "; running without directory mirror");
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
    uint32_t demo_anchors = 0;
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
        } else if (arg == "--demo-anchors" && i + 1 < argc) {
            demo_anchors = static_cast<uint32_t>(std::atoi(argv[++i]));
        }
    }
    disco::UdpBeaconTransport report_transport;
    std::unique_ptr<session::FleetReporter> reporter;
    if (report_to_port != 0) {
        if (component_id == 0) {
            logger->warning(
                "report-to-port set without --component-id; full report disabled");
        } else if (!report_transport.open()) {
            logger->warning("report transport open failed; full report disabled");
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

    logger->info("Configuration:");
    logger->info("  Listen: " + config.host + ":" + std::to_string(config.port));
    logger->info("  Database: " + config.dbHost + ":" + std::to_string(config.dbPort)
                 + "/" + config.dbName);
    if (mirror) {
        std::string line = "  Directory mirror: listen 0.0.0.0:"
                           + std::to_string(mirror_feed.port());
        if (mirror_owner_port != 0) {
            line += " (snapshot requests to " + mirror_owner_host + ":"
                    + std::to_string(mirror_owner_port) + ")";
        }
        logger->info(line);
    }
    if (reporter) {
        logger->info("  Fleet report: component=" + std::to_string(component_id)
                     + " zone=" + std::to_string(zone_id) + " -> " + report_to_host
                     + ":" + std::to_string(report_to_port) + " (interval "
                     + std::to_string(report_interval_ms) + "ms)");
    }

    // 注册信号处理
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    try {
        BaseServer server(config);
        g_server = &server;

        logger->info("Starting server...");
        server.start();

        // 冒烟驱动面（§6 窗口处置批）：--demo-anchors N 造 N 个假想会话锚点
        //（player 1..N，gateway=1，home_zone = --zone-id），全量重报/死亡行
        // 窗口处置真进程链路方有非零条目可观察。骨架期驱动面——真实会话经
        // 登录/gateway 流落锚后此开关即弃用。
        if (demo_anchors > 0 && server.anchor_manager()) {
            for (uint64_t i = 1; i <= demo_anchors; ++i) {
                const auto anchor = server.anchor_manager()->activate(i);
                if (!anchor) {
                    continue;
                }
                session::SessionBinding binding;
                binding.session_id = 1000 + i;
                binding.gateway_id = 1;
                anchor->bind_session(binding);
                anchor->set_home_zone_id(zone_id);
            }
            demoLog->info("seeded " + std::to_string(demo_anchors)
                          + " smoke anchors (gateway=1, zone=" + std::to_string(zone_id)
                          + ")");
        }

        logger->info("Server is running. Press Ctrl+C to stop.");

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
            reportLog->info("startup full report sent: parts=" + std::to_string(sent));
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
                    reportLog->info("periodic full report sent: parts="
                                    + std::to_string(sent));
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
                mirrorLog->info("rx=" + std::to_string(mirror_rx)
                                + " entries=" + std::to_string(cur_size) + " seq="
                                + std::to_string(cur_seq)
                                + (cur_stale ? " STALE(等待快照)" : ""));
            }
        }

        logger->info("Server stopped.");

    } catch (const std::exception& e) {
        logger->error(std::string("Error: ") + e.what());
        return 1;
    }

    g_server = nullptr;
    return 0;
}
