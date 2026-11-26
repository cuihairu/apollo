#pragma once

#include "apollo/game/core/entity.hpp"
#include "apollo/game/base/gameplay_snapshot.hpp"
#include "apollo/game/world/world.hpp"
#include "apollo/game/world/world_session_manager.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

namespace apollo::game::world {

// 换幕（Scene Transfer）流程模块——P1-1，lifecycle §2.3 六差距收口：
//
//   prepare（准入）→ begin（会话 TransferringOut）→ detach（旧 Avatar 销毁）
//   → attach（新 scene 重建 Avatar，Avatar 投影）→ state sync（会话字段确认）
//   → complete（resume 回 Active）
//
// 六差距逐条落点：
//   1. 准入/驳回   —— Prepare 阶段显式校验（会话态、目标 scene 存在、
//                     目标 instance 态放行），驳回原因可观察；
//   2. 对象投影   —— detach 销毁旧 Avatar，attach 以 Avatar 投影重建
//                     （不再字段搬运）；
//   3. 失败回滚   —— attach 失败（重复进入等）回滚：会话 abort + 原 scene
//                     原落点重进；回滚再失败给 RollbackFailed（终态错误）；
//   4. 断线在转移中 —— 会话层原语 suspend_from_transfer（Transferring→
//                     Suspended 挂机窗口，pending 保留）；重连后可确认
//                     （complete）或回滚（abort）——见 world_session.hpp；
//   5. AOI 重建   —— attach 走 Scene::enter（AOI 入列随之完成）；
//   6. 目标不存在 —— prepare 显式驳回（PrepareFailed），不再静默同场搬运。

// 转移请求（落点由玩法/请求方给出；换幕坐标不带走——易失态清零）
struct SceneTransferRequest {
    apollo::game::core::PlayerId player_id{};
    std::uint64_t target_scene_id = 0;
    SceneAoi::Vec3 landing{};
    // 可选投影源 = 只读玩法快照（值语义）：nullopt = 无快照，Avatar 以出厂
    // 态进场。跨界只传快照值，不传 base::Avatar* 指针（todo L84：禁止跨界
    // 使用本地 Anchor 指针修改资料）；结算类结果单向落回 Base 不属本流程。
    std::optional<apollo::game::base::GameplaySnapshot> gameplay_snapshot;
};

// 流程步骤（失败定位可观察）
enum class SceneTransferStep : std::uint8_t {
    Prepare = 0,
    BeginSession,
    Detach,
    Attach,
    Complete,
};

// 结果码（六差距收口的可观察面）
enum class SceneTransferResult : std::uint8_t {
    Ok = 0,
    UnknownSession,     // 会话不存在
    RejectedByState,    // 准入驳回：会话非 Active（转移/离场中不可再发起）
    PrepareFailed,      // 准入驳回：目标 scene 不存在 / 目标 instance 态不放行
    AttachFailed,       // attach 失败（已回滚至原 scene）
    RollbackFailed,     // 回滚也失败（原 scene 不可回）——调用方须隔离会话
    CommitInconsistent, // D16/D25 提交后失败：目标成员资格已提交保留（不自动
                        // 回滚），调用方按已提交决定对账/隔离
};

std::string_view to_string(SceneTransferResult result);

struct SceneTransferOutcome {
    SceneTransferResult result = SceneTransferResult::Ok;
    SceneTransferStep failed_at = SceneTransferStep::Prepare;
    std::uint64_t source_scene_id = 0;
    std::uint64_t target_scene_id = 0;

    [[nodiscard]] bool ok() const noexcept { return result == SceneTransferResult::Ok; }
};

// 执行换幕：单进程两 scene 间端到端（World + 会话管理器即全部依赖；
// 跨进程的镜像/路由面留 P3）。失败时保证「要么落在目标、要么回到原场」
// 的不变量，绝不悬空（RollbackFailed 除外——该终态必须上报）。
SceneTransferOutcome execute_scene_transfer(World& world,
                                            WorldSessionManager& sessions,
                                            const SceneTransferRequest& request);

// ---- D16 迁移状态机：完整运行态随迁 + 五步逐步确认 ----
//
// 与 execute_scene_transfer（D15 换场原语，离场重建、易失态清零）的差异：
//   1. 运行态随迁——源冻结捕获 AvatarRuntimeState（实体 ID/血量/输入序号
//      水位），目标播种；快照（如有）只管长期属性，运行态覆盖播种值；
//      落点坐标仍是请求易失态（不随迁）。
//   2. 序反转——目标激活后才源清理（D15 是先 detach 后 attach）：
//      条件提交前源对象全程未离场，取消即源解冻（无重建）。
//   3. 五步逐步确认——confirmed_steps 逐位记录，失败定位到步；提交点
//      （目标成员资格成立）之后不再自动回滚（D25：按已提交决定完成）。
//
// 迁移 ID 持久提交（D16 §3.2 持久提交阶段）与跨 World 协调（D17）留
// 跨进程批；本入口是同进程显式换场的迁移路径，五步语义与确认轨迹由
// 后续 D20/D21 更新演练复用。
enum class SceneMigrationStep : std::uint8_t {
    SourceFreeze = 0,   // 源冻结：准入校验 + 运行态捕获 + Avatar 冻结
    TargetPrepare,      // 目标未激活准备：Avatar 重建 + 运行态播种（未进场）
    Commit,             // 条件提交：目标成员资格成立（此后不自动回滚）
    RouteProject,       // 路由投影更新：会话转移窗口 + 字段切换
    ActivateAndCleanup, // 目标激活（complete resume）与源清理（幂等）
};

std::string_view to_string(SceneMigrationStep step);

struct SceneMigrationOutcome {
    SceneTransferResult result = SceneTransferResult::Ok;
    SceneMigrationStep failed_at = SceneMigrationStep::SourceFreeze;
    std::uint64_t source_scene_id = 0;
    std::uint64_t target_scene_id = 0;
    std::uint32_t confirmed_steps = 0;  // bit i = SceneMigrationStep i 已确认

    [[nodiscard]] bool step_confirmed(SceneMigrationStep step) const noexcept {
        return (confirmed_steps & (std::uint32_t{1} << static_cast<std::uint32_t>(step))) != 0;
    }
    [[nodiscard]] bool ok() const noexcept { return result == SceneTransferResult::Ok; }
};

// 执行迁移：同进程显式换场的 D16 路径（运行态随迁 + 五步确认）。
// 准入/驳回语义与 execute_scene_transfer 一致；另驳回「源无 Avatar」
// （运行态无从捕获，PrepareFailed——不静默降级为重建）。
SceneMigrationOutcome execute_migration_transfer(World& world,
                                                 WorldSessionManager& sessions,
                                                 const SceneTransferRequest& request);

} // namespace apollo::game::world
