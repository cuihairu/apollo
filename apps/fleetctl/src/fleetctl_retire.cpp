#include "fleetctl_internal.hpp"

namespace fleetctl {

namespace {

std::atomic<std::uint64_t> next_retire_id{1};  // 0 保留（对端静默面）

struct RetireReply {
    bool accepted = false;
    std::int32_t code = 0;
    std::uint64_t cell_id = 0;
    std::int64_t management_epoch = 0;
    std::string detail;
};

struct RetireCapture {
    std::mutex m;
    std::map<std::uint64_t, RetireReply> replies;
};
} // namespace

// 缩容编排（CellAppMgr 缩容语义的命令面）：begin 置 World 单方缩容意图——
// 目标停止认领新 Space、名下归属回池，在途活动在 cell 本地自然排空
//（不强断）；--cancel 只清意图，已被他人重认领的不取回。命令是意图下达，
// 不等待排空收敛——收敛状态经 Space 分配视图观测。op 字面量 = 963 契约
// 词汇（1=begin 2=cancel），与 world 受理面同口径。
int runRetire(const Options& opt) {
    if (opt.targets.empty()) {
        std::cerr << "fleetctl: retire requires --target H:P" << std::endl;
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
    RetireCapture capture;
    dial.set_frame_handler(
        id_of(MsgId::cell_retire_reply),
        [&capture, reply_id = id_of(MsgId::cell_retire_reply)](
            std::uint16_t, const std::uint8_t* data, std::size_t size) {
            std::vector<wire::FieldValue> fields;
            if (!wire::decode_payload(reply_id, data, size, fields)
                || fields.size() != 6) {
                return;
            }
            RetireReply r;
            r.accepted = fields[1].integer != 0;
            r.code = static_cast<std::int32_t>(fields[2].integer);
            r.cell_id = static_cast<std::uint64_t>(fields[4].integer);
            r.management_epoch = fields[3].integer;
            r.detail = fields[5].text;
            const auto request_id = static_cast<std::uint64_t>(fields[0].integer);
            std::lock_guard<std::mutex> lock(capture.m);
            capture.replies[request_id] = r;
        });
    if (!dial.waitUp()) {
        std::cout << "RETIRE_UNREACHABLE target=" << label << std::endl;
        return 3;
    }

    // op：1=begin（置意图） 2=cancel（撤意图）
    const std::int32_t op = opt.retire_cancel ? 2 : 1;
    const auto request_id = next_retire_id.fetch_add(1, std::memory_order_relaxed);
    std::vector<wire::FieldValue> fields(3);
    fields[0].integer = static_cast<std::int64_t>(request_id);
    fields[1].integer = op;
    fields[2].integer = static_cast<std::int64_t>(opt.cell_id);
    std::vector<std::uint8_t> payload;
    if (!wire::encode_payload(id_of(MsgId::cell_retire_request), fields, payload)
        || !dial.send_frame(id_of(MsgId::cell_retire_request), payload)) {
        std::cout << "RETIRE_UNREACHABLE target=" << label << " (send failed)"
                  << std::endl;
        return 3;
    }
    const auto deadline = Clock::now() + kRpcTimeout;
    RetireReply r;
    bool got = false;
    while (Clock::now() < deadline) {
        {
            std::lock_guard<std::mutex> lock(capture.m);
            const auto it = capture.replies.find(request_id);
            if (it != capture.replies.end()) {
                r = it->second;
                got = true;
                break;
            }
        }
        std::this_thread::sleep_for(kPumpTick);
    }
    if (!got) {
        std::cout << "RETIRE_UNREACHABLE target=" << label << " (no reply)"
                  << std::endl;
        return 3;
    }
    if (!r.accepted) {
        std::cout << "RETIRE_REJECTED target=" << label << " cell=" << r.cell_id
                  << " code=" << r.code << " detail=" << r.detail << std::endl;
        return 2;
    }
    std::cout << (r.code == 2 ? "RETIRE_CANCELLED " : "RETIRE_BEGUN ")
              << "target=" << label << " cell=" << r.cell_id
              << " epoch=" << r.management_epoch << " detail=" << r.detail
              << std::endl;
    return 0;
}

} // namespace fleetctl
