// Machined——G-1 单机守护：编队目录面 + 发现应答（P3-1 批 D + 批 E）。
//
// 职责：bind UDP → 非阻塞收包分流（成员面 op 喂 DiscoveryRegistry；发现面
// Query 以 Advertise 回执到 Query.service_port@sender——目录被发现的引导路
// 径，§7 UDP 广播发现）→ 周期 expire 喂钟 → 死亡事件（异常死亡）与成员
// 增减落日志。单线程主循环、无内部线程——与 discovery 库「调用方单线程
// 驱动」纪律同源。
// 不在本批：拉起/重启监督面（term-contract §1.3「拉起/重启」半边）、
// 死亡事件的跨进程上报（manager 域消费端，随 G-1 收尾批）。
//
// 端点：0.0.0.0:9600 缺省（--port N 覆盖）；心跳周期 --interval N（毫秒，
// 缺省 1000，TTL = 3 × interval）。报文协议见 modules/net/discovery。

#include "apollo/net/discovery/directory_locator.hpp"
#include "apollo/net/discovery/discovery.hpp"
#include "apollo/net/discovery/udp_transport.hpp"

#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <thread>

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

} // namespace

int main(int argc, char** argv) {
    std::uint16_t bind_port = 9600;
    std::uint32_t interval_ms = 1000;
    for (int i = 1; i + 1 < argc; i += 2) {
        const std::string flag = argv[i];
        const int value = std::atoi(argv[i + 1]);
        if (flag == "--port" && value > 0 && value <= 65535) {
            bind_port = static_cast<std::uint16_t>(value);
        } else if (flag == "--interval" && value > 0) {
            interval_ms = static_cast<std::uint32_t>(value);
        }
    }

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    namespace disco = apollo::net::discovery;

    disco::UdpFeed feed;
    if (!feed.open(bind_port, "0.0.0.0")) {
        std::cerr << "[machined] bind 0.0.0.0:" << bind_port << " failed" << std::endl;
        return 1;
    }
    disco::UdpBeaconTransport reply_transport;  // Advertise 回执面（单播）
    if (!reply_transport.open()) {
        std::cerr << "[machined] reply transport open failed" << std::endl;
        return 1;
    }
    disco::DiscoveryRegistry registry(interval_ms);
    std::cout << "[machined] listening on 0.0.0.0:" << feed.port()
              << " (interval=" << interval_ms << "ms, ttl=" << registry.ttl_ms()
              << "ms)" << std::endl;

    std::uint64_t last_expire_ms = now_ms();
    std::size_t last_count = 0;
    while (g_run != 0) {
        const std::uint64_t now = now_ms();

        std::uint8_t buf[disco::kWireSize];
        std::string sender_host;
        std::uint16_t sender_port = 0;
        while (feed.try_receive(buf, sender_host, sender_port)) {
            disco::WirePacket p;
            if (!disco::WirePacket::decode_from(buf, p)) {
                std::cout << "[machined] dropped malformed packet" << std::endl;
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
                    std::cout << "[machined] advertised to " << sender_host << ":"
                              << p.service_port << std::endl;
                }
                continue;
            }
            if (!registry.on_packet(buf, now)) {
                std::cout << "[machined] dropped packet (op="
                          << static_cast<int>(p.op) << ")" << std::endl;
            }
        }

        if (now - last_expire_ms >= interval_ms) {  // 喂钟节奏 = 心跳周期
            registry.expire(now);
            last_expire_ms = now;
        }

        for (const auto& death : registry.drain_deaths()) {
            std::cout << "[machined] death component=" << death.component_id
                      << " zone=" << death.zone_id << " port=" << death.service_port
                      << std::endl;
        }
        const std::size_t count = registry.member_count();
        if (count != last_count) {
            std::cout << "[machined] members=" << count << std::endl;
            last_count = count;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::cout << "[machined] shutting down" << std::endl;
    return 0;
}
