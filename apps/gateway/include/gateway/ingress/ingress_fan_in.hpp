#pragma once

#include "gateway/ingress/client_ingress_server.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace gateway {

// 多监听扇入（L45 批③「允许多个监听及明文/TLS 共存」）：N 个客户端入口
// 子入口（TCP/WS/WSS）合成一个 ClientIngressServer——上层（GatewayServer
// 及观测面）只认一条入口缝，不感知下面有几个监听。
//
// 连接号空间按号段互斥分区：各子入口自带 connectionIdBase，扇入只做号段
// 路由、不重映射（子入口发出的号即全局号）。装配期校验号段互斥，重叠
// 即抛（否则 send/close 会打到错的入口）。
//
// send/close 按号段路由到对应子入口；越界号丢弃（同 TCP 入口迟到操作
// 容忍）。子入口 observer 由扇入持有并转发到同一 observer——回调仍在
// 泵线程，重入 send/close 走各子入口投递队列（先攒后发）。
//
// Windows：本件与传输无关（子入口各自处理平台分叉），无编译期分叉。
inline constexpr std::uint64_t kIngressFanInIdStride = std::uint64_t{1} << 40;

struct IngressFanInBinding {
    std::unique_ptr<ClientIngressServer> server;
    std::uint64_t connectionIdBase{0};  // 该子入口号段基址（号 = base + 1 起）
};

std::unique_ptr<ClientIngressServer> makeIngressFanIn(
    std::vector<IngressFanInBinding> bindings, std::uint64_t stride = kIngressFanInIdStride);

} // namespace gateway
