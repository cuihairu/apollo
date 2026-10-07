// 进程编队服务发现单测（P3-1 批 B 库层 + 批 C UDP 传输件 + G-1 收尾批增量①）。
//
// 覆盖（骨架口径——批 B 内存桩传输零 flake；批 C loopback 集成）：
//   1. WirePacket：编码→解码往返一致 + 偏移表（前 4 字节即 magic）；
//   2. 报文校验：错 magic / 错 version / 未知 op / 零 component_id 整包丢；
//   3. Registry：注册→members 可见；心跳/重复注册幂等刷新 last_seen；
//   4. TTL 死亡：expire 超时移出 + drain_deaths 收到；
//   5. 优雅注销：即时下线且不入死亡队列（与异常死亡语义分立）；
//   6. Beacon：发包走 transport、is_valid 拒发、beat 自增 seq；
//   7. UDP（批 C）：Feed 临时端口/幂等 open、loopback Beacon→Registry 全链、
//      守卫面（未 open 拒发/非法端点拒发/空收非阻塞/超长报文静默丢）；
//   8. 发现层（批 E）：Query/Advertise wire 往返 + 成员面隔离、定向广播
//      Query 可达、locator 两段原语 loopback 握手（两组真广播用例带环境
//      探测：定向广播 loopback 不可达——BSD 栈不保证 127.255.255.255
//      回环，CI macOS runner 实测——打 SKIP 按过计，可达环境照跑真断言）；
//   9. 死亡面（G-1 收尾批增量①）：DeathSubscribe/DeathNotify wire 往返 +
//      成员面隔离、DeathNotifier 订阅表（注册/刷新/过期/通知广播）、
//      DeathListener 订阅发包、death_event_from 解码守卫、订阅→通知
//      loopback 全链。

#include "apollo/net/discovery/directory_locator.hpp"
#include "apollo/net/discovery/discovery.hpp"
#include "apollo/net/discovery/udp_transport.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <thread>
    #include <unistd.h>
#endif

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

// ---- 批 C：UDP 传输件（loopback 集成）----

// 测试内直发任意长度报文（越出 BeaconTransport 32B 契约面，守卫面用例需要）
bool raw_sendto(std::uint16_t port, const char* data, std::size_t len) {
#ifdef _WIN32
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        return false;
    }
#else
    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) {
        return false;
    }
#endif
    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(port);
    dst.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    const bool ok = sendto(s, data, static_cast<int>(len), 0,
                           reinterpret_cast<const sockaddr*>(&dst), sizeof(dst)) ==
                    static_cast<int>(len);
#ifdef _WIN32
    closesocket(s);
#else
    ::close(s);
#endif
    return ok;
}

// 轮询取包（loopback 投递非即时——上限 2s 防调度抖动）
bool poll_receive(UdpFeed& feed, std::uint8_t (&buf)[kWireSize]) {
    for (int i = 0; i < 2000; ++i) {
        if (feed.try_receive(buf)) {
            return true;
        }
#ifdef _WIN32
        Sleep(1);
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
#endif
    }
    return false;
}

// 同上，捕获发送方端点（发现层用例）
bool poll_receive_with(UdpFeed& feed, std::uint8_t (&buf)[kWireSize],
                       std::string& sender_host, std::uint16_t& sender_port) {
    for (int i = 0; i < 2000; ++i) {
        if (feed.try_receive(buf, sender_host, sender_port)) {
            return true;
        }
#ifdef _WIN32
        Sleep(1);
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
#endif
    }
    return false;
}

// 定向广播 loopback 可达性探测（真广播断言的环境前提）：发一次 Query 广播
// 看能否自收。BSD 栈对 127.255.255.255 定向广播不保证 loopback 回环
//（CI macOS runner 实测不可达，Linux 部署目标可达）——不可达环境跳过
// 真广播断言（打 SKIP 按过计），可达环境照跑。
bool broadcast_loopback_reachable() {
    UdpFeed feed;
    if (!feed.open(0, "0.0.0.0")) {
        return false;
    }
    UdpBeaconTransport transport;
    if (!transport.open() || !transport.enable_broadcast()) {
        return false;
    }
    if (!DirectoryLocator::send_query(transport, "127.255.255.255",
                                      feed.port(), 0, make_member(65))) {
        return false;
    }
    std::uint8_t buf[kWireSize];
    std::string sender_host;
    std::uint16_t sender_port = 0;
    return poll_receive_with(feed, buf, sender_host, sender_port);
}

