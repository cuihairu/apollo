#include "player_store/player_store.hpp"
#include "player_store/migration_puller.hpp"
#include "player_store/migration_wire.hpp"
#include "apollo/core/log/log_manager.h"
#include "apollo/game/session/anchor_reward_sink.hpp"
#include "apollo/protocol/messages.hpp"
#include "apollo/protocol/codec.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <utility>

namespace player_store {

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
// PlayerStore 实现
//==============================================================================

PlayerStore::PlayerStore(const PlayerStoreConfig& config,
                         std::unique_ptr<PlayerStorage> storage)
    : config_(config)
    , database_(storage ? std::move(storage) : std::make_unique<DatabaseService>(config))
    , saveQueue_(std::make_unique<SaveQueue>(config_.workerThreads, config_.saveQueueCapacity))
    , anchorManager_(std::make_shared<apollo::game::session::AnchorManager>())
    , sessionLocator_(std::make_shared<apollo::game::session::SessionLocator>())
    , journal_(database_->ownsJournal() ? nullptr :
          std::make_unique<apollo::data::journal::PersistJournal>(config_.journalPath)) {
    // P1-4 真链路：SaveQueue 出队 → DatabaseService 落盘（workerLoop
    // 此前只 callback(true) 不写任何介质）
    saveQueue_->set_worker([this](const PlayerData& data) {
        return database_->savePlayer(data);
    });
}

std::uint64_t PlayerStore::appendJournal(const PlayerData& data) {
    if (!journal_) return 0; // Remote database owns its journal and write ordering.
    // write-ahead：先落 journal（恢复位点），再经 SaveQueue 异步落档；
    // 返回段全局序供落档水位记账（SaveTask::journalSeq）
    return journal_->append(std::to_string(data.playerId), data.toJson(),
                            getCurrentTimeMs());
}

std::size_t PlayerStore::drainJournal() {
    if (!journal_) return 0;
    // journal 消费侧（P1-5）：定额出队，把已落 journal 的载荷压到档案；
    // 载荷为全量快照，重放幂等。落盘一律经 SaveQueue 同玩家槽位（与
    // flush/RPC 同一串行点，直接落盘会与队列在途任务竞写同档）；队列
    // 已停（停机追平面）改同步直写——此拍生产者已全停，单线程安全。
    // 同玩家槽位忙时入队被拒：条目顶回 journal 队首，下轮重试（写序
    // 保持：同槽先入队者先落盘）
    return journal_->drain(
        [this](const apollo::data::journal::JournalEntry& e) {
            PlayerData data;
            try {
                data = PlayerData::fromJson(e.payload);
                data.playerId = static_cast<PlayerID>(std::stoull(e.key));
            } catch (const std::exception&) {
                return false;  // 脏条目保留，人工排查
            }
            std::lock_guard<std::mutex> lock(storeMutex_);
            // 只读态玩家（迁移已交权/接权排干窗）：档案不由本域队列写
            // （无双写不变量；冻结/接权段已同步直写收口，条目消费即可）
            if (const auto anchor = anchorManager_->find(data.playerId);
                anchor &&
                anchor->state() == apollo::game::session::AnchorState::Transferring) {
                return true;
            }
            // 接权前旧域条目（seq < 水位）：消费不落档——接权直写已是终值
            if (const auto wm = adoptWatermarks_.find(data.playerId);
                wm != adoptWatermarks_.end() && e.sequence < wm->second) {
                return true;
            }
            // 已落档水位：条目载荷 ≤ 既有落盘 = 旧快照——消费不倒灌；
            // > 水位 = 被合并跳过的唯一载荷载体，经队列落档（journalSeq
            // 随任务记账，落档后同序条目自然让位）
            if (const auto it = landedSeqs_.find(data.playerId);
                it != landedSeqs_.end() && e.sequence <= it->second) {
                return true;
            }
            if (saveQueue_->is_running()) {
                SaveTask task;
                task.playerId = data.playerId;
                task.data = std::move(data);
                task.journalSeq = e.sequence;
                task.createdAtMs = getCurrentTimeMs();
                return saveQueue_->enqueue(task);
            }
            return database_->savePlayer(data);
        },
        static_cast<std::size_t>(config_.journalDrainQuota));
}

PlayerStore::~PlayerStore() {
    stop();
}

std::shared_ptr<apollo::game::session::PlayerAnchor> PlayerStore::activatePlayer(PlayerID playerId) {
    // 存档串行化：fresh 判定、播种、清脏同一临界区——并发激活不得双播种
    std::lock_guard<std::mutex> lock(storeMutex_);
    PlayerData data;
    if (!database_->loadPlayer(playerId, data)) {
        return nullptr;
    }

    // 长期态播种（真实奖励持久化）：exp 以 Anchor 为持久真源——新锚点按
    // 现档播种，flush 收集覆盖写回；仅新锚点播种（既存锚点重复加值即双记）。
    const bool fresh = anchorManager_->find(playerId) == nullptr;
    auto anchor = activatePlayerAnchor(playerId);
    if (!anchor) {
        return nullptr;
    }
    if (fresh) {
        anchor->add_progress(apollo::game::session::AnchorRewardSink::kExpKey,
                             data.exp);
    }

    anchor->clear_dirty();
    return anchor;
}

void PlayerStore::set_directory_event_sink(
    apollo::game::session::PlayerDirectory::EventSink sink) {
    directoryEventSink_ = std::move(sink);
}

bool PlayerStore::bindSession(
    PlayerID playerId,
    const apollo::game::session::SessionBinding& binding
) {
    if (!anchorManager_->find(playerId)) {
        // 未激活先激活（activatePlayer 自持串行点）；拒绝路径的态判定在
        // 下方临界区内复核——find 与锁间窗口内冻结可能落位
        if (!activatePlayer(playerId)) {
            blog().warning("Player " + std::to_string(playerId)
                           + " bind rejected (no profile)");
            return false;
        }
    }

    // 冻结锚点拒绑（P3-6 迟到输入）：写权已交新进程（§9.2 ①），迟到
    // 重绑/重连不得把 Transferring 翻回 Online（否则源域重获写权 + 重回
    // 在册集，双写不变量破口）
    std::lock_guard<std::mutex> lock(storeMutex_);
    auto anchor = anchorManager_->find(playerId);
    if (!anchor) {
        return false;
    }
    if (anchor->state() == apollo::game::session::AnchorState::Transferring) {
        blog().warning("Player " + std::to_string(playerId)
                       + " bind rejected (frozen for migration)");
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

bool PlayerStore::unbindSession(protocol::SessionID sessionId) {
    const auto playerId = sessionLocator_->find_player_by_session(sessionId);
    if (!playerId.has_value()) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(storeMutex_);
        if (auto anchor = anchorManager_->find(*playerId)) {
            anchor->unbind_session(sessionId);
            anchor->set_state(apollo::game::session::AnchorState::Disconnected);
        }
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

std::shared_ptr<apollo::game::session::PlayerAnchor> PlayerStore::findAnchor(PlayerID playerId) const {
    return anchorManager_->find(playerId);
}

std::optional<PlayerID> PlayerStore::findPlayerBySession(protocol::SessionID sessionId) const {
    return sessionLocator_->find_player_by_session(sessionId);
}

void PlayerStore::start() {
    if (running_) return;
    // 停机位单向（G-3）：stop 后本实例不再服务——重启语义归进程重启，
    // 防止复位后静默落入维护中受理
    if (shuttingDown_) {
        throw std::runtime_error("PlayerStore cannot restart after stop (process restart instead)");
    }

    // 初始化数据库
    if (!database_->initialize()) {
        throw std::runtime_error("Failed to initialize database");
    }

    // 启动序列 restore→admission→ready（P1-5，lifecycle §3）：journal
    // replay 恢复未落档位点 → 准入检查（数据面可服务）→ Ready 才开 RPC
    if (journal_ && !journal_->open()) {
        throw std::runtime_error("Failed to open storage journal");
    }
    recovery_ = std::make_unique<apollo::game::session::RecoveryCoordinator>(
        "base-app",
        [this]() -> std::size_t {
            if (!journal_) return 0;
            return journal_->replay([this](const apollo::data::journal::JournalEntry& e) {
                PlayerData data;
                try {
                    data = PlayerData::fromJson(e.payload);
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
            if (!database_->listPlayers(ids)) return false;
            const auto restored =
                apollo::game::session::restore_anchors(*anchorManager_, ids);
            blog().info("Anchor restore: " + std::to_string(restored)
                        + " anchors offline-resumed");
            // 恢复锚点播种（真实奖励持久化）：flush 收集以 Anchor 为 exp 真源——
            // 未播种的恢复锚首次 flush 会把现档 exp 覆写归零
            for (const auto id : ids) {
                PlayerData profile;
                if (!database_->loadPlayer(id, profile)) continue;
                if (auto anchor = anchorManager_->find(id)) {
                    anchor->add_progress(
                        apollo::game::session::AnchorRewardSink::kExpKey,
                        profile.exp);
                    anchor->clear_dirty();
                }
            }
            return true;
        });
    if (!recovery_->run() || !recovery_->ready()) {
        throw std::runtime_error(std::string("Recovery failed at phase: ") +
                                 apollo::game::session::to_string(recovery_->phase()));
    }

    // 启动保存队列
    saveQueue_->start();

    running_ = true;

    // M1 base 受理面（P4-4 批③）：m1Port 非零 = link M1 面（dispatchRequest
    // 直通——nng REP 已随批⑤退役[桩传输死面]）；装配失败降级（警告 + 不拦
    // 启动，RPC 面缺席）。
    if (config_.m1Enabled || config_.m1Port != 0) {
        PlayerStoreLinkServerConfig m1Config;
        m1Config.host = config_.host;
        m1Config.port = config_.m1Port;
        m1Config.route_json = config_.m1RouteJson;
        m1Config.self_name = config_.m1SelfName;
        m1Config.endpoint_name = config_.m1EndpointName;
        m1Server_ = makePlayerStoreLinkServer(
            std::move(m1Config), [this](const std::uint8_t* frame, std::size_t size) {
                try {
                    return dispatchRequest(std::vector<std::uint8_t>(frame, frame + size));
                } catch (const std::exception&) {
                    protocol::ErrorMessage error{};
                    error.code = 1;
                    error.message = "Base request rejected";
                    return protocol::MessageCodec::encode(error);
                }
            });
        if (m1Server_->start()) {
            blog().info("M1 base face listening on " + config_.host + ":"
                        + std::to_string(m1Server_->listenPort()));
        } else {
            blog().warning("M1 base face degraded: route table unavailable ("
                           + config_.m1RouteJson + "), rpc face stays off");
            m1Server_.reset();
        }
    }

    // 启动自动保存线程
    autoSaveThread_ = std::thread(&PlayerStore::autoSaveLoop, this);

    blog().info("Base storage ready; M1 port=" + std::to_string(m1_listen_port()));
}

void PlayerStore::stop() {
    // 幂等闸（G-3）：信号处理器与析构可能先后各调一次——CAS 保证五阶段
    // 只走一遍（此前二次调用会重复 flush 与 double database shutdown）
    bool expected = false;
    if (!shuttingDown_.compare_exchange_strong(expected, true)) {
        return;
    }

    // ① 停收新请求（§10.2-①）：维护位已立（dispatchRequest 回维护中），
    // M1 面收线（泵线程停驱 + 监听拆线；§10.2-④ 的 net 断开同拍语义）。
    running_ = false;
    if (m1Server_) {
        m1Server_->stop();
        m1Server_.reset();
    }

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
    while ((journal_ ? journal_->pending() : 0) > 0) {
        const std::size_t applied = drainJournal();
        if ((journal_ ? journal_->pending() : 0) == 0) {
            break;
        }
        if (applied == 0) {
            // 定额内零进展：sink 拒绝（脏条目）——超时判定后告警放行
            if (std::chrono::steady_clock::now() >= deadline) {
                blog().error("journal drain timeout, " + std::to_string((journal_ ? journal_->pending() : 0))
                             + " entries left for restart replay");
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    // ⑤ 模块按依赖逆序停（§10.2-⑤）：存储最内层最后关
    database_->shutdown();
    blog().info("Base server stopped (journal pending: "
                + std::to_string((journal_ ? journal_->pending() : 0)) + ")");
}

std::vector<uint8_t> PlayerStore::dispatchRequest(const std::vector<uint8_t>& data) {
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

    // L69 受理面兼容底线：合法帧头下的畸形/旧形消息体（残缺 JSON、未升级
    // 客户端载荷）结构化 ERROR 回包（会话号保留），绝不向泵线程抛异常。
    try {
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

        // 玩家迁移族（P0-2 路 2；933/935 老 zone 受理、937 新 zone 受理——
        // 同一受理面两角色由编排决定。TYPE 钉生成器 MsgId（legacy 枚举不含
        // internal 族），case 标签回投 enum）
        case static_cast<protocol::MessageType>(migration::FreezeRequest::TYPE):
            return handlePlayerFreezeRequest(data);

        case static_cast<protocol::MessageType>(migration::SnapshotRequest::TYPE):
            return handlePlayerSnapshotRequest(data);

        case static_cast<protocol::MessageType>(migration::HandoffRequest::TYPE):
            return handlePlayerHandoffRequest(data);

        // PLAYER_ASSIGN_WORLD_REQUEST / PLAYER_RESOLVE_ROUTE_REQUEST
        // 归 apps/baseappmgr（目录 + 落点裁决），base-app 不再受理。

        case protocol::MessageType::PING:
            return handlePing(data);

        default:
            protocol::ErrorMessage err;
            err.code = static_cast<uint32_t>(protocol::MessageType::ERROR);
            err.message = "Unknown message type";
            return protocol::MessageCodec::encode(err, header.sessionId);
    }
    } catch (const std::exception&) {
        protocol::ErrorMessage err;
        err.code = static_cast<uint32_t>(protocol::MessageType::ERROR);
        err.message = "Malformed request body";
        return protocol::MessageCodec::encode(err, header.sessionId);
    }
}

std::vector<uint8_t> PlayerStore::handleDbLoadRequest(const std::vector<uint8_t>& request) {
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

std::vector<uint8_t> PlayerStore::handleDbSaveRequest(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto saveReq = protocol::MessageCodec::decodeBody<protocol::DbSaveRequest>(bodyData);

    protocol::DbSaveResponse response;
    response.success = false;

    // 存档串行化：态判定、exp 口径、journal 入段与入队同一临界区——与
    // 自动保存 flush 的入队点原子交错，同玩家任务序在 SaveQueue 槽内定序
    bool accepted = false;
    bool frozen = false;
    {
        std::lock_guard<std::mutex> lock(storeMutex_);
        // 冻结锚点拒存（P3-6 迟到输入）：迁移只读态不入 journal/队列，不置
        // Saving——旧路径在此翻写态（覆盖 Transferring）+ 入段，与无双写不变
        // 量（§9.2 ①）正面冲突
        const auto anchor = anchorManager_->find(saveReq.playerId);
        if (anchor &&
            anchor->state() == apollo::game::session::AnchorState::Transferring) {
            frozen = true;
        } else {
            // 异步保存
            SaveTask task;
            task.playerId = saveReq.playerId;
            task.data = PlayerData::fromJson(saveReq.jsonData);
            // exp 以锚点长期态为准（真实奖励持久化口径）：外部载荷的 exp
            // 可能是旧快照回放——锚点即真源，覆盖写入防旧值回卷
            if (anchor) {
                task.data.exp = anchor->progress(
                    apollo::game::session::AnchorRewardSink::kExpKey);
            }
            if (database_->ownsJournal()) {
                if (task.data.playerId != saveReq.playerId) {
                    response.errorMessage = "profile identity mismatch";
                } else {
                    response.success = database_->savePlayer(task.data);
                    if (anchor && response.success) anchor->clear_dirty();
                    if (!response.success) response.errorMessage = "database write not confirmed";
                }
                return protocol::MessageCodec::encode(response, header.sessionId);
            }
            task.createdAtMs = getCurrentTimeMs();

            // 先置 Saving 再入段/入队（worker finalize 才有 Saving 可回写；
            // 后置会与快速完成竞态把锚点卡死 Saving）
            const auto prev = anchor ? anchor->state()
                                     : apollo::game::session::AnchorState::Online;
            if (anchor) {
                anchor->mark_dirty("db_save_request");
                anchor->set_state(apollo::game::session::AnchorState::Saving);
            }
            // write-ahead（P1-5）：先落 journal 恢复位点，再异步落档——
            // 合并被拒时载荷由 journal 段承载（下轮 drain 经队列落档）
            task.journalSeq = appendJournal(task.data);
            task.callback = [this, playerId = saveReq.playerId,
                             seq = task.journalSeq](bool success) {
                finalizeSave(playerId, success, seq);
            };
            accepted = saveQueue_->enqueue(task);
            if (anchor && !accepted) {
                // 同玩家在途任务合并跳过：恢复原态留脏，载荷走 journal 段
                anchor->set_state(prev);
            }
        }
    }

    if (frozen) {
        response.errorMessage = "player frozen for migration";
        blog().warning("Save rejected for player " + std::to_string(saveReq.playerId)
                       + " (frozen for migration)");
        return protocol::MessageCodec::encode(response, header.sessionId);
    }

    // 简化处理：直接返回成功（实际应该等保存完成）
    response.success = true;

    blog().info("Queued save for player " + std::to_string(saveReq.playerId)
                + (accepted ? "" : " (coalesced)"));

    return protocol::MessageCodec::encode(response, header.sessionId);
}

std::vector<uint8_t> PlayerStore::handleDbQueryRequest(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);

    // 简化处理
    protocol::DbQueryResponse response;
    response.success = false;
    response.errorMessage = "Not implemented";

    return protocol::MessageCodec::encode(response, header.sessionId);
}

std::vector<uint8_t> PlayerStore::handlePlayerActivateRequest(const std::vector<uint8_t>& request) {
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

std::vector<uint8_t> PlayerStore::handlePlayerBindSessionRequest(const std::vector<uint8_t>& request) {
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

std::vector<uint8_t> PlayerStore::handlePing(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto ping = protocol::MessageCodec::decodeBody<protocol::Ping>(bodyData);

    protocol::Pong pong;
    pong.timestamp = ping.timestamp;

    return protocol::MessageCodec::encode(pong, header.sessionId);
}

void PlayerStore::autoSaveLoop() {
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

std::size_t PlayerStore::flushDirtyAnchors(const char* reason) {
    // 单临界区（串行化批）：态判定、基线读档、exp 收集、入段与入队原子
    // ——基线在锁内取（DatabaseService 缓存随每次 save 同步刷新，命中
    // 即取，无盘面 I/O 占锁；激活时已首载入缓存），锁内基线 + 锁内入队
    // 消灭「RPC 任务在基线读取与入队之间完成 → 旧字段覆盖新落档」的
    // 过期窗。锚点创建必经 activatePlayer（首载），故此处恒缓存命中。
    std::lock_guard<std::mutex> lock(storeMutex_);
    std::size_t flushed = 0;
    for (auto& anchor : anchorManager_->snapshot()) {
        if (!anchor || !anchor->needs_save()) {
            continue;
        }
        // 迁移中锚点跳过（P0-2 路 2 ① Transferring = 只读态）：写权已交
        // 新进程，老进程不再落盘（无双写不变量，§9.2）
        if (anchor->state() == apollo::game::session::AnchorState::Transferring) {
            continue;
        }

        SaveTask task;
        task.playerId = anchor->player_id();
        // 载荷以现档为基线（P1-4 落盘真做后，默认值载荷会清档）：
        // 无档即跳过——不凭空造档案（bootstrap 恶龙已废）。
        if (!database_->loadPlayer(task.playerId, task.data)) {
            blog().warning("Flush skipped for player " + std::to_string(task.playerId)
                            + " (no profile) (" + reason + ")");
            continue;
        }
        // exp 收集自 Anchor 长期态（真实奖励持久化）：奖励落锚点即真源，
        // flush 覆盖写回——不再仅重写旧档案丢增量（播种见 activatePlayer/
        // 启动恢复/adoptPlayer）。
        task.data.exp = anchor->progress(
            apollo::game::session::AnchorRewardSink::kExpKey);
        if (database_->ownsJournal()) {
            if (database_->savePlayer(task.data)) {
                anchor->clear_dirty();
                ++flushed;
            }
            continue;
        }
        task.createdAtMs = getCurrentTimeMs();

        // 先置 Saving 再入段/入队（worker finalize 才有 Saving 可回写；
        // 后置会与快速完成竞态把锚点卡死 Saving）
        const auto prev = anchor->state();
        anchor->set_state(apollo::game::session::AnchorState::Saving);
        // write-ahead（P1-5）：同 handleDbSaveRequest——合并被拒时载荷
        // 由 journal 段承载（下轮 drain 经队列落档），锚点留脏
        task.journalSeq = appendJournal(task.data);
        task.callback = [this, playerId = task.playerId,
                         seq = task.journalSeq](bool success) {
            finalizeSave(playerId, success, seq);
        };
        if (!saveQueue_->enqueue(task)) {
            anchor->set_state(prev);  // 同玩家在途：合并跳过，下轮再取
            continue;
        }
        ++flushed;
        blog().info("Flush queued for player " + std::to_string(task.playerId)
                    + " (" + reason + ")");
    }
    return flushed;
}

// ---- 玩家迁移受理面（P0-2 路 2 第一步，backup-revive §9.2 五步 zone 侧）----

bool PlayerStore::freezePlayer(PlayerID playerId) {
    {
        std::lock_guard<std::mutex> lock(storeMutex_);
        const auto anchor = anchorManager_->find(playerId);
        if (!anchor) {
            return false;
        }
        // 只读态前置（§9.2 ①）：先 Transferring 再排段——autoSave/RPC 保存
        // /journal drain 见只读态即闸（flushDirtyAnchors、handleDbSaveRequest、
        // drainJournal sink 三守卫），此后此玩家无新增队列任务
        anchor->set_state(apollo::game::session::AnchorState::Transferring);
    }
    // 在途排干（写序收口）：本玩家队列任务全部落盘后，冻结段同步直写才是
    // 最后落盘者——否则在途旧快照晚于冻结写落盘 = 旧覆盖新。等待不加锁
    // （worker finalize 需 storeMutex_，锁内自旋即死锁）；分片睡等
    // （macOS CI 裸自旋必 flake 口径）
    while (saveQueue_->has_pending_or_inflight(playerId)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    {
        std::lock_guard<std::mutex> lock(storeMutex_);
        const auto anchor = anchorManager_->find(playerId);
        if (!anchor) {
            return false;
        }
        // journal 段排平（write-ahead 已入段 → 同步直写追平档案：低频迁移面
        // ms 级——此后此玩家只读，无双写）
        if (anchor->needs_save()) {
            PlayerData data;
            if (database_->loadPlayer(playerId, data)) {
                // 冻结段同步排平：未 flush 的奖励增量随冻结落档（快照才带上
                // 最新 exp——迁移不丢奖励）；同步直写即落档——journal 段序
                // 直接记入水位（drain 回放同段消费不重写）
                data.exp = anchor->progress(
                    apollo::game::session::AnchorRewardSink::kExpKey);
                const auto seq = appendJournal(data);
                if (database_->savePlayer(data) && seq != 0) {
                    auto& landed = landedSeqs_[playerId];
                    if (seq > landed) {
                        landed = seq;
                    }
                }
                anchor->clear_dirty();
            }
        }
    }
    blog().info("Player " + std::to_string(playerId) + " frozen for migration");
    return true;
}

PlayerStore::PlayerSnapshotData PlayerStore::snapshotPlayer(PlayerID playerId) {
    PlayerSnapshotData out;
    // 串行化 + 前置 = 已冻结（Transferring）——未冻结拒（防半写态出快照，
    // §9.2 ①→② 序）；冻结排干完成后档案即终值，锁内读无并发写
    std::lock_guard<std::mutex> lock(storeMutex_);
    auto anchor = anchorManager_->find(playerId);
    if (!anchor
        || anchor->state() != apollo::game::session::AnchorState::Transferring) {
        out.error_message = "player not frozen";
        return out;
    }
    PlayerData data;
    if (!database_->loadPlayer(playerId, data)) {
        out.error_message = "no profile";
        return out;
    }
    out.success = true;
    out.profile_json = data.toJson();
    const auto& binding = anchor->session_binding();
    out.session_id = binding.session_id;
    out.gateway_id = binding.gateway_id;
    out.home_zone_id = anchor->home_zone_id();
    return out;
}

void PlayerStore::storeMigrationSnapshot(PlayerID playerId, PlayerSnapshotData snapshot) {
    std::lock_guard<std::mutex> lock(storeMutex_);
    migrationSnapshots_[playerId] = std::move(snapshot);
}

bool PlayerStore::adoptPlayer(PlayerID playerId, const std::string& profileJson,
                             std::uint64_t sessionId, std::uint32_t gatewayId,
                             std::uint32_t homeZoneId) {
    PlayerData data;
    if (!profileJson.empty()) {
        data = PlayerData::fromJson(profileJson);
    }
    data.playerId = playerId;  // 显式参为准（防御载荷漂移）

    {
        std::lock_guard<std::mutex> lock(storeMutex_);
        // 接权落档水位：此前的 journal 条目均属旧域——回放时消费不落档
        adoptWatermarks_[playerId] = (journal_ ? journal_->next_sequence() : 0);
        // 接权播种收敛（真实奖励持久化）：档案（快照）为本域 exp 权威——锚点
        // 归一到档案现值（既有锚点含启动播种/旧值时以差值收敛，不叠加重播）。
        auto anchor = anchorManager_->activate(playerId);
        if (!anchor) {
            return false;
        }
        anchor->add_progress(
            apollo::game::session::AnchorRewardSink::kExpKey,
            data.exp - anchor->progress(apollo::game::session::AnchorRewardSink::kExpKey));
        apollo::game::session::SessionBinding binding;
        binding.session_id = sessionId;
        binding.gateway_id = gatewayId;
        anchor->bind_session(binding);
        anchor->set_home_zone_id(homeZoneId);
        // 临时只读闸：Transferring 闸住 flush/RPC/drain 的同玩家新任务，
        // 在途任务排干后本域直写档案（终值 = 快照），无旧快照晚落覆盖
        anchor->set_state(apollo::game::session::AnchorState::Transferring);
    }
    // 在途排干（写序收口，同 freezePlayer；分片睡等不加锁）
    while (saveQueue_->has_pending_or_inflight(playerId)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    {
        std::lock_guard<std::mutex> lock(storeMutex_);
        const auto anchor = anchorManager_->find(playerId);
        if (!anchor) {
            return false;
        }
        if (!database_->savePlayer(data)) {
            anchor->set_state(apollo::game::session::AnchorState::Offline);
            return false;
        }
        anchor->clear_dirty();  // 档案已直写落盘——不脏（autoSave 不再碰）
        anchor->set_state(apollo::game::session::AnchorState::Online);
    }
    blog().info("Player " + std::to_string(playerId)
                + " adopted with write authority");
    return true;
}

// 迁移族 handler（933/935/937）：wire 面 = migration_wire 全帧；decode 失败
// 回 ErrorMessage（Unknown message type 同族错误回包惯例）。
std::vector<uint8_t> PlayerStore::handlePlayerFreezeRequest(
    const std::vector<uint8_t>& request) {
    protocol::SessionID session = 0;
    migration::FreezeRequest req;
    if (!migration::decodeFrame(request, req, session)) {
        protocol::ErrorMessage err;
        err.code = static_cast<uint32_t>(protocol::MessageType::ERROR);
        err.message = "Bad player freeze request";
        return protocol::MessageCodec::encode(
            err, protocol::MessageCodec::parseHeader(request).sessionId);
    }
    const bool accepted = freezePlayer(req.player_id);
    migration::FreezeReply reply;
    reply.player_id = req.player_id;
    reply.request_id = req.request_id;
    reply.accepted = accepted;
    if (!accepted) {
        reply.error_message = "unknown player or no anchor";
    }
    return migration::encodeFrame(reply, session);
}

std::vector<uint8_t> PlayerStore::handlePlayerSnapshotRequest(
    const std::vector<uint8_t>& request) {
    protocol::SessionID session = 0;
    migration::SnapshotRequest req;
    if (!migration::decodeFrame(request, req, session)) {
        protocol::ErrorMessage err;
        err.code = static_cast<uint32_t>(protocol::MessageType::ERROR);
        err.message = "Bad player snapshot request";
        return protocol::MessageCodec::encode(
            err, protocol::MessageCodec::parseHeader(request).sessionId);
    }
    const auto snap = snapshotPlayer(req.player_id);
    migration::SnapshotReply reply;
    reply.player_id = req.player_id;
    reply.request_id = req.request_id;
    reply.success = snap.success;
    reply.error_message = snap.error_message;
    reply.profile_json = snap.profile_json;
    reply.session_id = snap.session_id;
    reply.gateway_id = snap.gateway_id;
    reply.home_zone_id = snap.home_zone_id;
    return migration::encodeFrame(reply, session);
}

std::vector<uint8_t> PlayerStore::handlePlayerHandoffRequest(
    const std::vector<uint8_t>& request) {
    protocol::SessionID session = 0;
    migration::HandoffRequest req;
    if (!migration::decodeFrame(request, req, session)) {
        protocol::ErrorMessage err;
        err.code = static_cast<uint32_t>(protocol::MessageType::ERROR);
        err.message = "Bad player handoff request";
        return protocol::MessageCodec::encode(
            err, protocol::MessageCodec::parseHeader(request).sessionId);
    }
    // 重复接管拒绝（P3-6 权威代次高水位）：new_epoch ≤ 本店已接代 = 旧
    // 937 重放/迟到交权，拒于拉取之前——不重拉旧档、不二次 adopt（否则
    // 覆盖目标侧迁移后新写）。见 adoptedEpochs_ 成员注。
    {
        std::lock_guard<std::mutex> lock(storeMutex_);
        const auto adopted = adoptedEpochs_.find(req.player_id);
        if (adopted != adoptedEpochs_.end() &&
            req.new_epoch <= adopted->second) {
            migration::HandoffReply dup;
            dup.player_id = req.player_id;
            dup.request_id = req.request_id;
            dup.error_message =
                "duplicate takeover: epoch " + std::to_string(req.new_epoch)
                + " <= adopted " + std::to_string(adopted->second);
            blog().warning("Player " + std::to_string(req.player_id)
                           + " handoff rejected (" + dup.error_message + ")");
            return migration::encodeFrame(dup, session);
        }
    }
    // 快照落位前置（②'）：无暂存且 937 带源地址 = 现场拉取（五步②——
    // 新 zone 向老 zone 拨 935，老的已先 flush 再发；同步 ms 级，迁移面
    // 低频，批量异步化挂后续批）。源为空且无暂存 = 拒（②→③ 不可跳）。
    auto it = migrationSnapshots_.find(req.player_id);
    migration::HandoffReply reply;
    reply.player_id = req.player_id;
    reply.request_id = req.request_id;
    if (it == migrationSnapshots_.end() && !req.source_host.empty()
            && req.source_port != 0) {
        if (config_.m1RouteJson.empty()) {
            reply.error_message = "no route table for migration pull";
            return migration::encodeFrame(reply, session);
        }
        migration::MigrationPuller puller(config_.m1RouteJson, req.source_host,
                                          static_cast<std::uint16_t>(req.source_port));
        if (!puller.ready() || !puller.waitUp()) {
            reply.error_message = "migration pull: source unreachable";
            return migration::encodeFrame(reply, session);
        }
        auto pulled = puller.pull(req.player_id, req.request_id);
        puller.shutdown();
        if (!pulled.success) {
            reply.error_message = "migration pull failed: " + pulled.error_message;
            return migration::encodeFrame(reply, session);
        }
        storeMigrationSnapshot(req.player_id, std::move(pulled));
        it = migrationSnapshots_.find(req.player_id);
    }
    if (it == migrationSnapshots_.end()) {
        reply.error_message = "no migration snapshot stored";
        return migration::encodeFrame(reply, session);
    }
    std::string profile;
    std::uint32_t homeZone = 0;
    {
        std::lock_guard<std::mutex> lock(storeMutex_);
        profile = it->second.profile_json;
        homeZone = it->second.home_zone_id;
        migrationSnapshots_.erase(it);
    }
    reply.accepted = adoptPlayer(req.player_id, profile, req.session_id,
                                 req.gateway_id, homeZone);
    if (!reply.accepted) {
        reply.error_message = "adopt failed";
    } else {
        std::lock_guard<std::mutex> lock(storeMutex_);
        adoptedEpochs_[req.player_id] = req.new_epoch;  // 权威代次落账（§9.2 ③）
    }
    blog().info("Player " + std::to_string(req.player_id) + " handoff accepted (new epoch="
                + std::to_string(req.new_epoch) + ")");
    return migration::encodeFrame(reply, session);
}

int64_t PlayerStore::getCurrentTimeMs() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

std::shared_ptr<apollo::game::session::PlayerAnchor> PlayerStore::activatePlayerAnchor(PlayerID playerId) {
    auto anchor = anchorManager_->activate(playerId);
    if (!anchor) {
        return nullptr;
    }

    if (anchor->state() == apollo::game::session::AnchorState::Loading) {
        anchor->set_state(apollo::game::session::AnchorState::Online);
    }

    return anchor;
}

void PlayerStore::finalizeSave(PlayerID playerId, bool success, std::uint64_t journalSeq) {
    // 串行化：态守卫判定与回写同一临界区——与 freeze/adopt 的只读闸原子
    // 交错，不得覆盖 Transferring（双写不变量破口的收口）
    std::lock_guard<std::mutex> lock(storeMutex_);
    auto anchor = anchorManager_->find(playerId);
    if (!anchor) {
        return;
    }
    // 落档水位记账（成功才推进）：journal 回放序 ≤ 此序的条目已被本任务
    // 载荷（或更新者）覆盖——drain 消费不落档，旧快照不倒灌
    if (success && journalSeq != 0) {
        auto& landed = landedSeqs_[playerId];
        if (journalSeq > landed) {
            landed = journalSeq;
        }
    }

    // 态守卫（P0-2 路 2 ①）：SaveTask 在途期间锚可能已被迁移停写置
    // Transferring（freeze 与 autoSave 的竞态窗口）——finalize 不得覆盖
    // 只读态（推回 Online = 双写不变量破口）。仅 Saving 态回写结果态；
    // 其它态只清/标脏，态留给持有者。
    if (anchor->state() != apollo::game::session::AnchorState::Saving) {
        if (success) {
            anchor->clear_dirty();
        } else {
            anchor->mark_dirty("save_failed");
        }
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

} // namespace player_store
