#pragma once

#include "apollo/game/core/entity.hpp"
#include "apollo/game/session/player_anchor.hpp"
#include "apollo/game/world/world.hpp"
#include "apollo/game/world/world_session_manager.hpp"

#include <cstdint>
#include <string_view>

namespace apollo::game::world {

// 换幕（Scene Transfer）流程模块——P1-1，lifecycle §2.3 六差距收口：
//
//   prepare（准入）→ begin（会话 TransferringOut）→ detach（旧 Avatar 销毁）
//   → attach（新 scene 重建 Avatar，Anchor 投影）→ state sync（会话字段确认）
//   → complete（resume 回 Active）
//
// 六差距逐条落点：
//   1. 准入/驳回   —— Prepare 阶段显式校验（会话态、目标 scene 存在、
//                     目标 instance 态放行），驳回原因可观察；
//   2. 对象投影   —— detach 销毁旧 Avatar，attach 以 Anchor 投影重建
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
    // 可选投影源：单进程内存锚点（cell 进程无锚点存储时传 nullptr，
    // Avatar 以出厂态进场；结算类结果单向落回 Anchor 不属于本流程）。
    apollo::game::session::PlayerAnchor* anchor = nullptr;
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
    UnknownSession,    // 会话不存在
    RejectedByState,   // 准入驳回：会话非 Active（转移/离场中不可再发起）
    PrepareFailed,     // 准入驳回：目标 scene 不存在 / 目标 instance 态不放行
    AttachFailed,      // attach 失败（已回滚至原 scene）
    RollbackFailed,    // 回滚也失败（原 scene 不可回）——调用方须隔离会话
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

} // namespace apollo::game::world
