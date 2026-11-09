#include "gateway/gateway_server.hpp"
#include "gateway_server_detail.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"
#include <chrono>
#include <span>
#include <thread>
#include <vector>

//==============================================================================
// 会话与连接面（gateway_server 拆分单元：入口观察者回调/会话保留窗/
// 断线销毁收口/心跳超时线程）
//==============================================================================

namespace gateway {

namespace {

// 库面日志出口（server_detail 单定义，本单元转发——定义在 gateway_server.cpp）
apollo::core::log::Logger& glog() {
    return server_detail::glog();
}

std::vector<uint8_t> encodeDisconnectMessage(SessionID sessionId, PlayerID playerId, bool normalClose) {
    apollo::protocol::GatewayClientDisconnect message;
    message.sessionId = sessionId;
    message.playerId = playerId;
    message.normalClose = normalClose;
    return apollo::protocol::MessageCodec::encode(message);
}
} // namespace

void GatewayServer::onConnectionOpened(ConnectionID connectionId, const ClientEndpoint& endpoint) {
    // 以 ingress 分配的连接号登记（只 attach 不 register 是桩时代死代码
    // 的暗坑——首包即被 registry 查空落地）。
    connectionRegistry_->registerConnection(connectionId, endpoint);
    const auto sessionId = sessionManager_->createSession(endpoint.address, endpoint.port, connectionId);
    connectionRegistry_->attachSession(connectionId, sessionId);
}

void GatewayServer::onPacketReceived(ConnectionID connectionId, std::span<const std::uint8_t> payload) {
    connectionRegistry_->touch(connectionId);

    const auto connection = connectionRegistry_->getConnection(connectionId);
    if (!connection || connection->sessionId == 0) {
        return;
    }

    auto sessionId = connection->sessionId;
    bool haveHeader = false;
    apollo::protocol::MessageHeader header{};
    if (payload.size() >= sizeof(apollo::protocol::MessageHeader)) {
        // 会话连续（L53）：上行帧头携异地会话号 = 重连恢复尝试（客户端记
        // 住旧会话号，新连接首包带上）。信封残缺/魔数不符 = 按无恢复尝试
        // 处理（分发器另行按 Invalid 落地），入口不因畸形头抛穿。
        try {
            header = apollo::protocol::MessageCodec::parseHeader(payload.data(), payload.size());
            haveHeader = true;
            if (header.sessionId != 0 && header.sessionId != sessionId) {
                tryResumeSession(sessionId, header.sessionId, connectionId);
                const auto rebound = connectionRegistry_->getConnection(connectionId);
                if (rebound) {
                    sessionId = rebound->sessionId;
                }
            }
        } catch (const std::exception&) {
            // 畸形信封：不进恢复逻辑与序号闸（分发器另行按 Invalid 落地）
        }
    }

    // 上行重复/乱序规则（L51）：恢复解析后按最终会话过序号闸——重放/重复
    // 帧（≤ 水位）不再分发到业务（计数在 SessionManager）；乱序空档按流
    // 传输语义容忍。恢复重绑发生在闸前，故旧会话号的旧序重放落回原会话
    // 水位上判定。
    if (haveHeader && !sessionManager_->acceptUplinkSequence(sessionId, header.sequence)) {
        glog().warning("Uplink sequence gate dropped session " + std::to_string(sessionId)
                       + " sequence=" + std::to_string(header.sequence));
        return;
    }

    handleClientMessage(
        sessionId,
        std::vector<std::uint8_t>(payload.begin(), payload.end())
    );
}

void GatewayServer::tryResumeSession(SessionID freshSessionId, SessionID retainedSessionId,
                                     ConnectionID connectionId) {
    const auto fresh = sessionManager_->getSession(freshSessionId);
    if (!fresh || fresh->playerId != 0 || freshSessionId == retainedSessionId) {
        return;  // 本连接已认证（防抢线）或同号回显：非恢复语义
    }

    if (!sessionManager_->resumeSession(retainedSessionId, connectionId,
                                        config_.sessionRetentionMs)) {
        glog().info("Gateway session resume rejected for session "
                    + std::to_string(retainedSessionId) +
                    " (not retained, window expired, or already resumed)");
        return;  // 落回 fresh 会话 = 完整重登路径
    }

    sessionManager_->removeSession(freshSessionId);
    connectionRegistry_->attachSession(connectionId, retainedSessionId);

    const auto resumed = sessionManager_->getSession(retainedSessionId);
    glog().info("Gateway resumed session " + std::to_string(retainedSessionId) +
                " for player " + std::to_string(resumed ? resumed->playerId : 0) +
                " on new connection " + std::to_string(connectionId));
}

void GatewayServer::onConnectionClosed(ConnectionID connectionId, DisconnectReason reason) {
    const auto connection = connectionRegistry_->getConnection(connectionId);
    if (!connection) {
        return;
    }

    if (connection->sessionId != 0) {
        onClientDisconnect(connection->sessionId, reason == DisconnectReason::NormalClose);
        return;
    }

    connectionRegistry_->markClosed(connectionId);
    connectionRegistry_->removeConnection(connectionId);
}

void GatewayServer::onConnectionIdle(ConnectionID connectionId) {
    connectionRegistry_->markIdle(connectionId);
}

void GatewayServer::heartbeatCheckLoop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.heartbeatCheckIntervalMs));

        // 检查超时会话
        auto timeoutSessions = sessionManager_->checkTimeouts(config_.sessionTimeoutMs);

        for (auto sessionId : timeoutSessions) {
            ++heartbeatTimeoutClosures_;
            glog().info("Session " + std::to_string(sessionId)
                        + " timeout, disconnecting...");
            onClientDisconnect(sessionId, false);
        }

        // 会话连续（L53）：保留窗到期销毁——延迟的断线通知此时才下发后端。
        for (auto sessionId : sessionManager_->checkRetentions(config_.sessionRetentionMs)) {
            glog().info("Session " + std::to_string(sessionId)
                        + " reconnect window expired, disconnecting...");
            destroySessionAndNotify(sessionId, false);
        }
    }
}