bool test_udp_feed_lifecycle() {
    UdpFeed feed;
    TEST_ASSERT(feed.port() == 0 && !feed.is_open(), "未 open 端口 0");
    TEST_ASSERT(feed.open(0, "127.0.0.1"), "临时端口 bind");
    TEST_ASSERT(feed.is_open() && feed.port() != 0, "内核分配端口回读");
    const auto bound = feed.port();
    TEST_ASSERT(feed.open(0, "127.0.0.1"), "重复 open 幂等");
    TEST_ASSERT(feed.port() == bound, "幂等不改端口");
    feed.close();
    TEST_ASSERT(!feed.is_open() && feed.port() == 0, "close 复位");

    UdpFeed bad;
    TEST_ASSERT(!bad.open(0, "no.a.hostname.here"), "主机名（非点分 IPv4）拒绝");
    TEST_ASSERT(!bad.is_open(), "失败不残留句柄");
    return true;
}

bool test_udp_loopback_beacon_to_registry() {
    UdpFeed feed;
    TEST_ASSERT(feed.open(0, "127.0.0.1"), "目录端 bind");

    UdpBeaconTransport transport;
    TEST_ASSERT(transport.open(), "信标端 open");
    DiscoveryBeacon beacon(make_member(42, 4242), "127.0.0.1", feed.port(), transport, 1000);

    TEST_ASSERT(beacon.register_self(), "注册包上 wire");
    std::uint8_t buf[kWireSize];
    DiscoveryRegistry reg(1000);
    TEST_ASSERT(poll_receive(feed, buf), "loopback 收到注册包");
    TEST_ASSERT(reg.on_packet(buf, 100), "喂 Registry");
    TEST_ASSERT(reg.member_count() == 1 &&
                    reg.members()[0].member.component_id == 42 &&
                    reg.members()[0].member.service_port == 4242,
                "成员身份经 UDP 往返还原");

    TEST_ASSERT(beacon.beat(), "心跳上 wire");
    TEST_ASSERT(poll_receive(feed, buf), "收到心跳包");
    buf[6] = static_cast<std::uint8_t>(Op::Heartbeat);
    TEST_ASSERT(reg.on_packet(buf, 1100), "心跳喂 Registry");
    TEST_ASSERT(reg.members()[0].last_seen_ms == 1100, "last_seen 刷新");

    TEST_ASSERT(beacon.deregister(), "注销上 wire");
    TEST_ASSERT(poll_receive(feed, buf), "收到注销包");
    buf[6] = static_cast<std::uint8_t>(Op::Deregister);
    TEST_ASSERT(reg.on_packet(buf, 1200), "注销喂 Registry");
    TEST_ASSERT(reg.member_count() == 0 && reg.drain_deaths().empty(),
                "优雅下线且无死亡事件");
    return true;
}

bool test_udp_send_guards_and_oversize() {
    UdpFeed feed;
    TEST_ASSERT(feed.open(0, "127.0.0.1"), "目录端 bind");

    std::uint8_t buf[kWireSize];
    TEST_ASSERT(!feed.try_receive(buf), "空收非阻塞即返");

    UdpBeaconTransport closed_transport;
    TEST_ASSERT(!closed_transport.send_to("127.0.0.1", feed.port(), buf),
                "未 open 拒发");
    TEST_ASSERT(closed_transport.open(), "open");
    TEST_ASSERT(!closed_transport.send_to("no.a.hostname.here", feed.port(), buf),
                "非法端点拒发");
    TEST_ASSERT(!closed_transport.send_to("127.0.0.1", 0, buf), "零端口拒发");

    // 广播开关：未 open 拒绝；open 后幂等置位
    UdpBeaconTransport never_opened;
    TEST_ASSERT(!never_opened.enable_broadcast(), "未 open 开广播拒绝");
    TEST_ASSERT(closed_transport.enable_broadcast(), "open 后开广播");
    TEST_ASSERT(closed_transport.enable_broadcast(), "重复开广播幂等");

    // 超长报文（33B）静默丢，且不污染后续合法报文
    char garbage[kWireSize + 1] = {};
    TEST_ASSERT(raw_sendto(feed.port(), garbage, sizeof(garbage)), "超长报文已投递");
    TEST_ASSERT(!feed.try_receive(buf), "超长报文丢弃");
    TEST_ASSERT(!feed.try_receive(buf), "队列无残留");

    WirePacket p;
    p.component_id = 7;
    p.encode_to(buf);
    TEST_ASSERT(closed_transport.send_to("127.0.0.1", feed.port(), buf),
                "合法报文照发");
    TEST_ASSERT(poll_receive(feed, buf), "合法报文不受污染");
    return true;
}

