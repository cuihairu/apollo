#pragma once

// InterServerLink（net M1 内核 · 增量 4；net-abstraction §5.7 + §5.6 M1 行）。
//
// 进程间稳定连接的一等公民设施：拨号/接受产出的已连传输 attach 进来，
// 本类管握手、心跳、按 id 分发、可靠队列（seq 去重 + 累积 ack）、
// 四级水位背压、断连事件。零内部线程——全部由宿主 pump(now_ms) 驱动。
//
// 握手三元组（§5.7）：进程身份（组件名）+ route_version + authority_epoch，
// 双方 attach 后各发一帧 Hello（control 帧子类型 0）：
//   route_version 失配 → 断连（kRouteVersionMismatch）；
//   对端 epoch 与本 Link 实例记住的上次不同 → on_peer_restart（对端已重启，
//   ≠ 断线重连——上层在此清状态重协商，框架不代办）。
//
// 可靠语义（连接内 TCP 有序 + 跨重连 seq 续传的进程间最小版）：
//   发端 Reliable 帧入未确认队列（有界 = 水位容量），seq 在入队时分配、
//   每 Link 实例单调（跨重连不清零）；收端对 reliable 帧按 seq 去重
//   （≤ 已分发水位即重复，丢弃计数）并在泵末尾回累积 Ack；
//   发端收到 Ack 即出队——断线重连（同 epoch）后重发未确认帧 = 恰一次；
//   对端重启（epoch 变）→ 清可靠队列 + on_peer_restart 上层裁决。
//   Droppable（§5.7 OneWay 默认丢弃语义）：不 ack 不去重，断连即清队。
//
// 背压（§4.2/§5.7）：一条 Link 一组水位，判据 = reliable+droppable 队列
// 字节占用；服务器间不 trim——soft 起 Droppable 拒新、cut 起 Reliable 冻结、
// Control 走独立小预算；cut 持续 cut_close_ms → 断连（kBackpressureCut）。
//
// control 帧子类型（载荷首字节）：0=Hello 1=Ack 2=Ping 3=Pong。
// Hello 载荷：u32 魔数('LINK') + u64 route_version + u64 authority_epoch
//            + u32 名长 + 名字字节。
// Ack 载荷：u32 已确认最高 reliable seq（累积确认，§3 L2 思想）。
// Ping/Pong 载荷：空（收任意帧即刷新活性，pong 仅作礼节回应）。
//
// 业务帧载荷：u16 LE msg id + 载荷（信封纪律同 offload_wire；id 归契约，
// 分发经 Dispatcher——路由表/白名单/handler 由装配注入）。

