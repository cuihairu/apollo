#include "base/base_server.hpp"
#include "apollo/protocol/messages.hpp"
#include "apollo/protocol/codec.hpp"
#include <chrono>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace base {

namespace {

const char* toAnchorStateString(apollo::game::session::AnchorState state) {
    using apollo::game::session::AnchorState;

    switch (state) {
        case AnchorState::Loading:
            return "loading";
        case AnchorState::Online:
            return "online";
        case AnchorState::Transferring:
            return "transferring";
        case AnchorState::Disconnected:
            return "disconnected";
        case AnchorState::Saving:
            return "saving";
        case AnchorState::Offline:
            return "offline";
        default:
            return "unknown";
    }
}

} // namespace

//==============================================================================
// PlayerData 实现
//==============================================================================

std::string PlayerData::toJson() const {
    std::stringstream ss;
    ss << "{"
       << "\"playerId\":" << playerId << ","
       << "\"username\":\"" << username << "\","
       << "\"level\":" << level << ","
       << "\"exp\":" << exp << ","
       << "\"hp\":" << hp << ","
       << "\"maxHp\":" << maxHp << ","
       << "\"mp\":" << mp << ","
       << "\"maxMp\":" << maxMp << ","
       << "\"x\":" << x << ","
       << "\"y\":" << y << ","
       << "\"z\":" << z
       << "}";
    return ss.str();
}

PlayerData PlayerData::fromJson(const std::string& json) {
    PlayerData data;
    // 简化解析，实际应该用 nlohmann/json
    // 这里只做基础实现
    size_t pos = 0;

    auto extractString = [&json, &pos](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\":\"";
        size_t p = json.find(search, pos);
        if (p == std::string::npos) return "";
        p += search.length();
        size_t end = json.find("\"", p);
        if (end == std::string::npos) return "";
        std::string result = json.substr(p, end - p);
        pos = end + 1;
        return result;
    };

    auto extractInt = [&json, &pos](const std::string& key) -> int {
        std::string search = "\"" + key + "\":";
        size_t p = json.find(search, pos);
        if (p == std::string::npos) return 0;
        p += search.length();
        size_t end = json.find_first_of(",}", p);
        if (end == std::string::npos) return 0;
        pos = end + 1;
        return std::stoi(json.substr(p, end - p));
    };

    data.username = extractString("username");
    data.level = extractInt("level");
    data.exp = extractInt("exp");
    data.hp = extractInt("hp");
    data.maxHp = extractInt("maxHp");
    data.mp = extractInt("mp");
    data.maxMp = extractInt("maxMp");

    return data;
}

//==============================================================================
// BaseServer 实现
//==============================================================================

BaseServer::BaseServer(const BaseConfig& config)
    : config_(config)
    , database_(std::make_unique<DatabaseService>(config))
    , saveQueue_(std::make_unique<SaveQueue>(config_.workerThreads))
    , anchorManager_(std::make_shared<apollo::game::session::AnchorManager>())
    , sessionLocator_(std::make_shared<apollo::game::session::SessionLocator>()) {
}

BaseServer::~BaseServer() {
    stop();
}

std::shared_ptr<apollo::game::session::PlayerAnchor> BaseServer::activatePlayer(PlayerID playerId) {
    PlayerData data;
    if (!database_->loadPlayer(playerId, data)) {
        return nullptr;
    }

    auto anchor = activatePlayerAnchor(playerId);
    if (!anchor) {
        return nullptr;
    }

    anchor->clear_dirty();
    return anchor;
}

bool BaseServer::bindSession(
    PlayerID playerId,
    const apollo::game::session::SessionBinding& binding
) {
    auto anchor = anchorManager_->find(playerId);
    if (!anchor) {
        anchor = activatePlayer(playerId);
    }

    if (!anchor) {
        return false;
    }

    anchor->bind_session(binding);
    anchor->set_state(apollo::game::session::AnchorState::Online);
    sessionLocator_->bind(playerId, binding);
    return true;
}