// ---- 批 E：发现层（Query/Advertise + 广播引导）----

bool test_discovery_plane_ops_wire_and_registry_isolation() {
    // wire 往返：Query/Advertise 合法通过 decode
    std::uint8_t buf[kWireSize];
    {
        WirePacket p;
        p.op = Op::Query;
        p.component_id = 55;
        p.service_port = 4321;  // 回执端口语义
        p.encode_to(buf);
    }
    WirePacket out;
    TEST_ASSERT(WirePacket::decode_from(buf, out) && out.op == Op::Query &&
                    out.service_port == 4321,
                "Query wire 往返");

    {
        WirePacket p;
        p.op = Op::Advertise;
        p.component_id = kDirectoryComponentId;
        p.service_port = 9600;
        p.encode_to(buf);
    }
    TEST_ASSERT(WirePacket::decode_from(buf, out) && out.op == Op::Advertise &&
                    out.component_id == kDirectoryComponentId,
                "Advertise wire 往返");

    // 成员面隔离：发现层 op 不入 Registry（machined 路由层分流）
    DiscoveryRegistry reg(1000);
    TEST_ASSERT(!reg.on_packet(buf, 0), "Advertise 拒入成员面");
    TEST_ASSERT(reg.member_count() == 0, "无副作用");
    buf[6] = static_cast<std::uint8_t>(Op::Query);
    TEST_ASSERT(!reg.on_packet(buf, 0), "Query 拒入成员面");
    TEST_ASSERT(reg.drain_deaths().empty(), "死亡队列无扰动");
    return true;
}

bool test_udp_broadcast_query_reaches_directory() {
    if (!broadcast_loopback_reachable()) {
        std::cout << "SKIP: 定向广播 loopback 不可达（环境限制），跳过真广播断言"
                  << std::endl;
        return true;
    }
    UdpFeed feed;
    TEST_ASSERT(feed.open(0, "0.0.0.0"), "目录端 bind ANY（收广播前提）");

    UdpBeaconTransport transport;
    TEST_ASSERT(transport.open() && transport.enable_broadcast(), "广播发送端就绪");
    TEST_ASSERT(DirectoryLocator::send_query(transport, "127.255.255.255",
                                             feed.port(), 5432, make_member(66)),
                "Query 广播发出");
    std::uint8_t buf[kWireSize];
    std::string sender_host;
    std::uint16_t sender_port = 0;
    TEST_ASSERT(poll_receive_with(feed, buf, sender_host, sender_port),
                "广播被目录端收到");
    WirePacket out;
    TEST_ASSERT(WirePacket::decode_from(buf, out) && out.op == Op::Query &&
                    out.service_port == 5432,
                "Query 内容经广播还原（回执端口 5432）");
    TEST_ASSERT(!sender_host.empty() && sender_port != 0, "发送方端点已捕获");
    return true;
}