#include "apollo/net/link/dispatcher.hpp"
#include "apollo/net/link/frame.hpp"
#include "apollo/net/link/transport.hpp"
#include "apollo/net/link/watermark.hpp"

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace apollo::net::link {

enum class LinkState {
    kIdle,         // 未挂传输
    kHandshaking,  // 传输已挂，Hello 未齐
    kUp,           // 握手完成，业务可收发
    kDown,         // 断连（可 re-attach 重连）
};

enum class LinkDownReason {
    kLocalClose,           // 本端主动 close
    kPeerClosed,           // 对端关闭（读 EOF / 写 -1）
    kTransportError,       // 传输层错误
    kProtocolError,        // 帧校验 fatal（尾校验/超限）
    kHeartbeatTimeout,     // 心跳超时（含握手超时）
    kBackpressureCut,      // cut 水位持续超时
    kRouteVersionMismatch, // 握手 route_version 失配
};

struct LinkEventSink {
    std::function<void()> on_up;
    std::function<void(LinkDownReason)> on_down;
    // 对端重启（epoch 失配）：old/new 为本 Link 实例先后记住的两个 epoch。
    std::function<void(std::uint64_t old_epoch, std::uint64_t new_epoch)> on_peer_restart;
};

struct LinkConfig {
    std::string self_name;            // 进程身份（G-1 组件名）
    std::uint64_t route_version = 0;  // 握手比对（schema hash 前缀）
    std::uint64_t authority_epoch = 1;
    WatermarkConfig watermark;
    std::uint64_t cut_close_ms = 5000;            // cut 持续 → 断
    std::uint32_t heartbeat_interval_ms = 5000;   // 空闲 ping 间隔
    std::uint32_t heartbeat_timeout_ms = 15000;   // 无入帧（含握手）→ 断
    std::uint64_t max_reliable_bytes = 0;         // 0 = 随 watermark 容量
};

class InterServerLink {
public:
    explicit InterServerLink(LinkConfig config);
    ~InterServerLink();

    InterServerLink(const InterServerLink&) = delete;
    InterServerLink& operator=(const InterServerLink&) = delete;

    // 装配面：路由表 / 域白名单 / handler 挂到内部分发器。
    Dispatcher& dispatcher() { return dispatcher_; }

    void set_sink(LinkEventSink sink) { sink_ = std::move(sink); }

    // 挂已连传输（连接器/监听器产出）。kDown/kIdle 均可重挂（重连语义）；
    // 重挂保持可靠队列（同 epoch 重发未确认帧），清 droppable（§5.7 OneWay
    // 默认丢弃），重置心跳/协议状态。对端 epoch 记忆跨重连保留（重启可检）。
    // 业务帧待握手完成（kUp）才上线——重启检出（读对端 Hello 清队）必然
    // 先于本连接任何业务帧过线。
    void attach(std::unique_ptr<ILinkTransport> transport, std::uint64_t now_ms);

    // 发送入口（§2 四动作的进程间实例）。business payload = u16 id + 载荷，
    // 由本类封帧。kControl 是链路保留类，外部传它按拒绝计。
    // kHandshaking/kUp 受理入队；kIdle/kDown 仅 Reliable 受理（断连排队，
    // §5.7 in-flight 表）——Droppable 断连即拒（默认丢弃语义的直接投影）。
    SendCode send(std::uint16_t msg_id, const std::uint8_t* payload, std::size_t size,
                  SendClass cls);
    SendCode send(std::uint16_t msg_id, const std::vector<std::uint8_t>& payload,
                  SendClass cls) {
        return send(msg_id, payload.data(), payload.size(), cls);
    }

    // 宿主泵：读传输 → 拆帧 → 分发；刷发送队列；心跳与 cut 计时。
    // now_ms = 单调毫秒（注入，可测）；返回 false = 链路已断。
    bool pump(std::uint64_t now_ms);

    // 主动断连（对端将见 EOF）。
    void close(LinkDownReason reason = LinkDownReason::kLocalClose);

    LinkState state() const { return state_; }
    std::uint64_t peer_epoch() const { return peer_epoch_; }
    // 对端自报名（Hello 携带的组件名；空 = 握手未毕）。1:N 接收端据它
    // 归档/路由（LinkManager passive fan-out 按此键）。
    const std::string& peer_name() const { return peer_name_; }
    const LinkConfig& config() const { return config_; }

    // 观测面（§5.6 M1 验收：背压可见、断连可检）。
    struct Stats {
        std::uint64_t reliable_queued = 0;   // 累计受理 Reliable 帧数
        std::uint64_t reliable_acked = 0;    // 累计确认出队帧数
        std::uint64_t droppable_dropped = 0; // 断连清队丢弃帧数
        std::uint64_t send_rejected = 0;     // 背压拒收次数
        std::uint64_t dup_dropped = 0;       // 收端重复帧丢弃数
        std::uint64_t dispatched = 0;        // 分发器 dispatched（镜像）
        std::uint64_t dispatch_rejected = 0; // 分发器三类拒收合计（镜像）
        std::uint64_t reliable_cleared = 0;  // 对端重启清队的未确认帧数
    };
    const Stats& stats() const { return stats_; }
    std::uint64_t queued_bytes() const { return reliable_bytes_ + droppable_bytes_; }
    std::size_t reliable_pending() const { return reliable_queue_.size(); }

private:
    struct QueuedFrame {
        std::uint32_t seq = 0;
        std::vector<std::uint8_t> payload;  // u16 LE msg id + 载荷（帧载荷原样）
        std::uint64_t wire_size = 0;        // 封帧后字节数（水位记账口径）
    };

    // control 帧子类型。
    static constexpr std::uint8_t kCtrlHello = 0;
    static constexpr std::uint8_t kCtrlAck = 1;
    static constexpr std::uint8_t kCtrlPing = 2;
    static constexpr std::uint8_t kCtrlPong = 3;

    void enqueue_control(std::uint8_t subtype, const std::uint8_t* body, std::size_t size);
    void queue_hello();
    void handle_frame(const Frame& frame);
    void handle_control(const Frame& frame);
    void handle_hello(const std::uint8_t* body, std::size_t size);
    void handle_business(const Frame& frame);
    void handle_ack(const std::uint8_t* body, std::size_t size);
    void flush(std::uint64_t now_ms);
    void drain_acked(std::uint32_t acked_seq);
    bool write_transport();
    void finish_down(LinkDownReason reason);

    LinkConfig config_;
    Dispatcher dispatcher_;
    LinkEventSink sink_;
    Stats stats_;

    LinkState state_ = LinkState::kIdle;
    std::unique_ptr<ILinkTransport> transport_;
    FrameParser parser_;

    // 发送面：control / reliable（未确认）/ droppable 三队 + 已序列化余量。
    std::deque<std::vector<std::uint8_t>> control_queue_;
    std::uint64_t control_bytes_ = 0;
    std::deque<QueuedFrame> reliable_queue_;
    std::uint64_t reliable_bytes_ = 0;
    std::deque<QueuedFrame> droppable_queue_;
    std::uint64_t droppable_bytes_ = 0;
    std::vector<std::uint8_t> tx_buffer_;
    std::size_t tx_offset_ = 0;
    Watermark watermark_;
    std::size_t sent_count_ = 0;  // 本连接已写出、未确认的 reliable 数（重连归零）

    // seq 分配：每 Link 实例单调（跨重连延续；重连不清零——收端按它去重）。
    std::uint32_t next_seq_ = 1;

    // 握手 / 对端身份。
    std::uint64_t peer_epoch_ = 0;
    bool peer_epoch_seen_ = false;  // 本实例是否收到过 Hello（重启判定基准）
    std::string peer_name_;         // 对端自报名（Hello；空 = 握手未毕）

    // 收端去重 / ack 面（keyed by 对端 epoch；epoch 变即重置）。
    std::uint64_t rx_epoch_ = 0;
    std::uint32_t last_reliable_seq_ = 0;  // 已分发最高 reliable seq
    std::uint32_t ack_watermark_ = 0;      // 待回累积 ack
    bool ack_dirty_ = false;

    // 心跳 / cut 计时。
    std::uint64_t last_rx_ms_ = 0;
    std::uint64_t last_ping_ms_ = 0;
    CutSustainer cut_sustainer_;
};

}  // namespace apollo::net::link
