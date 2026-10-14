#pragma once
//
// GameConnection — L2 会话层 + L3 逻辑门面（net-abstraction.md §2/§3/§4）。
//
// 游戏逻辑唯一入口：send/subscribe/state/close，隐藏链路建立/握手/帧定界/
// 心跳/seq-ack/水位/跨线程迁移。链路底座 = InterServerLink（L1 载体），
// 本类在其上加客户端会话面。
//
// L2 会话面：
//   - 四通道队列（§4.1）：movement（不可靠，同 id 只发最新）/ attributes
//     （可靠有序，LOW 可 trim）/ events（可靠有序，不丢）/ control（独立
//     小预算，永不冻结）。水位判据 = 各通道环字节占用（§4.2）。
//   - 三返回码（§2）：kAccepted / kTrimmedLow（入队但同通道低优先级帧被
//     丢弃腾位）/ kBackPressured（拒收，调用方自行降级）。
//   - 场景线程投递（§3 线程模型）：pump 只入队，handler 由 owning 线程
//     drain_inbound() 执行——游戏逻辑无锁的前提。
//   - Resuming（§2/§3）：同 epoch 重连 + resume 窗口内 → kResuming →
//     on_resume（missing_seq 范围）→ kActive；过期/对端重启 = 全新会话。
//
// 过渡面注记：四通道环每次 pump 刷入链路队列（链路自身水位仍作传输背压
// 兜底）；真 L2-on-L0（IO 线程直刷环）随客户端传输批收敛。

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "apollo/net/link/dispatcher.hpp"
#include "apollo/net/link/interserver_link.hpp"
#include "apollo/net/link/transport.hpp"
#include "apollo/net/link/watermark.hpp"

namespace apollo::net::gameconnection {

// §4.1 四通道（映射 link SendClass：movement→kDroppable，其余→kReliable）。
enum class Channel {
    kMovement,    // 不可靠，同 id 只发最新（带时间戳取最新）
    kAttributes,  // 可靠有序，LOW 可 trim
    kEvents,      // 可靠有序，不丢，满了压水位
    kControl,     // 可靠，独立小队列保证饿不死
};

// 通道内优先级（attributes 的 trim/拒收分级行为依据）。
enum class Priority {
    kLow,
    kNormal,
    kHigh,
};

// §2 会话状态。kResuming = 断线重连恢复窗口（re-attach 握手期 + resume
// 回调执行中）；kClosed 含链路 kIdle（未挂传输）。
enum class SessionState {
    kHandshaking,  // 握手未完成（无 resume 候选）
    kActive,       // 握手完成，业务可收发
    kResuming,     // 重连恢复窗口
    kClosed,       // 断连/未挂传输
};

// §2 三返回码：入队 / 入队但同通道低优先级帧被丢弃腾位（结果统计上报）/
// 拒收（调用方自行降级，如跳过本次同步帧）。
enum class SendCode {
    kAccepted,
    kTrimmedLow,
    kBackPressured,
};

// L2 会话配置：四通道环容量 + 投递队列深度 + resume 窗口。
struct SessionConfig {
    std::uint64_t movement_capacity_bytes = 64 * 1024;
    std::uint64_t attributes_capacity_bytes = 256 * 1024;
    std::uint64_t events_capacity_bytes = 256 * 1024;
    std::uint64_t control_capacity_bytes = 16 * 1024;
    std::uint32_t inbound_queue_depth = 256;  // 场景线程投递队列深度（溢出丢最旧）
    std::uint64_t resume_ttl_ms = 30000;      // resume 窗口（0 = 禁用恢复）
};

// §2 resume 回调负载：missing_seq 范围 = 断线时「已发出未确认」的 reliable
// seq 区间（链路同 epoch 重发已保证传输恰一次；上层据范围决定快照补发，
// attribute-sync §6）。
struct ResumeInfo {
    std::uint32_t first_missing_seq = 0;
    std::uint32_t last_missing_seq = 0;
    bool resumed = false;
};

struct SessionSink {
    std::function<void()> on_active;   // 握手完成（全新或恢复后均发）
    std::function<void(const ResumeInfo&)> on_resume;  // 仅恢复成功路径
    std::function<void(apollo::net::link::LinkDownReason)> on_closed;
};

struct SessionStats {
    std::uint64_t trimmed_low_dropped = 0;   // TRIMMED_LOW 腾位丢弃帧数
    std::uint64_t movement_overwritten = 0;  // movement 同 id 覆盖数
    std::uint64_t movement_dropped = 0;      // movement 环满丢最旧数
    std::uint64_t inbound_dropped = 0;       // 投递队列溢出丢帧数
    std::uint64_t resumes = 0;               // resume 成功次数
};

class GameConnection {
public:
    explicit GameConnection(apollo::net::link::LinkConfig config,
                            SessionConfig session_config = {});
    ~GameConnection();

