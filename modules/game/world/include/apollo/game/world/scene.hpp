#pragma once

#include "apollo/game/attrsync/attr_sync_manager.hpp"
#include "apollo/game/core/entity.hpp"
#include "apollo/game/world/avatar.hpp"
#include "apollo/game/world/scene_aoi.hpp"

#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace apollo::script {
class ScriptRuntime;  // 前向声明（适配桥实现留 scene.cpp，头不拉 lua 面）
}

namespace apollo::game::world {

using EntityPtr = std::shared_ptr<apollo::game::core::Entity>;

// Scene tick 六阶段（attribute-sync 口径，concurrency.md §表）：
//   simulate → recalc → aoidecay → collect → budget+flush → persist-batch
// P0-3 落骨架：阶段顺序固定、逐阶段计数可观察；实体 on_update 在 simulate。
enum class SceneTickPhase : std::uint8_t {
    Simulate = 0,
    Recalc,
    AoiDecay,
    Collect,
    BudgetFlush,
    PersistBatch,
};

[[nodiscard]] std::string_view to_string(SceneTickPhase phase);

// Scene（契约 §1.1）：世界中一块有归属的逻辑单元——实体容器、AOI 边界、
// 地理数据载体。P0-3 升级为运行时容器：
//   - 拥有实体集合（承接 EntityManager 职责，行为等价迁移）；
//   - 拥有 AOI（SceneAoi，按 scene 隔离）；
//   - 玩家进出 API（enter/leave，Avatar 进 scene 生出 scene 死）；
//   - tick 六阶段骨架（单写者，scene 线程内权威）。
class Scene {
public:
    explicit Scene(std::string name = "DefaultScene");
    Scene(std::uint64_t scene_id, std::string name);

    // ---- 身份 ----
    [[nodiscard]] std::uint64_t scene_id() const noexcept;
    const std::string& get_name() const { return name_; }

    // ---- 实体集合（Scene 拥有）----
    void spawn_entity(EntityPtr entity);
    void despawn_entity(apollo::game::core::EntityId id);
    EntityPtr get_entity(apollo::game::core::EntityId id) const;
    size_t get_entity_count() const { return entities_.size(); }

    // ---- 玩家进出（Avatar 随进出生死；player-object-model §3）----
    // enter：Avatar 挂 scene + AOI 入列；重复进入拒绝（返回 false）。
    // position 为进场落点（出生点/门口由玩法规则决定）。
    bool enter(const AvatarPtr& avatar, const SceneAoi::Vec3& position);
    // leave：Avatar 出 scene（对象由调用方/Instance 处置——P0-3 由
    // cell-server 持容器，Instance::destroy 时清场）。
    bool leave(apollo::game::core::PlayerId player_id);
    // 断线挂机窗口（P1-6，lifecycle §2.4）：Avatar Active↔Suspended，
    // 驻留场景（AOI 保留）；窗口满由调用方经 leave() 移除销毁
    bool suspend_avatar(apollo::game::core::PlayerId player_id);
    bool resume_avatar(apollo::game::core::PlayerId player_id);

    [[nodiscard]] AvatarPtr get_avatar(apollo::game::core::PlayerId player_id) const;
    [[nodiscard]] bool has_avatar(apollo::game::core::PlayerId player_id) const noexcept;
    [[nodiscard]] std::size_t avatar_count() const noexcept;
    [[nodiscard]] const std::vector<apollo::game::core::PlayerId>& avatars() const noexcept;

    // ---- AOI（Scene 拥有；scene_id 隔离）----
    [[nodiscard]] SceneAoi& aoi() noexcept;
    [[nodiscard]] const SceneAoi& aoi() const noexcept;

    // ---- attrsync 接线（attribute-sync §4.1；2026-10-14 批）----
    // attach：既有实体/avatar 补注册 + 接管 AOI 事件 sink——
    //   Enter/Sync 首见（manager 无该 (observer, subject) viewer 态）→
    //     建 ViewerState + 产出快照帧（标记语义，负载组装留后续批）；
    //   视野内跨格 Sync → 不触发协议；
    //   Leave → 回收 ViewerState。
    // 快照帧经 drain_attrsync_frames 交宿主（发送面留 P3-2）。
    // attach 后 spawn_entity/enter 即时注册 entity id；detach 清 sink。
    // sink 自捕获 this——Scene 不拷贝（World 以 unique_ptr 持有）。
    void attach_attrsync(apollo::game::attrsync::AttrSyncManager& mgr);
    void detach_attrsync();
    [[nodiscard]] bool attrsync_attached() const noexcept;
    // 取走首见期累积的快照帧（此后重新累积）
    [[nodiscard]] std::vector<apollo::game::attrsync::AttrSyncFrame>
    drain_attrsync_frames();

