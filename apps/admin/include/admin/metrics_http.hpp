#pragma once

// 指标 HTTP 出口缝（apps/admin exporter：logging.md §5.1 单一 exporter 吐
// /metrics）。极小 responder——只答 GET /metrics → 200 text/plain，其余
// 路径 404；Host/版本不做校验（本机 dev 面，非对外服务）。
//
// 机制纪律（缝=组件自持、宿主泵驱动、零内部线程）：accept/read/write 全在
// pump(now_ms) 内，宿主 serve 循环每拍调一次。
//   读侧形状（TcpTransport::read 契约）：>0 追加半行缓冲；0 = 本拍无数据
//     （EAGAIN，非断连）；<0 = 对端关闭/错误即断连。
//   写侧出站盒（非阻塞 write 可短写/返回 0，重试是调用方职责）：每连接
//     存响应副本，每拍推进余量——不依赖对端一次收完。
//   连接占用超时（默认 5s，含未读完请求）：未排空/未收齐即弃，防慢消费
//     钉住连接槽；超上限（默认 16）即拒收计数。
//
// 断连降级：scrape 掉 = 数据面已聚在 hub（last-wins），下拍重拉即恢复；
// admin 自身不参与业务路径（logging.md §5 三禁）。
//
// Windows：link TCP 底座不进本批（POSIX only）——start 假、pump 空，
// 调用面无 #if（serve 在 Windows 退不可用，与 logger serve 同口径）。

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "admin/metrics_hub.hpp"
#include "apollo/net/link/tcp_transport.hpp"

namespace apollo::admin {

struct MetricsHttpConfig {
    std::string host = "0.0.0.0";
    std::uint16_t port = 9107;
    std::uint32_t write_timeout_ms = 5000;  // 连接占用上限（未收齐/未排空即弃）
    std::size_t max_connections = 16;       // 超上限拒收（exporter 非并发面）
};

#if !defined(_WIN32)

class MetricsHttp {
public:
    MetricsHttp(MetricsHttpConfig config, MetricsHub& hub);

    // 绑定监听（port 0 = 内核分配，start 后 port() 读回——测试用）
    bool start();
    void stop();
    void pump(std::uint64_t now_ms);

    std::uint16_t port() const;
    std::size_t connections() const;
    std::uint64_t served() const;   // 应答完成的连接
    std::uint64_t refused() const;  // 超连接上限拒收
    std::uint64_t aborted() const;  // 超时未收齐/未排空即弃

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

    MetricsHttpConfig config_;
    MetricsHub& hub_;
    apollo::net::link::TcpListener listener_;
    std::vector<Conn> conns_;
    std::atomic<std::uint64_t> served_{0};
    std::atomic<std::uint64_t> refused_{0};
    std::atomic<std::uint64_t> aborted_{0};
};

#else  // _WIN32

class MetricsHttp {
public:
    MetricsHttp(MetricsHttpConfig, MetricsHub&) {}  // 空件：非特异成员不可 = default（MSVC C2610）

    bool start() { return false; }
    void stop() {}
    void pump(std::uint64_t) {}

    std::uint16_t port() const { return 0; }
    std::size_t connections() const { return 0; }
    std::uint64_t served() const { return 0; }
    std::uint64_t refused() const { return 0; }
    std::uint64_t aborted() const { return 0; }
};

#endif  // !_WIN32

}  // namespace apollo::admin
