#include "gateway/gateway_server.hpp"
#include "gateway/ingress/client_ingress_server.hpp"
#include "player_store/business_wire.hpp"
#include "player_store/m1_server.hpp"
#include "store_host.hpp"
#include "world_app/world_app.hpp"
#include "apollo/game/base/avatar_reward_sink.hpp"
#include "apollo/net/link/frame.hpp"
#include "apollo/net/ws/frame.hpp"
#include "apollo/net/ws/handshake.hpp"
#include "apollo/core/log/log_manager.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <array>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <vector>
#include "gateway/ingress/gateway_connection_registry.hpp"
#include "gateway/ingress/client_packet_dispatcher.hpp"
#include "gateway/session_manager.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"

#include <iostream>

namespace {

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace netlink = apollo::net::link;
namespace ws = apollo::net::ws;

const auto kTick = std::chrono::milliseconds(2);

template <typename Pred>
bool wait_until(Pred pred, int timeoutMs) {
    for (int waited = 0; waited < timeoutMs; waited += 2) {
        if (pred()) return true;
        std::this_thread::sleep_for(kTick);
    }
    return pred();
}

std::uint16_t probe_free_port() {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    std::uint16_t port = 0;
    if (fd >= 0) {
        ::sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = 0;
        ::inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
        socklen_t length = sizeof address;
        if (::bind(fd, reinterpret_cast<::sockaddr*>(&address), sizeof address) == 0 &&
            ::getsockname(fd, reinterpret_cast<::sockaddr*>(&address), &length) == 0) {
            port = ntohs(address.sin_port);
        }
        ::close(fd);
    }
    return port;
}

std::vector<std::uint8_t> frame_bytes(std::uint32_t sequence, std::uint8_t flags,
                                      const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> frame;
    netlink::append_frame(frame, sequence, flags, payload);
    return frame;
}

std::uint64_t header_session_id(const std::vector<std::uint8_t>& payload) {
    apollo::protocol::MessageHeader header{};
    std::memcpy(&header, payload.data(), sizeof header);
    return header.sessionId;
}

std::vector<std::uint8_t> ping_packet(std::uint64_t sessionId, std::uint64_t timestamp) {
    apollo::protocol::Ping ping;
    ping.timestamp = timestamp;
    return apollo::protocol::MessageCodec::encode(ping, sessionId);
}

std::vector<std::uint8_t> activate_packet(std::uint64_t sessionId, std::uint64_t playerId) {
    apollo::protocol::PlayerActivateRequest request;
    request.playerId = playerId;
    return apollo::protocol::MessageCodec::encode(request, sessionId);
}

const char* kClientKey = "dGhlIHNhbXBsZSBub25jZQ==";

std::vector<std::uint8_t> http_upgrade_request() {
    const std::string request = std::string("GET /game HTTP/1.1\r\n"
        "Host: localhost\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
        "Sec-WebSocket-Key: ") + std::string(kClientKey) +
        "\r\nSec-WebSocket-Version: 13\r\n\r\n";
    return {request.begin(), request.end()};
}

void append_masked_frame(std::vector<std::uint8_t>& out, ws::OpCode opcode,
                         const std::vector<std::uint8_t>& payload, std::uint8_t maskKey) {
    out.push_back(0x80 | static_cast<std::uint8_t>(opcode));
    out.push_back(0x80 | static_cast<std::uint8_t>(payload.size()));
    (void)maskKey;
    for (int i = 0; i < 4; ++i) out.push_back(0x5a);
    for (std::size_t i = 0; i < payload.size(); ++i) {
        out.push_back(payload[i] ^ maskKey);
    }
}

class TcpTestClient {
public:
    ~TcpTestClient() { close(); }
    bool connect(std::uint16_t port) {
        fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd_ < 0) return false;
        timeval timeout{};
        timeout.tv_sec = 2;
        ::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
        ::sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        ::inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
        if (::connect(fd_, reinterpret_cast<::sockaddr*>(&address), sizeof address) != 0) {
            ::close(fd_);
            fd_ = -1;
            return false;
        }
        return true;
    }
    void sendAll(const std::vector<std::uint8_t>& bytes) {
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            const auto count = ::send(fd_, bytes.data() + offset, bytes.size() - offset, 0);
            if (count <= 0) {
                if (count < 0 && errno == EINTR) continue;
                return;
            }
            offset += static_cast<std::size_t>(count);
        }
    }
    bool readGameFrame(std::vector<std::uint8_t>& payload) {
        std::vector<netlink::Frame> gameFrames;
        for (;;) {
            pollfd descriptor{};
            descriptor.fd = fd_;
            descriptor.events = POLLIN;
            const int ready = ::poll(&descriptor, 1, 200);
            if (ready <= 0) {
                return false;
            }
            std::array<std::uint8_t, 4096> chunk{};
            ssize_t count = 0;
            do {
                count = ::recv(fd_, chunk.data(), chunk.size(), MSG_DONTWAIT);
            } while (count < 0 && errno == EINTR);
            if (count <= 0) return false;
            auto outcome = parser_.feed(chunk.data(), static_cast<std::size_t>(count));
            if (outcome.fatal || outcome.frames.empty()) continue;
            for (auto& parsed : outcome.frames) {
                if ((parsed.flags & netlink::kFlagControl) == 0) {
                    gameFrames.push_back(std::move(parsed));
                }
            }
            if (!gameFrames.empty()) {
                payload = std::move(gameFrames.front().payload);
                gameFrames.erase(gameFrames.begin());
                return true;
            }
        }
    }
    void close() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }
    int fd() const { return fd_; }
