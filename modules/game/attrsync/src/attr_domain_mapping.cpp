// attr_domain_mapping — 契约 sync 掩码 → AttrDomain 映射实现（attribute-sync §2.2）。

#include "apollo/game/attrsync/attr_domain_mapping.hpp"

namespace apollo::game::attrsync {

AttrDomain domain_from_sync_mask(std::uint8_t sync_mask) {
    // 最宽可见性胜出：WORLD > APPR/PROP > GUILD > TEAM > SELF
    if (sync_mask & kSyncMaskWorld) {
        return AttrDomain::kWorld;
    }
    if (sync_mask & (kSyncMaskAppr | kSyncMaskProp)) {
        return AttrDomain::kPublic;
    }
    if (sync_mask & kSyncMaskGuild) {
        return AttrDomain::kGuild;
    }
    if (sync_mask & kSyncMaskTeam) {
        return AttrDomain::kTeam;
    }
    if (sync_mask & kSyncMaskSelf) {
        return AttrDomain::kSelf;
    }
    // 空掩码（或仅 IMMEDIATE/DB 位）= 默认公开
    return AttrDomain::kPublic;
}

bool is_group_scoped(AttrDomain domain) {
    return domain == AttrDomain::kTeam || domain == AttrDomain::kGuild;
}

void apply_contract_domains(EntityAttrs& attrs,
                            const ContractAttrEntry* entries,
                            std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) {
        attrs.set_domain(entries[i].attr_id,
                         domain_from_sync_mask(entries[i].sync_mask));
    }
}

}  // namespace apollo::game::attrsync
