#pragma once

// Gateway M1 出口缝（M1 上层接线批 · 2b-iii）：cell 面换装 InterServerLink
// active 端点——legacy 世界帧（五族意图）→ 契约 wire（924-928，request_id
// 关联键）→ 可靠发送；929 应答回流 → pending 关联映射回收（超时清扫）。
//
// 线程纪律（ADR-015(a) 同 cell 接收端）：机制零内部线程——收发/握手/应答
// 回收全在宿主 pump(now_ms) 一拍内推进（gateway 泵线程 = acceptNewConnections
// 循环）；forwardIntent 允许受理线程与心跳线程并发调用，入互斥投递箱即返回，
// 翻译与发送在泵拍内完成（链路单写者 = 泵线程）。
//
// 观察面契约（与 legacy nng 通道死面同型）：前递失败/箱满/不可翻译一律落地
// 不阻塞调用方，只进计数——客户端可见回执面不变（legacy 转发从不回客户端）。

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace gateway {

struct M1EgressConfig {
    std::string host = "127.0.0.1";       // WorldApp M1 接收端地址
    std::uint16_t port = 9101;
    std::string route_json;               // 契约路由表（与 cell 同一事实源）
    std::uint64_t ack_timeout_ms = 5000;  // 应答超时：pending 关联清扫线
    std::size_t inbox_cap = 4096;         // 投递箱顶格：溢出落地（legacy 死面同型）
};

class M1Egress {
public:
    virtual ~M1Egress() = default;

    // 装配（路由表加载 + active 端点登记）。false = 路由表缺位/坏 JSON/登记
    // 失败——出口降级（起进程不拦，P3-4 处方①同口径）；连接不对端即时建立，
    // active 端点自带退避重连。
    virtual bool start() { return true; }
    virtual void stop() {}

    // 宿主泵一拍：投递箱排空（翻译+发送）→ 链路推进（收发/握手）→ 超时清扫
    virtual void pump(std::uint64_t now_ms) { (void)now_ms; }

    // legacy 世界帧前递（见类注线程纪律）。false = 不可前递（非五族意图 /
    // 帧残缺 / 箱满 / 未装配），消息落地。
    virtual bool forwardIntent(std::uint64_t session_id,
                               const std::vector<std::uint8_t>& legacy_frame) {
        (void)session_id;
        (void)legacy_frame;
        return false;
    }

    // 观测面
    virtual bool worldConnected() const { return false; }
    virtual std::uint64_t sentIntents() const { return 0; }
    virtual std::uint64_t ackedIntents() const { return 0; }        // 929 code 0
    virtual std::uint64_t failedAcks() const { return 0; }          // 929 code != 0
    virtual std::uint64_t timedOutIntents() const { return 0; }     // 清扫线回收
    virtual std::uint64_t unforwardableFrames() const { return 0; } // 非五族/残帧/箱满
    virtual std::uint64_t droppedSends() const { return 0; }        // 链路拒收（背压）
    // pending 关联账面（仅限宿主泵线程读——泵拍外的原子计数只有其余各面）
    virtual std::size_t pendingAcks() const { return 0; }
};

// 缝工厂：POSIX 真 M1 底座；Windows 退空件（link TCP 底座不进 M1 批，
// forwardIntent 恒 false——调用面无 #if）。
std::unique_ptr<M1Egress> makeM1Egress(M1EgressConfig config);

} // namespace gateway
