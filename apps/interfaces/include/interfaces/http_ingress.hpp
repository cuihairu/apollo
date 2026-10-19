#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace apollo::net::tls {
class TlsServerContext;
}

namespace interfaces {

// 独立 interfaces HTTP/HTTPS 入口（L45 批④；docs/index.md:40「支付和渠道
// 回调由可选 interfaces 的 HTTP/HTTPS 入口接收」的进程壳承载面）。纯泵
// 模型（零内部线程，宿主 pump() 驱动，observer 回调全在泵线程）——
// 传输/生命周期纪律与 gateway 入口同源。ws 侧出站面（平台回执 send）
// L50 业务接线时再开（届时补线程投递缝）。
//
// 路由隔离纪律（todo L45）：WS 只走专用 upgrade 路由（独立连接生命周期，
// 不进 HTTP handler）；HTTP 路由不受理升级（400）；升级路由上的普通
// HTTP 请求不降级（400）。响应一律 Connection: close（每请求一连接，
// L50 业务接线再议 keep-alive）。
//
// 预算面：请求头 8 KiB 上限（431）；请求体 1 MiB 上限（413，头齐即拒）；
// 请求超时 requestTimeout（0 = 不限；>0 = 首字节起 T 内未完成即 408 断连）；
// 并发上限 maxConnections。tls 置空 = 明文 HTTP（TcpListener 底座）；非空
// = HTTPS（TlsListener 底座，凭据须已装载）。
//
// Windows：link TCP/TLS 底座不进 M1 批（POSIX only）——本工厂 Windows 下
// 退回 null 桩（同签名编译期分叉）。

using ConnectionID = std::uint64_t;

struct ClientEndpoint {
    std::string address;
    std::uint16_t port{0};
};

enum class DisconnectReason {
    NormalClose,
    TransportError,
    Rejected,
};

struct HttpRequest {
    std::string method;
    std::string path;
    std::vector<std::pair<std::string, std::string>> headers;  // 名已小写
    std::vector<std::uint8_t> body;
    // 逐字节原始请求帧（请求行 + 头 + 声明体长的体）。验签保真面（todo
    // L62）：headers 面已小写、body 面只收声明体长——渠道规范若按原始
    // 大小写头或整帧签名，只此一处有原始形态。
    std::vector<std::uint8_t> raw_request;
};

struct HttpResponse {
    int status = 200;
    std::string reason = "OK";
    std::vector<std::pair<std::string, std::string>> headers;  // 额外头（可选）
    std::vector<std::uint8_t> body;
    // Content-Length / Connection: close 由入口统一补，不须手填。
};

using HttpHandler = std::function<HttpResponse(const HttpRequest&)>;

class IHttpIngressObserver {
public:
    virtual ~IHttpIngressObserver() = default;

    // 一次 HTTP 应答完成（含 4xx 预算/路由拒绝；ws 升级受理不在此面）。
    virtual void onHttpHandled(ConnectionID connectionId, const HttpRequest& request,
                               int status) = 0;
    // WS 专用路由生命周期（升级受理/二进制入帧/断开）。
    virtual void onWsOpened(ConnectionID connectionId, const ClientEndpoint& endpoint) = 0;
    virtual void onWsBinary(ConnectionID connectionId, std::span<const std::uint8_t> payload) = 0;
    virtual void onWsClosed(ConnectionID connectionId, DisconnectReason reason) = 0;
};

struct HttpIngressConfig {
    std::string host = "0.0.0.0";
    std::uint16_t port = 0;
    apollo::net::tls::TlsServerContext* tls = nullptr;  // 空 = 明文 http
    std::size_t maxConnections = 0;
    std::uint64_t connectionIdBase = 0;  // 号段基址（多监听并装分占互斥号段）
    // 请求超时预算（0 = 不限）：请求首字节起 T 内未完成（头不齐/体不齐）
    // → 408 + 断连。渠道回调重试语义下，挂死的半请求不吃并发预算。
    std::chrono::milliseconds requestTimeout{0};
};

class HttpIngressServer {
public:
    virtual ~HttpIngressServer() = default;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void pump() {}
    virtual std::uint16_t listenPort() const { return 0; }
    virtual void beginDrain() {}
    virtual std::size_t activeConnections() const { return 0; }
    virtual std::vector<ConnectionID> connectionIds() const { return {}; }

    // 路由注册（start 前生效）。http 路由任意路径一个 handler（全方法）；
    // ws 路由 = 专用升级路由（独立生命周期，不进 http handler）。
    virtual void setHttpRoute(std::string path, HttpHandler handler) = 0;
    virtual void setWsRoute(std::string path) = 0;

    virtual void setObserver(IHttpIngressObserver* observer) = 0;
};

std::unique_ptr<HttpIngressServer> makeHttpIngressServer(const HttpIngressConfig& config);

} // namespace interfaces
