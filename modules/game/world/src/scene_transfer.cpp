#include "apollo/game/world/scene_transfer.hpp"

#include "apollo/game/cell/avatar.hpp"
#include "apollo/game/world/instance.hpp"
#include "apollo/game/world/scene.hpp"

#include <string_view>

namespace apollo::game::world {

namespace {

// 会话 → 源场景定位：Avatar 挂 scene 归属是唯一权威（Scene::enter→
// attach_scene 维护），会话 space/instance 字段只是路由投影，不在此反推。
Scene* find_session_scene(World& world, const WorldSession& session) {
    for (Scene* scene : world.scenes()) {
        if (scene != nullptr && scene->has_avatar(session.player_id())) {
            return scene;
        }
    }
    return nullptr;
}

} // namespace

std::string_view to_string(SceneTransferResult result) {
    switch (result) {
    case SceneTransferResult::Ok:
        return "Ok";
    case SceneTransferResult::UnknownSession:
        return "UnknownSession";
    case SceneTransferResult::RejectedByState:
        return "RejectedByState";
    case SceneTransferResult::PrepareFailed:
        return "PrepareFailed";
    case SceneTransferResult::AttachFailed:
        return "AttachFailed";
    case SceneTransferResult::RollbackFailed:
        return "RollbackFailed";
    case SceneTransferResult::CommitInconsistent:
        return "CommitInconsistent";
    }
    return "Unknown";
}

std::string_view to_string(SceneMigrationStep step) {
    switch (step) {
    case SceneMigrationStep::SourceFreeze:
        return "SourceFreeze";
    case SceneMigrationStep::TargetPrepare:
        return "TargetPrepare";
    case SceneMigrationStep::Commit:
        return "Commit";
    case SceneMigrationStep::RouteProject:
        return "RouteProject";
    case SceneMigrationStep::ActivateAndCleanup:
        return "ActivateAndCleanup";
    }
    return "Unknown";
}

SceneTransferOutcome execute_scene_transfer(World& world,
                                            WorldSessionManager& sessions,
                                            const SceneTransferRequest& request) {
    SceneTransferOutcome outcome;
    outcome.target_scene_id = request.target_scene_id;

    // ---- ① 准备：会话在场、态放行、目标可入 ----
    auto session = sessions.find_by_player(request.player_id);
    if (!session) {
        outcome.result = SceneTransferResult::UnknownSession;
        outcome.failed_at = SceneTransferStep::Prepare;
        return outcome;
    }
    if (session->state() != WorldSessionState::Active) {
        // 准入驳回：仅 Active 可发起（转移中/挂机/离场均不可重入）
        outcome.result = SceneTransferResult::RejectedByState;
        outcome.failed_at = SceneTransferStep::Prepare;
        return outcome;
    }

    Scene* source = find_session_scene(world, *session);
    Scene* target = world.find_scene(request.target_scene_id);
    if (target == nullptr || target == source) {
        // 差距 ⑥：目标 scene 不存在（或同场无意义转移）显式驳回
        outcome.result = SceneTransferResult::PrepareFailed;
        outcome.failed_at = SceneTransferStep::Prepare;
        return outcome;
    }
    outcome.source_scene_id = source != nullptr ? source->scene_id() : 0;

    // 目标 instance 准入预检（不产生副作用；正式进入在 attach 一步）
    Instance* target_instance = world.find_instance_by_scene(request.target_scene_id);
    if (target_instance != nullptr) {
        const auto st = target_instance->state();
        if (st != Instance::State::Waiting && st != Instance::State::Running) {
            outcome.result = SceneTransferResult::PrepareFailed;
            outcome.failed_at = SceneTransferStep::Prepare;
            return outcome;
        }
    }

    // detach 前快照（回滚落点 + 投影源）
    const auto avatar_entity_id = session->avatar_entity_id();
    auto old_avatar = source != nullptr ? source->get_avatar(request.player_id) : nullptr;
    const SceneAoi::Vec3 origin = old_avatar != nullptr
        ? SceneAoi::Vec3{old_avatar->position().x, old_avatar->position().y,
                         old_avatar->position().z}
        : SceneAoi::Vec3{};
    Instance* source_instance =
        world.find_instance(session->map_instance_id());

    // ---- ② begin：会话进入可观察的 TransferringOut 窗口 ----
    session = sessions.transfer_session(session->session_id(),
                                        session->world_id(),
                                        target_instance != nullptr ? target_instance->id() : 0,
                                        request.target_scene_id,
                                        false);
    if (!session) {
        outcome.result = SceneTransferResult::RejectedByState;
        outcome.failed_at = SceneTransferStep::BeginSession;
        return outcome;
    }

    // ---- ③ detach：旧 Avatar 销毁（出 scene 死；对象由本流程收尾）----
    if (source != nullptr) {
        source->leave(request.player_id);
    }
    if (source_instance != nullptr) {
        source_instance->leave(request.player_id);
    }
    old_avatar.reset();  // 出 scene 死：引用即弃

    // ---- ④ attach：新 scene 重建 Avatar（Avatar 投影 + AOI 入列）----
    auto avatar = std::make_shared<apollo::game::cell::Avatar>(request.player_id, avatar_entity_id);
    if (request.gameplay_snapshot.has_value()) {
        avatar->project_from(*request.gameplay_snapshot);
    }
    if (!target->enter(avatar, request.landing) ||
        (target_instance != nullptr && !target_instance->enter(request.player_id))) {
        // 差距 ③：失败回滚——会话 abort + 原 scene 原落点重进
        outcome.result = SceneTransferResult::AttachFailed;
        outcome.failed_at = SceneTransferStep::Attach;

        if (sessions.abort_transfer(session->session_id()) == nullptr) {
            outcome.result = SceneTransferResult::RollbackFailed;
            return outcome;  // 会话悬空：调用方必须隔离/终结
        }
        if (source != nullptr) {
            auto restored = std::make_shared<apollo::game::cell::Avatar>(request.player_id, avatar_entity_id);
            if (request.gameplay_snapshot.has_value()) {
                restored->project_from(*request.gameplay_snapshot);
            }
            if (!source->enter(restored, origin)) {
                outcome.result = SceneTransferResult::RollbackFailed;
                return outcome;  // 原场不可回：调用方必须隔离/终结
            }
        }
        if (source_instance != nullptr) {
            source_instance->enter(request.player_id);
        }
        return outcome;
    }

    // ---- ⑤ complete：state sync（pending 字段落位）+ resume 回 Active ----
    if (sessions.complete_transfer(session->session_id()) == nullptr) {
        // 理论不可达（本流程内刚转入 TransferringOut）；防御性回滚
        outcome.result = SceneTransferResult::RollbackFailed;
        outcome.failed_at = SceneTransferStep::Complete;
        sessions.abort_transfer(session->session_id());
        return outcome;
    }

    outcome.result = SceneTransferResult::Ok;
    return outcome;
}

SceneMigrationOutcome execute_migration_transfer(World& world,
                                                 WorldSessionManager& sessions,
                                                 const SceneTransferRequest& request) {
    SceneMigrationOutcome outcome;
    outcome.target_scene_id = request.target_scene_id;

    const auto confirm = [&outcome](SceneMigrationStep step) {
        outcome.confirmed_steps |= std::uint32_t{1} << static_cast<std::uint32_t>(step);
    };

    // ---- ① 源冻结：准入校验 + 运行态捕获 + Avatar 冻结（不收输入/伤害）----
    auto session = sessions.find_by_player(request.player_id);
    if (!session) {
        outcome.result = SceneTransferResult::UnknownSession;
        return outcome;
    }
    if (session->state() != WorldSessionState::Active) {
        // 准入驳回：仅 Active 可发起（转移中/挂机/离场均不可重入）
        outcome.result = SceneTransferResult::RejectedByState;
        return outcome;
    }

    Scene* source = find_session_scene(world, *session);
    Scene* target = world.find_scene(request.target_scene_id);
    auto source_avatar = source != nullptr ? source->get_avatar(request.player_id) : nullptr;
    if (target == nullptr || target == source || source_avatar == nullptr) {
        // 目标不存在 / 同场无意义 / 源无 Avatar（运行态无从捕获）——
        // 无副作用驳回，不静默降级为重建
        outcome.result = SceneTransferResult::PrepareFailed;
        return outcome;
    }
    outcome.source_scene_id = source->scene_id();

    // 目标 instance 准入预检（不产生副作用；正式进入在条件提交一步）
    Instance* target_instance = world.find_instance_by_scene(request.target_scene_id);
    if (target_instance != nullptr) {
        const auto st = target_instance->state();
        if (st != Instance::State::Waiting && st != Instance::State::Running) {
            outcome.result = SceneTransferResult::PrepareFailed;
            return outcome;
        }
    }
    Instance* source_instance = world.find_instance(session->map_instance_id());

    const auto runtime = source_avatar->capture_runtime();  // 冻结前定格
    source_avatar->freeze();
    confirm(SceneMigrationStep::SourceFreeze);

    // ---- ② 目标未激活准备：Avatar 重建（实体 ID 不变）+ 运行态播种 ----
    auto staged = std::make_shared<apollo::game::cell::Avatar>(request.player_id,
                                                               runtime.entity_id);
    if (request.gameplay_snapshot.has_value()) {
        staged->project_from(*request.gameplay_snapshot);  // 长期属性（快照播种值）
    }
    if (!staged->restore_runtime(runtime)) {
        // 不可达（同 entity_id 构造）；防御：解冻回源、目标丢弃
        outcome.result = SceneTransferResult::PrepareFailed;
        outcome.failed_at = SceneMigrationStep::TargetPrepare;
        source_avatar->unfreeze();
        return outcome;
    }
    confirm(SceneMigrationStep::TargetPrepare);

    // ---- ③ 条件提交：目标成员资格成立（scene + instance）----
    // instance 语义同 D15：无实例空间（持续开放 Space）= 无需 enter 即成立
    const bool scene_entered = target->enter(staged, request.landing);
    const bool instance_entered =
        scene_entered && (target_instance == nullptr || target_instance->enter(request.player_id));
    if (!scene_entered || !instance_entered) {
        // 取消（D16 §3.2 准备失败唯一动作）：目标丢弃、源解冻——源对象
        // 全程未离场，无重建（区别于 D15 回滚的快照重建路径）
        if (scene_entered && !instance_entered) {
            target->leave(request.player_id);  // 目标丢弃（scene 已入而 instance 未入）
        }
        source_avatar->unfreeze();
        outcome.result = SceneTransferResult::AttachFailed;
        outcome.failed_at = SceneMigrationStep::Commit;
        return outcome;
    }
    confirm(SceneMigrationStep::Commit);
    // 提交点：此后按已提交决定完成，不再自动回滚（D25 激活期原则）

    // ---- ④ 路由投影更新：会话转移窗口 + 字段切换 ----
    session = sessions.transfer_session(session->session_id(),
                                        session->world_id(),
                                        target_instance != nullptr ? target_instance->id() : 0,
                                        request.target_scene_id,
                                        false);
    if (!session) {
        // 提交后失败（防御）：目标成员资格保留，清理源场成员防双场在册
        outcome.result = SceneTransferResult::CommitInconsistent;
        outcome.failed_at = SceneMigrationStep::RouteProject;
        source->leave(request.player_id);
        if (source_instance != nullptr) {
            source_instance->leave(request.player_id);
        }
        return outcome;
    }
    confirm(SceneMigrationStep::RouteProject);

    // ---- ⑤ 目标激活（complete resume）与源清理（幂等）----
    if (sessions.complete_transfer(session->session_id()) == nullptr) {
        // 理论不可达（本流程内刚转入 TransferringOut）；按已提交决定完成：
        // 目标保留、源清理照做、上报不一致（不自动回滚已提交迁移）
        outcome.result = SceneTransferResult::CommitInconsistent;
        outcome.failed_at = SceneMigrationStep::ActivateAndCleanup;
    }
    source->leave(request.player_id);  // 源清理（冻结态 Avatar 出列销毁）
    if (source_instance != nullptr) {
        source_instance->leave(request.player_id);
    }
    confirm(SceneMigrationStep::ActivateAndCleanup);
    if (outcome.result != SceneTransferResult::Ok) {
        return outcome;  // 提交后不一致：已逐步确认到源清理，调用方对账
    }
    return outcome;
}

} // namespace apollo::game::world
