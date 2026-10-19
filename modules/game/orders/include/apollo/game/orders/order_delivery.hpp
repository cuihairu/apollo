#pragma once

// 投递驱动（todo L63 批⑯）——把待办订单送到「玩家当前 Base」。
//
// 解析缝（BaseResolver）：每拍、每次尝试都重新解析玩家当前归属（地址 +
// 归属代次），驱动自身不缓存任何 Base 身份——迁移/升级换归属后下一次
// 尝试即投新家（resolver_calls 计数可证：尝试数 == 解析数）。返回
// nullopt（玩家离线/目录未起/迁移中）→ 本拍挂起，订单留在待办按退避
// 重试，不永久绑定 Base ID/IP。
//
// 通道缝（DeliveryChannel）：返回 true 表示业务确认完成（发货 executor
// 已落账），不是「对端收到」；false/异常 = 本拍未完成，退避重试。
//
// 退避：首次尝试立即；第 N 次失败后 next_due = now + initial * 2^(N-1)，
// 封顶 max_backoff；每拍预算 max_attempts_per_tick，超预算的待办留下拍
// （受理序稳定推进，不饿死队头）。

#include "apollo/game/orders/order_store.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>

namespace apollo::game::orders {

struct BaseEndpoint {
    std::string address;
    std::uint64_t generation = 0;  // 归属代次（完成投递的记账凭据）
};

// 玩家当前归属解析：nullopt = 当前无 Base（离线/不可达/目录丢失）。
using BaseResolver =
    std::function<std::optional<BaseEndpoint>(std::uint64_t player_id)>;

// 投递通道：true = 业务确认完成（非单纯收到）。
using DeliveryChannel =
    std::function<bool(const OrderRecord& record, const BaseEndpoint& base)>;

struct DeliveryConfig {
    std::uint64_t initial_backoff_ms = 1000;
    std::uint64_t max_backoff_ms = 60000;
    std::size_t max_attempts_per_tick = 16;
};

class OrderDelivery {
public:
    struct TickStats {
        std::size_t processed{0};  // 本拍到期受理的单数（预算内）
        std::size_t delivered{0};
        std::size_t deferred{0};   // 到期但本拍未完成（无 Base/拒绝/持久失败）
    };

    OrderDelivery(OrderStore& store, BaseResolver resolver,
                  DeliveryChannel channel,
                  DeliveryConfig config = {});

    // 推进一拍：消费到期待办（预算内），成功即 mark_delivered 持久置墓碑。
    TickStats tick(std::int64_t now_ms);

    // 第 N 次失败后的退避时长（initial * 2^(N-1)，封顶 max）。
    [[nodiscard]] std::uint64_t backoff_for(std::uint64_t failed_attempts) const;

    std::uint32_t attempts(std::uint64_t order_id) const;       // 失败计数（成功清零）
    const std::string* last_error(std::uint64_t order_id) const;  // 最近未完成原因
    std::uint64_t resolver_calls() const { return resolver_calls_; }

private:
    struct RetryState {
        std::uint32_t attempts{0};
        std::int64_t next_due_ms{0};
        std::string last_error;
    };

    void note_failure(std::uint64_t order_id, std::int64_t now_ms,
                      const char* reason);

    OrderStore& store_;
    BaseResolver resolver_;
    DeliveryChannel channel_;
    DeliveryConfig config_;
    std::map<std::uint64_t, RetryState> retry_;
    std::uint64_t resolver_calls_ = 0;
};

}  // namespace apollo::game::orders
