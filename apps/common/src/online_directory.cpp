#include "online_directory/online_directory.hpp"
#include "player_store/migration_wire.hpp"
#include "player_store/store_dial.hpp"
#include "apollo/core/log/log_manager.h"
#include "apollo/protocol/messages.hpp"
#include "apollo/protocol/codec.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace online_directory {

namespace migration = player_store::migration;

OnlineDirectory::OnlineDirectory(uint16_t port, std::string host)
    : host_(std::move(host))
    , port_(port) {
    // 目录事件面（P1-2）：事件落日志（可观察）；P3-1 增量②起追加监听链
    // （跨进程镜像 publisher 由进程壳经 set_directory_event_listener 接入）
    directory_.set_event_sink([this](const apollo::game::session::PlayerDirectory::Event& event) {
        static const char* kKindNames[] = {"SessionUp", "SessionDown", "SessionMoved",
                                           "SessionKicked"};
        apollo::core::log::global_log_manager()
            .createLogger("directory")
            ->info(std::string(kKindNames[static_cast<int>(event.kind)])
                   + " player=" + std::to_string(event.player_id) + " epoch="
                   + std::to_string(event.anchor_epoch) + " zone="
                   + std::to_string(event.zone_id) + " reason="
                   + std::to_string(event.reason));
        if (directory_listener_) {
            directory_listener_(event);
        }
    });
}

void OnlineDirectory::set_directory_event_listener(
    apollo::game::session::PlayerDirectory::EventSink listener) {
    directory_listener_ = std::move(listener);
}

void OnlineDirectory::set_admission_gate(std::function<bool()> gate) {
    admission_gate_ = std::move(gate);
}

void OnlineDirectory::set_drain(bool on) {
    drain_.store(on, std::memory_order_release);
}

std::size_t OnlineDirectory::intake_directory_full_report(
    const std::vector<apollo::game::session::MirrorEntry>& sessions) {
    return directory_.intake_full_report(sessions);
}

std::size_t OnlineDirectory::suspend_zone_sessions(std::uint32_t zone_id,
                                              std::uint64_t now_tick,
                                              std::uint64_t window_ticks) {
    return directory_.mark_suspended_by_zone(zone_id, now_tick, window_ticks);
}

std::size_t OnlineDirectory::suspend_gateway_sessions(std::uint32_t gateway_id,
                                                 std::uint64_t now_tick,
                                                 std::uint64_t window_ticks) {
    return directory_.mark_suspended_by_gateway(gateway_id, now_tick, window_ticks);
}

std::size_t OnlineDirectory::sweep_suspended(std::uint64_t now_tick) {
    return directory_.sweep(now_tick);
}

OnlineDirectory::~OnlineDirectory() {
    stop();
}

bool OnlineDirectory::assignWorld(
    PlayerID playerId,
    const apollo::game::base::WorldAssignment& assignment
) {
    if (playerId == 0) {
        return false;
    }
    // 恢复相位排他（P3-1 增量③）与更新排空（backup-revive §8 路1 缺口）
    // 共闸：任一闭合期拒新落点——落点不再指给更新窗内的编队进程
    if (drain_.load(std::memory_order_acquire)) {
        return false;
    }
    if (admission_gate_ && !admission_gate_()) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(assignmentsMutex_);
        assignments_[playerId] = assignment;
    }
    // 目录事件面（P1-2）：落点迁移产 SessionMoved；玩家尚无条目时目录
    // 无动作（SessionUp 由 bindSession 登记锚点承担）
    directory_.moved(playerId, assignment);
    return true;
}

bool OnlineDirectory::clearWorldAssignment(PlayerID playerId) {
    std::lock_guard<std::mutex> lock(assignmentsMutex_);
    return assignments_.erase(playerId) > 0;
}

bool OnlineDirectory::bindSession(PlayerID playerId, const apollo::game::base::SessionBinding& binding) {
    if (playerId == 0 || binding.session_id == 0) {
        return false;
    }
    // 恢复相位排他（P3-1 增量③）与更新排空（§8 路1 缺口）共闸
    if (drain_.load(std::memory_order_acquire)) {
        return false;
    }
    if (admission_gate_ && !admission_gate_()) {
        return false;
    }
    sessionLocator_.bind(playerId, binding);

    // 目录登记锚点（P1-2）：条目级真值 + SessionUp 事件；同账号已有条目
    // 时目录内完成顶号（SessionKicked 事件面）
    apollo::game::base::WorldAssignment assignment;
    {
        std::lock_guard<std::mutex> lock(assignmentsMutex_);
        const auto it = assignments_.find(playerId);
        if (it != assignments_.end()) {
            assignment = it->second;
        }
    }
    directory_.session_up(playerId, binding, assignment);
    return true;
}

