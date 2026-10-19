#include "gateway/ingress/ws_client_ingress_server.hpp"

#if !defined(_WIN32)

#include "apollo/core/log/log_manager.h"
#include "apollo/net/link/frame.hpp"
#include "apollo/net/link/tcp_transport.hpp"
#include "apollo/net/tls/tls_transport.hpp"
#include "apollo/net/ws/frame.hpp"
#include "apollo/net/ws/handshake.hpp"

#include <algorithm>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace gateway {

namespace netlink = apollo::net::link;
namespace netws = apollo::net::ws;

namespace {

apollo::core::log::Logger& glog() {
    return *apollo::core::log::global_log_manager().createLogger("gateway_ws_ingress");
}

// 跨线程投递面：send/close 从任意线程入队，泵线程出队落传输（TCP 入口同款）。
struct HandedOp {
    ConnectionID connectionId{0};
    bool isClose{false};
    DisconnectReason closeReason{DisconnectReason::TransportError};
    std::vector<std::uint8_t> payload;
};

// 观测事件：泵线程内先攒后发——observer 回调可能重入 send/close，不得在
// 遍历连接表期间同步触发。
struct ObserverEvent {
    enum class Kind { kOpened, kPacket, kClosed };

    Kind kind{Kind::kClosed};
    ConnectionID connectionId{0};
    ClientEndpoint endpoint;
    DisconnectReason reason{DisconnectReason::TransportError};
    std::vector<std::uint8_t> payload;
};

// 拒绝应答/close 回执滞留兜底：对端不读 socket 时残余写不尽（内核缓冲
// 塞满即 0），泵 N 拍后强拆，不滞留连接表。
inline constexpr std::uint32_t kStalledTxPumpLimit = 500;

struct WsIngressConnection {
    ConnectionID id{0};
    ClientEndpoint endpoint;
    std::unique_ptr<netlink::ILinkTransport> transport;
    netws::HandshakeParser handshake;
    netws::FrameParser frames;
    netlink::FrameParser netframes;
    std::vector<std::uint8_t> tx;  // 出站残余（101/400/控制回执/业务帧），泵线程独占
    std::vector<std::uint8_t> netframe;  // send 时组链路帧的中转缓冲（泵线程独占）
    std::uint32_t nextSeq{1};
    bool upgraded{false};      // 升级完成（101 已应答）
    bool closing{false};
    DisconnectReason closeReason{DisconnectReason::TransportError};
    std::uint32_t stalledPumps{0};  // closing 且残余写不尽的泵计数
};

class WsClientIngressServer final : public ClientIngressServer {
public:
    explicit WsClientIngressServer(WsIngressConfig config)
        : config_(std::move(config)), nextConnectionId_(config.connectionIdBase + 1) {}

    void start() override {
        if (!config_.tls) {
            // 明文 ws：TcpListener 底座。
            if (!tcpListener_.start(config_.host, config_.port)) {
                throw std::runtime_error("Failed to bind ws client ingress on "
                                         + config_.host + ":" + std::to_string(config_.port));
            }
        } else {
            // wss：TlsListener 底座（上下文未装载即拒绝——凭据是显式部署集成）。
            if (!tlsListener_.start(*config_.tls, config_.host, config_.port)) {
                throw std::runtime_error("Failed to bind wss client ingress on "
                                         + config_.host + ":" + std::to_string(config_.port)
                                         + " (tls context not loaded or bind refused)");
            }
        }
        started_ = true;
        glog().info("ws client ingress listening on " + config_.host + ":"
                    + std::to_string(listenPort()) + " path " + config_.path
                    + (config_.tls ? " (wss)" : " (ws)"));
    }

    void stop() override {
        // 前置契约同 TCP 入口：宿主先停驱泵线程再调 stop。
        if (!started_) {
            return;
        }
        close_listener();
        for (auto& [id, conn] : connections_) {
            conn->transport->close();
            if (conn->upgraded) {
                // 未升级即拆的半开握手 socket 不产生任何事件。
                ObserverEvent event;
                event.kind = ObserverEvent::Kind::kClosed;
                event.connectionId = id;
                event.reason = DisconnectReason::NormalClose;
                events_.push_back(std::move(event));
            }
        }
        connections_.clear();
        {
            std::lock_guard<std::mutex> lock(handoffMutex_);
            handoff_.clear();
        }
        started_ = false;
        fire_events();
    }

    void pump() override {
        if (!started_) {
            return;
        }

        drain_handoff();
        accept_new();
        io_round();
        reap_closed();
        fire_events();
    }

