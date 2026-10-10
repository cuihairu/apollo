// Machined——G-1 单机守护：编队目录面 + 发现应答 + 监督面 + 死亡事件跨进程
// 上报（P3-1 批 D/E/F + G-1 收尾批增量①）。
//
// 职责：bind UDP → 非阻塞收包分流（成员面 op 喂 DiscoveryRegistry；发现面
// Query 以 Advertise 回执到 Query.service_port@sender——目录被发现的引导路
// 径，§7 UDP 广播发现；死亡面 DeathSubscribe 喂 DeathNotifier 订阅表）→
// 周期 expire 喂钟 → 死亡事件（TTL 判死 + 监督面子进程死亡）合流转发：
// 落日志 + DeathNotify 上报全部在册订阅者（manager 域消费端，§7「mgr 向
// machined 注册死亡监听」）。单线程主循环、无内部线程——与 discovery 库
// 「调用方单线程驱动」纪律同源。
//
// 端点：0.0.0.0:9600 缺省（--port N 覆盖）；心跳周期 --interval N（毫秒，
// 缺省 1000，TTL = 3 × interval）。报文协议见 modules/net/discovery。

#include "apollo/runtime/crash_capture.hpp"
#include "apollo/core/log/log_manager.h"
#include "apollo/net/discovery/directory_locator.hpp"
#include "apollo/net/discovery/discovery.hpp"
#include "apollo/net/discovery/udp_transport.hpp"
#include "supervisor.hpp"

#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

namespace {

volatile std::sig_atomic_t g_run = 1;

void on_signal(int) {
    g_run = 0;
}

std::uint64_t now_ms() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

const char* death_kind_name(apollo::net::discovery::DeathKind kind) {
    switch (kind) {
    case apollo::net::discovery::DeathKind::TtlExpired:
        return "ttl_expired";
    case apollo::net::discovery::DeathKind::ChildDied:
        return "child_died";
    case apollo::net::discovery::DeathKind::GaveUp:
        return "gave_up";
    }
    return "unknown";
}

void log_supervisor_events(apollo::core::log::Logger& log,
                           const std::vector<machined::SupervisorEvent>& events) {
    // 消息正文不再带 [machined] 前缀——cat=machined 由 console 模式渲染
    for (const auto& e : events) {
        switch (e.kind) {
        case machined::SupervisorEvent::Kind::Born:
            log.info("born " + e.name);
            break;
        case machined::SupervisorEvent::Kind::Died:
            log.info("child death " + e.name + " exit=" + std::to_string(e.exit_code));
            break;
        case machined::SupervisorEvent::Kind::Restarted:
            log.info("restart " + e.name + " (attempt " + std::to_string(e.restart_count)
                     + ")");
            break;
        case machined::SupervisorEvent::Kind::GivenUp:
            log.warning("give up " + e.name + " after " + std::to_string(e.restart_count)
                        + " restarts");
            break;
        }
    }
}

// 日志接线（ADR-013 L1+L2）：console 人读 + 本地结构化文件真相源并行；
// push 出口留接口不实现（collector 面 M1 后，logging.md §5）
apollo::core::log::LoggerPtr initLogging() {
    auto& logs = apollo::core::log::global_log_manager();
    apollo::core::log::LogManagerConfig config;
    config.processIdentity = "machined";
    config.fileEnabled = true;
    config.fileConfig.directory = "log";
    config.fileConfig.baseName = "machined";
    config.fileConfig.structuredOutput = true;  // §5.1 结构化行（六键固定序）
    logs.initialize(config);
    return logs.createLogger("machined");
}

} // namespace