private:
    int fd_ = -1;
    netlink::FrameParser parser_;
};

class WsTestClient {
public:
    ~WsTestClient() { close(); }
    bool connect(std::uint16_t port) {
        if (!tcp_.connect(port)) return false;
        sendAll(http_upgrade_request());
        for (int waited = 0; waited < 500; waited += 2) {
            if (recvHttpResponse()) return true;
            std::this_thread::sleep_for(kTick);
        }
        return false;
    }
    void sendAll(const std::vector<std::uint8_t>& bytes) {
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            const auto count = ::send(fd(), bytes.data() + offset, bytes.size() - offset, 0);
            if (count <= 0) {
                if (count < 0 && errno == EINTR) continue;
                return;
            }
            offset += static_cast<std::size_t>(count);
        }
    }
    void sendGameFrame(const std::vector<std::uint8_t>& payload) {
        std::vector<std::uint8_t> frame;
        append_masked_frame(frame, ws::OpCode::kBinary, payload, 0x5a);
        sendAll(frame);
    }
    bool readGameFrame(std::vector<std::uint8_t>& payload) {
        for (;;) {
            if (parseOneGameFrame(payload)) return true;
            pollfd descriptor{};
            descriptor.fd = fd();
            descriptor.events = POLLIN;
            const int ready = ::poll(&descriptor, 1, 200);
            if (ready <= 0) return false;
            std::array<std::uint8_t, 4096> chunk{};
            ssize_t count = 0;
            do {
                count = ::recv(fd(), chunk.data(), chunk.size(), MSG_DONTWAIT);
            } while (count < 0 && errno == EINTR);
            if (count <= 0) return false;
            buffer_.insert(buffer_.end(), chunk.begin(), chunk.begin() + count);
        }
    }
    void close() { tcp_.close(); }
