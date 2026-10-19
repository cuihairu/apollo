#include "apollo/game/orders/order_delivery.hpp"

#include <algorithm>

namespace apollo::game::orders {

OrderDelivery::OrderDelivery(OrderStore& store, BaseResolver resolver,
                             DeliveryChannel channel, DeliveryConfig config)
    : store_(store),
      resolver_(std::move(resolver)),
      channel_(std::move(channel)),
      config_(config) {}

OrderDelivery::TickStats OrderDelivery::tick(std::int64_t now_ms) {
    TickStats stats;
    for (const OrderRecord& record : store_.pending()) {
        if (stats.processed >= config_.max_attempts_per_tick) {
            break;  // 预算用尽：余单留待办，下拍按受理序继续
        }
        const auto rit = retry_.find(record.order_id);
        if (rit != retry_.end() && now_ms < rit->second.next_due_ms) {
            continue;  // 退避未到期
        }
        ++stats.processed;
        ++resolver_calls_;  // 每次尝试必解析：不缓存 Base 身份的直接证据
        std::optional<BaseEndpoint> base;
        if (resolver_) {
            base = resolver_(record.player_id);
        }
        if (!base || base->generation == 0 || base->address.empty()) {
            note_failure(record.order_id, now_ms, "no current base");
            ++stats.deferred;
            continue;
        }
        bool completed = false;
        if (channel_) {
            try {
                completed = channel_(record, *base);
            } catch (...) {
                completed = false;  // 通道异常按未完成计，链不断
            }
        }
        if (!completed) {
            note_failure(record.order_id, now_ms, "channel not confirmed");
            ++stats.deferred;
            continue;
        }
        // 业务已确认；持久置墓碑失败则不冒充完成——留在待办下拍重投
        // （executor 将收到重复投递，判重归 L64 订单事务域，此处不谎报）。
        if (store_.mark_delivered(record.order_id, base->generation, now_ms)) {
            retry_.erase(record.order_id);
            ++stats.delivered;
        } else {
            note_failure(record.order_id, now_ms, "delivery ack not durable");
            ++stats.deferred;
        }
    }
    return stats;
}

std::uint64_t OrderDelivery::backoff_for(std::uint64_t failed_attempts) const {
    // 逐倍增长（封顶即止，防移位溢出）。
    std::uint64_t value = config_.initial_backoff_ms;
    for (std::uint64_t i = 1; i < failed_attempts && value < config_.max_backoff_ms;
         ++i) {
        value *= 2;
    }
    return std::min(value, config_.max_backoff_ms);
}

void OrderDelivery::note_failure(std::uint64_t order_id, std::int64_t now_ms,
                                 const char* reason) {
    RetryState& state = retry_[order_id];
    state.attempts += 1;
    state.next_due_ms =
        now_ms + static_cast<std::int64_t>(backoff_for(state.attempts));
    state.last_error = reason;
}

std::uint32_t OrderDelivery::attempts(std::uint64_t order_id) const {
    const auto it = retry_.find(order_id);
    return it == retry_.end() ? 0 : it->second.attempts;
}

const std::string* OrderDelivery::last_error(std::uint64_t order_id) const {
    const auto it = retry_.find(order_id);
    return it == retry_.end() ? nullptr : &it->second.last_error;
}

}  // namespace apollo::game::orders
