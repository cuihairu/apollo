#include "gateway/ingress/tcp_client_ingress_server.hpp"

#if !defined(_WIN32)

#include "apollo/core/log/log_manager.h"
#include "apollo/net/link/frame.hpp"
#include "apollo/net/link/tcp_transport.hpp"

#include <algorithm>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace gateway {

namespace netlink = apollo::net::link;

namespace {

apollo::core::log::Logger& glog() {
    return *apollo::core::log::global_log_manager().createLogger("gateway_ingress");
}

// 跨线程投递面：send/close 从任意线程入队，泵线程出队落 socket。
struct HandedOp {
    ConnectionID connectionId{0};
    bool isClose{false};
    DisconnectReason closeReason{DisconnectReason::TransportError};
    std::vector<std::uint8_t> payload;
};

// 观测事件：泵线程内先攒后发——observer 回调可能重入 send/close，
// 不得在遍历连接表期间同步触发。
struct ObserverEvent {
    enum class Kind { kOpened, kPacket, kClosed };

    Kind kind{Kind::kClosed};
    ConnectionID connectionId{0};
    ClientEndpoint endpoint;
    DisconnectReason reason{DisconnectReason::TransportError};
    std::vector<std::uint8_t> payload;
};

struct IngressConnection {
    ConnectionID id{0};
    ClientEndpoint endpoint;
    std::unique_ptr<netlink::ILinkTransport> transport;
    netlink::FrameParser parser;
    std::vector<std::uint8_t> tx;  // 出站残余（短写余量），泵线程独占
    std::uint32_t nextSeq{1};      // 入口侧无去重语义，仅按帧格式填位
    bool closing{false};
    DisconnectReason closeReason{DisconnectReason::TransportError};
};

class TcpClientIngressServer final : public ClientIngressServer {
public:
    TcpClientIngressServer(std::string host, std::uint16_t port, std::size_t maxConnections)
        : host_(std::move(host)), port_(port), maxConnections_(maxConnections) {}

    void start() override {
        if (!listener_.start(host_, port_)) {
            throw std::runtime_error("Failed to bind client ingress on "
                                     + host_ + ":" + std::to_string(port_));
        }
        started_ = true;
        glog().info("client ingress listening on " + host_ + ":"
                    + std::to_string(listener_.port()));
    }