bool test_locator_handshake_loopback() {
    if (!broadcast_loopback_reachable()) {
        std::cout << "SKIP: 定向广播 loopback 不可达（环境限制），跳过真广播断言"
                  << std::endl;
        return true;
    }
    UdpFeed directory_feed;
    TEST_ASSERT(directory_feed.open(0, "0.0.0.0"), "目录端 bind");

    // 请求方：广播 Query（回执端口 = 自己的临时 feed）
    UdpFeed reply_feed;
    TEST_ASSERT(reply_feed.open(0, "0.0.0.0"), "请求方回执 feed");
    UdpBeaconTransport transport;
    TEST_ASSERT(transport.open() && transport.enable_broadcast(), "请求方广播就绪");
    TEST_ASSERT(DirectoryLocator::send_query(transport, "127.255.255.255",
                                             directory_feed.port(),
                                             reply_feed.port(), make_member(77)),
                "Query 广播");

    // 目录侧自演 machined：收 Query → Advertise 回执到回执端口
    std::uint8_t buf[kWireSize];
    std::string sender_host;
    std::uint16_t sender_port = 0;
    TEST_ASSERT(poll_receive_with(directory_feed, buf, sender_host, sender_port),
                "目录端收到 Query");
    WirePacket q;
    TEST_ASSERT(WirePacket::decode_from(buf, q) && q.op == Op::Query, "op=Query");
    UdpBeaconTransport reply_transport;
    TEST_ASSERT(reply_transport.open(), "目录回执面 open");
    {
        WirePacket adv;
        adv.op = Op::Advertise;
        adv.component_id = kDirectoryComponentId;
        adv.service_port = directory_feed.port();
        std::uint8_t abuf[kWireSize];
        adv.encode_to(abuf);
        TEST_ASSERT(reply_transport.send_to(sender_host, q.service_port, abuf),
                    "Advertise 回执发出");
    }

    // 请求方 collect_reply 收敛
    DirectoryEndpoint ep;
    TEST_ASSERT(DirectoryLocator::collect_reply(reply_feed, 2000, ep),
                "collect_reply 命中");
    TEST_ASSERT(ep.port == directory_feed.port(), "目录端口经 Advertise 还原");
    TEST_ASSERT(ep.is_valid(), "端点有效");
    return true;
}

// ---- G-1 收尾批增量①：死亡事件跨进程上报 ----

bool test_death_plane_wire() {
    // DeathSubscribe wire 往返（component = 订阅者，service_port = 监听端口）
    std::uint8_t buf[kWireSize];
    {
        WirePacket p;
        p.op = Op::DeathSubscribe;
        p.component_id = kManagerComponentId;
        p.service_port = 9601;
        p.encode_to(buf);
    }
    WirePacket out;
    TEST_ASSERT(WirePacket::decode_from(buf, out) &&
                    out.op == Op::DeathSubscribe &&
                    out.component_id == kManagerComponentId &&
                    out.service_port == 9601,
                "DeathSubscribe wire 往返");

    // DeathNotify wire 往返：reserved0 = kind，seq = exit_code
    {
        WirePacket p;
        p.op = Op::DeathNotify;
        p.component_id = 0xDEADull;
        p.zone_id = 3;
        p.service_port = 7000;
        p.reserved0 = static_cast<std::uint8_t>(DeathKind::ChildDied);
        p.seq = 1;  // exit_code
        p.encode_to(buf);
    }
    DeathEvent death;
    TEST_ASSERT(WirePacket::decode_from(buf, out) && out.op == Op::DeathNotify,
                "DeathNotify 解码通过");
    TEST_ASSERT(death_event_from(out, death), "DeathEvent 还原");
    TEST_ASSERT(death.member.component_id == 0xDEADull && death.member.zone_id == 3 &&
                    death.member.service_port == 7000 &&
                    death.kind == DeathKind::ChildDied && death.exit_code == 1,
                "DeathEvent 字段（身份/kind/exit_code）");

    // 未知 op 依旧整包丢（新 op 不放宽未知 op 校验）
    buf[6] = 8;
    TEST_ASSERT(!WirePacket::decode_from(buf, out), "未知 op 8 仍丢");
    return true;
}

bool test_registry_isolates_death_plane() {
    DiscoveryRegistry reg(1000);
    std::uint8_t buf[kWireSize];
    WirePacket p;
    p.op = Op::DeathSubscribe;
    p.component_id = kManagerComponentId;
    p.service_port = 9601;
    p.encode_to(buf);
    TEST_ASSERT(!reg.on_packet(buf, 0), "DeathSubscribe 拒入成员面");
    buf[6] = static_cast<std::uint8_t>(Op::DeathNotify);
    TEST_ASSERT(!reg.on_packet(buf, 0), "DeathNotify 拒入成员面");
    TEST_ASSERT(reg.member_count() == 0, "成员面无副作用");
    TEST_ASSERT(reg.drain_deaths().empty(), "死亡队列无扰动");
    return true;
}

