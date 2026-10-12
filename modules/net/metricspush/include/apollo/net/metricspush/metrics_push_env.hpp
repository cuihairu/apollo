#pragma once

// APOLLO_METRICS_PUSH_ADDR 环境接线面（P3-3 批：app main 一处调用全接，
// 与 make_log_push_from_env 成对）。
//
// 语义（logging.md §5.1，指标永不构成运行依赖）：变量未设/空 = push 关；
// 坏值或装配失败 = nullptr（指标上行降级，进程本体不受影响）。返回非空
// = app 持句柄（进程期存活）。默认 provider = 进程级 MetricRegistry 快照
// 渲染（proc = self_name，源端嵌标签）。
//
// Windows：link TCP 底座不进本批（POSIX only）——恒 nullptr，调用面无 #if。

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

#include "apollo/core/metrics/metric_registry.h"
#include "apollo/net/metricspush/metrics_push_client.hpp"

#ifndef APOLLO_METRICS_PUSH_ROUTE_JSON
#define APOLLO_METRICS_PUSH_ROUTE_JSON ""
#endif

namespace apollo::net::metricspush {

// "host:port" 解析（host = IPv4 字面量/主机名；IPv6 字面量不在 dev 面）。
// 与 log_push_env.hpp 同型但独立定义（两模块无依赖边，容忍复制）。
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

// 环境装配：APOLLO_METRICS_PUSH_ADDR（host:port）非空 → 起
// MetricsPushClient（路由表 = 构建期契约路径）。未设/坏值/装配失败 =
// nullptr（关/降级）。
inline std::shared_ptr<MetricsPushClient> make_metrics_push_from_env(
    std::string self_name) {
#if defined(_WIN32)
    (void)self_name;
    return nullptr;
#else
    const char* addr = std::getenv("APOLLO_METRICS_PUSH_ADDR");
    if (addr == nullptr || *addr == '\0') {
        return nullptr;
    }
    std::string host;
    std::uint16_t port = 0;
    if (!parse_host_port(addr, host, port)) {
        return nullptr;
    }
    MetricsPushClientConfig config;
    config.host = std::move(host);
    config.port = port;
    config.route_json = APOLLO_METRICS_PUSH_ROUTE_JSON;
    config.self_name = self_name;  // 复制：provider 闭包同需 proc
    auto client = std::make_shared<MetricsPushClient>(
        std::move(config),
        [proc = std::move(self_name)] {
            return render_exposition(
                apollo::core::metrics::MetricRegistry::instance().snapshot(),
                proc);
        });
    if (!client->start()) {
        return nullptr;  // 路由表缺失/坏 = 降级无推面
    }
    return client;
#endif
}

} // namespace apollo::net::metricspush
