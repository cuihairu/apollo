#include "apollo/net/discovery/discovery.hpp"

#include <algorithm>

namespace apollo::net::discovery {

namespace {

// 定长小端序读写原语（wire 序 = 小端；x86/ARM 宿主直接映射，逐字段编码保
// 端序明确）
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

} // namespace

// ---- WirePacket：定长编码（偏移表见头文件注释）----

void WirePacket::encode_to(std::uint8_t (&buf)[kWireSize]) const noexcept {
    std::memset(buf, 0, kWireSize);
    put_u32(buf + 0, magic);
    put_u16(buf + 4, version);
    buf[6] = static_cast<std::uint8_t>(op);
    buf[7] = reserved0;
    put_u64(buf + 8, component_id);
    put_u32(buf + 16, zone_id);
    put_u16(buf + 20, service_port);
    std::memcpy(buf + 22, reserved1, 3);
    put_u32(buf + 25, seq);
    // 29..31 保持 memset 零（尾随保留）
}

bool WirePacket::decode_from(const std::uint8_t (&buf)[kWireSize],
                             WirePacket& out) noexcept {
    if (get_u32(buf + 0) != kWireMagic || get_u16(buf + 4) != kWireVersion) {
        return false;  // magic/version 不符整包丢（版本协商骨架期无——单版期）
    }
    out.magic = kWireMagic;
    out.version = kWireVersion;
    const auto raw_op = buf[6];
    if (raw_op != static_cast<std::uint8_t>(Op::Register) &&
        raw_op != static_cast<std::uint8_t>(Op::Heartbeat) &&
        raw_op != static_cast<std::uint8_t>(Op::Deregister) &&
        raw_op != static_cast<std::uint8_t>(Op::Query) &&
        raw_op != static_cast<std::uint8_t>(Op::Advertise)) {
        return false;  // 未知 op 丢
    }
    out.op = static_cast<Op>(raw_op);
    out.reserved0 = buf[7];
    out.component_id = get_u64(buf + 8);
    out.zone_id = get_u32(buf + 16);
    out.service_port = get_u16(buf + 20);
    std::memcpy(out.reserved1, buf + 22, 3);
    out.seq = get_u32(buf + 25);
    return true;
}

// ---- DiscoveryBeacon ----

DiscoveryBeacon::DiscoveryBeacon(MemberId self, std::string directory_host,
                                 std::uint16_t directory_port,
                                 BeaconTransport& transport,
                                 std::uint32_t interval_ms)
    : self_(self)
    , directory_host_(std::move(directory_host))
    , directory_port_(directory_port)
    , transport_(&transport)
    , interval_ms_(interval_ms) {
}

bool DiscoveryBeacon::register_self() {
    return send(Op::Register);
}

bool DiscoveryBeacon::beat() {
    ++seq_;
    return send(Op::Heartbeat);
}

bool DiscoveryBeacon::deregister() {
    return send(Op::Deregister);
}

bool DiscoveryBeacon::send(Op op) {
    if (!self_.is_valid() || transport_ == nullptr) {
        return false;
    }
    WirePacket p;
    p.op = op;
    p.component_id = self_.component_id;
    p.zone_id = self_.zone_id;
    p.service_port = self_.service_port;
    p.seq = seq_;
    std::uint8_t buf[kWireSize];
    p.encode_to(buf);
    return transport_->send_to(directory_host_, directory_port_, buf);
}

// ---- DiscoveryRegistry ----

DiscoveryRegistry::DiscoveryRegistry(std::uint32_t heartbeat_interval_ms)
    : interval_ms_(heartbeat_interval_ms) {
}

bool DiscoveryRegistry::on_packet(const std::uint8_t (&buf)[kWireSize],
                                  std::uint64_t now_ms) {
    WirePacket p;
    if (!WirePacket::decode_from(buf, p)) {
        return false;
    }
    MemberId m;
    m.component_id = p.component_id;
    m.zone_id = p.zone_id;
    m.service_port = p.service_port;
    m.seq = p.seq;
    if (!m.is_valid()) {
        return false;  // 零组件号非法（编队身份不变式）
    }
    if (p.op == Op::Query || p.op == Op::Advertise) {
        return false;  // 发现层 op 不入成员面（machined 路由层分流应答）
    }

    auto it = std::find_if(entries_.begin(), entries_.end(),
                           [&](const RegistryEntry& e) {
                               return e.member.component_id == m.component_id;
                           });

    if (p.op == Op::Deregister) {
        if (it != entries_.end()) {
            entries_.erase(it);  // 优雅注销直接下线（不入死亡队列——主动行为
                                 // 与异常死亡语义分立，mgr 消费端只见异常）
        }
        return true;
    }

    if (it != entries_.end()) {
        it->member = m;        // 重复注册/心跳 = 刷新（幂等；服务端口可变更）
        it->last_seen_ms = now_ms;
    } else {
        entries_.push_back(RegistryEntry{m, now_ms});
    }
    return true;
}

void DiscoveryRegistry::expire(std::uint64_t now_ms) {
    const std::uint64_t ttl = ttl_ms();
    std::vector<RegistryEntry> alive;
    alive.reserve(entries_.size());
    for (auto& e : entries_) {
        if (now_ms - e.last_seen_ms > ttl) {
            deaths_.push_back(e.member);  // 超时死亡入队（mgr 消费端拉取）
        } else {
            alive.push_back(e);
        }
    }
    entries_ = std::move(alive);
}

std::vector<RegistryEntry> DiscoveryRegistry::members() const {
    return entries_;
}

std::size_t DiscoveryRegistry::member_count() const noexcept {
    return entries_.size();
}

std::uint32_t DiscoveryRegistry::ttl_ms() const noexcept {
    return interval_ms_ * kDeathTtlFactor;
}

std::vector<MemberId> DiscoveryRegistry::drain_deaths() {
    auto out = std::move(deaths_);
    deaths_.clear();
    return out;
}

} // namespace apollo::net::discovery
