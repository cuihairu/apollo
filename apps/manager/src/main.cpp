#include "apollo/runtime/crash_capture.hpp"
#include "apollo/core/log/log_manager.h"
#include "manager/manager_app.hpp"
#include "apollo/game/session/directory_mirror.hpp"
#include "apollo/game/session/fleet_recovery.hpp"
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
#include <unordered_map>

using manager::ManagerApp;

namespace disco = apollo::net::discovery;
namespace session = apollo::game::session;

// 全局服务器指针
static ManagerApp* g_mgr = nullptr;

// 信号处理（信号上下文保持直写 console——logger 面带锁，信号路径不进）
void signalHandler(int signal) {
    if (g_mgr) {
        std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
        g_mgr->stop();
    }
}

// 日志接线（ADR-013 L1+L2）：console 人读 + 本地结构化文件真相源并行；
// push 出口留接口不实现（collector 面 M1 后，logging.md §5）。
// 领域子日志（recovery/mirror/window/death）= §5.1 cat= 检索键
static apollo::core::log::LoggerPtr initLogging() {
    auto& logs = apollo::core::log::global_log_manager();
    apollo::core::log::LogManagerConfig config;
    config.processIdentity = "manager";
    config.fileEnabled = true;
    config.fileConfig.directory = "log";
    config.fileConfig.baseName = "manager";
    config.fileConfig.structuredOutput = true;  // §5.1 结构化行（六键固定序）
    logs.initialize(config);
    return logs.createLogger("manager");
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
                      << "  --recovery-port <port>     Fleet recovery FullReport intake port; 0 disables (default: 0)\n"
                      << "  --recovery-expected <n>    Expected full-report senders (converge when all reported; default: 0)\n"
                      << "  --recovery-timeout-ms <ms> Recovery phase timeout window (default: 5000)\n"
                      << "  --suspend-window-ticks <n> Suspend window ticks for death-row disposition, tick = 1s loop beat (default: 30)\n"
                      << "  --gateway-component <cid:gid> Death component -> gateway id mapping, repeatable (gateway-row disposition)\n"
                      << "  --help, -h                 Show this help\n";
            std::exit(0);
        }
    }

    return port;
}

// 稳态钟（恢复相位超时判定用——与目录面 tick 无关，仅单调毫秒）
std::uint64_t getSteadyNowMs() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