private:
    bool recvHttpResponse() {
        constexpr std::string_view terminator = "\r\n\r\n";
        std::array<std::uint8_t, 4096> chunk{};
        while (buffer_.size() < 4 ||
               std::search(buffer_.begin(), buffer_.end(), terminator.begin(), terminator.end()) ==
                   buffer_.end()) {
            const auto count = ::recv(fd(), chunk.data(), chunk.size(), MSG_DONTWAIT);
            if (count <= 0) {
                if (count < 0 && errno == EAGAIN) return false;
                return false;
            }
            buffer_.insert(buffer_.end(), chunk.begin(), chunk.begin() + count);
        }
        const auto end = std::search(buffer_.begin(), buffer_.end(),
                                     terminator.begin(), terminator.end());
        const bool upgraded = std::string(buffer_.begin(), end).find(" 101 ") != std::string::npos;
        buffer_.clear();
        return upgraded;
    }
    int fd() const { return tcp_.fd(); }
    TcpTestClient tcp_;
    std::vector<std::uint8_t> buffer_;

    bool parseOneGameFrame(std::vector<std::uint8_t>& payload) {
        if (buffer_.size() < 2 || (buffer_[1] & 0x80) != 0) return false;
        const std::size_t len7 = buffer_[1] & 0x7f;
        std::size_t header = 2;
        std::size_t length = len7;
        if (len7 == 126) {
            if (buffer_.size() < 4) return false;
            length = (static_cast<std::size_t>(buffer_[2]) << 8) | buffer_[3];
            header = 4;
        } else if (len7 == 127) {
            if (buffer_.size() < 10) return false;
            length = 0;
            for (std::size_t offset = 2; offset < 10; ++offset) {
                length = (length << 8) | buffer_[offset];
            }
            header = 10;
        }
        if (buffer_.size() < header + length) return false;

        netlink::FrameParser gameFrames;
        const auto outcome = gameFrames.feed(
            buffer_.data() + static_cast<long>(header), length);
        buffer_.erase(buffer_.begin(),
                      buffer_.begin() + static_cast<long>(header + length));
        if (outcome.fatal || outcome.frames.empty()) return false;
        payload = std::move(outcome.frames.front().payload);
        return true;
    }
};
#endif

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

