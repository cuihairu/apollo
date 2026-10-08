#pragma once

// 恢复相位跨进程化（G-1 收尾批增量③；session-and-online-directory §6
// manager 行 + net-abstraction §7 恢复相位排他的 G-1 平面骨架）。
//
// 设计口径（§6 崩溃恢复 = 全量重报重建）：manager 死 → machined 拉起 →
// 恢复相位排他（拒新直至收敛）→ 各 Zone/gateway **全量重报**现存会话 →
// 收敛开放。目录无 journal（易失索引，重建 < 1s @ 5000 条）。本文件给
// 该平面的三件：
//
//   - FullReport wire（"APD2" 家族 kind=4，与增量②目录镜像面同 magic
//     分 kind；多分片同 SnapshotReply 纪律）；
//   - FleetReporter   报告方（Zone/gateway 侧）：本地现存会话 → 分片
//     FullReport 连发（SendFn 注入，同 DirectoryPublisher 无 socket）；
//   - FleetRecoveryCoordinator manager 侧：恢复相位状态机（Normal ↔
//     Recovering），分片重组 + 收敛判定（全部在册报告方已报 **或** 超时
//     开放——恢复相位不无限扣住准入，§6「排他直至收敛」的骨架兜底）。
//
// intake 语义（restore-not-kick，§6）：报告方现存会话重建目录——已有
// 条目保 epoch 刷新绑定列、无条目 session_up 写入、**绝不出 Kicked**
// （恢复期无顶号）。具体落 PlayerDirectory::intake_full_report（app 层
// 经 ReportIntake 回调接线）。
//
// 范围注记：本增量只覆盖 §6 manager 行（恢复相位排他 + 全量重报 +
// 收敛开放）；Zone/gateway 死亡行的窗口处置（mark_suspended）需要花名
// 册 zone 列与 tick 驱动，留后续批。
//
// wire 格式（FullReport，kind=4）：公共头 8B（同 APD2）+ component_id
// u64 | zone_id u32 | report_id u32 | total_parts u16 | part_index u16 |
// entry_count u16 | entry[entry_count]×69B（条目格式与镜像面同——
// 69B 投影条目；报告方无 epoch 概念，epoch 列恒 0，裁决键归 manager）。
//
// 并发纪律：单线程驱动、调用方喂钟（now_ms），与 discovery/镜像面同源。

#include "apollo/game/session/directory_mirror.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace apollo::game::session {

// ---- wire 常量（FullReport，kind=4）----

inline constexpr std::uint8_t kDirectoryWireKindFleetReport = 4;
// FullReport 分片头（component_id + zone_id + report_id + total_parts +
// part_index + entry_count）
inline constexpr std::size_t kFleetReportPartHeaderSize = 30;

struct FleetReportPart {
    std::uint64_t component_id = 0;  // 报告方组件号（0 = 非法，整包丢）
    std::uint32_t zone_id = 0;       // 报告方宿主域（0 = 未分组）
    std::uint32_t report_id = 0;     // 报告轮次（同轮同 id；换 id = 新一轮）
    std::uint16_t total_parts = 1;
    std::uint16_t part_index = 0;    // 从 0 起
    std::vector<MirrorEntry> entries;
};

[[nodiscard]] std::size_t encode_fleet_report(const FleetReportPart& part,
                                              std::uint8_t* buf, std::size_t cap);
[[nodiscard]] bool decode_fleet_report(const std::uint8_t* data, std::size_t len,
                                       FleetReportPart& out);
// kind 判别（先路由后解码——管理侧收包分流用）
[[nodiscard]] bool is_fleet_report(const std::uint8_t* data, std::size_t len);

// ---- FleetReporter：报告方（Zone/gateway 侧）----

class FleetReporter {
public:
    // 发包口注入（同 DirectoryPublisher::SendFn；app 层接 UDP sendto）
    using SendFn = std::function<bool(const std::uint8_t* data, std::size_t len)>;

