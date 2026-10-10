#include "base/base_server.hpp"
#include "apollo/core/log/log_manager.h"
#include "apollo/protocol/messages.hpp"
#include "apollo/protocol/codec.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace base {

namespace {

// 库面日志出口：cat=base_server（§5.1 cat= 检索键；主程已接文件面）
apollo::core::log::Logger& blog() {
    return *apollo::core::log::global_log_manager().createLogger("base_server");
}

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
    , sessionLocator_(std::make_shared<apollo::game::session::SessionLocator>())
    , journal_(std::make_unique<apollo::data::journal::PersistJournal>(config_.journalPath)) {
    // P1-4 真链路：SaveQueue 出队 → DatabaseService 落盘（workerLoop
    // 此前只 callback(true) 不写任何介质）
    saveQueue_->set_worker([this](const PlayerData& data) {
        return database_->savePlayer(data);
    });
}

std::size_t BaseServer::appendJournal(const PlayerData& data) {
    // write-ahead：先落 journal（恢复位点），再经 SaveQueue 异步落档
    return journal_->append(std::to_string(data.playerId), data.toJson(),
                            getCurrentTimeMs())
               ? 1u
               : 0u;
}

std::size_t BaseServer::drainJournal() {
    // journal 消费侧（P1-5）：定额出队，把已落 journal 的载荷压到档案；
    // 载荷为全量快照，重放幂等
    return journal_->drain(
        [this](const apollo::data::journal::JournalEntry& e) {
            PlayerData data = PlayerData::fromJson(e.payload);
            try {
                data.playerId = static_cast<PlayerID>(std::stoull(e.key));
            } catch (const std::exception&) {
                return false;  // 脏条目保留，人工排查
            }
            return database_->savePlayer(data);
        },
        static_cast<std::size_t>(config_.journalDrainQuota));
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
    // 停机位单向（G-3）：stop 后本实例不再服务——重启语义归进程重启，
    // 防止复位后静默落入维护中受理
    if (shuttingDown_) {
        throw std::runtime_error("BaseServer cannot restart after stop (process restart instead)");
    }

    // 初始化数据库
    if (!database_->initialize()) {
        throw std::runtime_error("Failed to initialize database");
    }

    // 启动序列 restore→admission→ready（P1-5，lifecycle §3）：journal
    // replay 恢复未落档位点 → 准入检查（数据面可服务）→ Ready 才开 RPC
    journal_->open();
    recovery_ = std::make_unique<apollo::game::session::RecoveryCoordinator>(
        "zone-app",
        [this]() -> std::size_t {
            return journal_->replay([this](const apollo::data::journal::JournalEntry& e) {
                PlayerData data = PlayerData::fromJson(e.payload);
                try {
                    data.playerId = static_cast<PlayerID>(std::stoull(e.key));
                } catch (const std::exception&) {
                    return false;
                }
                return database_->savePlayer(data);
            });
        },
        [this]() {
            // Anchor 恢复（档案在、会话无，Offline 待登录）+ 数据面准入
            std::vector<PlayerID> ids;
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator(config_.dataDir, ec)) {
                const auto name = entry.path().filename().string();
                if (name.rfind("player_", 0) == 0 && name.size() > 10 &&
                    name.substr(name.size() - 5) == ".json") {
                    try {
                        ids.push_back(static_cast<PlayerID>(
                            std::stoull(name.substr(7, name.size() - 12))));
                    } catch (const std::exception&) {
                    }
                }
            }
            const auto restored =
                apollo::game::session::restore_anchors(*anchorManager_, ids);
            blog().info("Anchor restore: " + std::to_string(restored)
                        + " anchors offline-resumed");
            return !ec;
        });
    if (!recovery_->run() || !recovery_->ready()) {
        throw std::runtime_error(std::string("Recovery failed at phase: ") +
                                 apollo::game::session::to_string(recovery_->phase()));
    }

    // 启动保存队列
    saveQueue_->start();

    // 创建 RPC 服务器
    server_ = std::make_unique<protocol::RepSocket>(
        protocol::make_tcp_url(config_.host, config_.port)
    );

    server_->setRequestHandler([this](const std::vector<uint8_t>& data) -> std::vector<uint8_t> {
        return dispatchRequest(data);
    });

    running_ = true;

    server_->start();

    // 启动自动保存线程
    autoSaveThread_ = std::thread(&BaseServer::autoSaveLoop, this);

    blog().info("Base server listening on " + config_.host + ":"
                + std::to_string(config_.port));
}

