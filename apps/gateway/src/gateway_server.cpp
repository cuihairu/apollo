#include "gateway/gateway_server.hpp"
#include "gateway/ingress/ingress_fan_in.hpp"
#include "gateway/ingress/tcp_client_ingress_server.hpp"
#include "gateway/ingress/ws_client_ingress_server.hpp"
#include "apollo/core/log/log_manager.h"
#include "apollo/net/tls/tls_transport.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>
#if !defined(_WIN32)
#include <unistd.h>
#endif
#include "gateway_server_detail.hpp"

namespace gateway {

namespace server_detail {

// 库面日志出口单定义（gateway_server 拆分单元共享——detail 头声明，
// cat=gateway_server 检索键不变）
apollo::core::log::Logger& glog() {
    return *apollo::core::log::global_log_manager().createLogger("gateway_server");
}

} // namespace server_detail

namespace {

// 库面日志出口（server_detail 单定义，本单元转发——定义在 gateway_server.cpp）
apollo::core::log::Logger& glog() {
    return server_detail::glog();
}

constexpr std::uint32_t kM1BaseForwardTimeoutMs = 500;

std::optional<std::pair<std::string, std::uint16_t>> parseEndpoint(
    const std::string& endpoint) {
    std::string host;
    std::string portText;

    if (!endpoint.empty() && endpoint.front() == '[') {
        const auto close = endpoint.find(']');
        if (close == std::string::npos || close + 2 != endpoint.size() ||
            endpoint[close + 1] != ':') {
            return std::nullopt;
        }
        host = endpoint.substr(1, close - 1);
        portText = endpoint.substr(close + 2);
    } else {
        const auto colon = endpoint.rfind(':');
        if (colon == std::string::npos || colon == 0 || colon + 1 == endpoint.size()) {
            return std::nullopt;
        }
        host = endpoint.substr(0, colon);
        portText = endpoint.substr(colon + 1);
    }

    if (host.empty() || portText.empty() || portText.size() > 5) {
        return std::nullopt;
    }
    for (const char digit : portText) {
        if (digit < '0' || digit > '9') {
            return std::nullopt;
        }
    }

    const auto port = std::strtoul(portText.c_str(), nullptr, 10);
    if (port == 0 || port > std::numeric_limits<std::uint16_t>::max()) {
        return std::nullopt;
    }
    return std::pair<std::string, std::uint16_t>{host, static_cast<std::uint16_t>(port)};
}
} // namespace

#if !defined(_WIN32)
namespace {

// 客户端入口多监听装配（L45 批③）：TCP 明文主入口 + 可选 WS/WSS 子入口，
// 号段互斥分区（TCP 0 / WS 1<<40 / WSS 2<<40）。wsPort/wssPort 均 0 =
// 单 TCP 入口原形态（不套扇入）。WSS 凭据缺失/装载失败即装配拒绝（凭据
// 是显式部署集成，net-abstraction §5.9——不静默降级成明文）；监听绑定
// 失败归 start 面（同 TCP 口径，响亮失败）。
struct AssembledIngress {
    std::unique_ptr<apollo::net::tls::TlsServerContext> tlsContext;
    std::unique_ptr<ClientIngressServer> server;
};

AssembledIngress assemble_client_ingress(const GatewayConfig& config) {
    const std::size_t max_conns = static_cast<std::size_t>(config.maxConnections);
    std::vector<IngressFanInBinding> bindings;
    bindings.push_back(IngressFanInBinding{
        makeTcpClientIngressServer(config.host, config.port, max_conns, 0), 0});

    std::unique_ptr<apollo::net::tls::TlsServerContext> tls_context;
    if (config.wsPort != 0) {
        WsIngressConfig ws;
        ws.host = config.host;
        ws.port = config.wsPort;
        ws.path = config.wsPath;
        ws.maxConnections = max_conns;
        ws.connectionIdBase = 1 * kIngressFanInIdStride;
        bindings.push_back(IngressFanInBinding{
            makeWsClientIngressServer(ws), ws.connectionIdBase});
    }
    if (config.wssPort != 0) {
        if (config.tlsCertPath.empty() || config.tlsKeyPath.empty()) {
            throw std::runtime_error(
                "wss ingress enabled but tls cert/key not configured (--tls-cert/--tls-key)");
        }
        tls_context = std::make_unique<apollo::net::tls::TlsServerContext>();
        const auto loaded = tls_context->load(config.tlsCertPath, config.tlsKeyPath);
        if (!loaded.ok) {
            throw std::runtime_error("wss tls context load failed: " + loaded.error);
        }
        WsIngressConfig wss;
        wss.host = config.host;
        wss.port = config.wssPort;
        wss.path = config.wsPath;
        wss.tls = tls_context.get();
        wss.maxConnections = max_conns;
        wss.connectionIdBase = 2 * kIngressFanInIdStride;
        bindings.push_back(IngressFanInBinding{
            makeWsClientIngressServer(wss), wss.connectionIdBase});
    }

    if (bindings.size() == 1) {
        return {nullptr, std::move(bindings[0].server)};
    }
    return {std::move(tls_context), makeIngressFanIn(std::move(bindings))};
}

} // namespace
#endif // !_WIN32

namespace {

// 准入传输选装（P4-4 批①）：loginM1Port 非零 = link M1 面（拨 login 准入
// 受理面）；装配缺位降级（处方①）= 退化容忍面，起进程不拦。
std::unique_ptr<SessionAdmissionService> makeAdmissionService(const GatewayConfig& config) {
    if (config.loginM1Port == 0) {
        return makeDefaultSessionAdmissionService();
    }
    auto linkService = makeLinkSessionAdmissionService(
        config.loginM1Host, config.loginM1Port, config.worldM1RouteJson);
    if (!linkService) {
        glog().warning("gateway login M1 admission degraded: route table unavailable ("
                       + config.worldM1RouteJson + "), admission degrades tolerant");
        return makeDefaultSessionAdmissionService();
    }
    return linkService;
}

} // namespace

//==============================================================================
// GatewayServer 生命周期与装配（构造/启停/Base 前递/监听端口观测）
//==============================================================================

GatewayServer::GatewayServer(const GatewayConfig& config)
    : config_(config)
    , sessionManager_(std::make_unique<SessionManager>())
    , packetDispatcher_(std::make_unique<ClientPacketDispatcher>())
    , connectionRegistry_(std::make_unique<GatewayConnectionRegistry>())
    , admissionService_(makeAdmissionService(config))
    , messageRouter_(std::make_unique<MessageRouter>(config)) {
    // 多监听装配在构造体（凭据装载失败即此处抛出）：tlsContext_ 先于
    // ingressServer_ 声明 → 后于其析构（监听器持有其 SSL_CTX）。
#if !defined(_WIN32)
    auto assembled = assemble_client_ingress(config_);
    tlsContext_ = std::move(assembled.tlsContext);
    ingressServer_ = std::move(assembled.server);
#else
    ingressServer_ = makeTcpClientIngressServer(
        config_.host, config_.port, static_cast<std::size_t>(config_.maxConnections));
#endif
}

GatewayServer::~GatewayServer() {
    stop();
}

void GatewayServer::start() {
    if (running_) return;

    // 客户端帧下行出口（P2 真实下行；start 前装配）：
    // World 939 回带 → M1 出口解字段 → sendToClient（会话在即真投）。
    messageRouter_->setDeliverToClient(
        [this](SessionID sessionId, const std::vector<uint8_t>& message) {
            sendToClient(sessionId, message);
        });
    messageRouter_->setBaseForwarding(
        [this](SessionID sessionId, const std::vector<uint8_t>& message) {
            std::function<std::optional<std::string>(PlayerID)> resolver;
            {
                std::lock_guard<std::mutex> guard(baseResolverMutex_);
                resolver = baseResolver_;
            }
            if (resolver) {
                ++baseForwardResolves_;
                const auto session = sessionManager_ ? sessionManager_->getSession(sessionId) : nullptr;
                if (!session || session->playerId == 0) {
                    ++baseForwardRejections_;
                    glog().warning("Base forward rejected: session " + std::to_string(sessionId) +
                                   " has no bound player");
                    return std::uint64_t{0};
                }

                std::optional<std::string> endpoint;
                try {
                    endpoint = resolver(session->playerId);
                } catch (const std::exception& ex) {
                    ++baseForwardRejections_;
                    glog().warning("Base forward rejected: resolver failed for player " +
                                   std::to_string(session->playerId) + ": " + ex.what());
                    return std::uint64_t{0};
                }
                if (!endpoint || endpoint->empty()) {
                    ++baseForwardRejections_;
                    glog().warning("Base forward rejected: player " + std::to_string(session->playerId) +
                                   " has no current Base");
                    return std::uint64_t{0};
                }

                const auto client = baseClientForEndpoint(*endpoint);
                if (!client || !client->peerUp()) {
                    ++baseForwardRejections_;
                    glog().warning("Base forward rejected: endpoint " + *endpoint + " is unreachable");
                    return std::uint64_t{0};
                }
                const auto result = client->callAsync(
                    message,
                    std::chrono::milliseconds(kM1BaseForwardTimeoutMs),
                    [this, sessionId](std::optional<std::vector<std::uint8_t>> response) {
                        if (!response) {
                            glog().warning("Base forward timeout for session "
                                           + std::to_string(sessionId));
                            return;
                        }
                        if (messageRouter_ && messageRouter_->baseResponse()) {
                            messageRouter_->baseResponse()(sessionId, std::move(response));
                        }
                    });
                return result;
            }

            if (!baseClient_) return std::uint64_t{0};
            const auto result = baseClient_->callAsync(
                message,
                std::chrono::milliseconds(kM1BaseForwardTimeoutMs),
                [this, sessionId](std::optional<std::vector<std::uint8_t>> response) {
                    if (!response) {
                        glog().warning("Base forward timeout for session "
                                       + std::to_string(sessionId));
                        return;
                    }
                    if (messageRouter_ && messageRouter_->baseResponse()) {
                        messageRouter_->baseResponse()(sessionId, std::move(response));
                    }
                });
            return result;
        },
        [this](SessionID sessionId, std::optional<std::vector<std::uint8_t>> response) {
            if (response) sendToClient(sessionId, *response);
        });

    // 启动消息路由器
    messageRouter_->start();
    ingressServer_->setObserver(this);
    // 监听 socket 归 ingress 真入口（link TcpListener）——bind 失败即 throw，
    // 与旧裸 socket 块同契约。
    ingressServer_->start();

    // M1 路由解析客户端面（P4-4 批④）：directoryM1Port 非零 = 拨 manager
    // 控制面；装配失败降级（警告 + 不拦启动），路由解析缺席 → 默认路由兜底。
    if (config_.directoryM1Port != 0) {
        M1RouteClientConfig routeConfig;
        routeConfig.host = config_.directoryM1Host;
        routeConfig.port = config_.directoryM1Port;
        routeConfig.route_json = config_.worldM1RouteJson;
        m1RouteClient_ = makeM1RouteClient(std::move(routeConfig));
        if (m1RouteClient_) {
            glog().info("Gateway M1 route client dialing " + config_.directoryM1Host
                        + ":" + std::to_string(config_.directoryM1Port));
        } else {
            glog().warning("gateway M1 route client degraded: route table unavailable ("
                           + config_.worldM1RouteJson + "), default route stays");
        }
    }

    // Base M1 激活面（L53 登录串联第三环）：baseM1Port 非零 = 拨 Base
    // PlayerStore M1 受理面，准入通过后激活锚点 + 绑定会话；装配失败降级
    // （警告 + 认证退容忍面——面缺席 ≠ 会话非法，准入面装配降级同口径）。
    if (config_.baseM1Port != 0) {
        M1BaseClientConfig baseConfig;
        baseConfig.host = config_.baseM1Host;
        baseConfig.port = config_.baseM1Port;
        baseConfig.route_json = config_.worldM1RouteJson;
        baseClient_ = makeM1BaseClient(std::move(baseConfig));
        if (baseClient_) {
            glog().info("Gateway M1 base client dialing " + config_.baseM1Host + ":"
                        + std::to_string(config_.baseM1Port));
        } else {
            glog().warning("gateway M1 base client degraded: route table unavailable ("
                           + config_.worldM1RouteJson + "), base activation off");
        }
    }

    // 排空控制受理面（P4 排空批）：装配失败降级（警告 + ops 不可达，客户端
    // 服务不受影响——出口降级同口径）。
    {
        GatewayControlConfig controlConfig;
        controlConfig.host = config_.host;
        controlConfig.port = config_.controlM1Port;
        controlConfig.route_json = config_.worldM1RouteJson;
        controlServer_ = makeGatewayControlServer(
            std::move(controlConfig), [this](const DrainControlRequest& request) {
                return handleDrainRequest(request);
            });
        if (!controlServer_->start()) {
            glog().warning("gateway drain control face degraded: bind/route failed ("
                           + config_.worldM1RouteJson + "), drain ops unreachable");
            controlServer_.reset();
        }
    }

    running_ = true;

    acceptThread_ = std::thread(&GatewayServer::acceptNewConnections, this);

    // 启动心跳检查线程
    heartbeatThread_ = std::thread(&GatewayServer::heartbeatCheckLoop, this);

    glog().info("Gateway server listening on " + config_.host + ":"
                + std::to_string(listenPort()));
}

void GatewayServer::setBaseResolver(
    std::function<std::optional<std::string>(PlayerID)> resolver) {
    std::lock_guard<std::mutex> guard(baseResolverMutex_);
    baseResolver_ = std::move(resolver);
}

std::uint64_t GatewayServer::baseForwardResolves() const {
    return baseForwardResolves_.load(std::memory_order_acquire);
}

std::uint64_t GatewayServer::baseForwardRejections() const {
    return baseForwardRejections_.load(std::memory_order_acquire);
}

std::shared_ptr<M1BaseClient> GatewayServer::baseClientForEndpoint(const std::string& endpoint) {
    const auto parsed = parseEndpoint(endpoint);
    if (!parsed) {
        return nullptr;
    }

    // 锁只圈表操作；peerUp 等待与返回后的使用都走共享持有副本——停机清表
    // 与调用面并发不悬垂（audit M1：裸指针曾在锁释放后逃逸）。
    {
        std::lock_guard<std::mutex> guard(baseClientMutex_);
        const auto found = routedBaseClients_.find(endpoint);
        if (found != routedBaseClients_.end()) {
            return found->second;
        }
    }

    M1BaseClientConfig clientConfig;
    clientConfig.host = parsed->first;
    clientConfig.port = parsed->second;
    clientConfig.route_json = config_.worldM1RouteJson;
    auto client = makeM1BaseClient(std::move(clientConfig));
    if (!client || !client->start()) {
        glog().warning("gateway Base forward route degraded: endpoint " + endpoint +
                       " unavailable");
        return nullptr;
    }

    std::shared_ptr<M1BaseClient> live;
    {
        std::lock_guard<std::mutex> guard(baseClientMutex_);
        const auto existing = routedBaseClients_.find(endpoint);
        if (existing != routedBaseClients_.end()) {
            client->stop();
            live = existing->second;
        } else {
            live = std::move(client);
            routedBaseClients_.emplace(endpoint, live);
        }
    }

    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(kM1BaseForwardTimeoutMs);
    while (!live->peerUp() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!live->peerUp()) {
        // 只撤自己落表的条目（对端迟迟不上线）：并发面可能已换条目。
        std::lock_guard<std::mutex> clear(baseClientMutex_);
        const auto liveEntry = routedBaseClients_.find(endpoint);
        if (liveEntry != routedBaseClients_.end() && liveEntry->second == live) {
            liveEntry->second->stop();
            routedBaseClients_.erase(liveEntry);
        }
        return nullptr;
    }
    return live;
}

void GatewayServer::stop() {
    running_ = false;

    // 心跳线程先收线：其超时路径经 send/close 投递队列与入口交互，且
    // onClientDisconnect 会触 forwardToWorld/forwardToBaseApp——路由器
    // （M1 出口/后端通道）与入口都得等它停驱后再拆。
    if (heartbeatThread_.joinable()) {
        heartbeatThread_.join();
    }

    // 泵线程停驱后才拆路由器与入口（TcpClientIngressServer::stop 契约：
    // 连接表泵线程独占，跨线程并发拆表不在契约内；M1 出口链路面同纪律，
    // forwardToWorld 的调用方全在上述两线程内）。
    if (acceptThread_.joinable()) {
        acceptThread_.join();
    }

    messageRouter_->stop();
    if (controlServer_) {
        controlServer_->stop();
        controlServer_.reset();
    }
    if (m1RouteClient_) {
        m1RouteClient_->stop();
        m1RouteClient_.reset();
    }
    {
        // Base 前递客户端面排空（共享持有：并发调用面若仍持有副本，stop
        // 后其 callAsync 落在已停实例上返回 0，不悬垂）。此序在入口拆除前
        // ——failPendingCalls 同步回收的应答回调仍可安全走 sendToClient。
        std::lock_guard<std::mutex> guard(baseClientMutex_);
        for (auto& [endpoint, client] : routedBaseClients_) {
            client->stop();
        }
        routedBaseClients_.clear();
    }
    if (baseClient_) {
        baseClient_->stop();
        baseClient_.reset();
    }
    if (ingressServer_) {
        ingressServer_->stop();
    }

    for (auto& thread : workerThreads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    workerThreads_.clear();

    // 准入面最后收口（全部调用线程已 join——in-flight admit 先于泵停驱）。
    if (admissionService_) {
        admissionService_->shutdown();
    }
}

//==============================================================================
// 观测面（连接/会话计数与实际监听端口读回）
//==============================================================================

std::optional<ClientEndpoint> GatewayServer::findClientEndpoint(SessionID sessionId) const {
    if (!connectionRegistry_) {
        return std::nullopt;
    }
    return connectionRegistry_->findEndpointBySession(sessionId);
}

std::size_t GatewayServer::getRegisteredConnectionCount() const {
    return connectionRegistry_ ? connectionRegistry_->getConnectionCount() : 0;
}

std::size_t GatewayServer::getSessionCount() const {
    return sessionManager_ ? sessionManager_->getSessionCount() : 0;
}

// 实际监听端口（port 0 = 内核分配，测试与编排面经此读回）。
std::uint16_t GatewayServer::listenPort() const {
    return ingressServer_ ? ingressServer_->listenPort() : 0;
}

std::uint16_t GatewayServer::wsListenPort() const {
#if !defined(_WIN32)
    // 装配序（assemble_client_ingress）：TCP 0，WS 1。
    if (config_.wsPort == 0 || !ingressServer_) return 0;
    const auto ports = ingressServer_->listenPorts();
    return ports.size() > 1 ? ports[1] : 0;
#else
    return 0;
#endif
}

std::uint16_t GatewayServer::wssListenPort() const {
#if !defined(_WIN32)
    // 装配序：TCP 0，[WS]，WSS——按配置跳缺位。
    if (config_.wssPort == 0 || !ingressServer_) return 0;
    const auto ports = ingressServer_->listenPorts();
    const std::size_t idx = 1 + (config_.wsPort != 0 ? 1 : 0);
    return ports.size() > idx ? ports[idx] : 0;
#else
    return 0;
#endif
}

} // namespace gateway