void GatewayServer::onClientDisconnect(SessionID sessionId, bool normalClose) {
    auto session = sessionManager_->getSession(sessionId);
    if (!session) return;

    // 会话连续（L53 断线重连）：已认证会话掉线进保留窗——连接拆掉、会话
    // 留存（playerId/路由快照/会话号原样），后端断线通知延到窗到期才下发
    // （窗内恢复即无通知；Zone 侧 907/908 resume 握手随 world 链路批接线，
    // 本批为 gateway 本地面）。未认证会话无可保留（重连即全新会话），
    // retention=0 关窗回旧语义（掉线即销毁）。
    if (config_.sessionRetentionMs > 0) {
        const auto connectionId = session->connectionId;  // 先取：mark 后置 0
        if (sessionManager_->markDisconnected(sessionId)) {
            glog().info("Session " + std::to_string(sessionId)
                        + " disconnected, retained for reconnect window");
            if (connectionId != 0) {
                if (ingressServer_) {
                    ingressServer_->close(connectionId,
                                          normalClose ? DisconnectReason::NormalClose
                                                      : DisconnectReason::IdleTimeout);
                }
                connectionRegistry_->markClosed(connectionId);
                connectionRegistry_->removeConnection(connectionId);
            }
            return;
        }
    }

    destroySessionAndNotify(sessionId, normalClose);
}

void GatewayServer::destroySessionAndNotify(SessionID sessionId, bool normalClose) {
    auto session = sessionManager_->getSession(sessionId);
    if (!session) return;

    const auto endpoint = connectionRegistry_->findEndpointBySession(sessionId);
    const auto clientAddress = endpoint.has_value() ? endpoint->address : session->clientIP;
    const auto clientPort = endpoint.has_value() ? endpoint->port : session->clientPort;

    glog().info("Client " + clientAddress + ":" + std::to_string(clientPort)
                + " disconnected (" + (normalClose ? "normal" : "timeout") + ")");

    // 通知后端服务
    auto data = encodeDisconnectMessage(sessionId, session->playerId, normalClose);

    if (session->state == SessionState::IN_GAME) {
        messageRouter_->forwardToWorld(sessionId, session->routeSnapshot, data);
        messageRouter_->forwardToBaseApp(sessionId, data);
    }

    if (session->connectionId != 0) {
        // 会话没了，TCP 连接也须真断（心跳超时路径尤甚——不断则成僵尸
        // 连接，包包落地）；对已断连接是迟到操作，入口侧落地丢弃。
        if (ingressServer_) {
            ingressServer_->close(session->connectionId,
                                  normalClose ? DisconnectReason::NormalClose
                                              : DisconnectReason::IdleTimeout);
        }
        connectionRegistry_->markClosed(session->connectionId);
        connectionRegistry_->removeConnection(session->connectionId);
    }

    // 移除会话
    sessionManager_->removeSession(sessionId);
}

} // namespace gateway