    GameConnection(const GameConnection&) = delete;
    GameConnection& operator=(const GameConnection&) = delete;

    // 挂已连传输（连接器/监听器产出）。断线重连 = 再 attach。
    void attach(std::unique_ptr<apollo::net::link::ILinkTransport> transport,
                std::uint64_t now_ms);

    // 宿主泵：四通道环刷入链路（链路背压即停，留环下拍再刷）→ 读传输/
    // 拆帧/入投递队列/心跳与 cut 计时。返回 false = 链路已断。
    bool pump(std::uint64_t now_ms);

    // 场景线程投递：owning 线程调用，执行 pump 期积压的 handler（§3 线程
    // 模型：回调只发生在场景线程）。溢出丢最旧（计数 session_stats）。
    void drain_inbound();

    // 唯一发送入口。channel 定四通道语义；priority 参与 attributes 的
    // trim/拒收分级（§4.2 行为表）。
    SendCode send(std::uint16_t msg_id, const std::vector<std::uint8_t>& payload,
                  Channel ch, Priority p);
    SendCode send(std::uint16_t msg_id, const std::uint8_t* payload, std::size_t size,
                  Channel ch, Priority p);

    // 订阅（handler 由 owning 线程 drain_inbound() 执行，不在 pump 线程）。
    void subscribe(std::uint16_t msg_id, apollo::net::link::Dispatcher::Handler handler);

    // 路由表与域白名单（装配期注入；不装 = 默认开放）。
    void set_route_table(const apollo::net::link::RouteTable* table);
    void allow_domain(std::string domain);

    // 会话事件面（on_resume 仅同 epoch 重连 + 窗口内成功路径）。
    void set_session_sink(SessionSink sink);

    SessionState state() const;

    // 主动断连。
    void close(const std::string& reason);

    // 内部访问（测试/装配用）。
    apollo::net::link::InterServerLink& link() { return link_; }
    const apollo::net::link::InterServerLink::Stats& stats() const { return link_.stats(); }
    const SessionStats& session_stats() const { return session_stats_; }
    // 各通道环字节占用（§4.2 水位判据可见性）。
    std::uint64_t channel_bytes(Channel ch) const;

private:
    struct Pending {
        std::uint16_t msg_id;
        Priority priority = Priority::kNormal;
        std::vector<std::uint8_t> payload;  // 业务载荷（不含 id 信封）
    };

    static constexpr std::size_t kWField = 2;  // u16 msg id 信封
    static std::uint64_t wire_size(std::size_t body_size) {
        return apollo::net::link::kFrameHeaderSize + apollo::net::link::kFrameTailSize +
               kWField + body_size;
    }

    struct ChannelQueue {
        std::deque<Pending> frames;
        std::uint64_t bytes = 0;
        std::uint64_t capacity = 0;
        apollo::net::link::Watermark wm{apollo::net::link::WatermarkConfig{}};
    };

    SendCode admit_movement(std::uint16_t msg_id, Pending&& f);
    SendCode admit_attributes(ChannelQueue& q, Priority p, std::uint64_t sz, Pending&& f);
    // events / control FIFO：freeze_at_cut = events（§4.2 reliable 冻结行），
    // control 豁免。
    SendCode admit_fifo(ChannelQueue& q, Pending&& f, bool freeze_at_cut);
    void flush_channels();
    bool forward_front(ChannelQueue& q, apollo::net::link::SendClass cls);
    void on_link_up();
    void on_link_down(apollo::net::link::LinkDownReason reason);

    apollo::net::link::InterServerLink link_;
    SessionConfig cfg_;
    SessionSink sink_;
    SessionStats session_stats_;

    ChannelQueue queues_[4];  // kMovement/kAttributes/kEvents/kControl

    // 场景线程投递队列（pump 线程生产、owning 线程消费——本类跨线程面）。
    struct Inbound {
        std::uint16_t msg_id;
        std::vector<std::uint8_t> payload;
    };
    std::mutex inbound_mutex_;
    std::deque<Inbound> inbound_queue_;
    std::unordered_map<std::uint16_t, apollo::net::link::Dispatcher::Handler> handlers_;

    // resume 记账（断线时快照，重连握手完成后判定）。
    std::uint64_t last_pump_ms_ = 0;
    bool ever_up_ = false;
    bool resume_pending_ = false;
    bool resuming_now_ = false;  // on_resume 回调执行窗口内（state() 观测面）
    std::uint64_t epoch_at_down_ = 0;
    std::uint64_t down_ms_ = 0;
    std::uint32_t missing_lo_ = 0;
    std::uint32_t missing_hi_ = 0;
};

}  // namespace apollo::net::gameconnection
