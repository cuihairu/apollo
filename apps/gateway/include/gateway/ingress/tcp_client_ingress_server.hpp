#pragma once

#include "gateway/ingress/client_ingress_server.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace gateway {

// M1 客户端真入口（net-abstraction §5.6「同一套语义、同一个层」：客户端路径
// 与进程间路径共用 link 帧格式，只差传输与拓扑）。线形 = link Frame
// （16B 头 + 载荷 + CRC 尾），载荷 = MessageHeader+body 整包
// （ClientPacketDispatcher 现行解析面不感知分帧）。control 帧是
// InterServerLink 专用（frame.hpp）——客户端发出即契约外消息，断连不静默。
//
// 线程模型：零内部线程，宿主 pump() 驱动（accept/收帧/发帧全在泵线程）；
// send/close 任意线程可调，经投递队列在泵线程落 socket；observer 回调全部
// 在泵线程（事件先攒后发，防遍历连接表期间重入）。
//
// Windows：link TCP 底座不进 M1 批（POSIX only）——本工厂 Windows 下退回
// null 桩（同签名编译期分叉，装配面无 #if）。
//
// connectionIdBase：连接号空间的基址（号 = base + 1 起）。多监听并装
// （TCP/WS/WSS 扇入）时各子入口分占互不重叠的号段——上层按号路由 send/close。
std::unique_ptr<ClientIngressServer> makeTcpClientIngressServer(
    std::string host, std::uint16_t port, std::size_t maxConnections = 0,
    std::uint64_t connectionIdBase = 0);

} // namespace gateway