    // ---- ScriptRuntime 接线（scripting-lua §5.1 钩子窗口宿主侧）----
    // attach：与 attrsync 共架适配桥——每 tick Recalc 尾、Collect 前，
    // attrsync 写入集（HookChange）转 ScriptRuntime::AttrChange 快照跑
    // 钩子窗口（白名单取契约表、帧预算归 Scene 配置），修订值写回走
    // attrsync 正规写路径落存储 + history + sink。attach 次序无关。
    // 指针仅存引用（attach 期有效；sink 捕获 this，Scene 不拷贝）。
    void attach_script(apollo::script::ScriptRuntime& rt);
    void detach_script();
    [[nodiscard]] bool script_attached() const noexcept;
    // §5.2 帧级钩子预算（默认 200k ≈ 0.5ms 量级，宿主按实测校准）。
    void set_attr_hook_frame_budget(std::uint64_t budget) noexcept;
    [[nodiscard]] std::uint64_t attr_hook_frame_budget() const noexcept;

    // ---- §7.1 admin attach 执行队列（单写者纪律）----
    // 投递面（任意线程，队列自持互斥——Scene 唯一跨线程面，同 WorldSessionManager
    // 的跨线程惯例）：入队即返回；队列满或未 attach_script 时拒收返回 false。
    // 执行面：Scene::tick 首拍固定调试点沙盒执行（eval_debug 语义全管辖——
    // 预算/限流/结果截断），结果按 id 回传；未取走的结果按深度上限丢最旧。
    struct DebugEvalRequest {
        std::uint64_t id;    // transport 关联回传
        std::string source;
    };
    struct DebugEvalResult {
        std::uint64_t id;
        bool ok = false;
        std::string result;  // 结果串（截断归 ScriptRuntime 管辖）
    };
    bool enqueue_debug_eval(std::uint64_t id, const std::string& source);
    std::vector<DebugEvalResult> drain_debug_eval_results();
    void set_debug_eval_queue_depth(std::size_t depth) noexcept;
    [[nodiscard]] std::size_t debug_eval_queue_depth() const noexcept;

    // ---- tick 六阶段 ----
    void update(float delta_time);        // 兼容旧调用（转发 tick）
    void tick(double delta_seconds) noexcept;
    [[nodiscard]] std::uint64_t tick_count() const noexcept;
    // 各阶段累计执行次数（可观察；正常时各阶段 == tick_count）
    [[nodiscard]] std::uint64_t phase_count(SceneTickPhase phase) const noexcept;

private:
    void run_phase(SceneTickPhase phase) noexcept;
    void on_aoi_event(const SceneAoi::Event& event) noexcept;
    // attrsync 与 script 均在位时架桥（attach_script / attach_attrsync 入口）。
    void install_attr_hook_bridge();

    std::uint64_t scene_id_ = 0;
    std::string name_;
    std::unordered_map<std::uint64_t, EntityPtr> entities_;
    std::unordered_map<std::uint64_t, AvatarPtr> avatars_;  // key = PlayerId.value()
    std::vector<apollo::game::core::PlayerId> avatar_order_;  // 进场顺序可观察
    SceneAoi aoi_;
    apollo::game::attrsync::AttrSyncManager* attrsync_ = nullptr;  // attach 期有效
    std::vector<apollo::game::attrsync::AttrSyncFrame> attrsync_outbound_;
    apollo::script::ScriptRuntime* script_ = nullptr;  // attach 期有效
    std::uint64_t attr_hook_frame_budget_ = 200000;    // §5.2 帧级钩子预算

    // §7.1 attach 执行队列：唯一跨线程面（投递互斥锁 + 请求/结果双端）。
    std::mutex debug_queue_mutex_;
    std::deque<DebugEvalRequest> debug_queue_;
    std::deque<DebugEvalResult> debug_results_;
    std::size_t debug_eval_queue_depth_ = 8;  // 默认与 eval 同 tick 限流对齐
    void run_debug_eval_window();             // tick 首拍固定调试点
    std::uint64_t tick_count_ = 0;
    std::uint64_t phase_counts_[6] = {0, 0, 0, 0, 0, 0};
};

} // namespace apollo::game::world