bool BaseServer::unbindSession(protocol::SessionID sessionId) {
    const auto playerId = sessionLocator_->find_player_by_session(sessionId);
    if (!playerId.has_value()) {
        return false;
    }

    if (auto anchor = anchorManager_->find(*playerId)) {
        anchor->unbind_session(sessionId);
        anchor->set_state(apollo::game::session::AnchorState::Disconnected);
    }

    sessionLocator_->unbind_session(sessionId);
    return true;
}

bool BaseServer::assignWorld(
    PlayerID playerId,
    const apollo::game::session::WorldAssignment& assignment
) {
    auto anchor = anchorManager_->find(playerId);
    if (!anchor) {
        return false;
    }

    anchor->assign_world(assignment);
    if (assignment.is_assigned()) {
        anchor->set_state(apollo::game::session::AnchorState::Online);
    }
    return true;
}

bool BaseServer::clearWorldAssignment(PlayerID playerId) {
    auto anchor = anchorManager_->find(playerId);
    if (!anchor) {
        return false;
    }

    anchor->clear_world_assignment();
    return true;
}

std::shared_ptr<apollo::game::session::PlayerAnchor> BaseServer::findAnchor(PlayerID playerId) const {
    return anchorManager_->find(playerId);
}

std::optional<PlayerID> BaseServer::findPlayerBySession(protocol::SessionID sessionId) const {
    return sessionLocator_->find_player_by_session(sessionId);
}

std::optional<apollo::game::session::WorldAssignment> BaseServer::resolveWorldAssignment(
    PlayerID playerId,
    protocol::SessionID sessionId
) const {
    if (playerId == 0 && sessionId != 0) {
        const auto resolved = sessionLocator_->find_player_by_session(sessionId);
        if (!resolved.has_value()) {
            return std::nullopt;
        }
        playerId = *resolved;
    }

    const auto anchor = anchorManager_->find(playerId);
    if (!anchor) {
        return std::nullopt;
    }

    const auto& assignment = anchor->world_assignment();
    if (!assignment.is_assigned()) {
        return std::nullopt;
    }

    return assignment;
}

std::optional<apollo::game::session::SessionBinding> BaseServer::resolveSessionBinding(
    PlayerID playerId,
    protocol::SessionID sessionId
) const {
    if (sessionId != 0) {
        const auto binding = sessionLocator_->find_by_player(
            playerId != 0 ? playerId : sessionLocator_->find_player_by_session(sessionId).value_or(0));
        if (binding.has_value()) {
            return binding;
        }
    }

    if (playerId == 0) {
        return std::nullopt;
    }

    return sessionLocator_->find_by_player(playerId);
}

void BaseServer::start() {
    if (running_) return;

    // 初始化数据库
    if (!database_->initialize()) {
        throw std::runtime_error("Failed to initialize database");
    }

    // 启动保存队列
    saveQueue_->start();

    // 创建 RPC 服务器
    server_ = std::make_unique<protocol::RepSocket>(
        protocol::make_tcp_url(config_.host, config_.port)
    );

    server_->setRequestHandler([this](const std::vector<uint8_t>& data) -> std::vector<uint8_t> {
        auto header = protocol::MessageCodec::parseHeader(data);
        auto msgType = static_cast<protocol::MessageType>(header.type);

        switch (msgType) {
            case protocol::MessageType::DB_LOAD_REQUEST:
                return handleDbLoadRequest(data);

            case protocol::MessageType::DB_SAVE_REQUEST:
                return handleDbSaveRequest(data);

            case protocol::MessageType::DB_QUERY_REQUEST:
                return handleDbQueryRequest(data);

            case protocol::MessageType::PLAYER_ACTIVATE_REQUEST:
                return handlePlayerActivateRequest(data);

            case protocol::MessageType::PLAYER_BIND_SESSION_REQUEST:
                return handlePlayerBindSessionRequest(data);

            case protocol::MessageType::PLAYER_ASSIGN_WORLD_REQUEST:
                return handlePlayerAssignWorldRequest(data);

            case protocol::MessageType::PLAYER_RESOLVE_ROUTE_REQUEST:
                return handlePlayerResolveRouteRequest(data);

            case protocol::MessageType::PING:
                return handlePing(data);

            default:
                protocol::ErrorMessage err;
                err.code = static_cast<uint32_t>(protocol::MessageType::ERROR_MESSAGE);
                err.message = "Unknown message type";
                return protocol::MessageCodec::encode(err, header.sessionId);
        }
    });

    running_ = true;

    server_->start();

    // 启动自动保存线程
    autoSaveThread_ = std::thread(&BaseServer::autoSaveLoop, this);

    std::cout << "Base server listening on " << config_.host << ":" << config_.port << std::endl;
}

