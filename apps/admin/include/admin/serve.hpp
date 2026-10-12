#pragma once

// AdminApp serve 受面子命令选项（metrics admin 通道：logging.md §5.1
// 单一 exporter 的两面——link passive 收 931 快照 + HTTP 吐 /metrics）。
// 平台无关（CLI 面）；装配面在 admin_serve.cpp（link TCP 底座 POSIX only）。

#include <cstdint>
#include <string>

namespace apollo::admin {

struct ServeOptions {
    std::string host = "0.0.0.0";    // metrics link 监听主机
    std::uint16_t port = 9106;       // metrics link 监听端口
    std::string http_host = "0.0.0.0";
    std::uint16_t http_port = 9107;  // exporter HTTP 监听端口
    std::string route_json;          // 契约路由 JSON（空 = 构建期路径）
    std::uint64_t stale_ms = 30000;  // proc_up 陈旧判据窗口
};

int run_serve(const ServeOptions& options);

}  // namespace apollo::admin
