#pragma once

// APOLLO_LOG_PUSH_ADDR 环境接线面（L4 批③：app main 一处调用全接）。
//
// 语义（logging.md §5）：变量未设/空 = push 关（v1 单出口不变）；坏值或
// 装配失败 = 降级本地-only（本地结构化文件恒为真相源，收集永不构成运行
// 依赖）。返回非空 = config.pushSink 可绑；句柄由 pushSink 捕获持有
// （进程期存活），返回值仅供判空与提前接线。
//
// Windows：link TCP 底座不进本批（POSIX only）——恒 nullptr，调用面无 #if。

#include <cstdlib>
#include <memory>
#include <string>

#include "apollo/net/logpush/log_push_client.hpp"

#ifndef APOLLO_LOG_PUSH_ROUTE_JSON
#define APOLLO_LOG_PUSH_ROUTE_JSON ""
#endif

namespace apollo::net::logpush {

// "host:port" 解析（host = IPv4 字面量/主机名；IPv6 字面量不在 dev 面）
inline bool parse_host_port(const char* addr, std::string& host,
                            std::uint16_t& port) {
    const std::string s(addr);
    const auto colon = s.rfind(':');
    if (colon == std::string::npos || colon == 0 || colon + 1 == s.size()) {
        return false;
    }
    const std::string port_part = s.substr(colon + 1);
    if (port_part.size() > 5) {
        return false;
    }
    unsigned long value = 0;
    for (const char c : port_part) {
        if (c < '0' || c > '9') {
            return false;
        }
        value = value * 10 + static_cast<unsigned long>(c - '0');
    }
    if (value == 0 || value > 65535) {
        return false;
    }
    host = s.substr(0, colon);
    port = static_cast<std::uint16_t>(value);
    return true;
}

// 环境装配：APOLLO_LOG_PUSH_ADDR（host:port）非空 → 起 LogPushClient
// （路由表 = 构建期契约路径）。未设/坏值/装配失败 = nullptr（关/降级）。
inline std::shared_ptr<LogPushClient> make_log_push_from_env(
    std::string self_name) {
#if defined(_WIN32)
    (void)self_name;
    return nullptr;
#else
    const char* addr = std::getenv("APOLLO_LOG_PUSH_ADDR");
    if (addr == nullptr || *addr == '\0') {
        return nullptr;
    }
    std::string host;
    std::uint16_t port = 0;
    if (!parse_host_port(addr, host, port)) {
        return nullptr;
    }
    LogPushClientConfig config;
    config.host = std::move(host);
    config.port = port;
    config.route_json = APOLLO_LOG_PUSH_ROUTE_JSON;
    config.self_name = std::move(self_name);
    auto client = std::make_shared<LogPushClient>(std::move(config));
    if (!client->start()) {
        return nullptr;  // 路由表缺失/坏 = 降级本地-only
    }
    return client;
#endif
}

} // namespace apollo::net::logpush