void BaseServer::stop() {
    running_ = false;
    if (server_) {
        server_->stop();
    }
    server_.reset();

    saveQueue_->stop();

    if (autoSaveThread_.joinable()) {
        autoSaveThread_.join();
    }

    database_->shutdown();
}

std::vector<uint8_t> BaseServer::handleDbLoadRequest(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto loadReq = protocol::MessageCodec::decodeBody<protocol::DbLoadRequest>(bodyData);

    protocol::DbLoadResponse response;
    response.success = false;

    PlayerData data;
    if (database_->loadPlayer(loadReq.playerId, data)) {
        auto anchor = activatePlayer(loadReq.playerId);
        response.success = true;
        response.jsonData = data.toJson();

        std::cout << "Loaded player " << loadReq.playerId << " data" << std::endl;
    } else {
        response.errorMessage = "Player not found";
    }

    return protocol::MessageCodec::encode(response, header.sessionId);
}

std::vector<uint8_t> BaseServer::handleDbSaveRequest(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto saveReq = protocol::MessageCodec::decodeBody<protocol::DbSaveRequest>(bodyData);

    protocol::DbSaveResponse response;
    response.success = false;

    // 异步保存
    SaveTask task;
    task.playerId = saveReq.playerId;
    task.data = PlayerData::fromJson(saveReq.jsonData);
    task.callback = [this, playerId = saveReq.playerId](bool success) {
        finalizeSave(playerId, success);
    };
    task.createdAtMs = getCurrentTimeMs();

    saveQueue_->enqueue(task);

    if (auto anchor = anchorManager_->find(saveReq.playerId)) {
        anchor->mark_dirty("db_save_request");
        anchor->set_state(apollo::game::session::AnchorState::Saving);
    }

    // 简化处理：直接返回成功（实际应该等保存完成）
    response.success = true;

    std::cout << "Queued save for player " << saveReq.playerId << std::endl;

    return protocol::MessageCodec::encode(response, header.sessionId);
}

std::vector<uint8_t> BaseServer::handleDbQueryRequest(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);

    // 简化处理
    protocol::DbQueryResponse response;
    response.success = false;
    response.errorMessage = "Not implemented";

    return protocol::MessageCodec::encode(response, header.sessionId);
}

std::vector<uint8_t> BaseServer::handlePlayerActivateRequest(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto activateReq = protocol::MessageCodec::decodeBody<protocol::PlayerActivateRequest>(bodyData);

    protocol::PlayerActivateResponse response;
    response.success = false;
    response.playerId = activateReq.playerId;

    if (auto anchor = activatePlayer(activateReq.playerId)) {
        response.success = true;
        response.state = toAnchorStateString(anchor->state());
    } else {
        response.errorMessage = "Failed to activate player anchor";
    }

    return protocol::MessageCodec::encode(response, header.sessionId);
}

std::vector<uint8_t> BaseServer::handlePlayerBindSessionRequest(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto bindReq = protocol::MessageCodec::decodeBody<protocol::PlayerBindSessionRequest>(bodyData);

    protocol::PlayerBindSessionResponse response;
    response.success = false;

    apollo::game::session::SessionBinding binding;
    binding.session_id = bindReq.sessionId;
    binding.gateway_id = bindReq.gatewayId;
    binding.gateway_addr = bindReq.gatewayAddr;
    binding.bind_time_ms = getCurrentTimeMs();

    if (bindSession(bindReq.playerId, binding)) {
        response.success = true;
    } else {
        response.errorMessage = "Failed to bind player session";
    }

    return protocol::MessageCodec::encode(response, header.sessionId);
}

