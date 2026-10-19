#pragma once

#include "gateway/ingress/client_ingress_server.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace apollo::net::tls {
class TlsServerContext;
}

namespace gateway {

// WS/WSS 客户端游戏入口（L45 批③：todo「游戏 gateway 支持 WS/WSS 监听，
// 与 TCP 共存」）。语义与 TcpClientIngressServer 同一套、同一层——ws
// binary 消息载荷 = link Frame 字节流，帧语义/准入面/观测事件全部复用
// TCP 入口口径（net-abstraction §5.6「同一套语义、同一个层」，只差传输）。
//
// 升级握手（RFC 6455 §4.2）：合法升级且路径命中 path 才受理；不合法或
// 路径不命中一律 400 拒绝 + AdmissionRejected 断连——升级路由不降级成
// 普通 HTTP（HTTP 服务的 WS 走 interfaces 专用路由，批④）。未完成升级
// 即断的连接不上报 observer（Open 只对已升级会话发）。
//
// 契约外载荷纪律（同 TCP 入口）：客户端发 text 消息 / ws 帧协议错 /
// netlink control 帧 → 断连不静默（TransportError）。
//
// tls 置空 = 明文 ws（TcpListener 底座）；非空 = wss（TlsListener 底座，
// 上下文须已 load 成功——未装载即装配拒绝，start 抛）。
//
// 线程模型同 TCP 入口：零内部线程，宿主 pump() 驱动；send/close 任意
// 线程入队经投递队列，observer 回调全在泵线程。
//
// Windows：link TCP 底座不进 M1 批（POSIX only）——本工厂 Windows 下退回
// null 桩（同签名编译期分叉）。
struct WsIngressConfig {
    std::string host = "0.0.0.0";
    std::uint16_t port = 0;
    std::string path = "/game";  // 升级路径判据（不命中 = no-such-route 拒绝）
    apollo::net::tls::TlsServerContext* tls = nullptr;  // 空 = 明文 ws
    std::size_t maxConnections = 0;
    std::uint64_t connectionIdBase = 0;  // 号段基址（多监听并装分占互斥号段）
};

std::unique_ptr<ClientIngressServer> makeWsClientIngressServer(const WsIngressConfig& config);

} // namespace gateway
