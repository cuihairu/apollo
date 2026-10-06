// 进程编队服务发现单测（P3-1 批 B，G-1 库层）。
//
// 覆盖（骨架口径——内存桩传输，无 socket）：
//   1. WirePacket：编码→解码往返一致 + 偏移表（前 4 字节即 magic）；
//   2. 报文校验：错 magic / 错 version / 未知 op / 零 component_id 整包丢；
//   3. Registry：注册→members 可见；心跳/重复注册幂等刷新 last_seen；
//   4. TTL 死亡：expire 超时移出 + drain_deaths 收到；
//   5. 优雅注销：即时下线且不入死亡队列（与异常死亡语义分立）；
//   6. Beacon：发包走 transport、is_valid 拒发、beat 自增 seq。

#include "apollo/net/discovery/discovery.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#define TEST_ASSERT(cond, msg)                                                               \
    do {                                                                                     \
        if (!(cond)) {                                                                       \
            std::cerr << "FAIL: " << (msg) << " (" << __FILE__ << ":" << __LINE__ << ")"     \
                      << std::endl;                                                          \
            return false;                                                                    \
        }                                                                                    \
    } while (0)

namespace {

using namespace apollo::net::discovery;

// 内存桩传输：捕获目的端点与全部原始报文（wire 字节）
struct CapturingTransport : BeaconTransport {
    std::string host;
    std::uint16_t port = 0;
    std::vector<std::uint8_t> last_packet;
    int send_count = 0;

