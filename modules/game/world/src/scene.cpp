#include "apollo/game/world/scene.hpp"

#include "apollo/game/core/entity.hpp"
#include "apollo/script/script_runtime.hpp"

#include <algorithm>
#include <iterator>
#include <string_view>
#include <utility>

namespace apollo::game::world {

using apollo::game::core::EntityId;

std::string_view to_string(SceneTickPhase phase) {
    switch (phase) {
    case SceneTickPhase::Simulate:
        return "simulate";
    case SceneTickPhase::Recalc:
        return "recalc";
    case SceneTickPhase::AoiDecay:
        return "aoidecay";
    case SceneTickPhase::Collect:
        return "collect";
    case SceneTickPhase::BudgetFlush:
        return "budget+flush";
    case SceneTickPhase::PersistBatch:
        return "persist-batch";
    }
    return "unknown";
}

Scene::Scene(std::string name)
    : name_(std::move(name)) {
}

Scene::Scene(std::uint64_t scene_id, std::string name)
    : scene_id_(scene_id)
    , name_(std::move(name)) {
}

std::uint64_t Scene::scene_id() const noexcept {
    return scene_id_;
}

void Scene::spawn_entity(EntityPtr entity) {
    if (entity && entity->get_id().is_valid()) {
        entities_[entity->get_id().value()] = entity;
        if (attrsync_ != nullptr) {
            attrsync_->register_entity(entity->get_id().value());
        }
        entity->on_spawn();
    }
}

void Scene::despawn_entity(EntityId id) {
    auto it = entities_.find(id.value());
    if (it != entities_.end()) {
        it->second->on_despawn();
        entities_.erase(it);
    }
}

EntityPtr Scene::get_entity(EntityId id) const {
    auto it = entities_.find(id.value());
    return it != entities_.end() ? it->second : nullptr;
}

bool Scene::enter(const apollo::game::cell::AvatarPtr& avatar, const SceneAoi::Vec3& position) {
    if (!avatar || !avatar->player_id().is_valid()) {
        return false;
    }
    const auto pid = avatar->player_id();
    if (avatars_.count(pid.value()) != 0) {
        return false;  // 重复进入拒绝
    }

    avatars_[pid.value()] = avatar;
    avatar_order_.push_back(pid);

    // 进 scene 生：挂 scene 归属 + 权威落点 + AOI 入列。
    // attrsync 注册先于 AOI 入列——首见事件（他方视角的本 avatar 快照）
    // 要求 subject 已注册（attribute-sync §4.1）。
    avatar->attach_scene(scene_id_);
    avatar->set_position({position.x, position.y, position.z});
    if (attrsync_ != nullptr) {
        attrsync_->register_entity(avatar->entity_id().value());
    }
    aoi_.enter(avatar->entity_id(), position);
    return true;
}

bool Scene::suspend_avatar(apollo::game::core::PlayerId player_id) {
    const auto avatar = get_avatar(player_id);
    if (!avatar || avatar->state() != apollo::game::cell::AvatarState::Active) {
        return false;
    }
    avatar->suspend();
    return true;
}

bool Scene::resume_avatar(apollo::game::core::PlayerId player_id) {
    const auto avatar = get_avatar(player_id);
    if (!avatar || avatar->state() != apollo::game::cell::AvatarState::Suspended) {
        return false;
    }
    avatar->resume();
    return true;
}

bool Scene::leave(apollo::game::core::PlayerId player_id) {
    auto it = avatars_.find(player_id.value());
    if (it == avatars_.end()) {
        return false;
    }

    // 出 scene 死：AOI 出列 + 归属解除（对象生命周期由持有方收尾）
    aoi_.leave(it->second->entity_id());
    it->second->detach_scene();
    avatars_.erase(it);

    auto order_it = std::find(avatar_order_.begin(), avatar_order_.end(), player_id);
    if (order_it != avatar_order_.end()) {
        avatar_order_.erase(order_it);
    }
    return true;
}

apollo::game::cell::AvatarPtr Scene::get_avatar(apollo::game::core::PlayerId player_id) const {
    auto it = avatars_.find(player_id.value());
    return it != avatars_.end() ? it->second : nullptr;
}

bool Scene::has_avatar(apollo::game::core::PlayerId player_id) const noexcept {
    return avatars_.count(player_id.value()) != 0;
}

std::size_t Scene::avatar_count() const noexcept {
    return avatars_.size();
}

const std::vector<apollo::game::core::PlayerId>& Scene::avatars() const noexcept {
    return avatar_order_;
}

SceneAoi& Scene::aoi() noexcept {
    return aoi_;
}

const SceneAoi& Scene::aoi() const noexcept {
    return aoi_;
}

void Scene::attach_attrsync(apollo::game::attrsync::AttrSyncManager& mgr) {
    attrsync_ = &mgr;

    // 既有实体/avatar 补注册（后续 spawn/enter 即时注册）
    for (const auto& [id, entity] : entities_) {
        (void)entity;
        mgr.register_entity(id);
    }
    for (const auto& pid : avatar_order_) {
        mgr.register_entity(avatars_.at(pid.value())->entity_id().value());
    }

    aoi_.set_event_sink(
        [this](const SceneAoi::Event& event) { on_aoi_event(event); });

    install_attr_hook_bridge();
}

void Scene::attach_script(apollo::script::ScriptRuntime& rt) {
    script_ = &rt;
    install_attr_hook_bridge();
}

void Scene::detach_script() {
    script_ = nullptr;
    {
        std::lock_guard<std::mutex> lock(debug_queue_mutex_);
        debug_queue_.clear();  // 在队请求随 detach 丢弃（不执行）
    }
    if (attrsync_ != nullptr) {
        attrsync_->clear_attr_hook();
    }
}

bool Scene::script_attached() const noexcept {
    return script_ != nullptr;
}

void Scene::set_attr_hook_frame_budget(std::uint64_t budget) noexcept {
    attr_hook_frame_budget_ = budget;
}

std::uint64_t Scene::attr_hook_frame_budget() const noexcept {
    return attr_hook_frame_budget_;
}

bool Scene::enqueue_debug_eval(std::uint64_t id, const std::string& source) {
    if (script_ == nullptr) {
        return false;
    }
    std::lock_guard<std::mutex> lock(debug_queue_mutex_);
    if (debug_queue_.size() >= debug_eval_queue_depth_) {
        return false;  // 队列满拒收（打爆防护）
    }
    debug_queue_.push_back({id, source});
    return true;
}

std::vector<Scene::DebugEvalResult> Scene::drain_debug_eval_results() {
    std::vector<Scene::DebugEvalResult> out;
    {
        std::lock_guard<std::mutex> lock(debug_queue_mutex_);
        out.assign(debug_results_.begin(), debug_results_.end());
        debug_results_.clear();
    }
    return out;
}

void Scene::set_debug_eval_queue_depth(std::size_t depth) noexcept {
    debug_eval_queue_depth_ = depth;
}

std::size_t Scene::debug_eval_queue_depth() const noexcept {
    return debug_eval_queue_depth_;
}

void Scene::run_debug_eval_window() {
    std::deque<DebugEvalRequest> pending;
    {
        std::lock_guard<std::mutex> lock(debug_queue_mutex_);
        pending.swap(debug_queue_);
    }
    for (auto& req : pending) {
        std::string result;
        const bool ok = script_->eval_debug(req.source, result);
        std::lock_guard<std::mutex> lock(debug_queue_mutex_);
        if (debug_results_.size() >= debug_eval_queue_depth_) {
            debug_results_.pop_front();  // 未取走的结果丢最旧（FIFO 有界）
        }
        debug_results_.push_back({req.id, ok, std::move(result)});
    }
}

void Scene::install_attr_hook_bridge() {
    if (attrsync_ == nullptr || script_ == nullptr) {
        return;
    }
    // §5.1 适配桥：attrsync 注入缝（HookChange）↔ ScriptRuntime 钩子窗口
    // （AttrChange 快照拷贝）。修订值写回 HookChange，由 attrsync 正规写
    // 路径落账；白名单取契约表数据面，帧预算逐次取 Scene 配置。
    auto* rt = script_;
    auto* mgr = attrsync_;
    mgr->set_attr_hook(
        [rt, mgr, budget = &attr_hook_frame_budget_](
            std::vector<apollo::game::attrsync::AttrSyncManager::HookChange>& changes) {
            std::vector<apollo::script::ScriptRuntime::AttrChange> window;
            window.reserve(changes.size());
            for (const auto& ch : changes) {
                window.push_back({ch.entity, ch.attr_id, ch.old_value, ch.new_value});
            }
            rt->run_attr_hook_window(window, rt->contract_write_whitelist(), *budget);
            for (std::size_t i = 0; i < changes.size(); ++i) {
                changes[i].new_value = window[i].new_value;
            }
        });
}

void Scene::detach_attrsync() {
    attrsync_ = nullptr;
    attrsync_outbound_.clear();
    aoi_.set_event_sink(nullptr);
}

bool Scene::attrsync_attached() const noexcept {
    return attrsync_ != nullptr;
}

std::vector<apollo::game::attrsync::AttrSyncFrame> Scene::drain_attrsync_frames() {
    auto out = std::move(attrsync_outbound_);
    attrsync_outbound_.clear();
    return out;
}

void Scene::on_aoi_event(const SceneAoi::Event& event) noexcept {
    if (attrsync_ == nullptr) {
        return;
    }
    const auto observer = event.observer.value();
    const auto subject = event.subject.value();

    switch (event.kind) {
    case SceneAoi::Event::Kind::Enter:
    case SceneAoi::Event::Kind::Sync:
        // 首见（Enter 或进场铺底 Sync）→ 建 ViewerState + 快照帧；
        // 既有视野的跨格 Sync 不触发协议（§4.1）。
        if (!attrsync_->has_viewer(observer, subject)) {
            auto frames = attrsync_->on_viewer_enter_collect(observer, subject);
            attrsync_outbound_.insert(
                attrsync_outbound_.end(),
                std::make_move_iterator(frames.begin()),
                std::make_move_iterator(frames.end()));
        }
        break;
    case SceneAoi::Event::Kind::Leave:
        attrsync_->on_viewer_leave(observer, subject);
        break;
    }
}

void Scene::update(float delta_time) {
    tick(static_cast<double>(delta_time));
}

void Scene::tick(double delta_seconds) noexcept {
    // 六阶段固定顺序（attribute-sync 口径）；P0-3 骨架：simulate 驱动实体，
    // 其余阶段计数占位（recalc/aoidecay/collect/budget+flush/persist-batch
    // 随 P1 持久化与属性管线接入）。
    ++tick_count_;
    if (script_ != nullptr) {
        script_->set_tick(tick_count_);  // §6.1 熔断/WARN 记账（tick 归宿主喂）
        run_debug_eval_window();         // §7.1 固定调试点（tick 首拍）
    }
    run_phase(SceneTickPhase::Simulate);
    for (auto& [id, entity] : entities_) {
        (void)id;
        entity->on_update(static_cast<float>(delta_seconds));
    }
    run_phase(SceneTickPhase::Recalc);
    // scripting-lua §5.1：钩子窗口——dirty 固定后、广播收集前执行。
    // 无桥（script 未 attach）attrsync 侧空载零调用。
    if (attrsync_ != nullptr) {
        attrsync_->apply_attr_hook();
    }
    run_phase(SceneTickPhase::AoiDecay);
    run_phase(SceneTickPhase::Collect);
    run_phase(SceneTickPhase::BudgetFlush);
    // §5.2 帧级预算归还：下一帧恢复。
    if (script_ != nullptr) {
        script_->reset_attr_frame();
    }
    run_phase(SceneTickPhase::PersistBatch);
}

void Scene::run_phase(SceneTickPhase phase) noexcept {
    ++phase_counts_[static_cast<std::size_t>(phase)];
}

std::uint64_t Scene::tick_count() const noexcept {
    return tick_count_;
}

std::uint64_t Scene::phase_count(SceneTickPhase phase) const noexcept {
    return phase_counts_[static_cast<std::size_t>(phase)];
}

} // namespace apollo::game::world