#include "squad_instance/squad_instance.hpp"

#include <algorithm>

namespace squad_instance {

SquadInstanceService::SquadInstanceService(apollo::game::social::PartyManager& parties,
                                           apollo::game::world::World* local_world,
                                           std::uint32_t local_world_id)
    : parties_(parties), local_world_(local_world), local_world_id_(local_world_id) {}

void SquadInstanceService::register_world(std::uint32_t world_id, std::size_t max_instances) {
    worlds_[world_id].max_instances = max_instances;
}

void SquadInstanceService::set_max_squad_size(std::size_t max_members) noexcept {
    max_squad_size_ = max_members;
}

void SquadInstanceService::set_member_online(MemberOnlineFn fn) { member_online_ = std::move(fn); }

void SquadInstanceService::set_remote_spawner(RemoteSpawnFn fn) {
    remote_spawner_ = std::move(fn);
}

void SquadInstanceService::set_instance_started(InstanceStartedFn fn) {
    instance_started_ = std::move(fn);
}

void SquadInstanceService::set_instance_closing(InstanceClosingFn fn) {
    instance_closing_ = std::move(fn);
}

void SquadInstanceService::set_anchor_resolver(AnchorResolveFn fn) {
    anchor_resolver_ = std::move(fn);
}

void SquadInstanceService::set_credential_dir(const std::string& dir) {
    credential_store_ = std::make_unique<SettlementCredentialStore>(dir);
}

std::size_t SquadInstanceService::ingest_restored_credentials() {
    if (!credential_store_) return 0;
    std::size_t ingested = 0;
    for (const auto& [party_id, marks] : credential_store_->load_all()) {
        pending_restore_marks_[party_id] = marks;
        ++ingested;
    }
    return ingested;
}

SquadInstanceService::EnterError SquadInstanceService::enter(std::uint64_t party_id,
                                                             std::uint64_t player_id) {
    const auto* binding = find_by_party(party_id);
    if (!binding) return EnterError::NoBinding;
    if (binding->world_id != local_world_id_ || !local_world_)
        return EnterError::NotLocal;
    const bool in_roster =
        std::find(binding->members.begin(), binding->members.end(), player_id) !=
        binding->members.end();
    if (!in_roster) return EnterError::NotRoster;

    auto* instance = local_world_->find_instance(binding->instance_id);
    if (!instance) return EnterError::NoBinding;
    // 首进驱动建场：Create→Initialize→Waiting（成员可预进），失败即拒。
    if (instance->state() == apollo::game::world::Instance::State::Create) {
        if (!instance->initialize() || !instance->ready())
            return EnterError::StateRejected;
    }
    // 准入窗口 = Waiting/Running；结算期及终态拒绝迟到进入。
    if (instance->state() != apollo::game::world::Instance::State::Waiting &&
        instance->state() != apollo::game::world::Instance::State::Running)
        return EnterError::StateRejected;
    if (!instance->enter(apollo::game::core::PlayerId{player_id}))
        return EnterError::Duplicate;
    return EnterError::Ok;
}

SquadInstanceService::EnterError SquadInstanceService::start_gameplay(
    std::uint64_t party_id, std::uint64_t requester_id) {
    const auto* binding = find_by_party(party_id);
    if (!binding) return EnterError::NoBinding;
    if (binding->world_id != local_world_id_ || !local_world_)
        return EnterError::NotLocal;
    auto* party = parties_.find(party_id);
    if (!party || party->leader_player_id() != requester_id)
        return EnterError::NotLeader;

    auto* instance = local_world_->find_instance(binding->instance_id);
    if (!instance) return EnterError::NoBinding;
    // 全员到齐才准启动：受理快照成员须全部在场。
    for (const auto member : binding->members) {
        if (!instance->has_player(apollo::game::core::PlayerId{member}))
            return EnterError::NotAllEntered;
    }
    if (!instance->start()) return EnterError::StateRejected;
    if (instance_started_) instance_started_(binding->instance_id, binding->scene_id);
    return EnterError::Ok;
}

SquadInstanceService::LeaveError SquadInstanceService::leave(std::uint64_t party_id,
                                                             std::uint64_t player_id) {
    const auto* binding = find_by_party(party_id);
    if (!binding) return LeaveError::NoBinding;
    if (binding->world_id != local_world_id_ || !local_world_)
        return LeaveError::NotLocal;
    const bool in_roster = std::find(binding->members.begin(), binding->members.end(),
                                     player_id) != binding->members.end();
    if (!in_roster) return LeaveError::NotRoster;
    auto* instance = local_world_->find_instance(binding->instance_id);
    if (!instance) return LeaveError::NoBinding;
    if (!instance->leave(apollo::game::core::PlayerId{player_id}))
        return LeaveError::NotInInstance;
    // 全员退出（掉线同口径）= 失败判定：Running 期无人可续。
    if (instance->state() == apollo::game::world::Instance::State::Running &&
        instance->player_count() == 0) {
        fail_running(*binding, instance);
    }
    return LeaveError::Ok;
}

bool SquadInstanceService::abort(std::uint64_t party_id) {
    const auto* binding = find_by_party(party_id);
    if (!binding) return false;
    if (binding->world_id != local_world_id_ || !local_world_) return false;
    auto* instance = local_world_->find_instance(binding->instance_id);
    if (!instance) return false;
    // 前置结算相位（Create..Running）才可取消收尾；结算期走 dismiss。
    if (!instance->cancel()) return false;
    return evict_and_release(*binding);
}

void SquadInstanceService::fail_running(const ActiveSquadInstance& binding,
                                        apollo::game::world::Instance* instance) {
    if (auto it = gameplay_.find(binding.instance_id); it != gameplay_.end())
        it->second.all_exited = true;
    // 失败线：取消流程不发成功奖励（battle 随 cancel 复位，无结算事件流）。
    instance->cancel();
}

bool SquadInstanceService::evict_and_release(const ActiveSquadInstance& binding) {
    auto* instance = local_world_ ? local_world_->find_instance(binding.instance_id)
                                  : nullptr;
    if (!instance) return false;
    // 移出玩家：按受理名册逐一移出（destroy 前置约束）。
    for (const auto member : binding.members) {
        instance->leave(apollo::game::core::PlayerId{member});
    }
    // 结算链按当前相位推进至终态（Finishing→Rewarding→Draining→Destroyed）。
    const auto advanced = [&]() {
        if (instance->state() == apollo::game::world::Instance::State::Finishing &&
            !instance->settle())
            return false;
        if (instance->state() == apollo::game::world::Instance::State::Rewarding &&
            !instance->drain())
            return false;
        if (instance->state() == apollo::game::world::Instance::State::Draining &&
            !instance->destroy())
            return false;
        return instance->state() == apollo::game::world::Instance::State::Destroyed;
    }();
    if (!advanced) return false;
    // 双回收（release 内触发 closing 钩子→销毁 Instance 与临时 Scene；
    // 实体/AOI 随 Scene 释放），账面清除。
    return release(binding.instance_id);
}

SquadInstanceService::GameplayError SquadInstanceService::resolve_running(
    std::uint64_t party_id, const ActiveSquadInstance** binding,
    apollo::game::world::Instance** instance) {
    *binding = find_by_party(party_id);
    if (!*binding) return GameplayError::NoBinding;
    if ((*binding)->world_id != local_world_id_ || !local_world_)
        return GameplayError::NotLocal;
    auto* inst = local_world_->find_instance((*binding)->instance_id);
    if (!inst) return GameplayError::NoBinding;
    // 目标、刷怪、击杀与超时判定只在 Running 玩法期受理；
    // 结算期（Finishing 起）及终态一律拒绝迟到输入。
    if (inst->state() != apollo::game::world::Instance::State::Running)
        return GameplayError::NotRunning;
    *instance = inst;
    return GameplayError::Ok;
}

SquadInstanceService::GameplayError SquadInstanceService::set_goal(
    std::uint64_t party_id, const GameplayGoal& goal) {
    const ActiveSquadInstance* binding = nullptr;
    apollo::game::world::Instance* instance = nullptr;
    if (const auto resolved = resolve_running(party_id, &binding, &instance);
        resolved != GameplayError::Ok) {
        return resolved;
    }
    if (goal.monsters_to_defeat == 0) return GameplayError::InvalidGoal;
    auto& state = gameplay_[binding->instance_id];
    if (state.goal_set) return GameplayError::AlreadySet;
    state.goal = goal;
    state.goal_set = true;
    state.goal_base_ticks = instance->tick_count();
    return GameplayError::Ok;
}

SquadInstanceService::GameplayError SquadInstanceService::spawn_wave(
    std::uint64_t party_id, std::uint64_t count) {
    const ActiveSquadInstance* binding = nullptr;
    apollo::game::world::Instance* instance = nullptr;
    if (const auto resolved = resolve_running(party_id, &binding, &instance);
        resolved != GameplayError::Ok) {
        return resolved;
    }
    auto& state = gameplay_[binding->instance_id];
    if (!state.goal_set) return GameplayError::NoGoal;
    if (count == 0) return GameplayError::InvalidRequest;
    // 配额封顶：副本内怪物总量不超出目标所需。
    if (state.spawned + count > state.goal.monsters_to_defeat)
        return GameplayError::WaveTooLarge;
    auto* scene = local_world_->find_scene(binding->scene_id);
    if (!scene) return GameplayError::NoBinding;
    for (std::uint64_t i = 0; i < count; ++i) {
        const auto id = apollo::game::core::EntityId{++monster_seq_};
        scene->spawn_entity(std::make_shared<MonsterEntity>(id));
        state.monsters.push_back(id);
    }
    state.spawned += count;
    return GameplayError::Ok;
}

SquadInstanceService::GameplayError SquadInstanceService::report_monster_defeated(
    std::uint64_t party_id, std::uint64_t monster_entity_id,
    std::uint64_t reporter_player_id) {
    const ActiveSquadInstance* binding = nullptr;
    apollo::game::world::Instance* instance = nullptr;
    if (const auto resolved = resolve_running(party_id, &binding, &instance);
        resolved != GameplayError::Ok) {
        return resolved;
    }
    auto& state = gameplay_[binding->instance_id];
    if (!state.goal_set) return GameplayError::NoGoal;
    const auto monster_id = apollo::game::core::EntityId{monster_entity_id};
    const auto it = std::find(state.monsters.begin(), state.monsters.end(), monster_id);
    if (it == state.monsters.end()) return GameplayError::UnknownMonster;
    state.monsters.erase(it);
    if (auto* scene = local_world_->find_scene(binding->scene_id))
        scene->despawn_entity(monster_id);
    ++state.defeated;
    // 击杀 → battle 输入（分值正路径）：随下一拍 Instance::tick 进入输入序。
    apollo::game::battle::BattleInput input;
    input.player_id = reporter_player_id;
    input.opcode = 1;  // 攻击命中（骨架期唯一判定语义）
    state.pending_battle_inputs.push_back(input);
    // 目标完成判定成立：finish 随下一拍（advance_tick 段）——本拍先让击杀
    // 分值进入 battle 输入序，随后结算，命中的分值不丢。
    if (state.defeated >= state.goal.monsters_to_defeat) state.goal_met = true;
    return GameplayError::Ok;
}

SquadInstanceService::GameplayError SquadInstanceService::advance_tick(
    std::uint64_t party_id, std::uint64_t ticks, double delta_seconds) {
    const ActiveSquadInstance* binding = nullptr;
    apollo::game::world::Instance* instance = nullptr;
    if (const auto resolved = resolve_running(party_id, &binding, &instance);
        resolved != GameplayError::Ok) {
        return resolved;
    }
    auto& state = gameplay_[binding->instance_id];
    // 已受理击杀的 battle 输入随首拍进入（instance 玩法时钟的下一 tick）。
    // 输入的 tick 戳记在入拍时打——判定段按 (in.tick == tick_index) 过滤，
    // 入队时刻的拍号未知，不能提前定。
    auto battle_inputs = std::move(state.pending_battle_inputs);
    state.pending_battle_inputs.clear();
    for (std::uint64_t i = 0; i < ticks; ++i) {
        if (i == 0 && !battle_inputs.empty()) {
            const auto ride_tick =
                static_cast<std::uint32_t>(instance->tick_count() + 1);
            for (auto& in : battle_inputs) in.tick = ride_tick;
            instance->tick(delta_seconds, battle_inputs);
        } else {
            instance->tick(delta_seconds);
        }
    }
    // 目标完成（成功线）：分值已入 battle 输入序，Instance 进入结算阶段。
    if (state.goal_met && !state.timed_out) instance->finish();
    // 超时（失败线）：取消流程不发成功奖励，同样进入结算阶段。
    if (state.goal_set && !state.timed_out && state.goal.timeout_ticks != 0 &&
        instance->tick_count() - state.goal_base_ticks >= state.goal.timeout_ticks) {
        state.timed_out = true;
        instance->cancel();
    }
    return GameplayError::Ok;
}

std::optional<GameplayProgress> SquadInstanceService::gameplay(
    std::uint64_t party_id) const {
    const auto* binding = find_by_party(party_id);
    if (!binding) return std::nullopt;
    const auto it = gameplay_.find(binding->instance_id);
    if (it == gameplay_.end()) return std::nullopt;
    GameplayProgress progress;
    progress.goal_set = it->second.goal_set;
    progress.monsters_to_defeat = it->second.goal.monsters_to_defeat;
    progress.monsters_spawned = it->second.spawned;
    progress.monsters_defeated = it->second.defeated;
    progress.timed_out = it->second.timed_out;
    progress.all_exited = it->second.all_exited;
    progress.monsters_alive.reserve(it->second.monsters.size());
    for (const auto& monster : it->second.monsters)
        progress.monsters_alive.push_back(monster.value());
    if (binding->world_id == local_world_id_ && local_world_) {
        if (auto* instance = local_world_->find_instance(binding->instance_id))
            progress.ticks_elapsed = instance->tick_count();
    }
    return progress;
}

SettlementLedger::ApplyError SquadInstanceService::credit_settlement(
    std::uint64_t party_id, std::uint64_t player_id, std::int64_t score) {
    const auto* binding = find_by_party(party_id);
    if (!binding) return SettlementLedger::ApplyError::UnknownPlayer;
    const auto it = settlements_.find(binding->instance_id);
    if (it == settlements_.end()) return SettlementLedger::ApplyError::UnknownPlayer;
    return it->second->apply(player_id, score);
}

std::size_t SquadInstanceService::retry_settlement(std::uint64_t party_id) {
    const auto* binding = find_by_party(party_id);
    if (!binding) return 0;
    const auto it = settlements_.find(binding->instance_id);
    return it == settlements_.end() ? 0 : it->second->retry();
}

void SquadInstanceService::restore_settlement(
    std::uint64_t party_id, const std::vector<std::uint64_t>& applied_players) {
    const auto* binding = find_by_party(party_id);
    if (!binding) return;
    if (const auto it = settlements_.find(binding->instance_id); it != settlements_.end())
        it->second->restore(applied_players);
}

std::optional<SettlementLedger::Snapshot> SquadInstanceService::settlement(
    std::uint64_t party_id) const {
    const auto* binding = find_by_party(party_id);
    if (!binding) return std::nullopt;
    const auto it = settlements_.find(binding->instance_id);
    if (it == settlements_.end()) return std::nullopt;
    return it->second->snapshot();
}

bool SquadInstanceService::dismiss(std::uint64_t party_id) {
    const auto* binding = find_by_party(party_id);
    if (!binding) return false;
    if (binding->world_id != local_world_id_ || !local_world_) return false;
    auto* instance = local_world_->find_instance(binding->instance_id);
    if (!instance) return false;
    // 仅入结算后可收尾；玩法期（Create..Running）返回 false（策略收尾走 abort）。
    switch (instance->state()) {
        case apollo::game::world::Instance::State::Finishing:
        case apollo::game::world::Instance::State::Rewarding:
        case apollo::game::world::Instance::State::Draining:
        case apollo::game::world::Instance::State::Destroyed:
            break;
        default:
            return false;
    }
    return evict_and_release(*binding);
}

LaunchError SquadInstanceService::validate(const LaunchRequest& request,
                                           std::vector<std::uint64_t>& members) const {
    if (request.map_id == 0) return LaunchError::InvalidRequest;
    auto* party = parties_.find(request.party_id);
    if (!party) return LaunchError::PartyNotFound;
    if (party->leader_player_id() != request.requester_id) return LaunchError::NotLeader;
    members = party->members();
    if (members.empty() || members.size() > max_squad_size_) return LaunchError::PartyTooLarge;
    for (const auto member : members) {
        if (member_online_ && !member_online_(member)) return LaunchError::MemberOffline;
        if (busy_members_.contains(member)) return LaunchError::MemberBusy;
    }
    return LaunchError::Ok;
}

std::optional<std::uint32_t> SquadInstanceService::pick_world() const {
    // 本地优先：省一次跨进程派生；否则按 world_id 升序取首个空位。
    if (const auto it = worlds_.find(local_world_id_);
        it != worlds_.end() && it->second.used < it->second.max_instances) {
        return local_world_id_;
    }
    for (const auto& [world_id, slot] : worlds_) {
        if (slot.used < slot.max_instances) return world_id;
    }
    return std::nullopt;
}

LaunchResult SquadInstanceService::launch(const LaunchRequest& request) {
    LaunchResult result;
    // 排空闸（P4 更新排空批）：闭合期停分新副本——既有副本照常服务至
    // 结算销毁；分配端按 Draining 重选 World 或取消预留。
    if (drain_.load(std::memory_order_acquire)) {
        result.error = LaunchError::Draining;
        return result;
    }
    std::vector<std::uint64_t> members;
    result.error = validate(request, members);
    if (result.error != LaunchError::Ok) return result;

    const auto chosen = pick_world();
    if (!chosen) {
        result.error = LaunchError::NoWorldCapacity;
        return result;
    }
    result.world_id = *chosen;

    if (*chosen == local_world_id_) {
        if (!local_world_) {
            result.error = LaunchError::SpawnRejected;
            return result;
        }
        // 独立 Instance 配独立临时 Scene：一 scene 至多挂一 instance（P2 口径）。
        apollo::game::world::SceneDescriptor descriptor;
        descriptor.map_id = request.map_id;
        descriptor.map_name =
            request.map_name.empty() ? "squad-instance" : request.map_name;
        descriptor.width = request.width;
        descriptor.height = request.height;
        if (!descriptor.is_valid()) {
            result.error = LaunchError::InvalidRequest;
            return result;
        }
        auto* scene = local_world_->create_scene(descriptor);
        if (!scene) {
            result.error = LaunchError::SpawnRejected;
            return result;
        }
        const auto name = "squad-" + std::to_string(request.party_id) + "-" +
                          std::to_string(name_seq_++);
        auto instance = local_world_->create_instance(scene->scene_id(), name);
        if (!instance) {
            local_world_->destroy_scene(scene->scene_id());
            result.error = LaunchError::SpawnRejected;
            return result;
        }
        // 受理即开账：结算名册 = 受理快照成员；battle 结算段经账本（IRewardSink
        // 单向口）→ AnchorRewardSink 语义落锚点（P2-4 幂等落账）。
        auto ledger = std::make_unique<SettlementLedger>();
        ledger->begin(members, anchor_resolver_);
        // 去重凭据面（真实奖励持久化）：落账先持久提交 (小队, 玩家, 分值)
        // 凭据，提交失败挂账不落。
        if (credential_store_) {
            ledger->set_credential_sink(
                [store = credential_store_.get(), party_id = request.party_id](
                    std::uint64_t player_id, std::int64_t score) {
                    return store->record_applied(party_id, player_id, score);
                });
            // 代次延续（重启装载）：崩溃前已落账玩家回填已落标记——重放
            // 不双记；标记回写凭据（代次二再崩不丢）；挂起标记消费。
            if (const auto rit = pending_restore_marks_.find(request.party_id);
                rit != pending_restore_marks_.end()) {
                std::vector<std::uint64_t> applied;
                applied.reserve(rit->second.size());
                for (const auto& [player_id, score] : rit->second) {
                    credential_store_->record_applied(request.party_id, player_id,
                                                       score);
                    applied.push_back(player_id);
                }
                ledger->restore(applied);
                pending_restore_marks_.erase(rit);
            }
        }
        // battle 与 instance 同生：enter 段收参战者（Instance::enter 转发）、
        // tick 段随 Instance 玩法时钟推进、finish 段结算落账；seed 取
        // instance_id（确定性可复算基线，玩法数值随玩法批）。
        auto battle = std::make_unique<apollo::game::battle::BattleRuntime>(
            instance->id(), instance->id(), ledger.get());
        if (!instance->attach_battle(std::move(battle))) {
            local_world_->destroy_instance(instance->id());
            local_world_->destroy_scene(scene->scene_id());
            result.error = LaunchError::SpawnRejected;
            return result;
        }
        settlements_[instance->id()] = std::move(ledger);
        result.scene_id = scene->scene_id();
        result.instance_id = instance->id();
    } else {
        if (!remote_spawner_) {
            result.error = LaunchError::SpawnRejected;
            return result;
        }
        auto spawned = remote_spawner_(*chosen, request);
        if (!spawned) {
            result.error = LaunchError::SpawnRejected;
            return result;
        }
        result.instance_id = spawned->first;
        result.scene_id = spawned->second;
    }

    ActiveSquadInstance binding;
    binding.party_id = request.party_id;
    binding.world_id = result.world_id;
    binding.instance_id = result.instance_id;
    binding.scene_id = result.scene_id;
    binding.members = std::move(members);
    for (const auto member : binding.members) busy_members_.insert(member);
    party_to_instance_[binding.party_id] = binding.instance_id;
    ++worlds_[result.world_id].used;
    active_.emplace(result.instance_id, std::move(binding));
    return result;
}

bool SquadInstanceService::release(std::uint64_t instance_id) {
    auto it = active_.find(instance_id);
    if (it == active_.end()) return false;
    if (it->second.world_id == local_world_id_) {
        if (!local_world_) return false;
        auto* instance = local_world_->find_instance(instance_id);
        if (!instance || !instance->is_destroyed()) return false;
        // closing 钩子在 Scene/Instance 回收前触发：宿主在此销毁私有脚本
        // 状态并摘除副本内残留（钩子见 set_instance_closing）。
        if (instance_closing_) instance_closing_(instance_id);
        if (!local_world_->destroy_instance(instance_id)) return false;
    }
    erase_binding(it);
    return true;
}

const ActiveSquadInstance* SquadInstanceService::find_by_party(
    std::uint64_t party_id) const noexcept {
    const auto it = party_to_instance_.find(party_id);
    if (it == party_to_instance_.end()) return nullptr;
    const auto bind = active_.find(it->second);
    return bind != active_.end() ? &bind->second : nullptr;
}

const ActiveSquadInstance* SquadInstanceService::find_by_instance(
    std::uint64_t instance_id) const noexcept {
    const auto it = active_.find(instance_id);
    return it != active_.end() ? &it->second : nullptr;
}

std::vector<ActiveSquadInstance> SquadInstanceService::activeBindings() const {
    std::vector<ActiveSquadInstance> bindings;
    bindings.reserve(active_.size());
    for (const auto& [instance_id, binding] : active_) {
        bindings.push_back(binding);
    }
    return bindings;
}

std::size_t SquadInstanceService::active_count() const noexcept { return active_.size(); }

std::size_t SquadInstanceService::used_capacity(std::uint32_t world_id) const noexcept {
    const auto it = worlds_.find(world_id);
    return it != worlds_.end() ? it->second.used : 0;
}

void SquadInstanceService::erase_binding(
    std::unordered_map<std::uint64_t, ActiveSquadInstance>::iterator it) {
    if (const auto pit = party_to_instance_.find(it->second.party_id);
        pit != party_to_instance_.end()) {
        party_to_instance_.erase(pit);
    }
    gameplay_.erase(it->second.instance_id);
    settlements_.erase(it->second.instance_id);
    // 凭据生命周期 = 结算代次：终局回收（重启装载的挂起标记同队已消费）
    if (credential_store_) credential_store_->erase(it->second.party_id);
    for (const auto member : it->second.members) busy_members_.erase(member);
    auto wit = worlds_.find(it->second.world_id);
    if (wit != worlds_.end() && wit->second.used > 0) --wit->second.used;
    active_.erase(it);
}

} // namespace squad_instance
