#include "player_store/player_store.hpp"
#include "player_store/business_wire.hpp"
#include "player_store/migration_puller.hpp"
#include "player_store/migration_wire.hpp"
#include "player_store_detail.hpp"
#include "apollo/core/log/log_manager.h"
#include "apollo/game/base/avatar_reward_sink.hpp"
#include "apollo/protocol/messages.hpp"
#include "apollo/protocol/codec.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <utility>

namespace player_store {

namespace {

// 库面日志出口（detail 单定义，本单元转发——定义在 player_store.cpp）
apollo::core::log::Logger& blog() {
    return store_detail::blog();
}

const char* toAvatarStateString(apollo::game::base::AvatarState state) {
    using apollo::game::base::AvatarState;

    switch (state) {
        case AvatarState::Loading:
            return "loading";
        case AvatarState::Online:
            return "online";
        case AvatarState::Transferring:
            return "transferring";
        case AvatarState::Disconnected:
            return "disconnected";
        case AvatarState::Saving:
            return "saving";
        case AvatarState::Offline:
            return "offline";
        default:
            return "unknown";
    }
}

} // namespace

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

        // 结算分值入账（P1 基础业务与结算迁入；958——World 结算返回业务
        // 请求，锚点写权在本 store）
        case static_cast<protocol::MessageType>(business::ScoreCreditRequest::TYPE):
            return handlePlayerScoreCreditRequest(data);

        // PLAYER_ASSIGN_WORLD_REQUEST / PLAYER_RESOLVE_ROUTE_REQUEST
        // 归 baseappmgr 域（目录 + 落点裁决），base 不再受理。

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
        auto avatar = activatePlayer(loadReq.playerId);
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
        const auto avatar = avatarManager_->find(saveReq.playerId);
        if (avatar &&
            avatar->state() == apollo::game::base::AvatarState::Transferring) {
            frozen = true;
        } else {
            // 异步保存
            SaveTask task;
            task.playerId = saveReq.playerId;
            task.data = PlayerData::fromJson(saveReq.jsonData);
            // exp 以锚点长期态为准（真实奖励持久化口径）：外部载荷的 exp
            // 可能是旧快照回放——锚点即真源，覆盖写入防旧值回卷
            if (avatar) {
                task.data.exp = avatar->progress(
                    apollo::game::base::AvatarRewardSink::kExpKey);
            }
            if (database_->ownsJournal()) {
                if (task.data.playerId != saveReq.playerId) {
                    response.errorMessage = "profile identity mismatch";
                } else {
                    // 写端自持代次（L64 Base 半；database 半验单调）
                    task.data.writeGen = nextWriteGen(saveReq.playerId, nullptr);
                    response.success = database_->savePlayer(task.data);
                    if (avatar && response.success) avatar->clear_dirty();
                    if (!response.success) response.errorMessage = "database write not confirmed";
                }
                return protocol::MessageCodec::encode(response, header.sessionId);
            }
            task.createdAtMs = getCurrentTimeMs();

            // 先置 Saving 再入段/入队（worker finalize 才有 Saving 可回写；
            // 后置会与快速完成竞态把锚点卡死 Saving）
            const auto prev = avatar ? avatar->state()
                                     : apollo::game::base::AvatarState::Online;
            if (avatar) {
                avatar->mark_dirty("db_save_request");
                avatar->set_state(apollo::game::base::AvatarState::Saving);
            }
            // write-ahead（P1-5）：先落 journal 恢复位点，再异步落档——
            // 合并被拒时载荷由 journal 段承载（下轮 drain 经队列落档）
            task.journalSeq = appendJournal(task.data);
            task.callback = [this, playerId = saveReq.playerId,
                             seq = task.journalSeq](bool success) {
                finalizeSave(playerId, success, seq);
            };
            accepted = saveQueue_->enqueue(task);
            if (avatar && !accepted) {
                // 同玩家在途任务合并跳过：恢复原态留脏，载荷走 journal 段
                avatar->set_state(prev);
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

    if (auto avatar = activatePlayer(activateReq.playerId)) {
        response.success = true;
        response.state = toAvatarStateString(avatar->state());
    } else {
        response.errorMessage = "Failed to activate player avatar";
        blog().warning("Player activation rejected id=" + std::to_string(activateReq.playerId));
    }

    return protocol::MessageCodec::encode(response, header.sessionId);
}

std::vector<uint8_t> PlayerStore::handlePlayerBindSessionRequest(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto bindReq = protocol::MessageCodec::decodeBody<protocol::PlayerBindSessionRequest>(bodyData);

    protocol::PlayerBindSessionResponse response;
    response.success = false;

    apollo::game::base::SessionBinding binding;
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

} // namespace player_store

