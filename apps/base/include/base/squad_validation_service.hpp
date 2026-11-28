#pragma once

// Base 侧小队资格盖章与凭证签发编排（D11 issuer=Base 批③，04 §4.3）：
// WorldSquadClient（批①哑管道）之上的宿主裁决层——入向 972 资格校验请求
// 逐成员盖资格章（档案存在 + 未冻结，最小口径不发明门槛）回 973；全员
// 合格即发 974 容量预留申请（槽数=成员数）；入向 975 grant 且预留窗未过
// 逐成员签发 976 凭证帧（issuer=base:<id>，967 每条目一帧）。
//
// 资格规则（04 §4.3 资格规则最小口径）：成员在原 Base 档案存在 + 未冻结
// （迁移冻结标记在册，即 PlayerStore 锚点 Transferring）即合格；缺档与
// 冻结并存时报缺档码（1），failed_members 带命中码类别的成员清单。
//
// 名册权威仍在 cell 侧 PartyManager；Base 只对送来的名册快照盖章，不记账
// 不重试（哑管道纪律——973/974/976 发送失败只计数，重试语义另批）。
//
// 线程契约：与 WorldSquadClient 同缝——回调在其 pump() 内同步触发，本类
// 全部状态归属该 pump 上下文单线程；计数账面 mutex 守供宿主/测试跨线程读。
// 回调内可直接调 send_*（同泵上下文），不得重入 link.pump()。

#include "base/world_squad_client.hpp"

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace base {

// 资格视图 seam：生产适配 PlayerStore（锚点在册 = 档案存在；Transferring
// = 冻结），测试可编程注入。
class SquadEligibilityView {
public:
    virtual ~SquadEligibilityView() = default;
    [[nodiscard]] virtual bool member_exists(std::uint64_t player_id) const = 0;
    [[nodiscard]] virtual bool member_frozen(std::uint64_t player_id) const = 0;
};

// 编排骨面快照（泵上下文写，宿主/测试跨线程读——mutex 守）。计数是累计面。
struct SquadValidationCounters {
    std::uint64_t validations_in = 0;      // 收 972
    std::uint64_t accepted_out = 0;        // 发 973 accepted=1
    std::uint64_t rejections_out = 0;      // 发 973 accepted=0
    std::uint64_t capacity_requests_out = 0;  // 发 974
    std::uint64_t grants_in = 0;           // 收 975
    std::uint64_t credentials_out = 0;     // 发 976 逐成员帧
    std::uint64_t grants_rejected = 0;     // 975 granted=0（容量/实例/排空）
    std::uint64_t grants_expired = 0;      // 975 granted 但签发时窗已过
    std::uint64_t grants_unknown = 0;      // 975 无在册申请（重放/未知号）
    std::uint64_t send_failures = 0;       // 链路未 up 拒发累计
    std::uint64_t bad_rosters = 0;         // 972 名册空/解析非法（code 3）
};

// 973 code（契约口径）：0=全员合格；1=有成员档案缺席；2=有成员冻结；
// 3=名册空/非法；4=Base 不受理（本服务不判归属，World 路由面职责）。
struct SquadValidationServiceConfig {
    std::uint64_t base_id = 0;  // issuer 铸 "base:<id>"
};

class SquadValidationService {
public:
    // 装配即接管 link 的两条入向回调（972/975）；view 生命周期由宿主管
    // （引用持有，须活过本服务）。clock 缺省取 steady 挂钟毫秒（测试注入）。
    SquadValidationService(WorldSquadClient& link,
                           const SquadEligibilityView& view,
                           SquadValidationServiceConfig config,
                           std::function<std::uint64_t()> clock = {});

    [[nodiscard]] SquadValidationCounters counters() const;

    // 在册容量预留申请条数（974 已发待 975 裁决；观测面）。
    [[nodiscard]] std::size_t pending_capacity() const;

private:
    void on_validation(const SquadValidationRequest& request);
    void on_grant(const SquadCapacityGrant& grant);

    struct PendingCapacity {
        std::uint64_t party_id = 0;
        std::uint64_t instance_id = 0;
        std::uint64_t cell_id = 0;
        std::string roster_hash;
        std::vector<std::uint64_t> members;
    };

    void bump(std::uint64_t SquadValidationCounters::*field);

    WorldSquadClient& link_;
    const SquadEligibilityView& view_;
    SquadValidationServiceConfig config_;
    std::function<std::uint64_t()> clock_;

    mutable std::mutex mutex_;
    SquadValidationCounters counters_;
    std::unordered_map<std::uint64_t, PendingCapacity> pending_;
    std::uint64_t next_request_id_ = 1;      // 974 关联键（0 保留）
    std::uint64_t next_credential_id_ = 1;   // 976 凭证号（签发方分配）
};

}  // namespace base