bool OnlineDirectory::unbindSession(protocol::SessionID sessionId) {
    const auto playerId = sessionLocator_.find_player_by_session(sessionId);
    if (!playerId.has_value()) {
        return false;
    }
    sessionLocator_.unbind_session(sessionId);
    sessionLocator_.unbind_player(*playerId);
    directory_.session_down(*playerId);
    return true;
}

bool OnlineDirectory::reconcileDirectory(const std::vector<PlayerID>& reported_online) const {
    return directory_.reconcile(reported_online);
}

std::optional<PlayerID> OnlineDirectory::findPlayerBySession(protocol::SessionID sessionId) const {
    return sessionLocator_.find_player_by_session(sessionId);
}

std::optional<apollo::game::base::WorldAssignment> OnlineDirectory::resolveWorldAssignment(
    PlayerID playerId,
    protocol::SessionID sessionId
) const {
    if (playerId == 0 && sessionId != 0) {
        const auto resolved = sessionLocator_.find_player_by_session(sessionId);
        if (!resolved.has_value()) {
            return std::nullopt;
        }
        playerId = *resolved;
    }

    std::lock_guard<std::mutex> lock(assignmentsMutex_);
    const auto it = assignments_.find(playerId);
    if (it == assignments_.end() || !it->second.is_assigned()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<apollo::game::base::SessionBinding> OnlineDirectory::resolveSessionBinding(
    PlayerID playerId,
    protocol::SessionID sessionId
) const {
    if (sessionId != 0) {
        const auto resolved = sessionLocator_.find_player_by_session(sessionId);
        if (resolved.has_value()) {
            const auto binding = sessionLocator_.find_by_player(*resolved);
            if (binding.has_value()) {
                return binding;
            }
        }
    }

    if (playerId == 0) {
        return std::nullopt;
    }

    return sessionLocator_.find_by_player(playerId);
}

void OnlineDirectory::start() {
    if (running_) return;

    running_ = true;

    // M1 控制面（P4-4 批④）：m1_port_ 非零 = link M1 面（dispatchRequest
    // 直通——nng REP 已随批⑤退役[桩传输死面]）；装配失败降级（警告 + 不拦
    // 启动，控制面缺席）。
    if (m1_enabled_) {
        M1ControlServerConfig m1Config;
        m1Config.host = host_;
        m1Config.port = m1_port_;
        m1Config.route_json = m1_route_json_;
        m1Server_ = makeM1ControlServer(
            std::move(m1Config), [this](const std::uint8_t* frame, std::size_t size) {
                return dispatchRequest(std::vector<std::uint8_t>(frame, frame + size));
            });
        if (m1Server_->start()) {
            apollo::core::log::global_log_manager()
                .createLogger("login")
                ->info("M1 control face listening on " + host_ + ":"
                       + std::to_string(m1Server_->listenPort()));
        } else {
            apollo::core::log::global_log_manager()
                .createLogger("login")
                ->warning("M1 control face degraded: route table unavailable ("
                          + m1_route_json_ + "), nng transport stays");
            m1Server_.reset();
        }
    }

    apollo::core::log::global_log_manager()
        .createLogger("login")
        ->info("OnlineDirectory listening on " + host_ + ":" + std::to_string(port_));
}

void OnlineDirectory::stop() {
    running_ = false;

    // M1 面先收线（泵线程停驱 + 监听拆线）。
    if (m1Server_) {
        m1Server_->stop();
        m1Server_.reset();
    }
}

std::vector<uint8_t> OnlineDirectory::dispatchRequest(const std::vector<uint8_t>& data) {
    auto header = protocol::MessageCodec::parseHeader(data);
    auto msgType = static_cast<protocol::MessageType>(header.type);

    switch (msgType) {
        case protocol::MessageType::PLAYER_ASSIGN_WORLD_REQUEST:
            return handlePlayerAssignWorldRequest(data);

        case protocol::MessageType::PLAYER_RESOLVE_ROUTE_REQUEST:
            return handlePlayerResolveRouteRequest(data);

        // 玩家迁移编排命令（P0-2 路 2；ops/fleetctl → manager 执行面。
        // TYPE 钉生成器 MsgId（legacy 枚举不含 internal 族），case 标签回投）
        case static_cast<protocol::MessageType>(migration::MigrateRequest::TYPE):
            return handlePlayerMigrateRequest(data);

        case protocol::MessageType::PING:
            return handlePing(data);

        default:
            protocol::ErrorMessage err;
            err.code = static_cast<uint32_t>(protocol::MessageType::ERROR);
            err.message = "Unknown message type";
            return protocol::MessageCodec::encode(err, header.sessionId);
    }
}

std::vector<uint8_t> OnlineDirectory::handlePlayerAssignWorldRequest(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto assignReq = protocol::MessageCodec::decodeBody<protocol::PlayerAssignWorldRequest>(bodyData);

    protocol::PlayerAssignWorldResponse response;
    response.success = false;
    response.routeVersion = assignReq.routeVersion;

    apollo::game::base::WorldAssignment assignment;
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

std::vector<uint8_t> OnlineDirectory::handlePlayerResolveRouteRequest(const std::vector<uint8_t>& request) {
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

std::vector<uint8_t> OnlineDirectory::handlePing(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto ping = protocol::MessageCodec::decodeBody<protocol::Ping>(bodyData);

    protocol::Pong pong;
    pong.timestamp = ping.timestamp;

    return protocol::MessageCodec::encode(pong, header.sessionId);
}

// 编排命令入口（942→943；ops/fleetctl 同步等 943，耗时字段原样回带——
// §9.8 单玩家切换耗时实测即此面出数）。解析失败回 ErrorMessage；migratePlayer
// 的失败语义（含 ③ 后不可撤销）全在编排函数内，此层只转译。
// 迁移标识幂等（P3-6）：同 (player, request_id) 重放 = 缓存成功回执原样回
// 带（不重跑编排——重复编排会二次冻结源并二次接管目标）；失败不缓存，
// request_id = 0 不进账（见成员注）。
std::vector<uint8_t> OnlineDirectory::handlePlayerMigrateRequest(
    const std::vector<uint8_t>& request) {
    protocol::SessionID session = 0;
    migration::MigrateRequest req;
    if (!migration::decodeFrame(request, req, session)) {
        protocol::ErrorMessage err;
        err.code = static_cast<uint32_t>(protocol::MessageType::ERROR);
        err.message = "Bad player migrate request";
        return protocol::MessageCodec::encode(
            err, protocol::MessageCodec::parseHeader(request).sessionId);
    }

    if (req.request_id != 0) {
        const auto it = migrateReplies_.find(req.player_id);
        if (it != migrateReplies_.end() && it->second.first == req.request_id) {
            return it->second.second;
        }
    }

    const auto res = migratePlayer(req.player_id, req.source_host,
                                   static_cast<std::uint16_t>(req.source_port),
                                   req.target_host,
                                   static_cast<std::uint16_t>(req.target_port),
                                   req.target_zone_id);

    migration::MigrateReply reply;
    reply.request_id = req.request_id;
    reply.player_id = req.player_id;
    reply.ok = res.ok;
    reply.new_epoch = res.new_epoch;
    reply.error_message = res.error_message;
    reply.freeze_ms = res.freeze_ms;
    reply.handoff_ms = res.handoff_ms;
    reply.total_ms = res.total_ms;
    apollo::core::log::global_log_manager().createLogger("login")
        ->info("migrate player " + std::to_string(req.player_id)
               + " -> zone " + std::to_string(req.target_zone_id)
               + (res.ok ? std::string(" ok total_ms=") : std::string(" failed (")
                  + res.error_message + ") total_ms=")
               + std::to_string(res.total_ms));
    std::vector<uint8_t> out = migration::encodeFrame(reply, session);
    if (res.ok && req.request_id != 0) {
        migrateReplies_[req.player_id] = {req.request_id, out};
    }
    return out;
}

int64_t OnlineDirectory::getCurrentTimeMs() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

// ---- 玩家迁移编排（P0-2 路 2 五步，backup-revive §9.2；五步严格按序——
// 前一步未确认完成，后一步不许开始）----

void OnlineDirectory::set_snapshot_publisher(
    std::shared_ptr<apollo::game::session::DirectoryPublisher> publisher) {
    snapshot_publisher_ = std::move(publisher);
}

std::vector<apollo::game::session::MirrorEntry> OnlineDirectory::collectMirrorEntries() const {
    std::vector<apollo::game::session::MirrorEntry> entries;
    entries.reserve(directory_.size());
    for (const auto player_id : directory_.online_ids()) {
        const auto* entry = directory_.find(player_id);
        if (entry == nullptr) {
            continue;
        }
        apollo::game::session::MirrorEntry me;
        me.player_id = player_id;
        me.anchor_epoch = entry->anchor_epoch;
        me.session_id = entry->binding.session_id;
        me.gateway_id = entry->binding.gateway_id;
        me.zone_id = entry->zone_id;
        me.state = entry->state;
        me.assignment = entry->assignment;
        entries.push_back(me);
    }
    return entries;
}

OnlineDirectory::MigrationResult OnlineDirectory::migratePlayer(
    std::uint64_t player_id,
    const std::string& source_host, std::uint16_t source_port,
    const std::string& target_host, std::uint16_t target_port,
    std::uint32_t target_zone_id) {
    MigrationResult out;
    const auto t0 = std::chrono::steady_clock::now();
    auto msSince = [&t0]() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - t0).count();
    };

#if !APOLLO_NET_LINK_HAS_JSON
    (void)player_id; (void)source_host; (void)source_port;
    (void)target_host; (void)target_port; (void)target_zone_id;
    out.error_message = "migration dial unavailable (no json)";
    return out;
#else
    // 前置：目录条目在册 + 参数齐（zone 反查键 0 = 未声明，拒——同
    // mark_suspended_by_zone 口径）
    const auto* entry = directory_.find(player_id);
    if (entry == nullptr) {
        out.error_message = "unknown player in directory";
        return out;
    }
    if (target_zone_id == 0) {
        out.error_message = "target zone id required";
        return out;
    }
    if (source_port == 0 || target_port == 0) {
        out.error_message = "source/target m1 address required";
        return out;
    }
    const std::uint64_t oldEpoch = entry->anchor_epoch;
    const std::uint64_t sessionId = entry->binding.session_id;
    const std::uint32_t gatewayId = entry->binding.gateway_id;
    const auto assignment = entry->assignment;
    const std::uint64_t requestId = migrateSeq_++;

    // ① 停写（933 → 934 accepted）
    {
        migration::FreezeRequest req;
        req.player_id = player_id;
        req.request_id = requestId;
        player_store::StoreDial dial(m1_route_json_, "login", "world",
                            source_host, source_port);
        auto reply = dial.ready() && dial.waitUp()
                         ? dial.call(migration::encodeFrame(req))
                         : std::nullopt;
        if (!reply) {
            out.error_message = "freeze: source unreachable or timeout";
            return out;
        }
        migration::FreezeReply fr;
        protocol::SessionID session = 0;
        if (!migration::decodeFrame(*reply, fr, session) || !fr.accepted) {
            out.error_message = "freeze rejected: "
                + (fr.error_message.empty() ? std::string("undecodable reply")
                                            : fr.error_message);
            return out;
        }
    }
    out.freeze_ms = msSince();

    // ③ 交写权（937 → 938 accepted；epoch = 旧+1，§9.2 ③；带源地址——
    // 新 zone 现场拉 935/936，快的已先 flush 再发）
    const std::uint64_t newEpoch = oldEpoch + 1;
    {
        migration::HandoffRequest req;
        req.player_id = player_id;
        req.request_id = requestId;
        req.new_epoch = newEpoch;
        req.session_id = sessionId;
        req.gateway_id = gatewayId;
        req.source_host = source_host;
        req.source_port = source_port;
        player_store::StoreDial dial(m1_route_json_, "login", "world",
                            target_host, target_port);
        auto reply = dial.ready() && dial.waitUp()
                         ? dial.call(migration::encodeFrame(req))
                         : std::nullopt;
        if (!reply) {
            out.error_message = "handoff: target unreachable or timeout";
            return out;
        }
        migration::HandoffReply hr;
        protocol::SessionID session = 0;
        if (!migration::decodeFrame(*reply, hr, session) || !hr.accepted) {
            out.error_message = "handoff rejected: "
                + (hr.error_message.empty() ? std::string("undecodable reply")
                                            : hr.error_message);
            return out;  // ③ 后不可撤销面——失败停留 Transferring（§9.2 人工介入）
        }
    }
    out.handoff_ms = msSince();

    // ④ 目录改指 + 立即推快照（§9.2 ④：不许等 10s 快照周期）
    if (!directory_.moved(player_id, assignment, newEpoch, target_zone_id)) {
        out.error_message = "directory moved rejected";
        return out;
    }
    std::size_t pushed_parts = 0;
    if (snapshot_publisher_) {
        pushed_parts = snapshot_publisher_->publish_snapshot(collectMirrorEntries());
    }
    apollo::core::log::global_log_manager().createLogger("login")
        ->info("directory rehome: player " + std::to_string(player_id)
               + " -> zone " + std::to_string(target_zone_id)
               + " epoch=" + std::to_string(newEpoch)
               + " snapshot_parts=" + std::to_string(pushed_parts));

    out.new_epoch = newEpoch;
    out.new_zone_id = target_zone_id;
    out.ok = true;
    out.total_ms = msSince();
    return out;
#endif
}

} // namespace online_directory
