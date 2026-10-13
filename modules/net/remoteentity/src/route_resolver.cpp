#include "apollo/net/remote/route_resolver.hpp"

namespace apollo::net::remote {

std::optional<RemoteEntityRef> RouteResolver::resolve(std::uint64_t entity_id) const {
    auto it = routes_.find(entity_id);
    if (it == routes_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void RouteResolver::update_route(const RemoteEntityRef& ref) {
    routes_[ref.entity_id] = ref;
    ++route_version_;
}

void RouteResolver::clear() {
    routes_.clear();
}

std::uint64_t RouteResolver::route_version() const {
    return route_version_;
}

std::size_t RouteResolver::size() const {
    return routes_.size();
}

}  // namespace apollo::net::remote
