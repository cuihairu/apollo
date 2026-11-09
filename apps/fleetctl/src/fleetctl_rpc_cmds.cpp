#include "fleetctl_internal.hpp"

namespace fleetctl {

namespace {

// 945 回执捕获（泵线程写 / 主线程读；eval_id 关联——stale 回执忽略）。
struct EvalCapture {
    std::mutex m;
    std::uint64_t eval_id = 0;
    bool ok = false;
    std::string text;  // ok=1 → result；ok=0 → error 原文
    bool got = false;
};
} // namespace

int runAssign(const Options& opt, ControlDial& dial) {
    protocol::PlayerAssignWorldRequest req;
    req.playerId = opt.player_id;
    req.worldId = opt.world_id;
    req.mapId = opt.map_id;
    req.instanceId = opt.instance_id;
    req.spaceId = opt.space_id;
    req.routeVersion = opt.route_version;

    const auto reply = dial.call(protocol::MessageCodec::encode(req));
    if (!reply) {
        std::cout << "ASSIGN_UNREACHABLE player=" << opt.player_id << std::endl;
        return 3;
    }
    auto header = protocol::MessageCodec::parseHeader(*reply);
    if (static_cast<protocol::MessageType>(header.type)
        != protocol::MessageType::PLAYER_ASSIGN_WORLD_RESPONSE) {
        std::cout << "ASSIGN_UNREACHABLE player=" << opt.player_id
                  << " (bad reply type " << header.type << ")" << std::endl;
        return 3;
    }
    const auto res = protocol::MessageCodec::decodeBody<protocol::PlayerAssignWorldResponse>(
        std::vector<std::uint8_t>(reply->begin() + sizeof(protocol::MessageHeader),
                                  reply->end()));
    if (res.success) {
        std::cout << "ASSIGN_OK player=" << opt.player_id << " world=" << opt.world_id
                  << " route_version=" << res.routeVersion << std::endl;
        return 0;
    }
    std::cout << "ASSIGN_REJECTED player=" << opt.player_id
              << " error=" << res.errorMessage << std::endl;
    return 2;
}

int runResolve(const Options& opt, ControlDial& dial) {
    protocol::PlayerResolveRouteRequest req;
    req.playerId = opt.player_id;
    req.sessionId = 0;  // 按 playerId 反查

    const auto reply = dial.call(protocol::MessageCodec::encode(req));
    if (!reply) {
        std::cout << "RESOLVE_UNREACHABLE player=" << opt.player_id << std::endl;
        return 3;
    }
    auto header = protocol::MessageCodec::parseHeader(*reply);
    if (static_cast<protocol::MessageType>(header.type)
        != protocol::MessageType::PLAYER_RESOLVE_ROUTE_RESPONSE) {
        std::cout << "RESOLVE_UNREACHABLE player=" << opt.player_id
                  << " (bad reply type " << header.type << ")" << std::endl;
        return 3;
    }
    const auto res = protocol::MessageCodec::decodeBody<protocol::PlayerResolveRouteResponse>(
        std::vector<std::uint8_t>(reply->begin() + sizeof(protocol::MessageHeader),
                                  reply->end()));
    if (res.success) {
        std::cout << "RESOLVE_OK player=" << res.playerId
                  << " gateway=" << res.gatewayId << " addr=" << res.gatewayAddr
                  << " world=" << res.worldId << " map=" << res.mapId
                  << " instance=" << res.instanceId
                  << " route_version=" << res.routeVersion << std::endl;
        return 0;
    }
    std::cout << "RESOLVE_FAILED player=" << opt.player_id
              << " error=" << res.errorMessage << std::endl;
    return 2;
}

// 编排命令（P0-2 路 2 五步①③④ 入口；同步等 943——耗时字段即 §9.8 实测
// 出数面）。REQ/REPLY wire 面 = migration 手写族（TYPE 钉生成器 MsgId）。
int runMigrate(const Options& opt, ControlDial& dial) {
    migration::MigrateRequest req;
    req.player_id = opt.player_id;
    // 迁移标识（P3-6）：进程内单调（now_ms），目录按 (player, request_id)
    // 幂等——同标识重试回缓存回执，不重跑编排
    req.request_id = now_ms();
    req.source_host = opt.source_host;
    req.source_port = opt.source_port;
    req.target_host = opt.target_host;
    req.target_port = opt.target_port;
    req.target_zone_id = opt.target_zone_id;

    const auto reply = dial.call(migration::encodeFrame(req));
    if (!reply) {
        std::cout << "MIGRATE_UNREACHABLE player=" << opt.player_id << std::endl;
        return 3;
    }
    migration::MigrateReply res;
    protocol::SessionID session = 0;
    if (!migration::decodeFrame(*reply, res, session)) {
        std::cout << "MIGRATE_UNREACHABLE player=" << opt.player_id
                  << " (bad reply type "
                  << protocol::MessageCodec::parseHeader(*reply).type << ")"
                  << std::endl;
        return 3;
    }
    if (res.ok) {
        std::cout << "MIGRATE_OK player=" << res.player_id
                  << " zone=" << opt.target_zone_id
                  << " epoch=" << res.new_epoch
                  << " freeze_ms=" << res.freeze_ms
                  << " handoff_ms=" << res.handoff_ms
                  << " total_ms=" << res.total_ms << std::endl;
        return 0;
    }
    std::cout << "MIGRATE_FAILED player=" << res.player_id
              << " error=" << res.error_message
              << " total_ms=" << res.total_ms << std::endl;
    return 2;
}

// 脚本调试 eval（§7.1 attach transport 契约面）：944 裸帧直发，945 按
// eval_id 异步回传（Scene tick 边界执行——结果晚一拍，等回执而非 rpc 同步）。
int runEval(const Options& opt, ControlDial& dial, std::uint64_t eval_id) {
    EvalCapture cap;
    dial.set_frame_handler(
        id_of(MsgId::debug_eval_result),
        [&cap](std::uint16_t, const std::uint8_t* data, std::size_t size) {
            std::vector<wire::FieldValue> fields;
            if (!wire::decode_payload(id_of(MsgId::debug_eval_result), data, size, fields)
                || fields.size() != 4) {
                return;
            }
            std::lock_guard<std::mutex> lock(cap.m);
            cap.eval_id = static_cast<std::uint64_t>(fields[0].integer);
            cap.ok = fields[1].integer != 0;
            cap.text = fields[1].integer != 0 ? fields[2].text : fields[3].text;
            cap.got = true;
        });

    std::vector<wire::FieldValue> fields(3);
    fields[0].integer = static_cast<std::int64_t>(eval_id);
    fields[1].integer = static_cast<std::int64_t>(opt.scene_id);
    fields[2].text = opt.source;
    std::vector<std::uint8_t> payload;
    if (!wire::encode_payload(id_of(MsgId::debug_eval_request), fields, payload)
        || !dial.send_frame(id_of(MsgId::debug_eval_request), payload)) {
        std::cout << "EVAL_UNREACHABLE scene=" << opt.scene_id
                  << " (send failed)" << std::endl;
        return 3;
    }

    const auto deadline = Clock::now() + kRpcTimeout;  // 含 Scene tick 窗口的一拍
    while (Clock::now() < deadline) {
        {
            std::lock_guard<std::mutex> lock(cap.m);
            if (cap.got && cap.eval_id == eval_id) {
                if (cap.ok) {
                    std::cout << "EVAL_OK scene=" << opt.scene_id
                              << " result=" << cap.text << std::endl;
                    return 0;
                }
                std::cout << "EVAL_FAILED scene=" << opt.scene_id
                          << " error=" << cap.text << std::endl;
                return 2;
            }
        }
        std::this_thread::sleep_for(kPumpTick);
    }
    std::cout << "EVAL_UNREACHABLE scene=" << opt.scene_id
              << " (no reply within timeout)" << std::endl;
    return 3;
}

} // namespace fleetctl