bool test_death_notifier_subscribers() {
    CapturingTransport transport;
    DeathNotifier notifier(transport, 1000);
    TEST_ASSERT(notifier.ttl_ms() == 3000, "订阅者 TTL = 3 × 周期");
    TEST_ASSERT(notifier.subscriber_count() == 0, "初始无订阅者");

    // 守卫：零组件号 / 零端口拒绝
    TEST_ASSERT(!notifier.on_subscribe(0, 9601, "127.0.0.1", 0), "零组件号拒");
    TEST_ASSERT(!notifier.on_subscribe(2, 0, "127.0.0.1", 0), "零端口拒");
    TEST_ASSERT(notifier.subscriber_count() == 0, "拒绝无副作用");

    // 新订阅（true）+ 刷新（false，端点可迁移）
    TEST_ASSERT(notifier.on_subscribe(kManagerComponentId, 9601, "127.0.0.1", 100),
                "新订阅");
    TEST_ASSERT(notifier.subscriber_count() == 1, "订阅入表");
    TEST_ASSERT(!notifier.on_subscribe(kManagerComponentId, 9602, "127.0.0.1", 200),
                "重复订阅 = 刷新");
    TEST_ASSERT(notifier.subscriber_count() == 1, "刷新不新增条目");

    // 订阅者过期（TTL 兜底——订阅者死亡停推）
    notifier.expire(999999);
    TEST_ASSERT(notifier.subscriber_count() == 0, "超期订阅者移出");

    // 无订阅者 notify = true 且零发送
    MemberId dead = make_member(42, 7000);
    transport.send_count = 0;
    TEST_ASSERT(notifier.notify(dead, DeathKind::TtlExpired, 0) && transport.send_count == 0,
                "无订阅者 notify true 且零发送");

    // 双订阅者通知广播：每端点一包，报文可还原 DeathEvent
    TEST_ASSERT(notifier.on_subscribe(2, 9601, "127.0.0.1", 100), "订阅 A");
    TEST_ASSERT(notifier.on_subscribe(3, 9602, "10.0.0.5", 150), "订阅 B");
    transport.send_count = 0;
    TEST_ASSERT(notifier.notify(dead, DeathKind::ChildDied, 129),
                "通知广播发送成功");
    TEST_ASSERT(transport.send_count == 2, "每订阅者一包");

    DeathEvent death;
    WirePacket out;
    std::uint8_t buf[kWireSize];
    std::memcpy(buf, transport.last_packet.data(), kWireSize);
    TEST_ASSERT(WirePacket::decode_from(buf, out), "通知报文解码");
    TEST_ASSERT(death_event_from(out, death) &&
                    death.member.component_id == 42 &&
                    death.kind == DeathKind::ChildDied && death.exit_code == 129,
                "通知报文还原 DeathEvent（kind/exit_code）");

    // 死者身份非法拒绝
    TEST_ASSERT(!notifier.notify(MemberId{}, DeathKind::ChildDied, 0),
                "零组件号死者拒发");
    return true;
}

bool test_death_listener_subscribe_wire() {
    CapturingTransport transport;
    MemberId self;
    self.component_id = kManagerComponentId;
    self.service_port = 9601;
    DeathListener listener(self, "127.0.0.1", 9600, transport);

    TEST_ASSERT(listener.self().component_id == kManagerComponentId, "身份保留");
    TEST_ASSERT(listener.subscribe(), "订阅发送成功");
    TEST_ASSERT(transport.host == "127.0.0.1" && transport.port == 9600,
                "发往目录端点");
    WirePacket out;
    std::uint8_t buf[kWireSize];
    std::memcpy(buf, transport.last_packet.data(), kWireSize);
    TEST_ASSERT(WirePacket::decode_from(buf, out) &&
                    out.op == Op::DeathSubscribe &&
                    out.component_id == kManagerComponentId &&
                    out.service_port == 9601,
                "订阅报文 wire 还原（op/component/监听端口）");

    // 零组件号身份拒发
    DeathListener bad(MemberId{}, "127.0.0.1", 9600, transport);
    TEST_ASSERT(!bad.subscribe(), "零组件号拒发");
    TEST_ASSERT(transport.send_count == 1, "拒发无副作用");
    return true;
}