// 目录全量快照导出（P3-1 增量② owner 侧）：在线目录 Online 条目 → 镜像
// 投影条目（gateway_addr 不上 wire——镜像是定位面不是连接面）。
std::vector<session::MirrorEntry> collect_mirror_entries(const ManagerApp& mgr) {
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
    // 先于 crash capture：write() 在未初始化时会惰性按默认配置建管理器（仅 console），
    // 顺序颠倒会使本进程的文件面配置被默认初始化顶掉（initialize 幂等早退）
    const auto logger = initLogging();
    apollo::runtime::init_crash_capture(argc, argv, "manager");
    std::cout << "======================================" << std::endl;
    std::cout << "       Apollo ManagerApp             " << std::endl;
    std::cout << "======================================" << std::endl;

    auto& logs = apollo::core::log::global_log_manager();
    const auto recoveryLog = logs.createLogger("recovery");
    const auto mirrorLog = logs.createLogger("mirror");
    const auto windowLog = logs.createLogger("window");
    const auto deathLog = logs.createLogger("death");

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
            logger->warning("death feed open failed; running without death reporting");
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
            logger->warning(
                "mirror transport open failed; running without directory "
                "mirror publishing");
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

    // 恢复相位跨进程 intake（G-1 收尾批增量③）：bind 收包口收各 Zone/
    // gateway 全量重报（FullReport），恢复相位排他（拒新）直至收敛——
    // 全部在册报告方已报或超时。--recovery-port 0 = 关闭。
    uint16_t recovery_port = 0;
    uint32_t recovery_expected = 0;
    uint32_t recovery_timeout_ms = 5000;
    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        if (arg == "--recovery-port" && i + 1 < argc) {
            recovery_port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--recovery-expected" && i + 1 < argc) {
            recovery_expected = static_cast<uint32_t>(std::atoi(argv[++i]));
        } else if (arg == "--recovery-timeout-ms" && i + 1 < argc) {
            recovery_timeout_ms = static_cast<uint32_t>(std::atoi(argv[++i]));
        }
    }
    disco::UdpFeed recovery_feed;
    std::unique_ptr<session::FleetRecoveryCoordinator> recovery;
    // （协调器在 ManagerApp 构造后装配——intake 回调要落 mgr 目录）

    // §6 死亡行窗口处置面（G-1 收尾批遗留项）：掉线保活窗口 + gateway 组件
    // 映射。窗口以主循环秒拍为 tick（clock-and-time 单调口径，缺省 30 =
    // §4 建议值）；Zone 行反查键 = 死亡事件 zone_id（machined roster zone
    // 列），gateway 行反查键 = CLI 声明的 component → gateway 映射（死亡
    // wire 不带 gateway 身份，gateway-app 半成品——骨架期显式映射桥接）。
    uint32_t suspend_window_ticks = 30;
    std::unordered_map<uint64_t, uint32_t> gateway_by_component;
    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        if (arg == "--suspend-window-ticks" && i + 1 < argc) {
            suspend_window_ticks = static_cast<uint32_t>(std::atoi(argv[++i]));
        } else if (arg == "--gateway-component" && i + 1 < argc) {
            const std::string spec = argv[++i];
            const auto colon = spec.find(':');
            if (colon != std::string::npos) {
                const uint64_t cid =
                    std::strtoull(spec.substr(0, colon).c_str(), nullptr, 10);
                const uint32_t gid =
                    static_cast<uint32_t>(std::strtoul(spec.substr(colon + 1).c_str(), nullptr, 10));
                if (cid != 0 && gid != 0) {
                    gateway_by_component[cid] = gid;  // 0 值任一侧 = 未声明，不映射
                }
            }
        }
    }

    logger->info("Configuration:");
    logger->info("  Listen: 0.0.0.0:" + std::to_string(port));
    if (death_listener) {
        logger->info("  Death feed: machined " + machined_host + ":"
                     + std::to_string(machined_port) + " (listen port "
                     + std::to_string(death_feed.port()) + ")");
    }
    if (mirror_publisher) {
        logger->info("  Directory mirror: " + mirror_host + ":" + std::to_string(mirror_port)
                     + " (snapshot every " + std::to_string(mirror_snapshot_ms) + "ms)");
    }
    if (recovery_port != 0) {
        logger->info("  Fleet recovery: intake 0.0.0.0:" + std::to_string(recovery_port)
                     + " (expected reporters " + std::to_string(recovery_expected)
                     + ", timeout " + std::to_string(recovery_timeout_ms) + "ms)");
    }
    logger->info("  Suspend window: " + std::to_string(suspend_window_ticks) + " ticks"
                 + " (gateway mappings " + std::to_string(gateway_by_component.size())
                 + ")");

    // 注册信号处理
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    try {
        ManagerApp mgr(port);
        g_mgr = &mgr;

        // 恢复相位跨进程化（G-1 收尾批增量③）：进程启动 = machined 拉起
        // 后的恢复序（§6 manager 行）——恢复相位排他（拒新），收各 Zone/
        // gateway 全量重报，收敛（全报或超时）开放。
        if (recovery_port != 0 && recovery_feed.open(recovery_port, "0.0.0.0")) {
            recovery = std::make_unique<session::FleetRecoveryCoordinator>(
                recovery_expected, recovery_timeout_ms,
                [&mgr, &recoveryLog](
                    std::uint64_t component_id, std::uint32_t zone_id,
                    const std::vector<session::MirrorEntry>& sessions) {
                    const auto taken = mgr.intake_directory_full_report(sessions);
                    recoveryLog->info("full report from component="
                                      + std::to_string(component_id) + " zone="
                                      + std::to_string(zone_id) + " sessions="
                                      + std::to_string(sessions.size()) + " intake="
                                      + std::to_string(taken));
                });
            recovery->begin(getSteadyNowMs());
            mgr.set_admission_gate([&recovery]() {
                return recovery->admissible();
            });
            recoveryLog->info("恢复相位排他（拒新）：等全量重报，在册报告方 "
                              + std::to_string(recovery_expected) + "，超时 "
                              + std::to_string(recovery_timeout_ms) + "ms");
        }

        logger->info("Starting ManagerApp...");
        mgr.start();

        if (death_listener && death_listener->subscribe()) {
            logger->info("Death feed subscribed to machined " + machined_host
                         + ":" + std::to_string(machined_port));
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
            mirrorLog->info("Directory mirror: initial snapshot sent ("
                            + std::to_string(initial.size()) + " entries)");
        }

        logger->info("ManagerApp is running. Press Ctrl+C to stop.");

        auto since_snapshot = std::chrono::milliseconds(0);
        std::uint8_t recovery_buf[disco::kMaxDatagramSize];
        bool was_recovering = recovery && recovery->recovering();
        while (mgr.isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            since_snapshot += std::chrono::seconds(1);

            // 恢复相位轮询（增量③）：收 FullReport 分片 + 喂钟收敛判定
            if (recovery) {
                std::size_t rlen = 0;
                while (recovery_feed.try_receive(recovery_buf, sizeof(recovery_buf),
                                                 rlen)) {
                    session::FleetReportPart part;
                    if (session::decode_fleet_report(recovery_buf, rlen, part)) {
                        (void)recovery->on_report_part(part);
                    }
                }
                recovery->tick(getSteadyNowMs());
                if (was_recovering && !recovery->recovering()) {
                    was_recovering = false;
                    recoveryLog->info("收敛开放（准入恢复）：reported="
                                      + std::to_string(recovery->last_reported_count())
                                      + "/" + std::to_string(recovery_expected));
                    // 收敛即向镜像面发一轮全量（重建目录进投影，增量②面）
                    if (mirror_publisher) {
                        const auto entries = collect_mirror_entries(mgr);
                        mirror_publisher->publish_snapshot(entries);
                        recoveryLog->info("重建目录快照已发镜像：entries="
                                          + std::to_string(entries.size()));
                    }
                }
            }

            // 周期全量快照（增量②对账 healing 面：delta 丢失由快照收敛）
            if (mirror_publisher && since_snapshot.count() >= mirror_snapshot_ms) {
                since_snapshot = std::chrono::milliseconds(0);
                const auto entries = collect_mirror_entries(mgr);
                const auto parts = mirror_publisher->publish_snapshot(entries);
                mirrorLog->info("snapshot published: entries="
                                + std::to_string(entries.size()) + " parts="
                                + std::to_string(parts) + " seq="
                                + std::to_string(mirror_publisher->seq()));
            }

            // 窗口满扫描（§4 tick 驱动，本批）：主循环 1s 秒拍喂钟，到期
            // Suspended 删条目 + SessionDown(kReasonWindowExpired)——事件链
            // 自动进镜像面。0 终结不打日志（空转是常态）。无条件跑——窗口
            // 处置不依赖死亡订阅面在开。
            if (const auto expired =
                    mgr.sweep_suspended(getSteadyNowMs() / 1000);
                expired > 0) {
                windowLog->info("sweep expired=" + std::to_string(expired));
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
                deathLog->warning("component="
                                  + std::to_string(death.member.component_id)
                                  + " zone=" + std::to_string(death.member.zone_id)
                                  + " port=" + std::to_string(death.member.service_port)
                                  + " kind=" + kind
                                  + " exit=" + std::to_string(death.exit_code));
                // §6 死亡行窗口处置（本批）：Zone 行 = roster zone 列反查，
                // gateway 行 = CLI 映射反查；Online 条目批量进保活窗口，
                // 窗口满由主循环 sweep 终结（SessionDown 进镜像面）。
                const std::uint64_t now_tick = getSteadyNowMs() / 1000;
                if (death.member.zone_id != 0) {
                    const auto suspended = mgr.suspend_zone_sessions(
                        death.member.zone_id, now_tick, suspend_window_ticks);
                    windowLog->warning("zone death zone="
                                       + std::to_string(death.member.zone_id)
                                       + " suspended=" + std::to_string(suspended)
                                       + " (window " + std::to_string(suspend_window_ticks)
                                       + " ticks)");
                }
                const auto gw = gateway_by_component.find(death.member.component_id);
                if (gw != gateway_by_component.end() && gw->second != 0) {
                    const auto suspended = mgr.suspend_gateway_sessions(
                        gw->second, now_tick, suspend_window_ticks);
                    windowLog->warning("gateway death component="
                                       + std::to_string(death.member.component_id)
                                       + " gateway=" + std::to_string(gw->second)
                                       + " suspended=" + std::to_string(suspended)
                                       + " (window " + std::to_string(suspend_window_ticks)
                                       + " ticks)");
                }
            }
        }

        logger->info("ManagerApp stopped.");

    } catch (const std::exception& e) {
        logger->error(std::string("Error: ") + e.what());
        return 1;
    }

    g_mgr = nullptr;
    return 0;
}