    FleetReporter(std::uint64_t component_id, std::uint32_t zone_id, SendFn send);

    // 全量重报：现存会话分片连发（每片 ≤ kMaxSnapshotPartEntries）。返回
    // 发送分片数（空表 = 1 空片——manager 侧 intake 空集 = 无会话幸存）。
    // component_id 0 拒发（wire 非法）。
    std::size_t report_full(const std::vector<MirrorEntry>& sessions);

    [[nodiscard]] std::uint64_t component_id() const noexcept {
        return component_id_;
    }

private:
    std::uint64_t component_id_;
    std::uint32_t zone_id_;
    SendFn send_;
    std::uint32_t next_report_id_ = 1;
};

// ---- FleetRecoveryCoordinator：manager 侧恢复相位 ----

class FleetRecoveryCoordinator {
public:
    enum class Phase : std::uint8_t {
        Normal = 0,    // 收敛开放（准入放行）
        Recovering,    // 恢复相位排他（拒新，等全量重报）
    };

    // 全量重报 intake 回调（app 层接 PlayerDirectory::intake_full_report；
    // component/zone 透传供日志与路由）
    using ReportIntake = std::function<void(
        std::uint64_t component_id, std::uint32_t zone_id,
        const std::vector<MirrorEntry>& sessions)>;

    // expected_reporters = 在册报告方数（收敛判据①；0 = 无在册报告方——
    // 恢复相位靠超时开放）；timeout_ms = 收敛判据②（自 begin 起超时即
    // 开放，恢复相位不无限扣住准入）
    FleetRecoveryCoordinator(std::size_t expected_reporters, std::uint32_t timeout_ms,
                             ReportIntake intake);

    // 进恢复相位（幂等：仅 Normal → Recovering；已 Recovering 无动作——
    // 截止时间不因重复 begin 漂移）。manager 重启即调用（machined 拉起
    // 后的启动序）。
    void begin(std::uint64_t now_ms);

    // FullReport 分片收包（解码守卫在 decode_fleet_report，此处只做重组
    // 纪律：同 component 严格按 part_index 升序、换 report_id 即重启、
    // 非起始片无处挂靠丢）。完整收讫 → intake 回调 + 记入已报集。返回
    // false = 乱序/非法丢。
    bool on_report_part(const FleetReportPart& part);

    // 喂钟 + 收敛判定：已报数 ≥ 在册数 **或** 超时 → Normal（收敛开放）
    void tick(std::uint64_t now_ms);

    // 准入闸门查询（恢复相位排他——app 层接 bindSession/assignWorld 拒新）
    [[nodiscard]] bool admissible() const noexcept {
        return phase_ == Phase::Normal;
    }
    [[nodiscard]] bool recovering() const noexcept {
        return phase_ == Phase::Recovering;
    }
    [[nodiscard]] Phase phase() const noexcept {
        return phase_;
    }
    [[nodiscard]] std::size_t reported_count() const noexcept {
        return reported_.size();
    }
    // 最近一次收敛时的已报方数（收敛即清零 reported_，观测面用此读数）
    [[nodiscard]] std::size_t last_reported_count() const noexcept {
        return last_reported_;
    }

private:
    void converge();

    struct Reassembly {
        std::uint32_t report_id = 0;
        std::uint16_t total_parts = 0;
        std::uint16_t next_part_index = 0;
        std::vector<MirrorEntry> entries;
    };

    std::size_t expected_reporters_;
    std::uint32_t timeout_ms_;
    ReportIntake intake_;
    Phase phase_ = Phase::Normal;
    std::uint64_t begun_at_ms_ = 0;
    std::size_t last_reported_ = 0;
    std::unordered_map<std::uint64_t, Reassembly> reassembly_;
    std::unordered_set<std::uint64_t> reported_;
};

} // namespace apollo::game::session