std::vector<uint8_t> BaseServer::handlePlayerAssignWorldRequest(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto assignReq = protocol::MessageCodec::decodeBody<protocol::PlayerAssignWorldRequest>(bodyData);

    protocol::PlayerAssignWorldResponse response;
    response.success = false;
    response.routeVersion = assignReq.routeVersion;

    apollo::game::session::WorldAssignment assignment;
    assignment.world_id = assignReq.worldId;
    assignment.map_id = assignReq.mapId;
    assignment.instance_id = assignReq.instanceId;
    assignment.space_id = assignReq.spaceId;
    assignment.route_version = assignReq.routeVersion;

    if (assignWorld(assignReq.playerId, assignment)) {
        response.success = true;
    } else {
        response.errorMessage = "Failed to assign player world";
    }

    return protocol::MessageCodec::encode(response, header.sessionId);
}

std::vector<uint8_t> BaseServer::handlePlayerResolveRouteRequest(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    const auto resolveReq = protocol::MessageCodec::decodeBody<protocol::PlayerResolveRouteRequest>(bodyData);

    protocol::PlayerResolveRouteResponse response;
    response.success = false;
    response.playerId = resolveReq.playerId;
    response.sessionId = resolveReq.sessionId;

    const auto resolvedPlayerId = resolveReq.playerId != 0
        ? std::optional<PlayerID>(resolveReq.playerId)
        : findPlayerBySession(resolveReq.sessionId);

    if (!resolvedPlayerId.has_value()) {
        response.errorMessage = "Player not found for route resolution";
        return protocol::MessageCodec::encode(response, header.sessionId);
    }

    response.playerId = *resolvedPlayerId;

    const auto binding = resolveSessionBinding(*resolvedPlayerId, resolveReq.sessionId);
    const auto assignment = resolveWorldAssignment(*resolvedPlayerId, resolveReq.sessionId);
    if (!binding.has_value() || !assignment.has_value()) {
        response.errorMessage = "Player route is not ready";
        return protocol::MessageCodec::encode(response, header.sessionId);
    }

    response.success = true;
    response.sessionId = binding->session_id;
    response.gatewayId = binding->gateway_id;
    response.gatewayAddr = binding->gateway_addr;
    response.worldId = assignment->world_id;
    response.mapId = assignment->map_id;
    response.instanceId = assignment->instance_id;
    response.spaceId = assignment->space_id;
    response.routeVersion = assignment->route_version;

    return protocol::MessageCodec::encode(response, header.sessionId);
}

std::vector<uint8_t> BaseServer::handlePing(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto ping = protocol::MessageCodec::decodeBody<protocol::Ping>(bodyData);

    protocol::Pong pong;
    pong.timestamp = ping.timestamp;

    return protocol::MessageCodec::encode(pong, header.sessionId);
}

void BaseServer::autoSaveLoop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.autoSaveIntervalMs));

        // 定期保存缓存的数据
        // 这里简化处理
        std::cout << "Auto-save triggered" << std::endl;
    }
}

int64_t BaseServer::getCurrentTimeMs() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

std::shared_ptr<apollo::game::session::PlayerAnchor> BaseServer::activatePlayerAnchor(PlayerID playerId) {
    auto anchor = anchorManager_->activate(playerId);
    if (!anchor) {
        return nullptr;
    }

    if (anchor->state() == apollo::game::session::AnchorState::Loading) {
        anchor->set_state(apollo::game::session::AnchorState::Online);
    }

    return anchor;
}

void BaseServer::finalizeSave(PlayerID playerId, bool success) {
    auto anchor = anchorManager_->find(playerId);
    if (!anchor) {
        return;
    }

    if (success) {
        anchor->clear_dirty();
        anchor->set_state(apollo::game::session::AnchorState::Online);
        return;
    }

    anchor->mark_dirty("save_failed");
    anchor->set_state(apollo::game::session::AnchorState::Disconnected);
}

} // namespace base
