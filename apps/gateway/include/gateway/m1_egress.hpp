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
#include <functional>
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
    std::uint32_t gateway_id = 0;         // 本网关身份（0 = 未配；939 回带非 0
                                         // 且不匹配时落地不投——多网关防串投）
    // 被拒意图透明重试（L122 末批 · 用户拍板「网关内透明重试一次」）：World
    // 回 2001「simulation not hosted」（路由型拒绝——暂无可服务 cell）时，网关
    // 在链路内透明重发同一条意图一次（不重写协议、无客户端可见同步面——请求
    // 方仍按超时收敛）。重试仍拒/发送落地 = 终局失败（failed/dropped 计数），
    // 不再续试。重试载荷带原路由提示（入箱即定格纪律：路由事实以受理时刻为准，
    // 不因泵拍晚到改写——World 侧空间归属为权威裁决，提示只作池外回退）。
    bool intent_retry_enabled = true;
    std::uint64_t intent_retry_delay_ms = 500;  // 重试延迟（给 World/cell 就绪窗口）
};

// 客户端帧下行回调（P2 真实下行；契约 939 client_deliver 到达时调用）：
// session_id = 目标客户端会话；gateway_id = World 回带网关（0 = 不校验，
// 非零且不等于本网关身份时落地不投）；payload = 客户端 wire 全帧
//（MessageHeader + body），原样投递不解释。在宿主泵线程上回调。
using ClientDeliverFn = std::function<void(std::uint64_t session_id,
                                           std::uint32_t gateway_id,
                                           const std::vector<std::uint8_t>& payload)>;

// 路由失效回调（缓存失效批 · L122 切片）：World 回 2001「simulation not
// hosted」时调用（session_id = 被拒意图所属会话）。宿主把会话路由缓存作废
//（SessionManager::clearRoute），下一意图经 ensureRouteSnapshot 重解析——
// 迟到的旧 cell/空间提示不因缓存残留反复命中。仅路由型拒绝（2001）触发，
// 背压（2101）/成功（0）不触发。在宿主泵线程上回调。
using RouteInvalidateFn = std::function<void(std::uint64_t session_id)>;

class M1Egress {
public:
    virtual ~M1Egress() = default;

    // 装配（路由表加载 + active 端点登记）。false = 路由表缺位/坏 JSON/登记
    // 失败——出口降级（起进程不拦，P3-4 处方①同口径）；连接不对端即时建立，
    // active 端点自带退避重连。
    virtual bool start() { return true; }
    virtual void stop() {}

    // 下行装配（start 前设置）：939 到达时的客户端投递出口。
    virtual void setClientDeliver(ClientDeliverFn fn) { (void)fn; }

    // 路由失效装配（start 前设置）：World 2001「simulation not hosted」应答
    // 到达时回调（见 RouteInvalidateFn 注）。缺省空 = 仅计数不失效（legacy 面）。
    virtual void setRouteInvalidate(RouteInvalidateFn fn) { (void)fn; }

    // 宿主泵一拍：链路推进 → 已连接时投递箱排空（翻译+发送）→ 超时清扫。
    // 未发送帧留在有界箱；已发但未确认帧不跨 World 进程代次自动重放。
    virtual void pump(std::uint64_t now_ms) { (void)now_ms; }

    // legacy 世界帧前递（见类注线程纪律）。false = 不可前递（非五族意图 /
    // 帧残缺 / 箱满 / 未装配），消息落地。
    // 路由提示（多 cell 批，0 = 未带）：space_id/cell_id 从 RouteSnapshot 映
    // 入 932 信封——World 管理端据空间归属把模拟输入路由到当前 cell，不逐条
    // 经目录代理。缺省 0 = 旧调用面（单 cell 批）不携带，World 按首就绪回退。
    virtual bool forwardIntent(std::uint64_t session_id,
                               const std::vector<std::uint8_t>& legacy_frame,
                               std::uint64_t space_id = 0,
                               std::uint64_t cell_id = 0) {
        (void)session_id;
        (void)legacy_frame;
        (void)space_id;
        (void)cell_id;
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
    virtual std::uint64_t retriedIntents() const { return 0; }      // 2001 透明重试已发出
    virtual std::uint64_t deliveredClients() const { return 0; }    // 939 下行成功投递
    virtual std::uint64_t undeliverableClients() const { return 0; }// 939 残帧/身份不匹配
    // pending 关联账面（仅限宿主泵线程读——泵拍外的原子计数只有其余各面）
    virtual std::size_t pendingAcks() const { return 0; }
};

// 缝工厂：POSIX 真 M1 底座；Windows 退空件（link TCP 底座不进 M1 批，
// forwardIntent 恒 false——调用面无 #if）。
std::unique_ptr<M1Egress> makeM1Egress(M1EgressConfig config);

} // namespace gateway