    std::uint16_t listenPort() const override {
        return started_ ? (config_.tls ? tlsListener_.port() : tcpListener_.port()) : 0;
    }

    // 排空面（P4 排空批同款）：关监听 = 新连接内核层拒；既有连接照常服务。
    void beginDrain() override {
        if (!started_ || draining_) {
            return;
        }
        draining_ = true;
        close_listener();
        glog().info("ws client ingress drain begun: listener closed, " +
                    std::to_string(activeConnections()) +
                    " existing connections served");
    }

    std::size_t activeConnections() const override {
        std::size_t n = 0;
        for (const auto& [id, conn] : connections_) {
            if (conn->upgraded && !conn->closing) {
                ++n;  // 活跃 = 已升级会话（半开握手 socket 不计）
            }
        }
        return n;
    }

    std::vector<ConnectionID> connectionIds() const override {
        std::vector<ConnectionID> ids;
        ids.reserve(connections_.size());
        for (const auto& [id, conn] : connections_) {
            if (conn->upgraded && !conn->closing) {
                ids.push_back(id);
            }
        }
        return ids;
    }

    void send(ConnectionID connectionId, std::span<const std::uint8_t> payload) override {
        std::lock_guard<std::mutex> lock(handoffMutex_);
        handoff_.push_back(HandedOp{connectionId, false,
                                    DisconnectReason::TransportError,
                                    {payload.begin(), payload.end()}});
    }

    void close(ConnectionID connectionId, DisconnectReason reason) override {
        std::lock_guard<std::mutex> lock(handoffMutex_);
        handoff_.push_back(HandedOp{connectionId, true, reason, {}});
    }

    void setObserver(IClientIngressObserver* observer) override { observer_ = observer; }

private:
    void close_listener() {
        if (config_.tls) {
            tlsListener_.close();
        } else {
            tcpListener_.close();
        }
    }

    void drain_handoff() {
        std::deque<HandedOp> ops;
        {
            std::lock_guard<std::mutex> lock(handoffMutex_);
            ops.swap(handoff_);
        }
        for (auto& op : ops) {
            const auto it = connections_.find(op.connectionId);
            if (it == connections_.end() || it->second->closing) {
                continue;  // 迟到操作：连接已断即落地丢弃（会话竞态容忍）
            }
            if (op.isClose) {
                mark_closed(*it->second, op.closeReason);
                continue;
            }
            if (op.payload.size() > netlink::kMaxPayload) {
                glog().warning("drop oversize outbound payload (" +
                               std::to_string(op.payload.size()) + " bytes)");
                continue;
            }
            auto& conn = *it->second;
            if (!conn.upgraded) {
                continue;  // 未升级连接无业务出站（升级前只答握手）
            }
            // 出站 = link Frame 包进 ws binary 消息（同一线形、多一层封装）。
            conn.netframe.clear();
            netlink::append_frame(conn.netframe, conn.nextSeq++, 0, op.payload);
            netws::append_frame(conn.tx, netws::OpCode::kBinary, conn.netframe);
            flush_tx(conn);
        }
    }

    void accept_new() {
        if (draining_) {
            return;  // 排空期不接受新连接
        }
        for (;;) {
            std::unique_ptr<netlink::ILinkTransport> transport =
                config_.tls ? tlsListener_.accept() : tcpListener_.accept();
            if (!transport) {
                break;
            }
            if (max_capacity()) {
                glog().warning("ws client ingress at capacity, dropping incoming connection");
                transport->close();
                continue;
            }
            auto conn = std::make_unique<WsIngressConnection>();
            conn->endpoint = ClientEndpoint{transport->peerAddress(), transport->peerPort()};
            conn->transport = std::move(transport);
            // 注意：accept 即占号（号不回收复用），Open 事件延后到升级受理。
            conn->id = nextConnectionId_++;
            connections_.emplace(conn->id, std::move(conn));
        }
    }

    bool max_capacity() const {
        return config_.maxConnections != 0 && connections_.size() >= config_.maxConnections;
    }

    void io_round() {
        for (auto& [id, conn] : connections_) {
            if (conn->closing) {
                // 断开在途：只把残余写尽（400 拒绝 / close 回执），不再读。
                if (!conn->tx.empty()) {
                    flush_tx(*conn);
                    if (!conn->tx.empty() && ++conn->stalledPumps > kStalledTxPumpLimit) {
                        glog().warning("ws closing reply stalled, dropping connection");
                        conn->tx.clear();
                    }
                }
                continue;
            }
            flush_tx(*conn);
            if (conn->closing) {
                continue;
            }
            if (conn->upgraded) {
                read_frames(*conn);
            } else {
                read_handshake(*conn);
            }
            // 101/控制回执刚入队即尽力写（小包通常一轮写尽，少等一拍）。
            if (!conn->tx.empty()) {
                flush_tx(*conn);
            }
        }
    }