bool test_death_event_decode_guards() {
    std::uint8_t buf[kWireSize];
    DeathEvent death;

    // 非死亡通知 op 拒绝
    WirePacket p;
    p.op = Op::Heartbeat;
    p.component_id = 5;
    p.encode_to(buf);
    TEST_ASSERT(!death_event_from(p, death), "非 DeathNotify op 拒绝");

    // 死者零组件号拒绝
    p.op = Op::DeathNotify;
    p.component_id = 0;
    p.reserved0 = static_cast<std::uint8_t>(DeathKind::TtlExpired);
    p.encode_to(buf);
    TEST_ASSERT(!death_event_from(p, death), "零组件号死者拒绝");

    // kind 越界拒绝
    p.component_id = 5;
    p.reserved0 = 99;
    p.encode_to(buf);
    TEST_ASSERT(!death_event_from(p, death), "kind 越界拒绝");

    // GaveUp 合法通过（exit_code 0）
    p.reserved0 = static_cast<std::uint8_t>(DeathKind::GaveUp);
    p.seq = 0;
    p.encode_to(buf);
    TEST_ASSERT(death_event_from(p, death) && death.kind == DeathKind::GaveUp &&
                    death.exit_code == 0 && death.is_valid(),
                "GaveUp 合法通过");
    return true;
}

bool test_udp_death_loopback() {
    // machined 侧：订阅收包 feed + 通知回执面
    UdpFeed notifier_feed;
    TEST_ASSERT(notifier_feed.open(0, "127.0.0.1"), "machined 订阅面 bind");
    UdpBeaconTransport notify_transport;
    TEST_ASSERT(notify_transport.open(), "machined 通知面 open");
    DeathNotifier notifier(notify_transport, 1000);

    // manager 侧：死亡监听 feed（临时端口）+ 订阅发包
    UdpFeed death_feed;
    TEST_ASSERT(death_feed.open(0, "127.0.0.1"), "manager 监听 feed bind");
    UdpBeaconTransport sub_transport;
    TEST_ASSERT(sub_transport.open(), "manager 订阅面 open");
    MemberId self;
    self.component_id = kManagerComponentId;
    self.service_port = death_feed.port();
    DeathListener listener(self, "127.0.0.1", notifier_feed.port(), sub_transport);
    TEST_ASSERT(listener.subscribe(), "订阅上 wire");

    // machined 收订阅（捕获发送方端点）→ 注册
    std::uint8_t buf[kWireSize];
    std::string sender_host;
    std::uint16_t sender_port = 0;
    TEST_ASSERT(poll_receive_with(notifier_feed, buf, sender_host, sender_port),
                "machined 收到订阅");
    WirePacket p;
    TEST_ASSERT(WirePacket::decode_from(buf, p) && p.op == Op::DeathSubscribe,
                "订阅报文还原");
    TEST_ASSERT(notifier.on_subscribe(p.component_id, p.service_port, sender_host, 100),
                "订阅注册（新订阅者）");

    // 通知广播 → manager feed 收包 → 解码
    MemberId dead = make_member(42, 7000);
    TEST_ASSERT(notifier.notify(dead, DeathKind::ChildDied, 129), "通知发出");
    TEST_ASSERT(poll_receive_with(death_feed, buf, sender_host, sender_port),
                "manager 收到死亡通知");
    TEST_ASSERT(WirePacket::decode_from(buf, p) && p.op == Op::DeathNotify,
                "通知报文还原");
    DeathEvent death;
    TEST_ASSERT(death_event_from(p, death) &&
                    death.member.component_id == 42 &&
                    death.kind == DeathKind::ChildDied && death.exit_code == 129,
                "死亡事件跨 UDP 全链还原");
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
        {"udp_feed_lifecycle", test_udp_feed_lifecycle},
        {"udp_loopback_beacon_to_registry", test_udp_loopback_beacon_to_registry},
        {"udp_send_guards_and_oversize", test_udp_send_guards_and_oversize},
        {"discovery_plane_ops_wire_and_registry_isolation",
         test_discovery_plane_ops_wire_and_registry_isolation},
        {"udp_broadcast_query_reaches_directory", test_udp_broadcast_query_reaches_directory},
        {"locator_handshake_loopback", test_locator_handshake_loopback},
        {"death_plane_wire", test_death_plane_wire},
        {"registry_isolates_death_plane", test_registry_isolates_death_plane},
        {"death_notifier_subscribers", test_death_notifier_subscribers},
        {"death_listener_subscribe_wire", test_death_listener_subscribe_wire},
        {"death_event_decode_guards", test_death_event_decode_guards},
        {"udp_death_loopback", test_udp_death_loopback},
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