    bool send_to(const std::string& h, std::uint16_t p,
                 const std::uint8_t (&buf)[kWireSize]) override {
        host = h;
        port = p;
        last_packet.assign(buf, buf + kWireSize);
        ++send_count;
        return true;
    }
};

MemberId make_member(std::uint64_t cid, std::uint16_t port = 7000) {
    MemberId m;
    m.component_id = cid;
    m.zone_id = 1;
    m.service_port = port;
    m.seq = 0;
    return m;
}

bool test_wire_roundtrip_and_layout() {
    WirePacket p;
    p.op = Op::Heartbeat;
    p.component_id = 0x1122334455667788ull;
    p.zone_id = 3;
    p.service_port = 6001;
    p.seq = 42;

    std::uint8_t buf[kWireSize];
    p.encode_to(buf);

    TEST_ASSERT(buf[0] == 0x31 && buf[1] == 0x44 && buf[2] == 0x50 && buf[3] == 0x41,
                "magic 0x41504431 小端铺开（LSB 先行，偏移表锚点）");
    TEST_ASSERT(buf[29] == 0 && buf[30] == 0 && buf[31] == 0, "尾随 3B 保留零");

    WirePacket out;
    TEST_ASSERT(WirePacket::decode_from(buf, out), "合法报文解码成功");
    TEST_ASSERT(out.op == Op::Heartbeat && out.component_id == p.component_id &&
                    out.zone_id == p.zone_id && out.service_port == p.service_port &&
                    out.seq == p.seq,
                "解码往返字段一致");

    // 小端序锚点：seq=42 落 buf[25]=42
    TEST_ASSERT(buf[25] == 42 && buf[26] == 0, "seq 小端落在 offset 25");
    return true;
}

bool test_wire_rejects() {
    std::uint8_t buf[kWireSize];
    WirePacket p;
    p.component_id = 1;
    p.encode_to(buf);

    WirePacket out;
    TEST_ASSERT(WirePacket::decode_from(buf, out), "基线合法");

    buf[0] = 'X';  // 错 magic
    TEST_ASSERT(!WirePacket::decode_from(buf, out), "错 magic 丢");
    buf[0] = 'A';

    buf[4] = 9;  // 错 version（低字节）
    TEST_ASSERT(!WirePacket::decode_from(buf, out), "错 version 丢");
    buf[4] = 1;

    buf[6] = 99;  // 未知 op
    TEST_ASSERT(!WirePacket::decode_from(buf, out), "未知 op 丢");
    buf[6] = static_cast<std::uint8_t>(Op::Register);

    MemberId zero;
    TEST_ASSERT(!zero.is_valid(), "零 component_id 身份非法");
    return true;
}

bool test_registry_lifecycle() {
    DiscoveryRegistry reg(1000);
    TEST_ASSERT(reg.ttl_ms() == 3000, "TTL = 3 × 心跳周期");

    // 注册 → 可见
    std::uint8_t buf[kWireSize];
    {
        WirePacket p;
        p.op = Op::Register;
        p.component_id = 100;
        p.zone_id = 1;
        p.service_port = 7000;
        p.encode_to(buf);
    }
    TEST_ASSERT(reg.on_packet(buf, 0), "注册包接受");
    TEST_ASSERT(reg.member_count() == 1, "成员可见");
    TEST_ASSERT(reg.members()[0].member.component_id == 100 &&
                    reg.members()[0].member.service_port == 7000,
                "成员身份字段还原");

    // 心跳刷新 last_seen（TTL 内不死）
    buf[6] = static_cast<std::uint8_t>(Op::Heartbeat);
    TEST_ASSERT(reg.on_packet(buf, 1500), "心跳包接受");
    TEST_ASSERT(reg.members()[0].last_seen_ms == 1500, "心跳刷新 last_seen");

    // 重复注册 = 幂等刷新（不新增条目，端口可变更）
    buf[6] = static_cast<std::uint8_t>(Op::Register);
    buf[20] = 0x59; buf[21] = 0x1B;  // 7001 = 0x1B59 小端（端口变更）
    TEST_ASSERT(reg.on_packet(buf, 2000), "重复注册接受");
    TEST_ASSERT(reg.member_count() == 1, "重复注册不新增条目");
    TEST_ASSERT(reg.members()[0].member.service_port == 7001, "刷新携带新端口");
    TEST_ASSERT(reg.members()[0].last_seen_ms == 2000, "重复注册刷新 last_seen");

    // 未注册组件心跳视作注册（首包丢失不挂起——Heartbeat 语义）
    {
        WirePacket p;
        p.op = Op::Heartbeat;
        p.component_id = 200;
        p.zone_id = 2;
        p.service_port = 8000;
        p.encode_to(buf);
    }
    TEST_ASSERT(reg.on_packet(buf, 2100), "未注册心跳接受");
    TEST_ASSERT(reg.member_count() == 2, "心跳兼注册生效");
    return true;
}

bool test_registry_ttl_death() {
    DiscoveryRegistry reg(1000);
    std::uint8_t buf[kWireSize];

    auto inject = [&](std::uint64_t cid, std::uint64_t at) {
        WirePacket p;
        p.op = Op::Register;
        p.component_id = cid;
        p.service_port = static_cast<std::uint16_t>(cid);
        p.encode_to(buf);
        return reg.on_packet(buf, at);
    };

    TEST_ASSERT(inject(1, 0) && inject(2, 500) && inject(3, 2500), "三个成员先后注册");

    reg.expire(3000);  // 成员 1：3000-0=3000 > 3000? 不——TTL 判定用严格大于
    TEST_ASSERT(reg.drain_deaths().empty(), "3000 恰在 TTL 边界内不死（3000 > 3000 假）");
    TEST_ASSERT(reg.member_count() == 3, "边界内全活");

    reg.expire(3001);  // 成员 1（last=0）超时
    auto deaths = reg.drain_deaths();
    TEST_ASSERT(deaths.size() == 1 && deaths[0].component_id == 1, "成员 1 超时死亡入队");
    TEST_ASSERT(reg.member_count() == 2, "死者移出注册表");

    reg.expire(99999);  // 其余全超时
    deaths = reg.drain_deaths();
    TEST_ASSERT(deaths.size() == 2, "剩余成员全部超时");
    TEST_ASSERT(reg.drain_deaths().empty(), "死亡队列取空后为空（drain 语义）");
    TEST_ASSERT(reg.member_count() == 0, "注册表清空");
    return true;
}

bool test_graceful_deregister_bypasses_death_queue() {
    DiscoveryRegistry reg(1000);
    std::uint8_t buf[kWireSize];

    WirePacket p;
    p.op = Op::Register;
    p.component_id = 7;
    p.service_port = 7777;
    p.encode_to(buf);
    TEST_ASSERT(reg.on_packet(buf, 0), "注册");

    p.op = Op::Deregister;
    p.encode_to(buf);
    TEST_ASSERT(reg.on_packet(buf, 100), "注销包接受");
    TEST_ASSERT(reg.member_count() == 0, "注销即时下线");
    TEST_ASSERT(reg.drain_deaths().empty(), "优雅注销不入死亡队列（与异常死亡分立）");

    TEST_ASSERT(reg.on_packet(buf, 200), "重复注销幂等接受");
    TEST_ASSERT(reg.drain_deaths().empty(), "重复注销仍无死亡事件");

    reg.expire(999999);
    TEST_ASSERT(reg.drain_deaths().empty(), "注销后永不被 TTL 判死");
    return true;
}

bool test_beacon_sends_and_guards() {
    CapturingTransport transport;
    DiscoveryBeacon beacon(make_member(9, 9000), "127.0.0.1", 9600, transport, 1000);

    TEST_ASSERT(beacon.interval_ms() == 1000, "心跳周期语义值");
    TEST_ASSERT(beacon.register_self(), "注册发送成功");
    TEST_ASSERT(transport.send_count == 1 && transport.host == "127.0.0.1" &&
                    transport.port == 9600,
                "首包发往目录端点");

    // 首包可从 wire 还原身份
    WirePacket out;
    std::uint8_t buf[kWireSize];
    std::memcpy(buf, transport.last_packet.data(), kWireSize);
    TEST_ASSERT(WirePacket::decode_from(buf, out) && out.op == Op::Register &&
                    out.component_id == 9 && out.service_port == 9000,
                "首包 wire 还原身份");

    TEST_ASSERT(beacon.beat(), "心跳发送");
    std::memcpy(buf, transport.last_packet.data(), kWireSize);
    TEST_ASSERT(WirePacket::decode_from(buf, out) && out.op == Op::Heartbeat &&
                    out.seq == 1,
                "心跳 op 且 seq 自增");

    TEST_ASSERT(beacon.deregister(), "注销发送");
    std::memcpy(buf, transport.last_packet.data(), kWireSize);
    TEST_ASSERT(WirePacket::decode_from(buf, out) && out.op == Op::Deregister,
                "注销 op 上 wire");

    // 非法身份拒发（0 组件号）
    DiscoveryBeacon bad(MemberId{}, "127.0.0.1", 9600, transport, 1000);
    TEST_ASSERT(!bad.register_self(), "零组件号拒发");
    TEST_ASSERT(transport.send_count == 3, "拒发无副作用");
    return true;
}

bool test_registry_drops_invalid_identity() {
    DiscoveryRegistry reg(1000);
    std::uint8_t buf[kWireSize];
    WirePacket p;
    p.component_id = 0;  // 零组件号
    p.service_port = 1234;
    p.encode_to(buf);
    TEST_ASSERT(!reg.on_packet(buf, 0), "零组件号注册包整包丢");
    TEST_ASSERT(reg.member_count() == 0, "注册表无副作用");
    return true;
}

} // namespace

int main() {
    int passed = 0;
    int failed = 0;

    struct Case {
        const char* name;
        bool (*fn)();
    } cases[] = {
        {"wire_roundtrip_and_layout", test_wire_roundtrip_and_layout},
        {"wire_rejects", test_wire_rejects},
        {"registry_lifecycle", test_registry_lifecycle},
        {"registry_ttl_death", test_registry_ttl_death},
        {"graceful_deregister_bypasses_death_queue", test_graceful_deregister_bypasses_death_queue},
        {"beacon_sends_and_guards", test_beacon_sends_and_guards},
        {"registry_drops_invalid_identity", test_registry_drops_invalid_identity},
    };

    for (const auto& c : cases) {
        std::cout << "[ RUN  ] " << c.name << std::endl;
        if (c.fn()) {
            std::cout << "[  OK  ] " << c.name << std::endl;
            ++passed;
        } else {
            std::cout << "[ FAIL ] " << c.name << std::endl;
            ++failed;
        }
    }

    std::cout << "DiscoveryTests: " << passed << " passed, " << failed << " failed" << std::endl;
    return failed == 0 ? 0 : 1;
}