    // closing 无关：残余一律尽力写（400 与 close 回执恰是断开在途时才
    // 需要送达的字节）。写坏 → mark_closed 幂等返回，余量作废（对端已
    // 不可读）。
    void flush_tx(WsIngressConnection& conn) {
        while (!conn.tx.empty()) {
            const int n = conn.transport->write(conn.tx.data(), conn.tx.size());
            if (n < 0) {
                mark_closed(conn, DisconnectReason::TransportError);
                return;
            }
            if (n == 0) {
                return;  // 暂不可写：余量留到下个泵周期
            }
            conn.tx.erase(conn.tx.begin(), conn.tx.begin() + n);
        }
    }

    void read_handshake(WsIngressConnection& conn) {
        std::uint8_t buf[4096];
        for (;;) {
            const int n = conn.transport->read(buf, sizeof buf);
            if (n > 0) {
                const auto state = conn.handshake.feed(buf, static_cast<std::size_t>(n));
                if (state == netws::HandshakeParser::State::kNeedMore) {
                    if (static_cast<std::size_t>(n) < sizeof buf) {
                        return;  // 缓冲未满即收尽：少转一轮空读
                    }
                    continue;
                }
                if (state == netws::HandshakeParser::State::kBad) {
                    // 请求不可解析：400 + 断（无升级无事件）。
                    reject_upgrade(conn, "malformed websocket upgrade request");
                    return;
                }
                // kReady：升级判据 + 路由判据。
                const auto& request = conn.handshake.request();
                const bool path_ok = request.path == config_.path;
                if (!netws::is_valid_upgrade(request) || !path_ok) {
                    reject_upgrade(conn, path_ok ? "invalid websocket upgrade"
                                                 : "no such route");
                    return;
                }
                const auto accept_reply = netws::make_accept_response(request.key);
                conn.tx.insert(conn.tx.end(), accept_reply.begin(), accept_reply.end());
                conn.upgraded = true;
                ObserverEvent event;
                event.kind = ObserverEvent::Kind::kOpened;
                event.connectionId = conn.id;
                event.endpoint = conn.endpoint;
                events_.push_back(std::move(event));
                // 头部同 chunk 粘的首帧字节移交帧层（快客户端实态）。
                feed_frame_bytes(conn, conn.handshake.take_remainder());
                return;
            }
            if (n == 0) {
                return;  // 暂无数据（含 TLS 握手在跑——WANT_* 同形）
            }
            // -1：对端关闭与传输错误同形。未升级即断不上报。
            mark_closed(conn, DisconnectReason::NormalClose, /*emit=*/false);
            return;
        }
    }

    void reject_upgrade(WsIngressConnection& conn, const char* reason) {
        glog().warning("ws upgrade rejected from " + conn.endpoint.address + ":"
                       + std::to_string(conn.endpoint.port) + ": " + reason);
        const auto reply = netws::make_reject_response(reason);
        conn.tx.insert(conn.tx.end(), reply.begin(), reply.end());
        conn.closeReason = DisconnectReason::AdmissionRejected;
        conn.closing = true;   // 400 写尽即拆（io_round closing 支路）
        flush_tx(conn);
    }

    void read_frames(WsIngressConnection& conn) {
        std::uint8_t buf[4096];
        for (;;) {
            const int n = conn.transport->read(buf, sizeof buf);
            if (n > 0) {
                feed_frame_bytes(conn, std::span<const std::uint8_t>(buf, static_cast<std::size_t>(n)));
                if (conn.closing) {
                    return;
                }
                if (static_cast<std::size_t>(n) < sizeof buf) {
                    return;
                }
                continue;
            }
            if (n == 0) {
                return;
            }
            mark_closed(conn, DisconnectReason::NormalClose);
            return;
        }
    }

