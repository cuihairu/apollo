#pragma once

// 目录发现引导（G-1 批 E——发现层，§7「UDP 广播发现（跨机拓扑发现）」；
// KBE machine.cpp UDP 广播应答先例的最小对应物）。
//
// 进程启动期不知道目录（machined）端点时的引导路径：
//   1. send_query：向广播地址发 Query（service_port 承载**回执端口**——
//      请求方收 Advertise 的端口）；
//   2. collect_reply：在回执 feed 上等一条 Advertise → 目录端点
//      （host = 应答方源地址，port = 报文 service_port = 目录收包端口）。
// locate() = 两步的阻塞重试组合（真实进程启动期引导用；单线程测试走两步
// 原语自演两侧）。
//
// 广播口径：定向广播/子网广播地址由调用方给定（部署配置层；骨架期不做
// 子网掩码推导）。transport 须已 enable_broadcast。

#include "discovery.hpp"
#include "udp_transport.hpp"

#include <cstdint>
#include <string>

namespace apollo::net::discovery {

// 目录端点（machined 收包口）
struct DirectoryEndpoint {
    std::string host;
    std::uint16_t port = 0;

    [[nodiscard]] bool is_valid() const noexcept {
        return port != 0;
    }
};

class DirectoryLocator {
public:
    DirectoryLocator() = delete;

    // 发一次 Query 广播。reply_port = 本方收 Advertise 的端口（进
    // service_port 字段）。false = 未 open / 未开广播 / sendto 失败
    [[nodiscard]] static bool send_query(UdpBeaconTransport& transport,
                                         const std::string& broadcast_host,
                                         std::uint16_t directory_port,
                                         std::uint16_t reply_port, MemberId self);

    // 在 feed 上等待一条 Advertise（非阻塞轮询至多 wait_ms）。命中时 out =
    // 目录端点；false = 超时无应答（或只收到非 Advertise 报文——静默续等）
    [[nodiscard]] static bool collect_reply(UdpFeed& feed, std::uint32_t wait_ms,
                                            DirectoryEndpoint& out);

    // 阻塞组合：发 Query（tries 次、每次等 wait_ms_per_try）直到收到 Advertise
    [[nodiscard]] static bool locate(UdpBeaconTransport& transport,
                                     const std::string& broadcast_host,
                                     std::uint16_t directory_port, MemberId self,
                                     std::uint32_t tries,
                                     std::uint32_t wait_ms_per_try,
                                     DirectoryEndpoint& out);
};

} // namespace apollo::net::discovery
