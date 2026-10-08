#include "apollo/game/session/fleet_recovery.hpp"

#include <algorithm>
#include <cstring>

namespace apollo::game::session {

namespace {

// 定长小端序读写原语（与目录镜像面同纪律：逐字段编码不 memcpy 结构体）
void put_u16(std::uint8_t* p, std::uint16_t v) noexcept {
    p[0] = static_cast<std::uint8_t>(v & 0xFF);
    p[1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
}

void put_u32(std::uint8_t* p, std::uint32_t v) noexcept {
    for (int i = 0; i < 4; ++i) {
        p[i] = static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF);
    }
}

void put_u64(std::uint8_t* p, std::uint64_t v) noexcept {
    for (int i = 0; i < 8; ++i) {
        p[i] = static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF);
    }
}

std::uint16_t get_u16(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

std::uint32_t get_u32(const std::uint8_t* p) noexcept {
    std::uint32_t v = 0;
    for (int i = 0; i < 4; ++i) {
        v |= static_cast<std::uint32_t>(p[i]) << (8 * i);
    }
    return v;
}

std::uint64_t get_u64(const std::uint8_t* p) noexcept {
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v |= static_cast<std::uint64_t>(p[i]) << (8 * i);
    }
    return v;
}

// 镜像面 69B 条目编解码复用（directory_mirror.cpp 的 put_entry/get_entry
// 布局契约——条目列两平面同格式，不在本文件重复定偏移）
void put_entry(std::uint8_t* p, const MirrorEntry& e) noexcept {
    put_u64(p, e.player_id);
    put_u64(p + 8, e.anchor_epoch);
    put_u64(p + 16, e.session_id);
    put_u32(p + 24, e.gateway_id);
    put_u32(p + 28, e.zone_id);
    p[32] = static_cast<std::uint8_t>(e.state);
    put_u32(p + 33, e.assignment.world_id);
    put_u64(p + 37, e.assignment.map_id);
    put_u64(p + 45, e.assignment.instance_id);
    put_u64(p + 53, e.assignment.space_id);
    put_u64(p + 61, e.assignment.route_version);
}

MirrorEntry get_entry(const std::uint8_t* p) noexcept {
    MirrorEntry e;
    e.player_id = get_u64(p);
    e.anchor_epoch = get_u64(p + 8);
    e.session_id = get_u64(p + 16);
    e.gateway_id = get_u32(p + 24);
    e.zone_id = get_u32(p + 28);
    const auto raw_state = p[32];
    if (raw_state <= static_cast<std::uint8_t>(PlayerDirectory::EntryState::Leaving)) {
        e.state = static_cast<PlayerDirectory::EntryState>(raw_state);
    }
    e.assignment.world_id = get_u32(p + 33);
    e.assignment.map_id = get_u64(p + 37);
    e.assignment.instance_id = get_u64(p + 45);
    e.assignment.space_id = get_u64(p + 53);
    e.assignment.route_version = get_u64(p + 61);
    return e;
}

} // namespace

// ---- wire ----

std::size_t encode_fleet_report(const FleetReportPart& part, std::uint8_t* buf,
                                std::size_t cap) {
    if (buf == nullptr || part.component_id == 0 ||
        part.entries.size() > kMaxSnapshotPartEntries) {
        return 0;
    }
    const std::size_t need =
        kFleetReportPartHeaderSize + part.entries.size() * kDirectoryEntryWireSize;
    if (cap < need) {
        return 0;
    }
    std::memset(buf, 0, need);
    put_u32(buf, kDirectoryWireMagic);
    put_u16(buf + 4, kDirectoryWireVersion);
    buf[6] = kDirectoryWireKindFleetReport;
    // buf[7] reserved 恒零
    put_u64(buf + 8, part.component_id);
    put_u32(buf + 16, part.zone_id);
    put_u32(buf + 20, part.report_id);
    put_u16(buf + 24, part.total_parts);
    put_u16(buf + 26, part.part_index);
    put_u16(buf + 28, static_cast<std::uint16_t>(part.entries.size()));
    for (std::size_t i = 0; i < part.entries.size(); ++i) {
        put_entry(buf + kFleetReportPartHeaderSize + i * kDirectoryEntryWireSize,
                  part.entries[i]);
    }
    return need;
}

bool is_fleet_report(const std::uint8_t* data, std::size_t len) {
    return data != nullptr && len >= kFleetReportPartHeaderSize &&
           get_u32(data) == kDirectoryWireMagic &&
           get_u16(data + 4) == kDirectoryWireVersion &&
           data[6] == kDirectoryWireKindFleetReport;
}

