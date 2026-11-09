#include "world_app/world_app.hpp"
#include <chrono>

namespace world_app {

namespace {

// steady 单调毫秒（在途关联账时戳域；与 cellExpiryMs 同一单调域）。
std::uint64_t steady_now_ms() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

constexpr std::size_t kM1EvalBacklogMax = 64;
} // namespace

//==============================================================================
// M1 管理命令中继（944/952/956 受理转发 + 945/953/957/929 回程收账 +
// 在途关联账撤销与过期清扫）
//==============================================================================

void WorldApp::dropCellReplyAccountsForPeer(const std::string& peer) {
    // 在途关联账撤销（L101 断连存疑切片同上下文）：目标 cell 断连后永无
    // 回执——按 cell_peer 匹配撤账，并显式回「cell link lost」替代静默悬
    // 挂（eval/rollout/drain 三账同型）。账内条目本就应关联目标 cell；不
    // 匹配 peer 的条目保留（另一 cell 链路不受牵连）。
    auto drop = [&](auto& map, auto&& reply_fn) {
        for (auto it = map.begin(); it != map.end();) {
            if (it->second.cell_peer == peer) {
                reply_fn(it->second.fleetctl_peer, it->first);
                it = map.erase(it);
            } else {
                ++it;
            }
        }
    };
    drop(evalReplyPeers_, [this](const std::string& fp, std::uint64_t id) {
        sendM1EvalReply(fp, id, false, "", "cell link lost");
    });
    drop(rolloutReplyPeers_, [this](const std::string& fp, std::uint64_t id) {
        if (m1Link_) {
            m1Link_->sendModuleRolloutReply(fp, id, 0, false, -1, 0, 0,
                                            "cell link lost");
        }
    });
    drop(drainReplyPeers_, [this](const std::string& fp, std::uint64_t id) {
        if (m1Link_) {
            m1Link_->sendWorldDrainReply(fp, id, 0, false, 0, 0,
                                         "cell link lost");
        }
    });
}

void WorldApp::sweepExpiredCellReplyAccounts(std::uint64_t now_ms) {
    // 过期清扫（管理泵每拍，同注册表 driveCellRegistry 口径）：cell 回执
    // 在 cellExpiryMs 内未到即撤账（cell 死亡/回执丢失——断连面已撤，此
    // 面兜底挂起不回的链路级失效）。0 = 永不过期（同注册表面口径）。
    if (config_.cellExpiryMs == 0) {
        return;
    }
    auto sweep = [&](auto& map, auto&& reply_fn) {
        for (auto it = map.begin(); it != map.end();) {
            if (now_ms - it->second.sent_at_ms > config_.cellExpiryMs) {
                reply_fn(it->second.fleetctl_peer, it->first);
                it = map.erase(it);
            } else {
                ++it;
            }
        }
    };
    sweep(evalReplyPeers_, [this](const std::string& fp, std::uint64_t id) {
        sendM1EvalReply(fp, id, false, "", "eval reply timeout");
    });
    sweep(rolloutReplyPeers_, [this](const std::string& fp, std::uint64_t id) {
        if (m1Link_) {
            m1Link_->sendModuleRolloutReply(fp, id, 0, false, -1, 0, 0,
                                            "rollout reply timeout");
        }
    });
    sweep(drainReplyPeers_, [this](const std::string& fp, std::uint64_t id) {
        if (m1Link_) {
            m1Link_->sendWorldDrainReply(fp, id, 0, false, 0, 0,
                                         "drain reply timeout");
        }
    });
}

void WorldApp::onM1DebugEval(const std::string& peer, const DebugEvalRequest& request) {
    // 944 受理（§7.1 attach transport；管理泵上下文）：切片2 转发——World
    // 只承载管理命令，debug eval 经注册 cell 连接反向发送；cell 宿主处理后
    // 回 945（onCellEvalResult 按原 fleetctl peer 回传）。无合格 cell = 即时
    // 回执。
    if (request.eval_id == 0) {
        return;  // 0 保留（admin 关联键缺位，拒收面都不给）
    }
    if (evalReplyPeers_.size() >= kM1EvalBacklogMax) {
        sendM1EvalReply(peer, request.eval_id, false, "", "eval backlog full");
        return;
    }
    const auto cell_peer = firstRegisteredCellPeer();
    if (cell_peer.empty()
        || !m1Link_->sendCellDebugEvalRequest(cell_peer, request)) {
        sendM1EvalReply(peer, request.eval_id, false, "", "simulation not hosted");
        return;
    }
    evalReplyPeers_[request.eval_id] = PendingCellReply{
        peer, cell_peer, steady_now_ms()};
}

void WorldApp::onM1ModuleRollout(const std::string& peer,
                                 const ModuleRolloutRequest& request) {
    // 952 受理：切片2 转发——World 只承载管理命令，模块投递经注册 cell
    // 连接反向发送；cell 宿主处理后回 953（onCellRolloutReply 按原
    // fleetctl peer 回传）。无合格 cell = 即时回执。
    if (request.request_id == 0) {
        return;
    }
    const auto cell_peer = firstRegisteredCellPeer();
    if (cell_peer.empty()
        || !m1Link_->sendCellModuleRolloutRequest(cell_peer, request)) {
        m1Link_->sendModuleRolloutReply(peer, request.request_id, request.op,
                                        false, -1, 0, 0,
                                        "script runtime not ready");
        return;
    }
    rolloutReplyPeers_[request.request_id] = PendingCellReply{
        peer, cell_peer, steady_now_ms()};
}

void WorldApp::onM1WorldDrain(const std::string& peer,
                              const WorldDrainRequest& request) {
    // 956 受理：切片2 转发——World 只承载管理命令，排空命令经注册 cell
    // 连接反向发送；cell 宿主处理后回 957（onCellDrainReply 按原
    // fleetctl peer 回传）。无合格 cell = 即时回执。
    if (request.request_id == 0) {
        return;
    }
    const auto cell_peer = firstRegisteredCellPeer();
    if (cell_peer.empty()
        || !m1Link_->sendCellWorldDrainRequest(cell_peer, request)) {
        m1Link_->sendWorldDrainReply(peer, request.request_id, request.op,
                                     false, 0, 0, "squad service not ready");
        return;
    }
    drainReplyPeers_[request.request_id] = PendingCellReply{
        peer, cell_peer, steady_now_ms()};
}

void WorldApp::sendM1EvalReply(const std::string& peer, std::uint64_t eval_id, bool ok,
                                 const std::string& result, const std::string& error) {
    if (m1Link_) {
        m1Link_->sendEvalResult(peer, eval_id, ok, result, error);
    }
}

void WorldApp::onCellEvalResult(const std::string& /*peer*/,
                                const CellEvalResult& result) {
    // 切片2 转发收账：cell 宿主回 945（经注册连接），按 eval_id 关联原
    // fleetctl/admin peer 后回传。关联键缺位/未知 = 静默弃（尽力面）。
    const auto it = evalReplyPeers_.find(result.eval_id);
    if (it == evalReplyPeers_.end()) {
        return;
    }
    sendM1EvalReply(it->second.fleetctl_peer, result.eval_id, result.ok, result.result,
                    result.error);
    evalReplyPeers_.erase(it);
}

void WorldApp::onCellRolloutReply(const std::string& /*peer*/,
                                  const CellRolloutReply& reply) {
    const auto it = rolloutReplyPeers_.find(reply.request_id);
    if (it == rolloutReplyPeers_.end() || !m1Link_) {
        return;
    }
    m1Link_->sendModuleRolloutReply(it->second.fleetctl_peer, reply.request_id, reply.op,
                                    reply.ok, reply.state, reply.window_faults,
                                    reply.module_version, reply.detail);
    rolloutReplyPeers_.erase(it);
}

void WorldApp::onCellDrainReply(const std::string& /*peer*/,
                                const CellDrainReply& reply) {
    const auto it = drainReplyPeers_.find(reply.request_id);
    if (it == drainReplyPeers_.end() || !m1Link_) {
        return;
    }
    m1Link_->sendWorldDrainReply(it->second.fleetctl_peer, reply.request_id, reply.op,
                                 reply.ok, reply.state, reply.active_instances,
                                 reply.detail);
    drainReplyPeers_.erase(it);
}

void WorldApp::onCellIntentAck(const std::string& /*peer*/,
                               const CellIntentAck& ack) {
    // 切片2 意图转发回程：cell 宿主的 929 经 gateway_link() 转发 gateway
    //（sendIntentAck 已按 gateway_link 寻路）。0 关联键 = 拒收面静默。
    if (m1Link_ && ack.request_id != 0) {
        m1Link_->sendIntentAck(ack.request_id, ack.code, ack.message);
    }
}

} // namespace world_app
