#pragma once
//
// RouteResolver — 路由解析器（remote-entity-call 四件套之一）。
//
// 根据 entity_id 找到当前真实宿主（target_app），校验 route version，
// 决定是否需要转发。设计见 docs/zh/architecture/remote-entity-call-design.md
// §5.4 + §7 route 过期处理。
//
// 线程纪律：resolve 为 const 只读；update_route/clear 由宿主单写者调用
// （装配期或 route 刷新回调）。进程内路由表，跨进程路由刷新走 RouteControl
// 消息（envelope kind=RouteControl）。
//

#include <cstdint>
#include <optional>
#include <unordered_map>

#include "apollo/net/remote/entity_ref.hpp"

namespace apollo::net::remote {

class RouteResolver {
public:
    // 解析实体当前路由；nullopt = 未知实体（需 route 刷新）
    std::optional<RemoteEntityRef> resolve(std::uint64_t entity_id) const;

    // 更新/插入路由（route_version 单调递增）
    void update_route(const RemoteEntityRef& ref);

    // 清空路由表（进程重启/全量刷新）
    void clear();

    // 当前路由版本（每次 update_route 递增）
    std::uint64_t route_version() const;

    // 路由表大小
    std::size_t size() const;

private:
    std::unordered_map<std::uint64_t, RemoteEntityRef> routes_;
    std::uint64_t                                      route_version_ = 0;
};

}  // namespace apollo::net::remote