int main(int argc, char** argv) {
    // 先于 crash capture：write() 在未初始化时会惰性按默认配置建管理器（仅 console），
    // 顺序颠倒会使本进程的文件面配置被默认初始化顶掉（initialize 幂等早退）
    const auto logger = initLogging();
    std::uint16_t bind_port = 9600;
    std::uint32_t interval_ms = 1000;
    std::string roster_path;
    std::uint32_t backoff_ms = 1000;
    for (int i = 1; i < argc; ++i) {
        const std::string flag = argv[i];
        if (flag == "--roster" && i + 1 < argc) {
            roster_path = argv[++i];
            continue;
        }
        if (i + 1 >= argc) {
            break;
        }
        const int value = std::atoi(argv[i + 1]);
        if (flag == "--port" && value > 0 && value <= 65535) {
            bind_port = static_cast<std::uint16_t>(value);
            ++i;
        } else if (flag == "--interval" && value > 0) {
            interval_ms = static_cast<std::uint32_t>(value);
            ++i;
        } else if (flag == "--backoff" && value > 0) {
            backoff_ms = static_cast<std::uint32_t>(value);
            ++i;
        }
    }

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    namespace disco = apollo::net::discovery;

    disco::UdpFeed feed;
    if (!feed.open(bind_port, "0.0.0.0")) {
        logger->error("bind 0.0.0.0:" + std::to_string(bind_port) + " failed");
        return 1;
    }
    disco::UdpBeaconTransport reply_transport;  // Advertise 回执面（单播）
    if (!reply_transport.open()) {
        logger->error("reply transport open failed");
        return 1;
    }
    disco::DiscoveryRegistry registry(interval_ms);
    disco::DeathNotifier death_notifier(reply_transport, interval_ms);
    logger->info("listening on 0.0.0.0:" + std::to_string(feed.port())
                 + " (interval=" + std::to_string(interval_ms)
                 + "ms, ttl=" + std::to_string(registry.ttl_ms()) + "ms)");

    // 监督面（拉起/重启半边）：roster 花名册可选——缺省纯目录面
    std::vector<machined::RosterEntry> roster;
    if (!roster_path.empty()) {
        if (!machined::load_roster(roster_path, roster)) {
            logger->error("roster not readable: " + roster_path);
        }
    }
    machined::Supervisor supervisor(roster, 5, backoff_ms);
    std::vector<machined::SupervisorEvent> supervisor_events;
    supervisor.spawn_all(now_ms(), supervisor_events);
    log_supervisor_events(*logger, supervisor_events);
    supervisor_events.clear();

    std::uint64_t last_expire_ms = now_ms();
    std::size_t last_count = 0;
    while (g_run != 0) {
        const std::uint64_t now = now_ms();

        supervisor.poll(now, supervisor_events);
        log_supervisor_events(*logger, supervisor_events);
        // 死亡事件合流（增量①）：监督面子进程死亡 → DeathNotify 上报
        //（roster 声明 component_id 者上 wire，零 = 未入编队仅落日志；
        // zone 列同源携带——§6 死亡行窗口处置的反查键，manager 域消费）
        for (const auto& e : supervisor_events) {
            if (e.component_id == 0) {
                continue;
            }
            if (e.kind == machined::SupervisorEvent::Kind::Died) {
                disco::MemberId dead;
                dead.component_id = e.component_id;
                dead.zone_id = e.zone_id;
                death_notifier.notify(dead, disco::DeathKind::ChildDied,
                                      static_cast<std::uint32_t>(e.exit_code));
            } else if (e.kind == machined::SupervisorEvent::Kind::GivenUp) {
                disco::MemberId dead;
                dead.component_id = e.component_id;
                dead.zone_id = e.zone_id;
                death_notifier.notify(dead, disco::DeathKind::GaveUp, 0);
            }
        }
        supervisor_events.clear();

        std::uint8_t buf[disco::kWireSize];
        std::string sender_host;
        std::uint16_t sender_port = 0;
        while (feed.try_receive(buf, sender_host, sender_port)) {
            disco::WirePacket p;
            if (!disco::WirePacket::decode_from(buf, p)) {
                logger->warning("dropped malformed packet");
                continue;
            }
            if (p.op == disco::Op::Query) {
                // 发现面：Advertise 回执到 Query.service_port@sender
                //（回执端口语义见 directory_locator.hpp）
                disco::WirePacket adv;
                adv.op = disco::Op::Advertise;
                adv.component_id = disco::kDirectoryComponentId;
                adv.service_port = feed.port();
                std::uint8_t abuf[disco::kWireSize];
                adv.encode_to(abuf);
                if (p.service_port != 0 &&
                    reply_transport.send_to(sender_host, p.service_port, abuf)) {
                    logger->info("advertised to " + sender_host + ":"
                                 + std::to_string(p.service_port));
                }
                continue;
            }
            if (p.op == disco::Op::DeathSubscribe) {
                // 死亡面：订阅注册/刷新（幂等；通知回投 = sender:service_port）
                if (death_notifier.on_subscribe(p.component_id, p.service_port,
                                                sender_host, now)) {
                    logger->info("death subscriber component="
                                 + std::to_string(p.component_id) + "@" + sender_host
                                 + ":" + std::to_string(p.service_port));
                }
                continue;
            }
            if (!registry.on_packet(buf, now)) {
                logger->warning("dropped packet (op="
                                + std::to_string(static_cast<int>(p.op)) + ")");
            }
        }

        if (now - last_expire_ms >= interval_ms) {  // 喂钟节奏 = 心跳周期
            registry.expire(now);
            death_notifier.expire(now);
            last_expire_ms = now;
        }

        for (const auto& death : registry.drain_deaths()) {
            logger->warning("death component=" + std::to_string(death.component_id)
                            + " zone=" + std::to_string(death.zone_id)
                            + " port=" + std::to_string(death.service_port));
            death_notifier.notify(death, disco::DeathKind::TtlExpired, 0);
        }
        const std::size_t count = registry.member_count();
        if (count != last_count) {
            logger->info("members=" + std::to_string(count));
            last_count = count;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    logger->info("shutting down");
    supervisor.terminate_all();
    return 0;
}