    // ws 帧流字节入帧解析器：业务消息载荷 = link Frame 字节流（TCP 入口
    // 同一解析面），控制回执入 tx，协议错/契约外载荷一律断连。
    void feed_frame_bytes(WsIngressConnection& conn, std::span<const std::uint8_t> bytes) {
        if (bytes.empty()) {
            return;
        }
        const auto outcome = conn.frames.feed(bytes.data(), bytes.size());
        if (outcome.fatal) {
            mark_closed(conn, DisconnectReason::TransportError);
            return;
        }
        if (!outcome.pending_control.empty()) {
            conn.tx.insert(conn.tx.end(), outcome.pending_control.begin(),
                           outcome.pending_control.end());
        }
        if (outcome.peer_closed) {
            // close 回执已入 tx（写尽后拆）；会话层收到的是同一条断开事实。
            mark_closed(conn, DisconnectReason::NormalClose);
            return;
        }
        for (auto& message : outcome.messages) {
            if (conn.closing) {
                break;
            }
            if (message.opcode != netws::OpCode::kBinary) {
                // 游戏面只走 binary：text = 契约外载荷，断连不静默。
                mark_closed(conn, DisconnectReason::TransportError);
                return;
            }
            const auto net_outcome = conn.netframes.feed(message.payload);
            if (net_outcome.fatal) {
                // 链路帧损坏：帧流不可信，断连不静默跳过（TCP 入口同款纪律）。
                mark_closed(conn, DisconnectReason::TransportError);
                return;
            }
            for (auto& frame : net_outcome.frames) {
                if (conn.closing) {
                    break;
                }
                if (netlink::is_control(frame)) {
                    // control 帧是 InterServerLink 专用——客户端发出即契约外。
                    mark_closed(conn, DisconnectReason::TransportError);
                    return;
                }
                ObserverEvent event;
                event.kind = ObserverEvent::Kind::kPacket;
                event.connectionId = conn.id;
                event.payload = std::move(frame.payload);
                events_.push_back(std::move(event));
            }
        }
    }

    void reap_closed() {
        for (auto it = connections_.begin(); it != connections_.end();) {
            // 残余未写尽的不拆（io_round 在写；滞留有 stalledPumps 兜底强拆）。
            if (it->second->closing && it->second->tx.empty()) {
                it->second->transport->close();
                it = connections_.erase(it);
            } else {
                ++it;
            }
        }
    }

    // emit=false：未升级即断（对端握手前溜走）——socket 级事实，不产生
    // observer 事件（Open 只对已升级会话发）。
    void mark_closed(WsIngressConnection& conn, DisconnectReason reason, bool emit = true) {
        if (conn.closing) {
            return;  // 首因记账：不覆盖已有断开原因
        }
        conn.closing = true;
        conn.closeReason = reason;
        if (emit && conn.upgraded) {
            ObserverEvent event;
            event.kind = ObserverEvent::Kind::kClosed;
            event.connectionId = conn.id;
            event.reason = reason;
            events_.push_back(std::move(event));
        }
    }

    void fire_events() {
        std::vector<ObserverEvent> events;
        events.swap(events_);
        for (auto& event : events) {
            if (observer_ == nullptr) {
                break;
            }
            switch (event.kind) {
                case ObserverEvent::Kind::kOpened:
                    observer_->onConnectionOpened(event.connectionId, event.endpoint);
                    break;
                case ObserverEvent::Kind::kPacket:
                    observer_->onPacketReceived(
                        event.connectionId,
                        std::span<const std::uint8_t>(event.payload.data(), event.payload.size()));
                    break;
                case ObserverEvent::Kind::kClosed:
                    observer_->onConnectionClosed(event.connectionId, event.reason);
                    break;
            }
        }
    }

    WsIngressConfig config_;
    netlink::TcpListener tcpListener_;
    apollo::net::tls::TlsListener tlsListener_;
    std::unordered_map<ConnectionID, std::unique_ptr<WsIngressConnection>> connections_;
    std::vector<ObserverEvent> events_;
    std::deque<HandedOp> handoff_;
    std::mutex handoffMutex_;
    ConnectionID nextConnectionId_;
    IClientIngressObserver* observer_{nullptr};
    bool started_{false};
    bool draining_{false};  // 泵线程独占（beginDrain 归宿主泵线程调）
};

} // namespace

std::unique_ptr<ClientIngressServer> makeWsClientIngressServer(const WsIngressConfig& config) {
    return std::make_unique<WsClientIngressServer>(config);
}

} // namespace gateway

#else // _WIN32

namespace gateway {

std::unique_ptr<ClientIngressServer> makeWsClientIngressServer(const WsIngressConfig& /*config*/) {
    // link TCP/TLS 底座不进 M1 批（POSIX only）——Windows 退回 null 桩。
    return makeNullClientIngressServer();
}

} // namespace gateway

#endif // !_WIN32