bool test_gateway_route_snapshot_lifecycle() {
    std::cout << "Running: test_gateway_route_snapshot_lifecycle..." << std::endl;

    gateway::SessionManager manager;
    const auto sessionId = manager.createSession("127.0.0.1", 5000);

    gateway::RouteSnapshot routeSnapshot;
    routeSnapshot.worldId = 2;
    routeSnapshot.mapId = 101;
    routeSnapshot.instanceId = 202;
    routeSnapshot.spaceId = 303;
    routeSnapshot.routeVersion = 12;
    routeSnapshot.worldServerUrl = "tcp://127.0.0.1:9100";

    auto session = manager.bindPlayerAndRoute(sessionId, 1001, routeSnapshot);
    TEST_ASSERT(session != nullptr, "session exists after bind");
    TEST_ASSERT(session->playerId == 1001, "player bound");
    TEST_ASSERT(session->state == gateway::SessionState::IN_GAME, "in game after route assigned");
    TEST_ASSERT(session->routeSnapshot.routeVersion == 12, "route version stored");
    TEST_ASSERT(session->routeSnapshot.worldServerUrl == "tcp://127.0.0.1:9100", "route url stored");

    manager.clearRoute(sessionId);
    session = manager.getSession(sessionId);
    TEST_ASSERT(session != nullptr, "session still exists");
    TEST_ASSERT(session->state == gateway::SessionState::AUTHENTICATED, "back to authenticated after route clear");
    TEST_ASSERT(!session->routeSnapshot.isAssigned(), "route cleared");

    manager.assignRoute(sessionId, routeSnapshot);
    session = manager.getSession(sessionId);
    TEST_ASSERT(session->state == gateway::SessionState::IN_GAME, "in game after route reassigned");
    TEST_ASSERT(session->routeSnapshot.worldId == 2, "world id restored");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_gateway_uplink_sequence_gate() {
    std::cout << "Running: test_gateway_uplink_sequence_gate..." << std::endl;

    gateway::SessionManager manager;
    const auto sessionId = manager.createSession("127.0.0.1", 5000);
    manager.bindPlayer(sessionId, 1001);

    TEST_ASSERT(manager.duplicateUplinkFrames() == 0, "no duplicates yet");

    // 零序 = 无序占位直通：受理且不推水位、不计数。
    TEST_ASSERT(manager.acceptUplinkSequence(sessionId, 0), "zero-sequence passes");
    TEST_ASSERT(manager.duplicateUplinkFrames() == 0, "zero-sequence not counted");

    // 正常前进受理；同序/旧序重放拒收并计数；跳空视为流丢帧空洞容忍。
    TEST_ASSERT(manager.acceptUplinkSequence(sessionId, 10), "first forward accepted");
    TEST_ASSERT(!manager.acceptUplinkSequence(sessionId, 10), "same sequence gated");
    TEST_ASSERT(manager.acceptUplinkSequence(sessionId, 11), "next sequence accepted");
    TEST_ASSERT(manager.acceptUplinkSequence(sessionId, 20), "forward gap tolerated");
    TEST_ASSERT(!manager.acceptUplinkSequence(sessionId, 15), "stale sequence gated");
    TEST_ASSERT(manager.duplicateUplinkFrames() == 2, "both duplicates counted");

    // 零序在水位已推进后仍直通：占位帧不参与去重。
    TEST_ASSERT(manager.acceptUplinkSequence(sessionId, 0), "zero-sequence still passes");
    TEST_ASSERT(manager.duplicateUplinkFrames() == 2, "zero-sequence never counted");

    // 未知会话：不越权吞帧（分发层自会拒）。
    TEST_ASSERT(manager.acceptUplinkSequence(9999, 10), "unknown session not gated here");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_gateway_authenticate_session() {
    std::cout << "Running: test_gateway_authenticate_session..." << std::endl;

    gateway::GatewayConfig config;
    gateway::GatewayServer server(config);

    const auto sessionId = server.createPendingSession("127.0.0.1", 6000);
    TEST_ASSERT(sessionId != 0, "pending session created");
    TEST_ASSERT(server.getRegisteredConnectionCount() == 1, "pending session registered a connection");

    const auto endpoint = server.findClientEndpoint(sessionId);
    TEST_ASSERT(endpoint.has_value(), "endpoint resolved from connection registry");
    TEST_ASSERT(endpoint->address == "127.0.0.1", "endpoint address preserved");
    TEST_ASSERT(endpoint->port == 6000, "endpoint port preserved");

    TEST_ASSERT(
        server.authenticateSession(sessionId, 1001, "ticket-abc123"),
        "session authenticated with ticket"
    );

    TEST_ASSERT(
        !server.authenticateSession(sessionId, 1001, ""),
        "empty ticket rejected"
    );

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_gateway_connection_registry_bind_and_remove() {
    std::cout << "Running: test_gateway_connection_registry_bind_and_remove..." << std::endl;

    gateway::GatewayConnectionRegistry registry;
    const auto connectionId = registry.registerConnection({"10.0.0.8", 7000});
    TEST_ASSERT(connectionId != 0, "connection id allocated");
    TEST_ASSERT(registry.getConnectionCount() == 1, "connection stored");
    TEST_ASSERT(registry.attachSession(connectionId, 88), "session attached");

    const auto endpoint = registry.findEndpointBySession(88);
    TEST_ASSERT(endpoint.has_value(), "session endpoint resolved");
    TEST_ASSERT(endpoint->address == "10.0.0.8", "session endpoint address");
    TEST_ASSERT(endpoint->port == 7000, "session endpoint port");

    TEST_ASSERT(registry.markClosed(connectionId), "connection closed");
    TEST_ASSERT(registry.removeConnection(connectionId), "connection removed");
    TEST_ASSERT(registry.getConnectionCount() == 0, "connection registry empty after removal");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_client_packet_dispatcher_routes_message_kinds() {
    std::cout << "Running: test_client_packet_dispatcher_routes_message_kinds..." << std::endl;

    gateway::ClientPacketDispatcher dispatcher;

    apollo::protocol::Ping ping;
    ping.timestamp = 42;
    const auto pingEncoded = apollo::protocol::MessageCodec::encode(ping, 7);
    const auto pingResult = dispatcher.dispatch(
        std::span<const std::uint8_t>(pingEncoded.data(), pingEncoded.size()));
    TEST_ASSERT(pingResult.kind == gateway::ClientPacketKind::Heartbeat, "ping classified as heartbeat");

    apollo::protocol::ChatMessage chat;
    chat.sessionId = 8;
    chat.playerId = 1001;
    chat.playerName = "tester";
    chat.channel = apollo::protocol::ChatChannel::PRIVATE;
    chat.content = "hello";
    const auto chatEncoded = apollo::protocol::MessageCodec::encode(chat, 8);
    const auto chatResult = dispatcher.dispatch(
        std::span<const std::uint8_t>(chatEncoded.data(), chatEncoded.size()));
    TEST_ASSERT(chatResult.kind == gateway::ClientPacketKind::Chat, "chat classified as chat");

    apollo::protocol::CombatSkillCast cast;
    cast.casterId = 1;
    cast.targetId = 2;
    cast.skillId = 3;
    cast.casterPos = apollo::protocol::Position(1.0f, 2.0f, 3.0f);
    cast.targetPos = apollo::protocol::Position(4.0f, 5.0f, 6.0f);
    const auto worldEncoded = apollo::protocol::MessageCodec::encode(cast, 9);
    const auto worldResult = dispatcher.dispatch(
        std::span<const std::uint8_t>(worldEncoded.data(), worldEncoded.size()));
    TEST_ASSERT(worldResult.kind == gateway::ClientPacketKind::World, "combat classified as world");

    apollo::protocol::PlayerActivateRequest activate;
    activate.playerId = 1001;
    const auto baseEncoded = apollo::protocol::MessageCodec::encode(activate, 10);
    const auto baseResult = dispatcher.dispatch(
        std::span<const std::uint8_t>(baseEncoded.data(), baseEncoded.size()));
    TEST_ASSERT(baseResult.kind == gateway::ClientPacketKind::Base, "activate classified as base");

    const std::vector<std::uint8_t> invalid{1, 2, 3};
    const auto invalidResult = dispatcher.dispatch(
        std::span<const std::uint8_t>(invalid.data(), invalid.size()));
    TEST_ASSERT(invalidResult.kind == gateway::ClientPacketKind::Invalid, "short payload invalid");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_router_degrades_when_backends_unreachable() {
    std::cout << "Running: test_router_degrades_when_backends_unreachable..." << std::endl;

    // P3-4 处方①③回归：后端全缺位不 throw——gateway 起得来（exit=0），缺位面降级
    gateway::GatewayConfig config;
    config.loginAppUrl = "tcp://127.0.0.1:1";
    config.worldUrl = "tcp://127.0.0.1:1";

    gateway::MessageRouter router(config);
    bool threw = false;
    try {
        router.start();
    } catch (...) {
        threw = true;
    }
    TEST_ASSERT(!threw, "router start does not throw with unreachable backends");

    // 降级面：world 节点不可用 → 默认路由仍指名但不可指派，转发静默落空不崩
    const auto route = router.buildDefaultRoute();
    TEST_ASSERT(route.worldServerUrl == config.worldUrl, "default route still names world url");

    router.forwardToWorld(1, route, {1, 2, 3});
    router.forwardToBaseApp(1, {1, 2, 3});
    router.stop();

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_tcp_and_ws_route_to_base_and_cell() {
    std::cout << "Running: test_tcp_and_ws_route_to_base_and_cell..." << std::endl;
#if defined(_WIN32)
    std::cout << "  SKIPPED (link M1 transports are POSIX-only)" << std::endl;
    return true;
#else
    world_app::WorldAppConfig cellConfig;
    cellConfig.host = "127.0.0.1";
    cellConfig.m1Port = 0;
    cellConfig.m1RouteJson = APOLLO_TEST_ROUTE_JSON;
    world_app::WorldApp cell(cellConfig);
    cell.start();
    TEST_ASSERT(cell.m1Link().listenPort() != 0, "cell M1 listener started");

    std::filesystem::path baseDataPath = std::filesystem::temp_directory_path() /
        ("apollo-gateway-route-base-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(baseDataPath / "players");
    std::filesystem::create_directories(baseDataPath / "journal");
    player_store::PlayerStoreConfig baseStoreConfig;
    baseStoreConfig.host = "127.0.0.1";
    baseStoreConfig.m1Enabled = true;
    baseStoreConfig.m1Port = 0;
    baseStoreConfig.m1SelfName = "base";
    baseStoreConfig.m1EndpointName = "gateway";
    baseStoreConfig.m1RouteJson = APOLLO_TEST_ROUTE_JSON;
    baseStoreConfig.dataDir = (baseDataPath / "players").string();
    baseStoreConfig.journalPath = (baseDataPath / "journal/persist.log").string();
    StoreHost base(baseStoreConfig, 0, 0, 0, 0);
    TEST_ASSERT(base.store->m1_listen_port() != 0, "base M1 listener started");
    TEST_ASSERT(base.store->adoptPlayer(707, "", 0, 0, 2), "seed tcp player profile");
    TEST_ASSERT(base.store->adoptPlayer(708, "", 0, 0, 2), "seed ws player profile");

    gateway::GatewayConfig config;
    config.host = "127.0.0.1";
    config.port = 0;
    config.baseM1Host = "127.0.0.1";
    config.baseM1Port = base.store->m1_listen_port();
    config.worldM1RouteJson = APOLLO_TEST_ROUTE_JSON;
    config.worldM1Host = "127.0.0.1";
    config.worldM1Port = cell.m1Link().listenPort();
    config.gatewayId = 83;
    config.wsPort = probe_free_port();
    config.sessionTimeoutMs = 600000;
    config.heartbeatCheckIntervalMs = 50;
    gateway::GatewayServer server(config);
    server.start();
    TEST_ASSERT(wait_until([&] { return server.baseActivationReady(); }, 3000),
                "base link ready");
    TEST_ASSERT(server.baseActivationReady(), "base link stays ready");

    TcpTestClient tcp;
    TEST_ASSERT(tcp.connect(server.listenPort()), "tcp connected");
    tcp.sendAll(frame_bytes(1, 0, ping_packet(0, 1)));
    std::vector<std::uint8_t> handshakeReply;
    TEST_ASSERT(tcp.readGameFrame(handshakeReply), "TCP handshake reply");
    const auto tcpSessionId = header_session_id(handshakeReply);
    TEST_ASSERT(tcpSessionId != 0, "TCP session allocated");
    const bool tcpAuthenticated =
        server.authenticateSession(tcpSessionId, 707, "ticket");
    TEST_ASSERT(tcpAuthenticated, "TCP session authenticated");

    WsTestClient ws;
    TEST_ASSERT(ws.connect(server.wsListenPort()), "ws upgraded");
    ws.sendGameFrame(frame_bytes(1, 0, ping_packet(0, 1)));
    std::vector<std::uint8_t> wsHandshakeReply;
    bool wsHandshakeReplied = false;
    for (int waited = 0; waited < 2000 && !wsHandshakeReplied; waited += 10) {
        wsHandshakeReplied = ws.readGameFrame(wsHandshakeReply);
        if (!wsHandshakeReplied) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    TEST_ASSERT(wsHandshakeReplied, "WS handshake reply");
    const auto wsSessionId = header_session_id(wsHandshakeReply);
    TEST_ASSERT(wsSessionId != 0 && wsSessionId != tcpSessionId, "WS session allocated");
    TEST_ASSERT(server.authenticateSession(wsSessionId, 708, "ticket"),
                "WS session authenticated");
    player_store::business::ScoreCreditRequest businessRequest;
    businessRequest.request_id = 81;
    businessRequest.player_id = 708;
    businessRequest.score = 12;
    businessRequest.source_zone_id = 7;
    ws.sendGameFrame(frame_bytes(
        2, 0, player_store::business::encodeFrame(businessRequest, wsSessionId)));

    apollo::protocol::CellCreateEntity createIntent;
    createIntent.entityId = 707;
    createIntent.entityType = apollo::protocol::EntityType::PLAYER;
    createIntent.spaceId = 1;
    createIntent.position = apollo::protocol::Position(1.0f, 2.0f, 3.0f);
    const auto cellFrame = apollo::protocol::MessageCodec::encode(
        createIntent, tcpSessionId);
    tcp.sendAll(frame_bytes(2, 0, cellFrame));

    player_store::business::ScoreCreditRequest tcpBusinessRequest;
    tcpBusinessRequest.request_id = 82;
    tcpBusinessRequest.player_id = 707;
    tcpBusinessRequest.score = 12;
    tcpBusinessRequest.source_zone_id = 7;
    tcp.sendAll(frame_bytes(3, 0, player_store::business::encodeFrame(
        tcpBusinessRequest, tcpSessionId)));

    player_store::business::ScoreCreditReply tcpBusinessReply;
    player_store::business::ScoreCreditReply wsBusinessReply;
    apollo::protocol::SessionID tcpBusinessSession = 0;
    apollo::protocol::SessionID wsBusinessSession = 0;
    std::vector<std::uint8_t> reply;
    bool tcpReplied = false;
    for (int waited = 0; waited < 5000 && !tcpReplied; waited += 10) {
        if (tcp.readGameFrame(reply) &&
            player_store::business::decodeFrame(
                reply, tcpBusinessReply, tcpBusinessSession)) {
            tcpReplied = true;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    bool wsReplied = false;
    for (int waited = 0; waited < 5000 && !wsReplied; waited += 10) {
        if (ws.readGameFrame(reply) &&
            player_store::business::decodeFrame(
                reply, wsBusinessReply, wsBusinessSession)) {
            wsReplied = true;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    TEST_ASSERT(tcpReplied, "TCP Base contract-959 reply reached the same session");
    TEST_ASSERT(wsReplied, "WS Base contract-959 reply reached its own session");
    TEST_ASSERT(tcpBusinessSession == tcpSessionId && wsBusinessSession == wsSessionId,
                "Base replies preserve transport session addressing");
    TEST_ASSERT(tcpBusinessReply.accepted && wsBusinessReply.accepted &&
                tcpBusinessReply.request_id == tcpBusinessRequest.request_id &&
                wsBusinessReply.request_id == businessRequest.request_id,
                "Base business handler accepted both score requests");
    const auto tcpAvatar = base.store->findAvatar(707);
    const auto wsAvatar = base.store->findAvatar(708);
    TEST_ASSERT(tcpAvatar && wsAvatar &&
                tcpAvatar->progress(apollo::game::base::AvatarRewardSink::kExpKey) == 12 &&
                wsAvatar->progress(apollo::game::base::AvatarRewardSink::kExpKey) == 12,
                "Base score requests reached authoritative anchors");
    auto* egress = server.messageRouter() ? server.messageRouter()->egress() : nullptr;
    TEST_ASSERT(egress != nullptr, "cell egress wired");
    TEST_ASSERT(wait_until([&] { return egress->ackedIntents() == 1; }, 3000),
                "cell intent acknowledged");
    TEST_ASSERT(cell.m1Link().receivedIntents() == 1,
                "cell intent reached the M1 egress");

    std::error_code cleanupError;
    std::filesystem::remove_all(baseDataPath, cleanupError);
    cell.stop();
    server.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
#endif
}

} // namespace

int main() {
    std::cout << "=== Apollo Gateway Route Test Suite ===" << std::endl;
    apollo::core::log::global_log_manager().initialize(
        apollo::core::log::LogManagerConfig::createConsoleOnly(
            apollo::core::log::LogLevel::Debug));

    int total = 0;
    int passed = 0;

    auto run = [&](bool (*fn)()) {
        total++;
        if (fn()) {
            passed++;
        }
    };

    run(test_gateway_route_snapshot_lifecycle);
    run(test_gateway_uplink_sequence_gate);
    run(test_gateway_authenticate_session);
    run(test_gateway_connection_registry_bind_and_remove);
    run(test_client_packet_dispatcher_routes_message_kinds);
    run(test_router_degrades_when_backends_unreachable);
    run(test_tcp_and_ws_route_to_base_and_cell);

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total: " << total << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << (total - passed) << std::endl;

    return total == passed ? 0 : 1;
}
