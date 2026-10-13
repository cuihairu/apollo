#pragma once
//
// RemoteEntityRef — 远程实体引用（remote-entity-call 四件套之一）。
//
// 显式表达「调哪个实体、调的是 anchor/avatar/ghost、route 版本多少」。
// 设计见 docs/architecture/remote-entity-call-design.md §5.1。
//

#include <cstdint>
#include <string>

namespace apollo::net::remote {

enum class EntityDomain : std::int32_t {
    Anchor = 0,
    World  = 1,
    Proxy  = 2,
    Ghost  = 3,
};

enum class AuthorityRole : std::int32_t {
    Authoritative = 0,
    Replica       = 1,
    ForwardOnly   = 2,
};

struct RemoteEntityRef {
    std::uint64_t entity_id     = 0;
    EntityDomain  domain        = EntityDomain::Anchor;
    std::string   target_app;
    AuthorityRole role          = AuthorityRole::Authoritative;
    std::uint64_t route_version = 0;
    std::uint64_t shard_key     = 0;
};

}  // namespace apollo::net::remote
