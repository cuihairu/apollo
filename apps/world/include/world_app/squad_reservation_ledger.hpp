#pragma once

// 小队容量预留账本（D11 issuer=Base 批②，04 §4.3 预留生命周期）：Base 盖章
// 后持 roster_hash 申请容量（974 squad_capacity_request），World 对目标
// instance 所在 cell 记账预留并回 975 grant/拒绝码。S03 槽位语义锚定（07
// §3.1）：容量槽位 = 上报窗采样 / 心跳窗过期 / World 准入消费——本批容量
// 源经 CapacityView 注入（现役装配 = 配置面过桥，真自报随 S03 落地批换装），
// 账本只记自己的 hold（已预留未消费），有效余量 = 视图余量 − 本账 hold。
//
// 生命周期：request 预留（slots = 成员数）→ grant 带过期窗 → 成员 enter 逐
// 凭证 consume（每张释放一槽，hold 清零即销账）→ 到期未消费 sweep 整批回收
//（design：签发时预留已过期按拒绝处理 → validate 为签发面窗口检查，窗已过
// = code 4 回 Base）。拒绝码显式回传不静默降级：1=无容量 2=实例未知
// 3=排空中 4=窗已过（validate false 时签发面用）。
//
// 线程口径：单写者（M1 泵上下文 = gameThread_，同 CellSpaceReport 受理点
// 记账纪律）——无内部锁；测试单线程直驱。

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace world_app {

// 974 squad_capacity_request 解码视图（字段序 = messages.xml 974）。
struct SquadCapacityRequest {
    std::uint64_t request_id = 0;
    std::uint64_t party_id = 0;
    std::uint64_t instance_id = 0;
    std::uint64_t cell_id = 0;
    std::string roster_hash;
    std::int32_t slots = 0;
};

// 容量视图（注入缝）：容量记账事实源在 cell 自报面（S03），账本不发明。
class SquadCapacityView {
public:
    virtual ~SquadCapacityView() = default;
    // 目标 instance 是否已知（未知 → code 2）。
    virtual bool instance_known(std::uint64_t instance_id) const = 0;
    // 目标 instance 是否排空中（→ code 3）。
    virtual bool instance_draining(std::uint64_t instance_id) const = 0;
    // 目标 cell 当前可分配余量（未扣账本 hold 的原值；→ 差值不足 code 1）。
    virtual std::int32_t free_slots(std::uint64_t cell_id) const = 0;
};

// 975 squad_capacity_grant 回执字段（granted = code == 0）。
struct SquadCapacityGrant {
    std::uint64_t request_id = 0;
    bool granted = false;
    std::string reservation_id;   // granted 时非空
    std::uint64_t expires_at = 0; // granted 时 = now + window（毫秒）
    std::int32_t code = 0;        // 0=已预留 1=无容量 2=实例未知 3=排空中
};

class SquadReservationLedger {
public:
    // capacity_view 生命周期由调用方持有（≥ 账本）；grant_window_ms = 预留
    // 过期窗（毫秒，975 expires_at 事实源）。
    SquadReservationLedger(const SquadCapacityView& capacity_view,
                           std::uint64_t grant_window_ms);

    // 预留申请（974 受理点）：slots <= 0 按无容量拒（code 1，不记账）；
    // granted 时 reservation_id 非空（"sq-<n>" 账本单调铸造）。
    SquadCapacityGrant request(const SquadCapacityRequest& req,
                               std::uint64_t now_ms);

    // 签发面窗口检查（批③ Base 签发前查，窗已过 = code 4 拒）：live = 在册
    // 且未过期。不消费不推进。
    bool validate(const std::string& reservation_id,
                  std::uint64_t now_ms) const;

    // 成员 enter 凭证消费：每张释放一槽（hold -1），hold 清零即销账。
    // 未知/已过期/已销账 = false（过期即回收，见 sweep_expired）。
    bool consume(const std::string& reservation_id, std::uint64_t now_ms);

    // 到期整批回收（gameLoop 拍驱）：返回回收条数，hold 全释放。
    std::size_t sweep_expired(std::uint64_t now_ms);

    // 观测面：账本 hold 占用（cell 维度）；live 条数。
    std::int32_t held_slots(std::uint64_t cell_id) const;
    std::size_t live_count() const { return reservations_.size(); }

    // 预留快照（观测/测试断言用，单写者上下文读）。
    struct Reservation {
        std::string reservation_id;
        std::uint64_t party_id = 0;
        std::uint64_t instance_id = 0;
        std::uint64_t cell_id = 0;
        std::string roster_hash;
        std::int32_t slots = 0;       // 原始申请槽数
        std::int32_t held = 0;        // 未消费余槽
        std::uint64_t expires_at = 0;
    };
    bool find(const std::string& reservation_id, Reservation& out) const;

private:
    struct Entry {
        Reservation view;
    };

    const SquadCapacityView* view_;
    std::uint64_t grant_window_ms_;
    std::uint64_t next_id_ = 1;
    std::unordered_map<std::string, Entry> reservations_;
    // cell 维度 hold 合计（reservations_ 的投影，O(1) 读）。
    std::unordered_map<std::uint64_t, std::int32_t> holds_;
};

} // namespace world_app