bool decode_fleet_report(const std::uint8_t* data, std::size_t len,
                         FleetReportPart& out) {
    if (!is_fleet_report(data, len)) {
        return false;
    }
    const std::uint16_t count = get_u16(data + 28);
    if (count > kMaxSnapshotPartEntries ||
        len != kFleetReportPartHeaderSize +
                   static_cast<std::size_t>(count) * kDirectoryEntryWireSize) {
        return false;  // 条目数越限 / 长度与声明不符整包丢
    }
    out.component_id = get_u64(data + 8);
    if (out.component_id == 0) {
        return false;  // 零组件号报告丢（非合法编队成员）
    }
    out.zone_id = get_u32(data + 16);
    out.report_id = get_u32(data + 20);
    if (out.report_id == 0) {
        return false;  // 轮次 0 = 报告方未初始化（Reporter 自 1 起发号）
    }
    out.total_parts = get_u16(data + 24);
    out.part_index = get_u16(data + 26);
    if (out.total_parts == 0 || out.part_index >= out.total_parts) {
        return false;
    }
    out.entries.clear();
    out.entries.reserve(count);
    for (std::uint16_t i = 0; i < count; ++i) {
        out.entries.push_back(
            get_entry(data + kFleetReportPartHeaderSize + i * kDirectoryEntryWireSize));
    }
    return true;
}

// ---- FleetReporter ----

FleetReporter::FleetReporter(std::uint64_t component_id, std::uint32_t zone_id,
                             SendFn send)
    : component_id_(component_id)
    , zone_id_(zone_id)
    , send_(std::move(send)) {
}

std::size_t FleetReporter::report_full(const std::vector<MirrorEntry>& sessions) {
    if (!send_ || component_id_ == 0) {
        return 0;
    }
    const std::uint32_t report_id = next_report_id_++;
    std::size_t total_parts =
        (sessions.size() + kMaxSnapshotPartEntries - 1) / kMaxSnapshotPartEntries;
    if (total_parts == 0) {
        total_parts = 1;  // 空表 = 1 空片（intake 空集 = 无会话幸存）
    }
    std::size_t sent = 0;
    for (std::size_t part = 0; part < total_parts; ++part) {
        FleetReportPart p;
        p.component_id = component_id_;
        p.zone_id = zone_id_;
        p.report_id = report_id;
        p.total_parts = static_cast<std::uint16_t>(total_parts);
        p.part_index = static_cast<std::uint16_t>(part);
        const auto begin = sessions.begin() +
                           static_cast<std::ptrdiff_t>(part * kMaxSnapshotPartEntries);
        const auto end = sessions.begin() +
                         static_cast<std::ptrdiff_t>(
                             std::min(sessions.size(), (part + 1) * kMaxSnapshotPartEntries));
        p.entries.assign(begin, end);
        std::uint8_t buf[kFleetReportPartHeaderSize +
                         kMaxSnapshotPartEntries * kDirectoryEntryWireSize];
        const std::size_t len = encode_fleet_report(p, buf, sizeof(buf));
        if (len != 0 && send_(buf, len)) {
            ++sent;
        }
    }
    return sent;
}

// ---- FleetRecoveryCoordinator ----

FleetRecoveryCoordinator::FleetRecoveryCoordinator(std::size_t expected_reporters,
                                                   std::uint32_t timeout_ms,
                                                   ReportIntake intake)
    : expected_reporters_(expected_reporters)
    , timeout_ms_(timeout_ms)
    , intake_(std::move(intake)) {
}

void FleetRecoveryCoordinator::begin(std::uint64_t now_ms) {
    if (phase_ == Phase::Recovering) {
        return;  // 幂等：截止时间不因重复 begin 漂移
    }
    phase_ = Phase::Recovering;
    begun_at_ms_ = now_ms;
    reported_.clear();
    reassembly_.clear();
}

void FleetRecoveryCoordinator::converge() {
    phase_ = Phase::Normal;
    last_reported_ = reported_.size();
    reassembly_.clear();
    reported_.clear();
}

bool FleetRecoveryCoordinator::on_report_part(const FleetReportPart& part) {
    if (part.component_id == 0 || part.total_parts == 0 ||
        part.part_index >= part.total_parts) {
        return false;
    }
    Reassembly& r = reassembly_[part.component_id];
    if (part.part_index == 0) {
        if (r.report_id == part.report_id && r.next_part_index != 0) {
            return false;  // 首片重放（重组已推进到后继片）丢
        }
        if (r.report_id != part.report_id) {
            r = Reassembly{};  // 新一轮（或旧轮残骸）重启重组
            r.report_id = part.report_id;
        }
        r.total_parts = part.total_parts;
    } else if (r.report_id != part.report_id ||
               r.next_part_index != part.part_index ||
               r.total_parts != part.total_parts) {
        return false;  // 乱序/换轮：非起始片无处挂靠，丢
    }
    r.entries.insert(r.entries.end(), part.entries.begin(), part.entries.end());
    r.next_part_index = static_cast<std::uint16_t>(part.part_index + 1);
    if (r.next_part_index != r.total_parts) {
        return true;  // 重组中
    }

    // 完整收讫：intake + 记入已报集
    if (intake_) {
        intake_(part.component_id, part.zone_id, r.entries);
    }
    reported_.insert(part.component_id);
    reassembly_.erase(part.component_id);
    if (phase_ == Phase::Recovering && reported_.size() >= expected_reporters_) {
        converge();  // 收敛判据①：全部在册报告方已报
    }
    return true;
}

void FleetRecoveryCoordinator::tick(std::uint64_t now_ms) {
    if (phase_ != Phase::Recovering) {
        return;
    }
    // 收敛判据②：超时开放（恢复相位不无限扣住准入；迟到报告 intake 面照收）
    if (now_ms - begun_at_ms_ >= timeout_ms_) {
        converge();
    }
}

} // namespace apollo::game::session
