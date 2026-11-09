#include "fleetctl_internal.hpp"

namespace fleetctl {

namespace {

// 953 回执捕获（泵线程写 / 主线程读；request_id 关联——stale 回执忽略）。
struct RolloutCapture {
    std::mutex m;
    struct Reply {
        bool ok = false;
        std::int32_t state = 0;
        std::uint64_t window_faults = 0;
        std::uint64_t module_version = 0;
        std::string detail;
    };
    std::map<std::uint64_t, Reply> replies;
};

// 编排目标（一个 World 一条拨号）：canary 先行旗 + 裸帧面（952 发 / 953
// 收）；拨号失败 ready false = 目标不可达（exit 3）。
struct RolloutPeer {
    std::string label;  // H:P
    bool canary = false;
    std::unique_ptr<ControlDial> dial;
    RolloutCapture capture;
};

std::atomic<std::uint64_t> next_rollout_id{1};  // 0 保留（对端静默面）

// 单发 952 + 等 953（受理点同步回执——M1 泵在 world gameThread_，kRpcTimeout
// 含其 tick 一拍）。ok=false 时 detail 带回。source = 解析后的模块源
//（--source-file 已读入 / --source 原文）。
bool rollout_call(RolloutPeer& p, std::int32_t op, const Options& opt,
                  const std::string& source, RolloutCapture::Reply& out) {
    const std::uint64_t request_id = next_rollout_id.fetch_add(1, std::memory_order_relaxed);
    std::vector<wire::FieldValue> fields(8);
    fields[0].integer = static_cast<std::int64_t>(request_id);
    fields[1].integer = op;
    fields[2].text = opt.module_name;
    fields[3].integer = static_cast<std::int64_t>(opt.module_version);
    fields[4].text = opt.module_deps;
    fields[5].text = source;
    fields[6].integer = static_cast<std::int64_t>(opt.observe_ticks);
    fields[7].integer = static_cast<std::int64_t>(opt.fault_threshold);
    std::vector<std::uint8_t> payload;
    if (!wire::encode_payload(id_of(MsgId::module_rollout_request), fields, payload)
        || !p.dial->send_frame(id_of(MsgId::module_rollout_request), payload)) {
        return false;
    }
    const auto deadline = Clock::now() + kRpcTimeout;
    while (Clock::now() < deadline) {
        {
            std::lock_guard<std::mutex> lock(p.capture.m);
            const auto it = p.capture.replies.find(request_id);
            if (it != p.capture.replies.end()) {
                out = it->second;
                return true;
            }
        }
        std::this_thread::sleep_for(kPumpTick);
    }
    return false;
}

// 状态轮询到终态（kPromoted=6 / kRolledBack=7；rollout_timeout_s 上限）。
// 回 false = 超时未终态（exit 3 面）。out = 末次 status 快照。
bool rollout_wait_terminal(RolloutPeer& p, const Options& opt,
                           const std::string& source, RolloutCapture::Reply& out) {
    const auto deadline = Clock::now() + std::chrono::seconds(opt.rollout_timeout_s);
    while (Clock::now() < deadline) {
        RolloutCapture::Reply snap;
        if (!rollout_call(p, 4, opt, source, snap)) {
            return false;
        }
        out = snap;
        if (snap.state == 6 || snap.state == 7) {
            return true;
        }
        std::this_thread::sleep_for(100ms);  // 观察窗 tick 量级，轮询不必密
    }
    return false;
}

void rollout_print_target(const RolloutPeer& p, const RolloutCapture::Reply& r,
                          const char* tag) {
    std::cout << tag << " target=" << p.label << " state=" << r.state
              << " version=" << r.module_version
              << " faults=" << r.window_faults;
    if (!r.detail.empty()) {
        std::cout << " detail=" << r.detail;
    }
    std::cout << std::endl;
}
} // namespace

// 跨 World 模块编排（P4 仅代码更新；eval 944 同型裸帧面，一套 conductor 对
// N 个 World 各开一条拨号）：canary 集先行 prepare+commit，观察窗终态裁决
// ——kRolledBack 即止（余下目标未触碰，故障半径=灰度集）；kPromoted 再推
// 余下目标逐个到终态。源码默认从 --source-file 读，其次 --source。
int runModule(const Options& opt) {
    if (opt.targets.empty() || opt.module_name.empty() || opt.module_version == 0) {
        std::cerr << "fleetctl: module requires --target H:P (repeatable), --name,"
                     " --version" << std::endl;
        return 1;
    }
    std::string source = opt.source;
    if (!opt.source_file.empty()) {
        std::ifstream in(opt.source_file, std::ios::binary);
        std::ostringstream buffer;
        if (!in) {
            std::cerr << "fleetctl: cannot read --source-file " << opt.source_file
                      << std::endl;
            return 1;
        }
        buffer << in.rdbuf();
        source = buffer.str();
    }
    if (source.empty()) {
        std::cerr << "fleetctl: module requires --source-file F or --source SRC"
                  << std::endl;
        return 1;
    }

    // unique_ptr 持有（RolloutCapture 含 mutex 不可移动；vector 扩容需移动）
    std::vector<std::unique_ptr<RolloutPeer>> peers;
    for (const auto& label : opt.targets) {
        const auto colon = label.rfind(':');
        if (colon == std::string::npos) {
            std::cerr << "fleetctl: bad --target " << label << " (want H:P)"
                      << std::endl;
            return 1;
        }
        const auto port = static_cast<std::uint16_t>(
            std::strtoul(label.c_str() + colon + 1, nullptr, 10));
        if (port == 0) {
            std::cerr << "fleetctl: bad --target " << label << " (want H:P)"
                      << std::endl;
            return 1;
        }
        auto p = std::make_unique<RolloutPeer>();
        p->label = label;
        p->dial = std::make_unique<ControlDial>(
            APOLLO_FLEETCTL_CONTRACT_ROUTE_JSON, opt.peer /* world */,
            label.substr(0, colon), port);
        if (!p->dial->ready()) {
            std::cerr << "fleetctl: control face setup failed for " << label
                      << std::endl;
            return 1;
        }
        peers.push_back(std::move(p));
    }
    // 灰度子集（旗标按 target 顺序落；缺省 = 首个 target）
    std::size_t canary_count = opt.canaries.empty() ? 1 : opt.canaries.size();
    if (canary_count > peers.size()) {
        canary_count = peers.size();
    }
    for (std::size_t i = 0; i < peers.size(); ++i) {
        peers[i]->canary = i < canary_count;
    }

    // 953 回执挂接（kUp 前挂——先挂后发无竞速窗，runEval 同理）
    for (auto& peer : peers) {
        RolloutCapture* cap = &peer->capture;
        peer->dial->set_frame_handler(
            id_of(MsgId::module_rollout_reply),
            [cap](std::uint16_t, const std::uint8_t* data, std::size_t size) {
                std::vector<wire::FieldValue> fields;
                if (!wire::decode_payload(id_of(MsgId::module_rollout_reply), data, size,
                                          fields)
                    || fields.size() != 7) {
                    return;
                }
                RolloutCapture::Reply r;
                r.ok = fields[2].integer != 0;
                r.state = static_cast<std::int32_t>(fields[3].integer);
                r.window_faults = static_cast<std::uint64_t>(fields[4].integer);
                r.module_version = static_cast<std::uint64_t>(fields[5].integer);
                r.detail = fields[6].text;
                const std::uint64_t request_id =
                    static_cast<std::uint64_t>(fields[0].integer);
                std::lock_guard<std::mutex> lock(cap->m);
                cap->replies[request_id] = r;
            });
    }
    for (auto& peer : peers) {
        if (!peer->dial->waitUp()) {
            std::cout << "MODULE_UNREACHABLE target=" << peer->label << std::endl;
            return 3;
        }
    }

    // ① prepare 全量（受理即 kCanaryPending；任一败 = 零目标被触碰即止）
    for (auto& peer : peers) {
        RolloutPeer& p = *peer;
        RolloutCapture::Reply r;
        if (!rollout_call(p, 1, opt, source, r)) {
            std::cout << "MODULE_UNREACHABLE target=" << p.label
                      << " (no prepare reply)" << std::endl;
            return 3;
        }
        if (!r.ok) {
            rollout_print_target(p, r, "MODULE_PREPARE_FAILED");
            return 2;
        }
    }

    // ② canary 集提交 + 观察窗到终态
    for (auto& peer : peers) {
        if (!peer->canary) {
            continue;
        }
        RolloutPeer& p = *peer;
        RolloutCapture::Reply r;
        if (!rollout_call(p, 2, opt, source, r)) {
            std::cout << "MODULE_UNREACHABLE target=" << p.label
                      << " (no commit reply)" << std::endl;
            return 3;
        }
        if (!r.ok) {
            rollout_print_target(p, r, "MODULE_COMMIT_FAILED");
            return 2;
        }
    }
    std::vector<RolloutCapture::Reply> terminal(peers.size());
    for (std::size_t i = 0; i < peers.size(); ++i) {
        if (!peers[i]->canary) {
            continue;
        }
        if (!rollout_wait_terminal(*peers[i], opt, source, terminal[i])) {
            std::cout << "MODULE_OBSERVE_TIMEOUT target=" << peers[i]->label
                      << " state=" << terminal[i].state << std::endl;
            return 3;
        }
        if (terminal[i].state == 7) {
            rollout_print_target(*peers[i], terminal[i], "MODULE_TARGET_ROLLED_BACK");
            std::cout << "MODULE_ROLLED_BACK canary=" << peers[i]->label << std::endl;
            // 灰度即止 = 故障半径收口：其余目标只预备未提交，清理其预备轮
            //（世界侧 op3 abort 复位），不留半开状态阻断下一轮发布。
            for (auto& peer : peers) {
                if (peer->canary) {
                    continue;
                }
                RolloutCapture::Reply ar;
                if (!rollout_call(*peer, 3, opt, source, ar) || !ar.ok) {
                    std::cout << "MODULE_ABORT_FAILED target=" << peer->label
                              << " detail=" << ar.detail << std::endl;
                }
            }
            return 2;
        }
        rollout_print_target(*peers[i], terminal[i], "MODULE_TARGET_OK");
    }

    // ③ 灰度全绿 → 余下目标逐个提交到终态
    for (std::size_t i = 0; i < peers.size(); ++i) {
        if (peers[i]->canary) {
            continue;
        }
        RolloutCapture::Reply r;
        if (!rollout_call(*peers[i], 2, opt, source, r) || !r.ok) {
            std::cout << "MODULE_COMMIT_FAILED target=" << peers[i]->label
                      << " state=" << r.state << " detail=" << r.detail << std::endl;
            return 2;
        }
        if (!rollout_wait_terminal(*peers[i], opt, source, terminal[i])) {
            std::cout << "MODULE_OBSERVE_TIMEOUT target=" << peers[i]->label
                      << " state=" << terminal[i].state << std::endl;
            return 3;
        }
        if (terminal[i].state == 7) {
            rollout_print_target(*peers[i], terminal[i], "MODULE_TARGET_ROLLED_BACK");
            std::cout << "MODULE_ROLLED_BACK target=" << peers[i]->label << std::endl;
            return 2;
        }
        rollout_print_target(*peers[i], terminal[i], "MODULE_TARGET_OK");
    }

    std::cout << "MODULE_OK targets=" << peers.size()
              << " version=" << opt.module_version << std::endl;
    return 0;
}

} // namespace fleetctl
