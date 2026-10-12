#pragma once

// 迁移快照拉取客户端（P0-2 路 2 五步②：新 zone → 老 zone，契约 935/936）。
// 拨号底座 = ZoneDial（同目录）——本件只承载 935/936 请求应答语义。

#include "base/migration_wire.hpp"
#include "base/zone_dial.hpp"

#if APOLLO_NET_LINK_HAS_JSON

namespace base {
namespace migration {

class MigrationPuller {
public:
    MigrationPuller(const std::string& route_json, const std::string& host,
                    std::uint16_t port)
        : dial_(route_json, "base-migration", "base", host, port) {}

    bool ready() const { return dial_.ready(); }
    bool waitUp() const { return dial_.waitUp(); }
    void shutdown() { dial_.shutdown(); }

    // 935 → 936：快照拉取（解码/超时/断链统一收为 success=false）
    SnapshotData pull(std::uint64_t player_id, std::uint64_t request_id) {
        SnapshotData out;
        if (!dial_.ready()) {
            out.error_message = "puller not ready";
            return out;
        }
        SnapshotRequest req;
        req.player_id = player_id;
        req.request_id = request_id;
        auto reply = dial_.call(encodeFrame(req));
        if (!reply) {
            out.error_message = "snapshot request timed out";
            return out;
        }
        SnapshotReply msg;
        protocol::SessionID session = 0;
        if (!decodeFrame(*reply, msg, session)) {
            out.error_message = "snapshot reply undecodable";
            return out;
        }
        out.success = msg.success;
        out.error_message = msg.error_message;
        out.profile_json = std::move(msg.profile_json);
        out.session_id = msg.session_id;
        out.gateway_id = msg.gateway_id;
        out.home_zone_id = msg.home_zone_id;
        return out;
    }

private:
    ZoneDial dial_;
};

} // namespace migration
} // namespace base

#endif // APOLLO_NET_LINK_HAS_JSON
