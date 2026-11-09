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

} // namespace

// ---- 玩家迁移受理面（P0-2 路 2 第一步，backup-revive §9.2 五步 zone 侧）----

bool PlayerStore::freezePlayer(PlayerID playerId) {
    {
        std::lock_guard<std::mutex> lock(storeMutex_);
        const auto avatar = avatarManager_->find(playerId);
        if (!avatar) {
            return false;
        }
        // 只读态前置（§9.2 ①）：先 Transferring 再排段——autoSave/RPC 保存
        // /journal drain 见只读态即闸（flushDirtyAvatars、handleDbSaveRequest、
        // drainJournal sink 三守卫），此后此玩家无新增队列任务
        avatar->set_state(apollo::game::base::AvatarState::Transferring);
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
        const auto avatar = avatarManager_->find(playerId);
        if (!avatar) {
            return false;
        }
        // journal 段排平（write-ahead 已入段 → 同步直写追平档案：低频迁移面
        // ms 级——此后此玩家只读，无双写）
        if (avatar->needs_save()) {
            PlayerData data;
            if (database_->loadPlayer(playerId, data)) {
                // 冻结段同步排平：未 flush 的奖励增量随冻结落档（快照才带上
                // 最新 exp——迁移不丢奖励）；同步直写即落档——journal 段序
                // 直接记入水位（drain 回放同段消费不重写）
                data.exp = avatar->progress(
                    apollo::game::base::AvatarRewardSink::kExpKey);
                // 写端自持代次（L64 Base 半；database 半验单调）——基线即
                // 现档，作缺账播种真源
                data.writeGen = nextWriteGen(playerId, &data.writeGen);
                const auto seq = appendJournal(data);
                if (database_->savePlayer(data) && seq != 0) {
                    auto& landed = landedSeqs_[playerId];
                    if (seq > landed) {
                        landed = seq;
                    }
                }
                avatar->clear_dirty();
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
    auto avatar = avatarManager_->find(playerId);
    if (!avatar
        || avatar->state() != apollo::game::base::AvatarState::Transferring) {
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
    const auto& binding = avatar->session_binding();
    out.session_id = binding.session_id;
    out.gateway_id = binding.gateway_id;
    out.home_zone_id = avatar->home_zone_id();
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
        auto avatar = avatarManager_->activate(playerId);
        if (!avatar) {
            return false;
        }
        avatar->add_progress(
            apollo::game::base::AvatarRewardSink::kExpKey,
            data.exp - avatar->progress(apollo::game::base::AvatarRewardSink::kExpKey));
        apollo::game::base::SessionBinding binding;
        binding.session_id = sessionId;
        binding.gateway_id = gatewayId;
        avatar->bind_session(binding);
        avatar->set_home_zone_id(homeZoneId);
        // 临时只读闸：Transferring 闸住 flush/RPC/drain 的同玩家新任务，
        // 在途任务排干后本域直写档案（终值 = 快照），无旧快照晚落覆盖
        avatar->set_state(apollo::game::base::AvatarState::Transferring);
    }
    // 在途排干（写序收口，同 freezePlayer；分片睡等不加锁）
    while (saveQueue_->has_pending_or_inflight(playerId)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    {
        std::lock_guard<std::mutex> lock(storeMutex_);
        const auto avatar = avatarManager_->find(playerId);
        if (!avatar) {
            return false;
        }
        // 写端自持代次（L64 Base 半）：接权快照即播种真源，盖章后落库
        //——database 地板随接权推进，接管后旧进程迟到写必被拒
        data.writeGen = nextWriteGen(playerId, &data.writeGen);
        if (!database_->savePlayer(data)) {
            avatar->set_state(apollo::game::base::AvatarState::Offline);
            return false;
        }
        avatar->clear_dirty();  // 档案已直写落盘——不脏（autoSave 不再碰）
        avatar->set_state(apollo::game::base::AvatarState::Online);
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
        reply.error_message = "unknown player or no avatar";
    }
    return migration::encodeFrame(reply, session);
}

// 结算分值入账受理（958）：资料归属切换后的 Base 业务受理面——World 结算
// 以返回业务请求落账，不跨界持锚点指针。幂等/重放去重归调用方账本（World
// 侧 SettlementLedger）；本面无锚点或锚点 Transferring → accepted=0（调用
// 方挂账待补）。落账走 AvatarRewardSink 语义（exp 键 + mark_dirty 随
// SaveQueue 落档）。

std::vector<uint8_t> PlayerStore::handlePlayerScoreCreditRequest(
    const std::vector<uint8_t>& request) {
    protocol::SessionID session = 0;
    business::ScoreCreditRequest req;
    if (!business::decodeFrame(request, req, session)) {
        protocol::ErrorMessage err;
        err.code = static_cast<uint32_t>(protocol::MessageType::ERROR);
        err.message = "Bad player score credit request";
        return protocol::MessageCodec::encode(
            err, protocol::MessageCodec::parseHeader(request).sessionId);
    }
    business::ScoreCreditReply reply;
    reply.player_id = req.player_id;
    reply.request_id = req.request_id;
    const auto avatar = findAvatar(req.player_id);
    if (!avatar) {
        reply.error_message = "unknown player or no avatar";
    } else if (avatar->state() ==
               apollo::game::base::AvatarState::Transferring) {
        reply.error_message = "avatar transferring";
    } else {
        apollo::game::base::AvatarRewardSink sink(*avatar);
        sink.on_reward(req.player_id, req.score);
        reply.accepted = true;
    }
    return business::encodeFrame(reply, session);
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

} // namespace player_store

