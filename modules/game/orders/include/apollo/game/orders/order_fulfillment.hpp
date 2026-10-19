#pragma once

// 订单履约事务（todo L64 批⑰）——订单校验 + 奖励与去重凭据原子持久提交。
//
// 与 OrderStore 的分工：OrderStore 记「受理 → 已投递」的渠道回执账（确认收到
// ≠ 发货完成）；本类记「发货已完成」的去重凭据账，凭据 journal 是奖励落账的
// 唯一权威——append+flush 成功才算发货完成，凭据命中即幂等短路（不重复发、
// 不重复记账）。渠道重发、回执丢失、驱动重投（mark_delivered 落盘失败后的
// 再投递）在此都收敛为「一次发货」。当前 DB_LOAD/DB_SAVE 原型不做凭据原子性
// （档案写与凭据记之间可崩、无幂等锚），不能代替本域。
//
// 原子性口径：凭据 append（write-ahead）与奖励 apply 收在同一临界区——先
// append 凭据，再 apply；崩溃在两步之间由 open() 回放逐条重放 apply 补齐
// （apply 以凭据为幂等锚，可重入）。apply 抛异常：凭据保留（记账已成立）、
// 回执 granted=false（不谎报完成），重投时凭据命中且 apply 未落账 → 补放
// 一次（本进程内每单至多补放，成功后不再重复 apply）。
//
// 校验面（订单校验 + Base 唯一写权）：
//  1. 订单须经 OrderStore 受理（未受理单号拒——伪造单不发货）；
//  2. 载荷行格式双保险（accept 已保证，重查）；
//  3. 写权：authority_view(订单玩家) 的当前归属必须与投递端点同址同代次
//     （Base 迁移/并发登录后旧 Base 的迟到履约拒）；无归属（离线/目录丢失）拒。
//
// 凭据 journal 文本行（append-only 事件序，5 字段）：
//   order_id|player_id|base_generation|fulfilled_ms|payload
// 回放即重建凭据索引并重放 apply；凭据账自带最小重放载荷（不依赖 OrderStore）。
//
// 线程模型：类内互斥；不嵌套持有 OrderStore 锁（find() 返回后即释放）。

#include <cstdint>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>

#include "apollo/game/orders/order_delivery.hpp"  // BaseEndpoint, BaseResolver
#include "apollo/game/orders/order_store.hpp"     // OrderRecord

namespace apollo::game::orders {

class OrderFulfillment {
public:
    struct Result {
        bool granted{false};    // 可回执成功（本次新发或幂等命中）
        bool duplicate{false};  // 幂等命中：此前已发货，本次未再发
        bool rejected{false};   // 校验/提交拒绝
        std::string reason;     // 拒绝原因（观测面）
    };

    // 奖励落账执行器（业务回调）：以凭据为幂等锚，可重入（open() 回放期同函数）。
    using RewardApply = std::function<void(const OrderRecord&)>;

    OrderFulfillment(OrderStore& store, std::string credential_journal_path,
                     BaseResolver authority_view);

    // 打开/恢复：回放凭据 journal，重建索引 + 逐条重放 apply（补崩溃窗）。
    // apply 为空 → 回放仅重建索引（不重放奖励）。
    bool open(RewardApply apply);

    // 履约一笔：校验 → 凭据幂等 → 原子提交（append 后 apply）。
    Result fulfill(std::uint64_t order_id, const BaseEndpoint& via, std::int64_t now_ms);

    bool has_credential(std::uint64_t order_id) const;
    std::size_t credential_count() const;
    std::size_t replay_apply_failures() const;  // 回放期 apply 抛异常条数（观测面）

private:
    struct CredentialRecord {
        std::uint64_t order_id = 0;
        std::uint64_t player_id = 0;
        std::uint64_t base_generation = 0;
        std::int64_t fulfilled_ms = 0;
        std::string payload;
    };

    static OrderRecord to_record(const CredentialRecord& credential);
    std::string render_line(const CredentialRecord& credential) const;
    bool append_line(const std::string& line);
    bool replay(RewardApply& apply);

    OrderStore& store_;
    BaseResolver authority_view_;
    std::string journal_path_;
    RewardApply apply_;
    mutable std::mutex mutex_;
    std::map<std::uint64_t, CredentialRecord> credentials_;
    std::set<std::uint64_t> applied_;  // 本进程内 apply 已成功的单号
    std::ofstream out_;
    bool opened_ = false;
    std::size_t replay_apply_failures_ = 0;
};

}  // namespace apollo::game::orders
