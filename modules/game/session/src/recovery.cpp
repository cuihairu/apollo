#include "apollo/game/session/recovery.hpp"

#include "apollo/game/session/anchor_manager.hpp"
#include "apollo/game/session/player_anchor.hpp"

#include <iostream>
#include <mutex>

namespace apollo::game::session {

const char* to_string(RecoveryTier tier) {
    switch (tier) {
        case RecoveryTier::MustPersist:
            return "must-persist";
        case RecoveryTier::Recomputable:
            return "recomputable";
        case RecoveryTier::Lossy:
            return "lossy";
    }
    return "unknown";
}

const char* to_string(RecoveryPhase phase) {
    switch (phase) {
        case RecoveryPhase::Init:
            return "init";
        case RecoveryPhase::Restoring:
            return "restoring";
        case RecoveryPhase::Admitting:
            return "admitting";
        case RecoveryPhase::Ready:
            return "ready";
        case RecoveryPhase::Failed:
            return "failed";
    }
    return "unknown";
}

const std::vector<RecoveryItem>& recovery_manifest() {
    static const std::vector<RecoveryItem> manifest = {
        // lifecycle §3 恢复矩阵，逐行承载
        {"player_anchored_state", RecoveryTier::MustPersist,
         "write-behind journal + 档案落盘（P1-4）；重启 replay 恢复位点"},
        {"scene_runtime_state", RecoveryTier::Recomputable,
         "重开 scene（SceneDescriptor 重建，world.create_scene）"},
        {"instance_settlement", RecoveryTier::MustPersist,
         "结算结果单向落 Anchor 随档案落盘；中途 abort 可丢"},
        {"session_connection", RecoveryTier::Recomputable,
         "重连窗口内重建（P1-6 resume）；窗口外可丢"},
        {"service_topology", RecoveryTier::Recomputable,
         "manager 目录可重建（P3-1 跨进程化前为单点内存）"},
        {"battle_midstate", RecoveryTier::Lossy,
         "结算前可丢，以重赛恢复"},
    };
    return manifest;
}

RecoveryCoordinator::RecoveryCoordinator(std::string component, RestoreStep restore_step,
                                         AdmissionCheck admission)
    : component_(std::move(component))
    , restore_step_(std::move(restore_step))
    , admission_(std::move(admission)) {
}

bool RecoveryCoordinator::restore() {
    if (phase_ != RecoveryPhase::Init) {
        return false;
    }
    phase_ = RecoveryPhase::Restoring;

    if (restore_step_) {
        try {
            replayed_ = restore_step_();
        } catch (const std::exception& e) {
            std::cerr << "[" << component_ << "] recovery restore failed: " << e.what()
                      << std::endl;
            phase_ = RecoveryPhase::Failed;
            return false;
        }
    }

    std::cout << "[" << component_ << "] restore done: " << replayed_ << " entries"
              << std::endl;
    return true;
}

bool RecoveryCoordinator::admit() {
    if (phase_ != RecoveryPhase::Restoring) {
        return false;
    }
    phase_ = RecoveryPhase::Admitting;

    if (admission_ && !admission_()) {
        std::cerr << "[" << component_ << "] admission rejected" << std::endl;
        phase_ = RecoveryPhase::Failed;
        return false;
    }

    phase_ = RecoveryPhase::Ready;
    std::cout << "[" << component_ << "] recovery ready (replayed=" << replayed_ << ")"
              << std::endl;
    return true;
}

bool RecoveryCoordinator::run() {
    return restore() && admit();
}

std::size_t restore_anchors(AnchorManager& manager,
                            const std::vector<std::uint64_t>& player_ids) {
    std::size_t restored = 0;
    for (const auto player_id : player_ids) {
        auto anchor = manager.activate(player_id);
        if (!anchor) {
            continue;
        }
        // 恢复态 Offline：档案在、会话无；登录路径再转 Online
        anchor->set_state(AnchorState::Offline);
        ++restored;
    }
    return restored;
}

} // namespace apollo::game::session
