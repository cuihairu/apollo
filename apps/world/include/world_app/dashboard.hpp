#pragma once

// 内部状态 Dashboard responder（DASH-1，用户令 2026-11-02）。嵌入
// world/cellServer 进程，提供「人看的」状态页（/）与脚本化 JSON 查询面
// （/api/state）。技术栈按仓内等价物落地（docs/design/dashboard.md §2）：
// 不引入 Rust/axum，沿用 apps/admin MetricsHttp 泵驱动纪律——accept/read/
// write 全在 pump(now_ms) 内、宿主泵驱动、零内部线程、连接占用超时、超
// 上限拒收。
//
// 数据源（§4）：各模块已有状态接口，缺的先补只读查询面（本批由调用方
// 注入快照回调——responder 自身不持有业务对象，数据面与传输面解耦）。
//
// 安全模型（§3）：默认只绑环回；token 未设 = 无认证；设置后所有路由
// （HTML 与 JSON API）要求 `Authorization: Bearer <t>`，否则 401。不用
// 查询串传递 token（防浏览器历史/访问日志/Referer 泄漏）。只 accept 不
// 拨号；只读无任何状态变更端点。
//
// Windows：link TCP 底座不进本批（POSIX only）——与 MetricsHttp 同口径
// （start 假、pump 空、调用面无 #if）。

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "apollo/net/link/tcp_transport.hpp"

namespace world_app {

struct DashboardConfig {
    std::string host = "127.0.0.1";  // 默认只绑环回（§3 安全模型）
    std::uint16_t port = 0;          // 0 = 不装配（缺省关闭）
    std::string token;               // 空 = 无认证
    std::uint32_t write_timeout_ms = 5000;  // 连接占用上限
    std::size_t max_connections = 16;
};

// 状态快照回调：返回 /api/state 的 JSON 文本（不含任何状态变更端点）。
// 调用方（宿主进程）负责数据源接线；本类型只做 HTTP 传输面。
using DashboardStateFn = std::function<std::string()>;

#if !defined(_WIN32)

class DashboardResponder {
public:
    DashboardResponder(DashboardConfig config, DashboardStateFn state_fn);

    // 绑定监听（port 0 = 内核分配，start 后 port() 读回——测试用）。
    bool start();
    void stop();
    void pump(std::uint64_t now_ms);

    std::uint16_t port() const;
    std::size_t connections() const;
    std::uint64_t served() const;   // 应答完成的连接
    std::uint64_t refused() const;  // 超连接上限拒收
    std::uint64_t aborted() const;  // 超时未收齐/未排空即弃
    std::uint64_t unauthorized() const;  // token 缺失/错误拒收

private:
    struct Conn {
        std::unique_ptr<apollo::net::link::ILinkTransport> transport;
        std::string recv;      // 半行接收缓冲
        std::string response;  // 响应副本（出站盒）
        std::size_t sent = 0;
        std::uint64_t deadline_ms = 0;
        bool queued = false;   // 请求头已收齐、响应已生成
    };

    void accept_new();
    bool read_and_maybe_answer(Conn& conn, std::uint64_t now_ms);
    bool write_out(Conn& conn);
    bool authorized(const std::string& header) const;
    std::string route(const std::string& request, std::uint64_t now_ms) const;

    DashboardConfig config_;
    DashboardStateFn state_fn_;
    apollo::net::link::TcpListener listener_;
    std::vector<Conn> conns_;
    std::atomic<std::uint64_t> served_{0};
    std::atomic<std::uint64_t> refused_{0};
    std::atomic<std::uint64_t> aborted_{0};
    std::atomic<std::uint64_t> unauthorized_{0};
};

#else  // _WIN32

class DashboardResponder {
public:
    DashboardResponder(DashboardConfig, DashboardStateFn) {}

    bool start() { return false; }
    void stop() {}
    void pump(std::uint64_t) {}

    std::uint16_t port() const { return 0; }
    std::size_t connections() const { return 0; }
    std::uint64_t served() const { return 0; }
    std::uint64_t refused() const { return 0; }
    std::uint64_t aborted() const { return 0; }
    std::uint64_t unauthorized() const { return 0; }
};

#endif  // !_WIN32

}  // namespace world_app
