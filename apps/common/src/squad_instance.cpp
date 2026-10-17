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
    for (const auto member : it->second.members) busy_members_.erase(member);
    auto wit = worlds_.find(it->second.world_id);
    if (wit != worlds_.end() && wit->second.used > 0) --wit->second.used;
    active_.erase(it);
}

} // namespace squad_instance
