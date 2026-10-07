#pragma once

// 进程编队服务发现骨架（G-1，net-abstraction §7 P3 前置设计；improvement-plan
// P3-1 的库层。守护进程代码标识 `Machined` 已是 term-contract v1.0 §1.3 定稿
// 名——2026-10-07 勘误：批 B/C 曾记「machined 不入户 §0-2」系误读契约（§1.3
// 明列守护进程行、§2 禁用表无 machined），进程壳无命名阻塞）。
//
// 两类形（BW bwmachined/machine_guard 先例，仅取骨架不取全家桶）：
//   - DiscoveryBeacon   进程侧：注册 + 周期心跳 + 优雅注销的**报文产生端**；
//   - DiscoveryRegistry 目录侧：注册表 + 心跳 TTL 死亡判定 + 死亡事件队列
//     （manager 域消费端接口——BW registerDeathListener 先例的骨架对应物）。
//
// 传输分界：本文件只定 wire 报文与两侧行为语义，不含 socket——Beacon 经
// BeaconTransport 接口发包（测试注入内存桩），Registry 以 on_packet 收字节。
// UDP 传输库件（UdpBeaconTransport/UdpFeed）见 udp_transport.hpp（批 C）；
// 守护进程壳 = Machined 主循环接线。
// 报文单包定长、自带全量身份——无连接态、丢包由下一心跳自然补（最终一致，
// §7：拓扑小、变更低频，守护+广播够用）。
//
// 并发纪律：Registry::on_packet/expire/members 与 Beacon 全部方法均要求调用
// 方单线程驱动（目录侧 = 守护主循环；进程侧 = 编队事件轮询源）——与 control
// 通道的进程间延伸同型（§7：编队事件作 control 事件进各进程轮询源）。骨架期
// 不起内部线程、不依赖钟——时间由调用方注入（now_ms）。

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace apollo::net::discovery {

// ---- 报文常量（wire 纪律：单包定长 32B）----

inline constexpr std::uint32_t kWireMagic = 0x41504431u;  // "APD1"
inline constexpr std::uint16_t kWireVersion = 1;
inline constexpr std::size_t kWireSize = 32;

enum class Op : std::uint8_t {
    Register = 1,   // 注册（重复注册 = 刷新，幂等）
    Heartbeat = 2,  // 心跳（未注册组件视作注册——首包丢失不挂起）
    Deregister = 3, // 优雅注销
    Query = 4,      // 广播查询「谁是目录」（成员面之外——发现层，§7 UDP 广播）
    Advertise = 5,  // 目录应答（unicast 回执端口；service_port = 目录收包端口）
    DeathSubscribe = 6, // manager → machined：注册死亡监听（BW
                        // registerDeathListener 先例；service_port = 监听
                        // feed 端口；周期重发 = 订阅刷新，幂等）
    DeathNotify = 7,    // machined → 订阅者：死亡事件上报（G-1 编队事件
                        // 跨进程——component 三元 = 死者身份）
};

// 保留组件号（machined 自身不入编队表）：1 = 目录（Advertise）、
// 2 = manager（DeathSubscribe）
inline constexpr std::uint64_t kDirectoryComponentId = 1;
inline constexpr std::uint64_t kManagerComponentId = 2;

// 死亡类别（DeathNotify 的 reserved0 字节；seq 字节复用为 exit_code——
// 监督面死亡带退出码，TTL 死亡无退出码恒 0）
enum class DeathKind : std::uint8_t {
    TtlExpired = 0, // 编队成员心跳超时（目录面 TTL 判死）
    ChildDied = 1,  // 监督面子进程退出（waitpid 收割）
    GaveUp = 2,     // 监督面重启超限放弃
};

// 编队成员身份（G-1 组件三元组 + 心跳序号）
struct MemberId {
    std::uint64_t component_id = 0;  // 编队内唯一组件号（0 = 非法）
    std::uint32_t zone_id = 0;       // 宿主域（拓扑分组；0 = 未分组）
    std::uint16_t service_port = 0;  // 该组件的服务端口（消费方拨号用）
    std::uint32_t seq = 0;           // 报文序号（观测用；不参与存活判定）

    [[nodiscard]] bool is_valid() const noexcept {
        return component_id != 0;
    }

    [[nodiscard]] bool operator==(const MemberId&) const noexcept = default;
};

// 定长 wire 报文（32B：magic 4 | version 2 | op 1 | reserved 1 | component_id 8
// | zone_id 4 | service_port 2 | reserved 3 | seq 4 | 尾随 3 字节保留零——
// 偏移和 = 29，尾 3B 恒零留扩展位）。逐字段编解码（宿主序 ↔ wire 小端序），
// 不 memcpy 结构体——防对齐/端序漂移。
//
// op 专属字段语义（G-1 收尾批：死亡事件跨进程上报）：
//   - DeathSubscribe：component_id = 订阅者组件号（kManagerComponentId），
//     service_port = 订阅者死亡监听 feed 端口（通知回投端点 = 发送方地址 +
//     此端口）；周期重发 = 刷新（幂等），超 TTL 未刷新即过期停推。
//   - DeathNotify：component 三元 = 死者身份（component_id 必非零）；
//     reserved0 = DeathKind；seq = exit_code（ChildDied 带退出码，其余 0）。
struct WirePacket {
    std::uint32_t magic = kWireMagic;
    std::uint16_t version = kWireVersion;
    Op op = Op::Register;
    std::uint8_t reserved0 = 0;
    std::uint64_t component_id = 0;
    std::uint32_t zone_id = 0;
    std::uint16_t service_port = 0;
    std::uint8_t reserved1[3] = {};
    std::uint32_t seq = 0;  // 末字段（offset 25..28；29..31 尾随零保留）

    void encode_to(std::uint8_t (&buf)[kWireSize]) const noexcept;
    [[nodiscard]] static bool decode_from(const std::uint8_t (&buf)[kWireSize],
                                          WirePacket& out) noexcept;
};

// 发包口（进程侧依赖倒置：UDP 实现随进程壳批；测试注入捕获桩）
class BeaconTransport {
public:
    virtual ~BeaconTransport() = default;
    // 发送一条编码后的 wire 报文到目录端点；false = 发送失败（Beacon 上抛）
    virtual bool send_to(const std::string& host, std::uint16_t port,
                         const std::uint8_t (&buf)[kWireSize]) = 0;
};

// 注册表条目（目录侧视图）
struct RegistryEntry {
    MemberId member;
    std::uint64_t last_seen_ms = 0;
};

// ---- DiscoveryBeacon：进程侧信标 ----

class DiscoveryBeacon {
public:
    // interval_ms 为心跳周期语义值（调用方按周期驱动 beat()）；transport
    // 必注入（骨架期无缺省 socket 实现），调用方保其生命周期
    DiscoveryBeacon(MemberId self, std::string directory_host,
                    std::uint16_t directory_port, BeaconTransport& transport,
                    std::uint32_t interval_ms = 1000);

    [[nodiscard]] bool register_self();  // 首包注册；false = 传输失败
    [[nodiscard]] bool beat();           // 单次心跳（seq 自增）；false = 传输失败
    [[nodiscard]] bool deregister();     // 优雅注销（TTL 兜底仍覆盖注销包丢失）

    [[nodiscard]] const MemberId& self() const noexcept {
        return self_;
    }
    [[nodiscard]] std::uint32_t interval_ms() const noexcept {
        return interval_ms_;
    }

private:
    [[nodiscard]] bool send(Op op);

    MemberId self_;
    std::string directory_host_;
    std::uint16_t directory_port_ = 0;
    BeaconTransport* transport_ = nullptr;
    std::uint32_t interval_ms_ = 1000;
    std::uint32_t seq_ = 0;
};

// ---- DiscoveryRegistry：目录侧注册表 ----

class DiscoveryRegistry {
public:
    // 死亡判定 TTL = kDeathTtlFactor × 心跳周期（丢 2 包不死，丢 3 包判死）
    static constexpr std::uint32_t kDeathTtlFactor = 3;

    explicit DiscoveryRegistry(std::uint32_t heartbeat_interval_ms = 1000);

    // 收包口：处理一条 wire 报文。now_ms 由调用方注入（目录钟）。
    // 返回 false = 报文丢弃（magic/version 不符、身份非法、op 未知，或 op 属
    // 发现层 Query/Advertise——成员面与发现面分立，machined 路由层分流）
    [[nodiscard]] bool on_packet(const std::uint8_t (&buf)[kWireSize],
                                 std::uint64_t now_ms);

    // TTL 扫描：超时成员移出并死亡入队（调用方按周期喂钟）
    void expire(std::uint64_t now_ms);

    [[nodiscard]] std::vector<RegistryEntry> members() const;  // 存活成员表
    [[nodiscard]] std::size_t member_count() const noexcept;
    [[nodiscard]] std::uint32_t ttl_ms() const noexcept;
    [[nodiscard]] std::vector<MemberId> drain_deaths();  // 死亡事件消费（bw
                                                         // deathListener 面）

private:
    std::vector<RegistryEntry> entries_;
    std::vector<MemberId> deaths_;
    std::uint32_t interval_ms_ = 1000;
};

// ---- 死亡事件跨进程上报（G-1 收尾批增量①：machined death 事件 → manager
// 域消费端；§7「mgr 向 machined 注册死亡监听」的骨架对应物）----
//
// 死亡事件两源合流（machined 侧）：目录面 TTL 判死（Registry::drain_deaths）
// + 监督面子进程死亡（Supervisor Died/GivenUp，roster 声明组件号者上 wire）。
// 消费端（manager 侧）：DeathSubscribe 注册 + 周期刷新 → DeathFeed 收包解
// DeathEvent。传输注入同 Beacon 纪律——machined 侧复用 Advertise 回执面、
// 测试注入捕获桩；通知为 OneWay UDP 单播，丢失由订阅刷新 + 下一事件/对账
// 兜底（§7：事件丢 = 传输层续传的事，骨架期最终一致）。

// 死亡事件（消费端视图——DeathNotify 报文的解码形态）
struct DeathEvent {
    MemberId member;          // 死者身份（component_id 必非零）
    DeathKind kind = DeathKind::TtlExpired;
    std::uint32_t exit_code = 0;  // ChildDied 退出码；其余类别恒 0

    [[nodiscard]] bool is_valid() const noexcept {
        return member.is_valid();
    }
};

// DeathNotify 报文 → DeathEvent（op 非死亡通知 / 死者零组件号 / kind 越界
// 返回 false——调用方整包丢）
[[nodiscard]] bool death_event_from(const WirePacket& packet, DeathEvent& out);

// machined 侧：死亡监听订阅表 + 通知转发（单线程驱动，调用方喂钟）。
// 刷新周期约束：订阅者 TTL = kDeathTtlFactor × machined 心跳周期，而订阅
// 方（manager）刷新节奏自定——须保证刷新周期 < TTL（缺省 1s 刷新 ×
// TTL=3×interval → machined interval ≥ 500ms 为安全域；超期只是退化为
// 重新订阅，语义无损）。
class DeathNotifier {
public:
    // transport 注入（复用 BeaconTransport 32B 发包契约——machined 用
    // Advertise 回执面兼发）；订阅者 TTL = kDeathTtlFactor × 周期（与成员
    // 面同死纪律：订阅者死即停推；刷新周期约束见类注释）
    DeathNotifier(BeaconTransport& transport,
                  std::uint32_t heartbeat_interval_ms = 1000);

    // 订阅注册/刷新（幂等）：component_id 零 / service_port 零 拒绝。
    // host = 发送方端点（收包面捕获），通知回投 = host:service_port。
    // 返回是否为新订阅者（刷新返回 false——调用方日志去噪）。
    [[nodiscard]] bool on_subscribe(std::uint64_t component_id,
                                    std::uint16_t service_port,
                                    const std::string& host,
                                    std::uint64_t now_ms);

    // TTL 扫描：超期订阅者移出（调用方按周期喂钟）
    void expire(std::uint64_t now_ms);

    // 死亡通知广播到全部在册订阅者。返回 false = 有订阅者但全部发送失败；
    // 无订阅者返回 true（无事可做非失败）
    [[nodiscard]] bool notify(const MemberId& dead, DeathKind kind,
                              std::uint32_t exit_code);

    [[nodiscard]] std::size_t subscriber_count() const noexcept;
    [[nodiscard]] std::uint32_t ttl_ms() const noexcept;

private:
    struct Subscriber {
        std::uint64_t component_id = 0;
        std::string host;
        std::uint16_t port = 0;
        std::uint64_t last_seen_ms = 0;
    };

    BeaconTransport* transport_ = nullptr;
    std::uint32_t interval_ms_ = 1000;
    std::vector<Subscriber> subscribers_;
};

// manager 域消费端：死亡监听注册端（DiscoveryBeacon 同型——身份 + 目录端点
// + 注入传输；subscribe() 发 DeathSubscribe，self.service_port 承载监听
// feed 端口）。收包面归调用方（UdpFeed 轮询 + death_event_from 解码）。
class DeathListener {
public:
    DeathListener(MemberId self, std::string directory_host,
                  std::uint16_t directory_port, BeaconTransport& transport);

    // 发送订阅（幂等刷新语义在 machined 侧）；false = 身份非法 / 传输失败
    [[nodiscard]] bool subscribe();

    [[nodiscard]] const MemberId& self() const noexcept {
        return self_;
    }

private:
    MemberId self_;
    std::string directory_host_;
    std::uint16_t directory_port_ = 0;
    BeaconTransport* transport_ = nullptr;
};

} // namespace apollo::net::discovery
