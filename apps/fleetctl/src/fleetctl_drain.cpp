#include "fleetctl_internal.hpp"

namespace fleetctl {

namespace {

struct DrainCapture {
    std::mutex m;
    struct Reply {
        bool ok = false;
        std::int32_t state = 0;
        std::uint64_t active = 0;
        std::string detail;
    };
    std::map<std::uint64_t, Reply> replies;
};

std::atomic<std::uint64_t> next_drain_id{1};  // 0 保留（对端静默面）

// 排空 wire 面（按对端组件选契约对：gateway=954/955，world=956/957；回执
// 字段同构——6 字段，active 计数量纲随对端为连接数/副本数）
struct DrainWire {
    std::uint16_t request_id;
    std::uint16_t reply_id;
};

DrainWire drain_wire_for(const std::string& peer) {
    if (peer == "world") {
        return {id_of(MsgId::world_drain_request), id_of(MsgId::world_drain_reply)};
    }
    return {id_of(MsgId::gateway_drain_request), id_of(MsgId::gateway_drain_reply)};
}

// 请求单发 + 回执等待（一问一答；ControlDial 自带泵线程，本函数只等）
bool drain_call(ControlDial& dial, DrainCapture& capture, const DrainWire& wire,
                std::int32_t op, std::uint64_t deadline_s, DrainCapture::Reply& out) {
    const auto request_id = next_drain_id.fetch_add(1, std::memory_order_relaxed);
    std::vector<wire::FieldValue> fields(3);
    fields[0].integer = static_cast<std::int64_t>(request_id);
    fields[1].integer = op;
    fields[2].integer = static_cast<std::int64_t>(deadline_s);
    std::vector<std::uint8_t> payload;
    if (!wire::encode_payload(wire.request_id, fields, payload)
        || !dial.send_frame(wire.request_id, payload)) {
        return false;
    }
    const auto deadline = Clock::now() + kRpcTimeout;
    while (Clock::now() < deadline) {
        {
            std::lock_guard<std::mutex> lock(capture.m);
            const auto it = capture.replies.find(request_id);
            if (it != capture.replies.end()) {
                out = it->second;
                return true;
            }
        }
        std::this_thread::sleep_for(kPumpTick);
    }
    return false;
}
} // namespace

// 排空编排（session-and-online-directory §2）：begin 关闸停新（gateway 关
// 监听停新连接 / world 闭排空闸停分新副本），既有面服务至自然退出
//（--deadline 0）或明确期限（期限到强停——结果显形在 drained 回执 detail）。
// 编排面轮询 status 到 drained 终态；进程替换归 ops 面后续步骤（服务不
// 自退，排空态可观测）。--peer world 拨 World M1 面（956/957），缺省
// gateway（954/955）。
int runDrain(const Options& opt) {
    if (opt.targets.empty()) {
        std::cerr << "fleetctl: drain requires --target H:P" << std::endl;
        return 1;
    }
    const auto& label = opt.targets[0];
    const auto colon = label.rfind(':');
    const auto port = colon == std::string::npos
                          ? 0
                          : static_cast<std::uint16_t>(
                              std::strtoul(label.c_str() + colon + 1, nullptr, 10));
    if (colon == std::string::npos || port == 0) {
        std::cerr << "fleetctl: bad --target " << label << " (want H:P)" << std::endl;
        return 1;
    }

    ControlDial dial(APOLLO_FLEETCTL_CONTRACT_ROUTE_JSON, opt.peer,
                     label.substr(0, colon), port);
    if (!dial.ready()) {
        std::cerr << "fleetctl: control face setup failed (route table missing)"
                  << std::endl;
        return 1;
    }
    const DrainWire wire = drain_wire_for(opt.peer);
    DrainCapture capture;
    dial.set_frame_handler(
        wire.reply_id,
        [&capture, reply_id = wire.reply_id](
            std::uint16_t, const std::uint8_t* data, std::size_t size) {
            std::vector<wire::FieldValue> fields;
            if (!wire::decode_payload(reply_id, data, size, fields)
                || fields.size() != 6) {
                return;
            }
            DrainCapture::Reply r;
            r.ok = fields[2].integer != 0;
            r.state = static_cast<std::int32_t>(fields[3].integer);
            r.active = static_cast<std::uint64_t>(fields[4].integer);
            r.detail = fields[5].text;
            const auto request_id = static_cast<std::uint64_t>(fields[0].integer);
            std::lock_guard<std::mutex> lock(capture.m);
            capture.replies[request_id] = r;
        });
    if (!dial.waitUp()) {
        std::cout << "DRAIN_UNREACHABLE target=" << label << std::endl;
        return 3;
    }

    DrainCapture::Reply r;
    if (!drain_call(dial, capture, wire, 1, opt.drain_deadline_s, r)) {
        std::cout << "DRAIN_UNREACHABLE target=" << label << " (no begin reply)"
                  << std::endl;
        return 3;
    }
    if (!r.ok) {
        std::cout << "DRAIN_REJECTED target=" << label << " detail=" << r.detail
                  << std::endl;
        return 2;
    }
    std::cout << "DRAIN_BEGIN target=" << label << " state=" << r.state
              << " active=" << r.active << std::endl;

    // 轮询 status 到 drained（2）终态；--timeout 面内不出终态 = 超时退出
    const auto deadline = Clock::now() + std::chrono::seconds(opt.rollout_timeout_s);
    while (Clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if (!drain_call(dial, capture, wire, 2, 0, r)) {
            std::cout << "DRAIN_UNREACHABLE target=" << label << " (no status reply)"
                      << std::endl;
            return 3;
        }
        if (!r.ok) {
            std::cout << "DRAIN_REJECTED target=" << label << " detail=" << r.detail
                      << std::endl;
            return 2;
        }
        if (r.state == 2) {
            std::cout << "DRAIN_DRAINED target=" << label << " detail=" << r.detail
                      << std::endl;
            return 0;
        }
    }
    std::cout << "DRAIN_TIMEOUT target=" << label << " state=" << r.state
              << " active=" << r.active << std::endl;
    return 3;
}

} // namespace fleetctl
