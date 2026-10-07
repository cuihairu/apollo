#include "apollo/net/discovery/directory_locator.hpp"

#include <chrono>
#include <thread>

namespace apollo::net::discovery {

bool DirectoryLocator::send_query(UdpBeaconTransport& transport,
                                  const std::string& broadcast_host,
                                  std::uint16_t directory_port,
                                  std::uint16_t reply_port, MemberId self) {
    if (!self.is_valid()) {
        return false;
    }
    WirePacket p;
    p.op = Op::Query;
    p.component_id = self.component_id;
    p.zone_id = self.zone_id;
    p.service_port = reply_port;  // 回执端口语义（见头注释）
    std::uint8_t buf[kWireSize];
    p.encode_to(buf);
    return transport.send_to(broadcast_host, directory_port, buf);
}

bool DirectoryLocator::collect_reply(UdpFeed& feed, std::uint32_t wait_ms,
                                     DirectoryEndpoint& out) {
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(wait_ms);
    std::uint8_t buf[kWireSize];
    std::string sender_host;
    std::uint16_t sender_port = 0;
    while (std::chrono::steady_clock::now() < deadline) {
        if (feed.try_receive(buf, sender_host, sender_port)) {
            WirePacket p;
            if (WirePacket::decode_from(buf, p) && p.op == Op::Advertise &&
                p.component_id == kDirectoryComponentId && p.service_port != 0) {
                out.host = sender_host;
                out.port = p.service_port;
                return true;
            }
            // 非本层报文静默续等（wire 校验与 op 过滤同路）
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}

bool DirectoryLocator::locate(UdpBeaconTransport& transport,
                              const std::string& broadcast_host,
                              std::uint16_t directory_port, MemberId self,
                              std::uint32_t tries, std::uint32_t wait_ms_per_try,
                              DirectoryEndpoint& out) {
    UdpFeed reply_feed;
    if (!reply_feed.open(0, "0.0.0.0")) {  // 回执端口 = 内核分配临时端口
        return false;
    }
    for (std::uint32_t i = 0; i < tries; ++i) {
        if (!send_query(transport, broadcast_host, directory_port,
                        reply_feed.port(), self)) {
            return false;  // 发送面故障（未 open/未开广播）不重试——调用方处置
        }
        if (collect_reply(reply_feed, wait_ms_per_try, out)) {
            return true;
        }
    }
    return false;
}

} // namespace apollo::net::discovery
