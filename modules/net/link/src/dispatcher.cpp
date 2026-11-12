// 按 id 分发器实现（net M1 内核 · 增量 2）。

#include "apollo/net/link/dispatcher.hpp"

#include <exception>

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
        try {
            it->second(id, data, size);
        } catch (const std::exception&) {
            // 错误是数据不是控制流（正确性内核纪律）：handler 抛异常（坏 JSON/
            // 残帧——audit P1#11 原裸调 handler 无捕获，客户端残帧炸进程）不
            // 冒泡到传输层，帧按异常拒收计数，由调用方（link 收线）统计落地。
            ++stats_.handler_error;
            return DispatchResult::kHandlerError;
        }
        ++stats_.dispatched;
        return DispatchResult::kDispatched;
    }
    if (fallback_) {
        try {
            fallback_(id, data, size);
        } catch (const std::exception&) {
            ++stats_.handler_error;
            return DispatchResult::kHandlerError;
        }
    }
    ++stats_.no_handler;
    return DispatchResult::kNoHandler;
}

}  // namespace apollo::net::link
