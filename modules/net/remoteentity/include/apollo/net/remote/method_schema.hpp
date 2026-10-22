#pragma once
//
// RemoteMethodSchema — 远程方法 schema（remote-entity-call 四件套之一）。
//
// 可执行方法定义：方法名/别名/调用模式/目标域/参数类型/超时/幂等。
// 设计见 docs/zh/architecture/remote-entity-call-design.md §5.2。
//

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "apollo/net/remote/entity_ref.hpp"

namespace apollo::net::remote {

enum class InvokeMode : std::int32_t {
    OneWay       = 0,
    RequestReply = 1,
    ReliableEvent = 2,
};

struct RemoteMethodSchema {
    std::string              method_name;
    std::string              method_alias;
    InvokeMode               mode          = InvokeMode::OneWay;
    EntityDomain             target_domain = EntityDomain::Anchor;
    std::vector<std::string> arg_types;
    std::chrono::milliseconds timeout{5000};
    bool                     idempotent = false;
};

}  // namespace apollo::net::remote
