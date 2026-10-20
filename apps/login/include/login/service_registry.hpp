#pragma once

// 服务目录注册表（L123 服务目录首切片）：Base/World 向 loginServer 登记
// 逻辑身份、进程代次、管理地址、兼容事实与就绪状态。
//
// 裁决口径同 World 侧 960 注册面（world-cell-management §4.4）：首注册受理；
// 同 ID 同代次幂等刷新（自报面整体更新）；新代次替换（重启换代）；旧代次
// 拒绝不可覆盖。身份 = 逻辑 ID + 进程代次（PID/IP 不当身份）。
//
// 线程纪律：register_service / sweep_expired / 观测面只在 M1 服务注册面泵
// 线程调用（受理点同步裁决——同 WorldApp 注册表单写者上下文），类不加锁。
//
// 目录不另行分配 Cell/World（L123 行内拍板）：本表只登记自报事实，落点
// 分配仍在 World 管理面（962）；位置投影发布随 L123 后续批。
// 认证凭据面同 960 惯例另行挂账（裸帧受理）。

#include <cstdint>
#include <string>
#include <unordered_map>

namespace login {

// 受理裁决码（965 code；词汇与 961 kCellRegister* 同型）
inline constexpr std::int32_t kServiceRegisterAccepted = 1;
inline constexpr std::int32_t kServiceRegisterAcceptedIdempotent =
    2;  // 同 ID 同代次刷新
inline constexpr std::int32_t kServiceRegisterRejectedStaleIncarnation =
    3;  // 旧代次不可覆盖
inline constexpr std::int32_t kServiceRegisterRejectedInvalid =
    4;  // 身份/代次非法或种类未知

// 服务种类（964 service_kind；未知值驳回）
inline constexpr std::int32_t kServiceKindBase = 0;
inline constexpr std::int32_t kServiceKindWorld = 1;

// 一条服务登记（自报面 + 受理面回填）
struct ServiceRegistration {
    std::int32_t service_kind = 0;
    std::uint64_t logical_id = 0;    // Base=区服号 / World=世界号
    std::uint64_t incarnation = 0;   // 进程代次（重启自增）
    std::string host;                // 管理/服务监听地址
    std::int32_t port = 0;           // 0 = 未监听，仅登记
    std::int64_t compat = 0;         // 兼容事实投影（0 = 未声明，记录不裁决）
    bool ready = false;              // 进程存活不等于可承接
    std::string peer;                // 链路自报名（回执按名寻路）
    std::uint64_t registered_at_ms = 0;  // steady 毫秒（last-heard 语义）
};

struct ServiceRegisterVerdict {
    bool accepted = false;
    std::int32_t code = 0;
    std::string detail;
};

class ServiceRegistry {
public:
    // 受理点（M1 泵线程上下文）：request_id=0 的保留语义由传输面先行滤除，
    // 不进本表。verdict.accepted = 登记/刷新成立（受理均回执）。
    ServiceRegisterVerdict register_service(const ServiceRegistration& reg,
                                            std::uint64_t now_ms) {
        ServiceRegisterVerdict v;
        if (reg.logical_id == 0 || reg.incarnation == 0 ||
            (reg.service_kind != kServiceKindBase &&
             reg.service_kind != kServiceKindWorld)) {
            // 身份/代次非法显式驳回——逻辑 ID 与进程代次是登记前提（0 号即缺位）；
            // 未知种类无消费面，同驳回不登记
            v.code = kServiceRegisterRejectedInvalid;
            v.detail = "invalid logical_id, incarnation or service_kind";
            return v;
        }
        auto& entry = entries_[key(reg.service_kind, reg.logical_id)];
        if (entry.logical_id == 0) {
            entry = reg;
            entry.registered_at_ms = now_ms;
            v.accepted = true;
            v.code = kServiceRegisterAccepted;
            v.detail = "registered";
            return v;
        }
        if (reg.incarnation < entry.incarnation) {
            // 旧代次不可覆盖新登记（重启获新代次——旧连接回拨拒绝）
            v.code = kServiceRegisterRejectedStaleIncarnation;
            v.detail = "stale incarnation: registered=" +
                       std::to_string(entry.incarnation);
            return v;
        }
        const bool idempotent = reg.incarnation == entry.incarnation;
        // 同代次幂等刷新 / 新代次替换：自报面整体刷新（peer 也换——重连 =
        // 同名或新名链路，回执按名寻路）
        const auto registered_at = now_ms;
        entry = reg;
        entry.registered_at_ms = registered_at;
        v.accepted = true;
        v.code = idempotent ? kServiceRegisterAcceptedIdempotent
                            : kServiceRegisterAccepted;
        v.detail = idempotent ? "re-registered (same incarnation, refreshed)"
                              : "replaced (new incarnation)";
        return v;
    }

    // 条目数（测试/运维观测；按 (kind, id) 复合键计）
    std::size_t registered_service_count() const { return entries_.size(); }

    const ServiceRegistration* find_service(std::int32_t service_kind,
                                            std::uint64_t logical_id) const {
        const auto it = entries_.find(key(service_kind, logical_id));
        return it == entries_.end() ? nullptr : &it->second;
    }

    // 心跳失效窗（L123 心跳失效切片）：last-heard 距今超过 timeout_ms 的
    // 登记移出目录——目录只登记可寻址的活服务，失效即不可寻址；同代次/
    // 新代次再注册走首注册路径重入（受理面幂等裁决照常）。timeout_ms=0
    // 不扫（纯登记面用法）。同一单写者泵上下文周期调用（受理面泵拍）。
    // 返回本次扫出条数；expired_total 累计（观测面，不参与裁决）。
    std::size_t sweep_expired(std::uint64_t now_ms, std::uint64_t timeout_ms) {
        if (timeout_ms == 0) {
            return 0;
        }
        std::size_t swept = 0;
        for (auto it = entries_.begin(); it != entries_.end();) {
            // 时钟面守卫：登记时刻晚于扫拍（同拍受理/时钟粒度）按 0 龄计，
            // 不让无符号减法回绕成巨龄误扫。
            const auto age = now_ms > it->second.registered_at_ms
                                 ? now_ms - it->second.registered_at_ms
                                 : 0;
            if (age > timeout_ms) {
                it = entries_.erase(it);
                ++swept;
            } else {
                ++it;
            }
        }
        expired_total_ += swept;
        return swept;
    }

    std::uint64_t expired_total() const { return expired_total_; }

private:
    static std::uint64_t key(std::int32_t service_kind, std::uint64_t logical_id) {
        // 种类与逻辑 ID 复合键：两域数值空间独立（Base 7 与 World 7 并存）
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(service_kind))
                << 56) |
               (logical_id & ((static_cast<std::uint64_t>(1) << 56) - 1));
    }

    std::unordered_map<std::uint64_t, ServiceRegistration> entries_;
    std::uint64_t expired_total_ = 0;
};

} // namespace login
