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
std::vector<std::uint8_t> LoginServer::dispatchRequest(const std::vector<std::uint8_t>& request) {
    if (request.size() < sizeof(protocol::MessageHeader)) return {};
    try {
    const auto header = protocol::MessageCodec::parseHeader(request);
    switch (static_cast<protocol::MessageType>(header.type)) {
    case protocol::MessageType::LOGIN_REQUEST: return handleLoginRequest(request);
    case protocol::MessageType::GATEWAY_ASSIGN_REQUEST: return handleGatewayAssignRequest(request);
    case protocol::MessageType::PING: return handlePing(request);
    default: {
        protocol::ErrorMessage error;
        error.message = "Unsupported Login request";
        return protocol::MessageCodec::encode(error, header.sessionId);
    }
    }
    } catch (const std::exception&) {
        protocol::ErrorMessage error;
        error.message = "Malformed Login request";
        return protocol::MessageCodec::encode(error, 0);
    }
}

std::vector<uint8_t> LoginServer::handleLoginRequest(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto loginReq = protocol::MessageCodec::decodeBody<protocol::LoginRequest>(bodyData);

    protocol::LoginResponse response;
    response.success = false;
    response.sessionId = 0;
    response.playerId = 0;

    // 认证
    PlayerID playerId = 0;
    std::string errorMessage;
    if (authenticator_->authenticate(loginReq.username, loginReq.password, playerId, errorMessage)) {
        const auto gatewayUrl = gatewayAllocator_->selectGateway();
        if (gatewayUrl.empty()) {
            response.errorMessage = "No gateway available";
            return protocol::MessageCodec::encode(response, header.sessionId);
        }

        std::string gatewayHost;
        uint16_t gatewayPort = 0;
        if (!parseGatewayEndpoint(gatewayUrl, gatewayHost, gatewayPort)) {
            response.errorMessage = "Invalid gateway endpoint: " + gatewayUrl;
            return protocol::MessageCodec::encode(response, header.sessionId);
        }

        std::string loginTicket;
        const auto sessionId = sessionManager_->createSession(playerId, gatewayUrl, loginTicket);
        if (!preparePlayerOnline(playerId, sessionId, gatewayUrl, errorMessage)) {
            sessionManager_->removeSession(sessionId);
            response.errorMessage = errorMessage;
            return protocol::MessageCodec::encode(response, header.sessionId);
        }

        response.success = true;
        response.playerId = playerId;
        response.sessionId = sessionId;
        response.gatewayHost = gatewayHost;
        response.gatewayPort = gatewayPort;
        response.loginTicket = loginTicket;

        apollo::core::log::global_log_manager()
            .createLogger("login_server")
            ->info("Player " + loginReq.username + " (ID: " + std::to_string(playerId)
                   + ") logged in, session: " + std::to_string(response.sessionId)
                   + ", gateway: " + gatewayUrl);
    } else {
        response.errorMessage = errorMessage;
        apollo::core::log::global_log_manager()
            .createLogger("login_server")
            ->warning("Failed login attempt for " + loginReq.username + ": "
                      + errorMessage);
    }

    return protocol::MessageCodec::encode(response, header.sessionId);
}

std::vector<uint8_t> LoginServer::handleGatewayAssignRequest(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto assignReq = protocol::MessageCodec::decodeBody<protocol::GatewayAssignRequest>(bodyData);

    protocol::GatewayAssignResponse response;
    response.success = false;

    // 准入跨端校验（L174）：gateway 会话号与 login 会话号分属两个编号空间，
    // 以 ticket 反查 login 会话（会话号仅追踪用）；playerId 以反查结果为准
    // （ticket 绑定玩家，防请求侧伪报 playerId）。
    PlayerID playerId = 0;
    SessionID loginSessionId = 0;
    std::string gatewayUrl;
    if (sessionManager_->validateSessionByTicket(assignReq.loginTicket,
                                                 loginSessionId, playerId, gatewayUrl)) {
        response.success = true;
        response.token = assignReq.loginTicket;
        if (!parseGatewayEndpoint(gatewayUrl, response.gatewayHost, response.gatewayPort)) {
            response.success = false;
            response.errorMessage = "Invalid gateway endpoint";
        }
    } else {
        response.errorMessage = "Invalid or expired login ticket";
    }

    return protocol::MessageCodec::encode(response, header.sessionId);
}

std::vector<uint8_t> LoginServer::handlePing(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto ping = protocol::MessageCodec::decodeBody<protocol::Ping>(bodyData);

    protocol::Pong pong;
    pong.timestamp = ping.timestamp;

    return protocol::MessageCodec::encode(pong, header.sessionId);
}

} // namespace login
