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
// BaseServer 实现
//==============================================================================

BaseServer::BaseServer(const BaseConfig& config)
    : config_(config)
    , database_(std::make_unique<DatabaseService>(config))
    , saveQueue_(std::make_unique<SaveQueue>(config_.workerThreads))
    , anchorManager_(std::make_shared<apollo::game::session::AnchorManager>())
    , sessionLocator_(std::make_shared<apollo::game::session::SessionLocator>()) {
    // P1-4 真链路：SaveQueue 出队 → DatabaseService 落盘（workerLoop
    // 此前只 callback(true) 不写任何介质）
    saveQueue_->set_worker([this](const PlayerData& data) {
        return database_->savePlayer(data);
    });
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

void BaseServer::set_directory_event_sink(
    apollo::game::session::PlayerDirectory::EventSink sink) {
    directoryEventSink_ = std::move(sink);
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

    // 目录事件源①（P1-2）：会话建立上报 SessionUp 同形事件（权威目录在
    // manager 域；sink 缺省静默，跨进程总线上报随 P3）
    if (directoryEventSink_) {
        apollo::game::session::PlayerDirectory::Event event;
        event.kind = apollo::game::session::PlayerDirectory::Event::Kind::SessionUp;
        event.player_id = playerId;
        event.binding = binding;
        if (anchor->world_assignment().is_assigned()) {
            event.assignment = anchor->world_assignment();
        }
        directoryEventSink_(event);
    }
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

    // 目录事件源①（P1-2）：会话消亡上报 SessionDown 同形事件
    if (directoryEventSink_) {
        apollo::game::session::PlayerDirectory::Event event;
        event.kind = apollo::game::session::PlayerDirectory::Event::Kind::SessionDown;
        event.player_id = *playerId;
        event.reason = apollo::game::session::PlayerDirectory::kReasonLogout;
        directoryEventSink_(event);
    }
    return true;
}

std::shared_ptr<apollo::game::session::PlayerAnchor> BaseServer::findAnchor(PlayerID playerId) const {
    return anchorManager_->find(playerId);
}

std::optional<PlayerID> BaseServer::findPlayerBySession(protocol::SessionID sessionId) const {
    return sessionLocator_->find_player_by_session(sessionId);
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

            // PLAYER_ASSIGN_WORLD_REQUEST / PLAYER_RESOLVE_ROUTE_REQUEST
            // 归 apps/baseappmgr（目录 + 落点裁决），baseapp 不再受理。

            case protocol::MessageType::PING:
                return handlePing(data);

            default:
                protocol::ErrorMessage err;
                err.code = static_cast<uint32_t>(protocol::MessageType::ERROR);
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

    // 关闭协议（P0-4，lifecycle §2.6）：service 停 → 脏数据 flush（占位，
    // 真正落库接 P1 持久化）→ 保存队列 drain（SaveQueue::stop 后 worker
    // 清空余量）→ 数据库关闭。
    flushDirtyAnchors("shutdown_flush");
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

        // 自动保存（P0-4「Saving 真做」）：把脏锚点送入保存队列（六态
        // Disconnected/Saving 由 flush 与回调维护），不再仅打印。
        flushDirtyAnchors("auto_save");
    }
}

std::size_t BaseServer::flushDirtyAnchors(const char* reason) {
    std::size_t flushed = 0;
    for (auto& anchor : anchorManager_->snapshot()) {
        if (!anchor || !anchor->needs_save()) {
            continue;
        }

        SaveTask task;
        task.playerId = anchor->player_id();
        // 载荷以现档为基线（P1-4 落盘真做后，默认值载荷会清档）：
        // 无档即跳过——不凭空造档案（bootstrap 恶龙已废）。
        // Anchor 属性字段模型扩展后此处改为从 Anchor 收集（P2-4）。
        if (!database_->loadPlayer(anchor->player_id(), task.data)) {
            std::cout << "Flush skipped for player " << anchor->player_id()
                      << " (no profile) (" << reason << ")" << std::endl;
            continue;
        }
        task.callback = [this, playerId = anchor->player_id()](bool success) {
            finalizeSave(playerId, success);
        };
        task.createdAtMs = getCurrentTimeMs();

        anchor->set_state(apollo::game::session::AnchorState::Saving);
        saveQueue_->enqueue(task);
        ++flushed;
        std::cout << "Flush queued for player " << anchor->player_id() << " (" << reason
                  << ")" << std::endl;
    }
    return flushed;
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