    void stop() override {
        // 前置契约：宿主先停驱泵线程再调 stop（GatewayServer::stop 先 join
        // 泵线程）——连接表为泵线程独占，跨线程并发拆表不在契约内。
        if (!started_) {
            return;
        }
        listener_.close();
        for (auto& [id, conn] : connections_) {
            conn->transport->close();
            ObserverEvent event;
            event.kind = ObserverEvent::Kind::kClosed;
            event.connectionId = id;
            event.reason = DisconnectReason::NormalClose;
            events_.push_back(std::move(event));
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
        return started_ ? listener_.port() : 0;
    }

    // 排空面（P4 排空批）：关监听 = 新连接内核层拒（backlog 全 RST）；
    // 既有连接照常 io_round。幂等——排水期重入不再动监听器。
    void beginDrain() override {
        if (!started_ || draining_) {
            return;
        }
        draining_ = true;
        listener_.close();
        glog().info("client ingress drain begun: listener closed, " +
                    std::to_string(activeConnections()) +
                    " existing connections served");
    }

    std::size_t activeConnections() const override {
        std::size_t n = 0;
        for (const auto& [id, conn] : connections_) {
            if (!conn->closing) {
                ++n;
            }
        }
        return n;
    }

    std::vector<ConnectionID> connectionIds() const override {
        std::vector<ConnectionID> ids;
        ids.reserve(connections_.size());
        for (const auto& [id, conn] : connections_) {
            if (!conn->closing) {
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
            netlink::append_frame(conn.tx, conn.nextSeq++, 0, op.payload);
            flush_tx(conn);
        }
    }

    void accept_new() {
        if (draining_) {
            return;  // 排空期不接受新连接（监听已关，守卫兜底竞速入站的尾批）
        }
        while (auto transport = listener_.accept()) {
            if (maxConnections_ != 0 && connections_.size() >= maxConnections_) {
                glog().warning("client ingress at capacity, dropping incoming connection");
                transport->close();
                continue;
            }
            auto conn = std::make_unique<IngressConnection>();
            conn->id = nextConnectionId_++;
            conn->endpoint = ClientEndpoint{transport->peerAddress(), transport->peerPort()};
            conn->transport = std::move(transport);
            ObserverEvent event;
            event.kind = ObserverEvent::Kind::kOpened;
            event.connectionId = conn->id;
            event.endpoint = conn->endpoint;
            events_.push_back(std::move(event));
            connections_.emplace(conn->id, std::move(conn));
        }
    }

    void io_round() {
        for (auto& [id, conn] : connections_) {
            if (conn->closing) {
                continue;
            }
            flush_tx(*conn);
            if (!conn->closing) {
                read_frames(*conn);
            }
        }
    }

    void flush_tx(IngressConnection& conn) {
        while (!conn.tx.empty() && !conn.closing) {
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

    void read_frames(IngressConnection& conn) {
        std::uint8_t buf[4096];
        for (;;) {
            const int n = conn.transport->read(buf, sizeof buf);
            if (n > 0) {
                const auto outcome = conn.parser.feed(buf, static_cast<std::size_t>(n));
                if (outcome.fatal) {
                    // 分界损坏（len 超限/尾校验不过）：帧流已不可信，断连
                    // 不静默跳过（frame.hpp 容错分界纪律）。
                    mark_closed(conn, DisconnectReason::TransportError);
                    return;
                }
                for (auto& frame : outcome.frames) {
                    if (conn.closing) {
                        break;
                    }
                    if (netlink::is_control(frame)) {
                        // control 帧是 InterServerLink 专用——客户端发出即
                        // 契约外消息，断连（不开放契约外消息纪律）。
                        mark_closed(conn, DisconnectReason::TransportError);
                        return;
                    }
                    ObserverEvent event;
                    event.kind = ObserverEvent::Kind::kPacket;
                    event.connectionId = conn.id;
                    event.payload = std::move(frame.payload);
                    events_.push_back(std::move(event));
                }
                if (conn.closing) {
                    return;
                }
                if (static_cast<std::size_t>(n) < sizeof buf) {
                    return;  // 缓冲未满即收尽：少转一轮空读
                }
                continue;
            }
            if (n == 0) {
                return;  // 暂无数据
            }
            // -1：对端关闭与传输错误同形（缝语义）——按对端关闭记账，
            // 会话层收到的是同一条断开事实。
            mark_closed(conn, DisconnectReason::NormalClose);
            return;
        }
    }

    void reap_closed() {
        for (auto it = connections_.begin(); it != connections_.end();) {
            if (it->second->closing) {
                it->second->transport->close();
                it = connections_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void mark_closed(IngressConnection& conn, DisconnectReason reason) {
        if (conn.closing) {
            return;  // 首因记账：不覆盖已有断开原因
        }
        conn.closing = true;
        conn.closeReason = reason;
        ObserverEvent event;
        event.kind = ObserverEvent::Kind::kClosed;
        event.connectionId = conn.id;
        event.reason = reason;
        events_.push_back(std::move(event));
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

    std::string host_;
    std::uint16_t port_{0};
    std::size_t maxConnections_{0};
    netlink::TcpListener listener_;
    std::unordered_map<ConnectionID, std::unique_ptr<IngressConnection>> connections_;
    std::vector<ObserverEvent> events_;
    std::deque<HandedOp> handoff_;
    std::mutex handoffMutex_;
    ConnectionID nextConnectionId_{1};
    IClientIngressObserver* observer_{nullptr};
    bool started_{false};
    bool draining_{false};  // 泵线程独占（beginDrain 归宿主泵线程调）
};

} // namespace

std::unique_ptr<ClientIngressServer> makeTcpClientIngressServer(
    std::string host, std::uint16_t port, std::size_t maxConnections) {
    return std::make_unique<TcpClientIngressServer>(std::move(host), port, maxConnections);
}

} // namespace gateway

#else // _WIN32

namespace gateway {

std::unique_ptr<ClientIngressServer> makeTcpClientIngressServer(
    std::string /*host*/, std::uint16_t /*port*/, std::size_t /*maxConnections*/) {
    // link TCP 底座不进 M1 批（POSIX only）——Windows 退回 null 桩。
    return makeNullClientIngressServer();
}

} // namespace gateway

#endif // !_WIN32