void BaseServer::stop() {
    // 幂等闸（G-3）：信号处理器与析构可能先后各调一次——CAS 保证五阶段
    // 只走一遍（此前二次调用会重复 flush 与 double database shutdown）
    bool expected = false;
    if (!shuttingDown_.compare_exchange_strong(expected, true)) {
        return;
    }

    // ① 停收新请求（§10.2-①）：维护位已立（dispatchRequest 回维护中），
    // 关监听并等在途 handler 归还（RepSocket::stop = join worker——§10.2-④
    // 的 net 断开与此同拍：stub 传输树无在途帧面）
    running_ = false;
    if (server_) {
        server_->stop();
    }
    server_.reset();

    // ② 最后一轮变更进 journal（§10.2-②）：先停自动保存生产者（此前
    // join 排在队列 drain 之后——生产者仍可能在 drain 后再入队的竞序），
    // 再全量 flush 脏锚点（flush 先 journal 后入队，write-ahead 语义）
    if (autoSaveThread_.joinable()) {
        autoSaveThread_.join();
    }
    flushDirtyAnchors("shutdown_flush");

    // ③ 持久链追平（§10.2-③）：队列全量排空（SaveQueue::stop 的 worker
    // 至队空方退）→ journal 循环 drain 至 pending==0，带超时上限——超时
    // 告警并继续（日志完整性优先于停机速度：残留条目留待重启 replay，幂等）
    saveQueue_->stop();
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(config_.shutdownFlushTimeoutMs);
    while (journal_->pending() > 0) {
        const std::size_t applied = drainJournal();
        if (journal_->pending() == 0) {
            break;
        }
        if (applied == 0) {
            // 定额内零进展：sink 拒绝（脏条目）——超时判定后告警放行
            if (std::chrono::steady_clock::now() >= deadline) {
                blog().error("journal drain timeout, " + std::to_string(journal_->pending())
                             + " entries left for restart replay");
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    // ⑤ 模块按依赖逆序停（§10.2-⑤）：存储最内层最后关
    database_->shutdown();
    blog().info("Base server stopped (journal pending: "
                + std::to_string(journal_->pending()) + ")");
}

std::vector<uint8_t> BaseServer::dispatchRequest(const std::vector<uint8_t>& data) {
    // 维护闸（G-3，§10.2-①）：停机中不再受理新请求——受理面统一回维护
    // 中应答（与 Unknown message type 同族错误回包；stub 传输树无 control
    // 通道面，维护中语义落在受理入口）
    if (shuttingDown_) {
        protocol::ErrorMessage err;
        err.code = static_cast<uint32_t>(protocol::MessageType::ERROR);
        err.message = "Server is shutting down";
        return protocol::MessageCodec::encode(
            err, protocol::MessageCodec::parseHeader(data).sessionId);
    }

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
        // 归 apps/manager（目录 + 落点裁决），zone-app 不再受理。

        case protocol::MessageType::PING:
            return handlePing(data);

        default:
            protocol::ErrorMessage err;
            err.code = static_cast<uint32_t>(protocol::MessageType::ERROR);
            err.message = "Unknown message type";
            return protocol::MessageCodec::encode(err, header.sessionId);
    }
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

        blog().info("Loaded player " + std::to_string(loadReq.playerId) + " data");
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

    // write-ahead（P1-5）：先落 journal 恢复位点，再异步落档
    appendJournal(task.data);

    saveQueue_->enqueue(task);

    if (auto anchor = anchorManager_->find(saveReq.playerId)) {
        anchor->mark_dirty("db_save_request");
        anchor->set_state(apollo::game::session::AnchorState::Saving);
    }

    // 简化处理：直接返回成功（实际应该等保存完成）
    response.success = true;

    blog().info("Queued save for player " + std::to_string(saveReq.playerId));

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
        // 分片睡等（G-3）：running_ 翻 false 最迟一个分片内可观察——停机
        // join 不吃整个 autoSaveIntervalMs（§10.2 停机时延兜底；此前整段
        // sleep 使 SIGTERM 停机最长挂一个保存周期）
        int waited = 0;
        while (running_ && waited < config_.autoSaveIntervalMs) {
            const int slice = config_.autoSaveIntervalMs - waited < 50
                                  ? config_.autoSaveIntervalMs - waited
                                  : 50;
            std::this_thread::sleep_for(std::chrono::milliseconds(slice));
            waited += slice;
        }

        // 自动保存（P0-4「Saving 真做」）：把脏锚点送入保存队列（六态
        // Disconnected/Saving 由 flush 与回调维护），不再仅打印。
        flushDirtyAnchors("auto_save");

        // journal 消费侧（P1-5）：定额出队压档（attribute-sync §8.2
        // 每 tick 定比、有界队列）
        drainJournal();
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
            blog().warning("Flush skipped for player " + std::to_string(anchor->player_id())
                            + " (no profile) (" + reason + ")");
            continue;
        }
        task.callback = [this, playerId = anchor->player_id()](bool success) {
            finalizeSave(playerId, success);
        };
        task.createdAtMs = getCurrentTimeMs();

        // write-ahead（P1-5）：同 handleDbSaveRequest
        appendJournal(task.data);

        anchor->set_state(apollo::game::session::AnchorState::Saving);
        saveQueue_->enqueue(task);
        ++flushed;
        blog().info("Flush queued for player " + std::to_string(anchor->player_id())
                    + " (" + reason + ")");
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
