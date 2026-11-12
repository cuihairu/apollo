#include "login/login_server.hpp"
#include "apollo/core/log/log_manager.h"
#include "apollo/protocol/messages.hpp"
#include "apollo/protocol/codec.hpp"
#include <cstdint>
#include <iostream>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <filesystem>
#include "apollo/utils/crypto.h"

using apollo::utils::PasswordHash;

namespace login {
namespace {

// P4-4 批③换装缝（批⑤ nng 桩路径退役）：M1 面 = 唯一传输（全帧进出 +
// corr 关联归 linkrpc）。面装配缺位/失败一律 throw → preparePlayerOnline
// catch → 降级缝（桩时代同位：传输缺席/信封残缺 ≠ 会话事实）；仅响应
// success=false 才真拒。
constexpr std::uint32_t kM1BaseTimeoutMs = 500;  // 单调用同步等待上限

template <typename Req, typename Resp>
Resp callBaseRpc(M1BaseClient* m1, const Req& request, SessionID sessionId,
                 protocol::MessageType expect) {
    if (m1 == nullptr) {
        throw std::runtime_error("base M1 rpc face not assembled");
    }
    const auto response = m1->call(protocol::MessageCodec::encode(request, sessionId),
                                   std::chrono::milliseconds(kM1BaseTimeoutMs));
    if (!response) {
        throw std::runtime_error("base M1 rpc unreachable or timed out");
    }
    const auto header = protocol::MessageCodec::parseHeader(*response);
    if (static_cast<protocol::MessageType>(header.type) != expect) {
        throw std::runtime_error("base M1 rpc envelope mismatch");
    }
    const std::vector<std::uint8_t> body(response->begin() +
                                             sizeof(protocol::MessageHeader),
                                         response->end());
    Resp parsed;
    protocol::MessageCodec::decodeBody(body, parsed);
    return parsed;
}

} // namespace

bool LoginServer::preparePlayerOnline(
    PlayerID playerId,
    SessionID sessionId,
    const std::string& gatewayUrl,
    std::string& errorMessage
) {
    try {
        protocol::PlayerActivateRequest activateReq;
        activateReq.playerId = playerId;
        const auto activateResp =
            callBaseRpc<protocol::PlayerActivateRequest, protocol::PlayerActivateResponse>(
                worldStoreClient_.get(), activateReq, sessionId,
                protocol::MessageType::PLAYER_ACTIVATE_RESPONSE
            );

        if (!activateResp.success) {
            errorMessage = activateResp.errorMessage.empty()
                ? "BaseApp rejected player activation"
                : activateResp.errorMessage;
            return false;
        }

        protocol::PlayerBindSessionRequest bindReq;
        bindReq.playerId = playerId;
        bindReq.sessionId = sessionId;
        bindReq.gatewayId = 1;
        bindReq.gatewayAddr = gatewayUrl;
        bindReq.sessionVersion = 1;  // 登录即新会话首绑（重绑代次随网关重连面）

        const auto bindResp =
            callBaseRpc<protocol::PlayerBindSessionRequest, protocol::PlayerBindSessionResponse>(
                worldStoreClient_.get(), bindReq, sessionId,
                protocol::MessageType::PLAYER_BIND_SESSION_RESPONSE
            );

        if (!bindResp.success) {
            errorMessage = bindResp.errorMessage.empty()
                ? "BaseApp rejected session binding"
                : bindResp.errorMessage;
            return false;
        }

        if (directory_) {
            apollo::game::base::SessionBinding binding;
            binding.session_id = sessionId;
            binding.gateway_id = bindReq.gatewayId;
            binding.gateway_addr = gatewayUrl;
            binding.session_version = bindReq.sessionVersion;
            apollo::game::base::WorldAssignment assignment;
            assignment.world_id = config_.initialWorldId;
            assignment.map_id = config_.initialMapId;
            assignment.instance_id = config_.initialInstanceId;
            assignment.space_id = config_.initialSpaceId;
            assignment.route_version = 1;
            if (!directory_->bindSession(playerId, binding) || !directory_->assignWorld(playerId, assignment)) {
                errorMessage = "Login directory rejected admission";
                return false;
            }
            return true;
        }

        protocol::PlayerAssignWorldRequest assignReq;
        assignReq.playerId = playerId;
        assignReq.worldId = config_.initialWorldId;
        assignReq.mapId = config_.initialMapId;
        assignReq.instanceId = config_.initialInstanceId;
        assignReq.spaceId = config_.initialSpaceId;
        assignReq.routeVersion = 1;

        const auto assignResp =
            callBaseRpc<protocol::PlayerAssignWorldRequest, protocol::PlayerAssignWorldResponse>(
                m1OnlineDirectoryClient_.get(), assignReq, sessionId,
                protocol::MessageType::PLAYER_ASSIGN_WORLD_RESPONSE
            );

        if (!assignResp.success) {
            errorMessage = assignResp.errorMessage.empty()
                ? "BaseApp rejected initial world assignment"
                : assignResp.errorMessage;
            return false;
        }

        return true;
    } catch (const std::exception& ex) {
        // 未取得玩家数据和路由权威确认时，禁止签发可用的登录票据。
        apollo::core::log::global_log_manager()
            .createLogger("login_server")
            ->warning("Player admission failed for player "
                      + std::to_string(playerId) + ": " + ex.what());
        errorMessage = std::string("Player data or routing unavailable: ") + ex.what();
        return false;
    }
}

bool LoginServer::parseGatewayEndpoint(
    const std::string& gatewayUrl,
    std::string& outHost,
    uint16_t& outPort
) {
    constexpr const char* scheme = "tcp://";
    std::string endpoint = gatewayUrl;
    if (endpoint.rfind(scheme, 0) == 0) {
        endpoint.erase(0, 6);
    }

    const auto colon = endpoint.rfind(':');
    if (colon == std::string::npos) {
        return false;
    }

    outHost = endpoint.substr(0, colon);
    if (outHost.empty()) {
        return false;
    }

    try {
        const auto port = std::stoi(endpoint.substr(colon + 1));
        if (port < 0 || port > 65535) {
            return false;
        }
        outPort = static_cast<uint16_t>(port);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace login
