#pragma once

// 进程编队服务发现骨架（G-1，net-abstraction §7 P3 前置设计；improvement-plan
// P3-1 的库层——守护进程壳与进程命名属术语批，本层术语中立不涉新词）。
//
// 两类形（BW bwmachined/machine_guard 先例，仅取骨架不取全家桶）：
//   - DiscoveryBeacon   进程侧：注册 + 周期心跳 + 优雅注销的**报文产生端**；
//   - DiscoveryRegistry 目录侧：注册表 + 心跳 TTL 死亡判定 + 死亡事件队列
//     （manager 域消费端接口——BW registerDeathListener 先例的骨架对应物）。
//
// 传输分界（骨架口径）：本层只定 wire 报文与两侧行为语义，不含 socket——
// Beacon 经 BeaconTransport 接口发包（UDP 实现随进程壳批，测试注入内存桩），
// Registry 以 on_packet 收字节（UDP 收包循环归守护进程壳）。报文单包定长、
// 自带全量身份——无连接态、丢包由下一心跳自然补（最终一致，§7：拓扑小、
// 变更低频，守护+广播够用）。
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
    // 返回 false = 报文丢弃（magic/version 不符、身份非法或 op 未知）
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

} // namespace apollo::net::discovery
