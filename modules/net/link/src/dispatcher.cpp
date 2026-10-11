// 按 id 分发器实现（net M1 内核 · 增量 2）。

#include "apollo/net/link/dispatcher.hpp"

namespace apollo::net::link {

DispatchResult Dispatcher::dispatch(std::uint16_t id, const std::uint8_t* data,
                                    std::size_t size) {
    if (table_ == nullptr) {
        // 未装配路由表 = 未完成装配：按未知 id 拒（不猜）。
        ++stats_.unknown_id;
        return DispatchResult::kUnknownId;
    }
    const RouteEntry* entry = table_->find(id);
    if (entry == nullptr) {
        ++stats_.unknown_id;
        return DispatchResult::kUnknownId;
    }
    if (!allowed_.empty() && allowed_.find(entry->domain) == allowed_.end()) {
        ++stats_.domain_forbidden;
        return DispatchResult::kDomainForbidden;
    }
    const auto it = handlers_.find(id);
    if (it != handlers_.end() && it->second) {
        it->second(id, data, size);
        ++stats_.dispatched;
        return DispatchResult::kDispatched;
    }
    if (fallback_) {
        fallback_(id, data, size);
    }
    ++stats_.no_handler;
    return DispatchResult::kNoHandler;
}

}  // namespace apollo::net::link
