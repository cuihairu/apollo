#include "apollo/net/link/interserver_link.hpp"

#include <algorithm>
#include <cstring>

namespace apollo::net::link {

namespace {

// Hello 协议魔数 'LINK'（握手层自有，与帧层 magic 无关）。
constexpr std::uint32_t kHelloProto = 0x4C494E4BU;

void store32(std::uint8_t* p, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        p[i] = static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF);
    }
}

void store64(std::uint8_t* p, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        p[i] = static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF);
    }
}

std::uint16_t get16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

std::uint32_t get32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | static_cast<std::uint32_t>(p[1]) << 8 |
           static_cast<std::uint32_t>(p[2]) << 16 | static_cast<std::uint32_t>(p[3]) << 24;
}

std::uint64_t get64(const std::uint8_t* p) {
    std::uint64_t v = 0;
    for (int i = 7; i >= 0; --i) {
        v = (v << 8) | p[i];
    }
    return v;
}

std::size_t business_wire_size(std::size_t body_size) {
    return kFrameHeaderSize + kFrameTailSize + 2 + body_size;  // 2 = u16 msg id
}

// 业务帧载荷信封：u16 LE id + 载荷（一次性定长构造——不用 push_back 逐个
// 长大，绕开 GCC 15 -Wfree-nonheap-object 误报内联路径）。
std::vector<std::uint8_t> make_business_payload(std::uint16_t msg_id, const std::uint8_t* body,
                                                std::size_t size) {
    std::vector<std::uint8_t> payload(2 + size);
    payload[0] = static_cast<std::uint8_t>(msg_id & 0xFF);
    payload[1] = static_cast<std::uint8_t>(msg_id >> 8);
    if (size > 0) {
        std::memcpy(payload.data() + 2, body, size);
    }
    return payload;
}

}  // namespace

InterServerLink::InterServerLink(LinkConfig config)
    : config_(std::move(config)), watermark_(config_.watermark) {}

InterServerLink::~InterServerLink() {
    if (transport_) {
        transport_->close();
        transport_.reset();
    }
}

void InterServerLink::attach(std::unique_ptr<ILinkTransport> transport, std::uint64_t now_ms) {
    if (state_ == LinkState::kHandshaking || state_ == LinkState::kUp) {
        finish_down(LinkDownReason::kLocalClose);
    }
    transport_ = std::move(transport);
    parser_.reset();
    tx_buffer_.clear();
    tx_offset_ = 0;
    control_queue_.clear();
    control_bytes_ = 0;
    sent_count_ = 0;  // 本连接全量重发未确认 reliable（收端按 epoch+seq 去重兜底）
    state_ = LinkState::kHandshaking;
    last_rx_ms_ = now_ms;
    last_ping_ms_ = now_ms;
    cut_sustainer_.reset();
    queue_hello();
}

SendCode InterServerLink::send(std::uint16_t msg_id, const std::uint8_t* payload,
                               std::size_t size, SendClass cls) {
    if (cls == SendClass::kControl) {
        ++stats_.send_rejected;  // control 是链路保留类，外部不可用
        return SendCode::kBackPressured;
    }
    const std::uint64_t wire = business_wire_size(size);
    if (cls == SendClass::kDroppable) {
        // §5.7 OneWay 默认丢弃语义：断连期间 Droppable 不排队（直接拒）。
        if (state_ != LinkState::kUp && state_ != LinkState::kHandshaking) {
            ++stats_.send_rejected;
            return SendCode::kBackPressured;
        }
        if (watermark_.admit(watermark_.level(queued_bytes()), SendClass::kDroppable, wire) !=
            SendCode::kAccepted) {
            ++stats_.send_rejected;
            return SendCode::kBackPressured;
        }
        QueuedFrame q;
        q.seq = next_seq_++;
        q.payload = make_business_payload(msg_id, payload, size);
        q.wire_size = wire;
        droppable_bytes_ += wire;
        droppable_queue_.push_back(std::move(q));
        return SendCode::kAccepted;
    }
    // Reliable：断连期间也受理（§5.7 in-flight 表——重连后重发）。
    if (watermark_.admit(watermark_.level(queued_bytes()), SendClass::kReliable, wire) !=
        SendCode::kAccepted) {
        ++stats_.send_rejected;
        return SendCode::kBackPressured;
    }
    QueuedFrame q;
    q.seq = next_seq_++;
    q.payload = make_business_payload(msg_id, payload, size);
    q.wire_size = wire;
    reliable_bytes_ += wire;
    ++stats_.reliable_queued;
    reliable_queue_.push_back(std::move(q));
    return SendCode::kAccepted;
}

void InterServerLink::close(LinkDownReason reason) {
    if (state_ == LinkState::kHandshaking || state_ == LinkState::kUp) {
        finish_down(reason);
    }
}

bool InterServerLink::pump(std::uint64_t now_ms) {
    if (state_ != LinkState::kUp && state_ != LinkState::kHandshaking) {
        return false;
    }

    // 读侧：排空到暂无 / EOF。任意入帧即刷新活性（含握手帧）。
    std::uint8_t buf[16 * 1024];
    for (;;) {
        const int n = transport_->read(buf, sizeof buf);
        if (n < 0) {
            finish_down(LinkDownReason::kPeerClosed);
            return false;
        }
        if (n == 0) {
            break;
        }
        const ParseOutcome out = parser_.feed(buf, static_cast<std::size_t>(n));
        for (const Frame& frame : out.frames) {
            last_rx_ms_ = now_ms;
            handle_frame(frame);
            if (state_ != LinkState::kUp && state_ != LinkState::kHandshaking) {
                return false;  // 帧处理中触发断连（协议错误/失配）
            }
        }
        if (out.fatal) {
            finish_down(LinkDownReason::kProtocolError);
            return false;
        }
    }

    // 心跳：超时判活（握手期同表——握手超时即此门）。
    if (now_ms - last_rx_ms_ >= config_.heartbeat_timeout_ms) {
        finish_down(LinkDownReason::kHeartbeatTimeout);
        return false;
    }
    if (now_ms - last_ping_ms_ >= config_.heartbeat_interval_ms) {
        last_ping_ms_ = now_ms;
        enqueue_control(kCtrlPing, nullptr, 0);
    }

    // 背压 cut 持续 → 断（§4.2）。
    cut_sustainer_.update(watermark_.level(queued_bytes()) == WatermarkLevel::kCut, now_ms);
    if (cut_sustainer_.sustained_for(now_ms, config_.cut_close_ms)) {
        finish_down(LinkDownReason::kBackpressureCut);
        return false;
    }

    flush(now_ms);
    return state_ == LinkState::kUp || state_ == LinkState::kHandshaking;
}

void InterServerLink::enqueue_control(std::uint8_t subtype, const std::uint8_t* body,
                                      std::size_t size) {
    std::vector<std::uint8_t> payload(1 + size);
    payload[0] = subtype;
    if (size > 0) {
        std::memcpy(payload.data() + 1, body, size);
    }
    const std::uint64_t wire = kFrameHeaderSize + kFrameTailSize + payload.size();
    if (watermark_.admit_control(queued_bytes(), control_bytes_, wire) != SendCode::kAccepted) {
        return;  // control 预算耗尽：丢（Hello/首 Ack 不会触及——预算 16KiB）
    }
    control_bytes_ += wire;
    control_queue_.push_back(std::move(payload));
}

void InterServerLink::queue_hello() {
    const std::string& name = config_.self_name;
    std::vector<std::uint8_t> body(4 + 8 + 8 + 4 + name.size());
    std::uint8_t* p = body.data();
    store32(p, kHelloProto);
    store64(p + 4, config_.route_version);
    store64(p + 12, config_.authority_epoch);
    store32(p + 20, static_cast<std::uint32_t>(name.size()));
    if (!name.empty()) {
        std::memcpy(p + 24, name.data(), name.size());
    }
    enqueue_control(kCtrlHello, body.data(), body.size());
}

void InterServerLink::handle_frame(const Frame& frame) {
    if (is_control(frame)) {
        handle_control(frame);
    } else {
        handle_business(frame);
    }
}

void InterServerLink::handle_control(const Frame& frame) {
    if (frame.payload.empty()) {
        return;  // 无子类型，无法解释——忽略（空帧无害）
    }
    const std::uint8_t* body = frame.payload.data() + 1;
    const std::size_t size = frame.payload.size() - 1;
    switch (frame.payload[0]) {
        case kCtrlHello:
            handle_hello(body, size);
            break;
        case kCtrlAck:
            handle_ack(body, size);
            break;
        case kCtrlPing:
            enqueue_control(kCtrlPong, nullptr, 0);
            break;
        case kCtrlPong:
            break;  // 活性已由入帧刷新
        default:
            finish_down(LinkDownReason::kProtocolError);  // 未知子类型：内核从严
            break;
    }
}

void InterServerLink::handle_hello(const std::uint8_t* body, std::size_t size) {
    if (size < 4 + 8 + 8 + 4) {
        finish_down(LinkDownReason::kProtocolError);
        return;
    }
    if (get32(body) != kHelloProto) {
        finish_down(LinkDownReason::kProtocolError);
        return;
    }
    const std::uint64_t route_version = get64(body + 4);
    const std::uint64_t epoch = get64(body + 12);
    const std::uint32_t name_len = get32(body + 20);
    if (4 + 8 + 8 + 4 + name_len != size) {
        finish_down(LinkDownReason::kProtocolError);
        return;
    }
    if (route_version != config_.route_version) {
        finish_down(LinkDownReason::kRouteVersionMismatch);
        return;
    }

    // 对端自报名（握手三元组第一元；重连重握手覆盖——对端身份以最新为准）。
    peer_name_.assign(reinterpret_cast<const char*>(body) + 24, name_len);

    if (peer_epoch_seen_ && epoch != peer_epoch_) {
        // 对端重启（§5.7：epoch 失配 ≠ 断线重连）：清可靠队列（老一代永不再
        // ack），重置收端去重水位（新一代 seq 从头计），事件交上层裁决。
        const std::uint64_t old = peer_epoch_;
        stats_.reliable_cleared += reliable_queue_.size();
        reliable_queue_.clear();
        reliable_bytes_ = 0;
        sent_count_ = 0;
        last_reliable_seq_ = 0;
        ack_watermark_ = 0;
        ack_dirty_ = true;
        peer_epoch_ = epoch;
        rx_epoch_ = epoch;
        if (sink_.on_peer_restart) {
            sink_.on_peer_restart(old, epoch);
        }
    } else if (!peer_epoch_seen_) {
        peer_epoch_ = epoch;
        peer_epoch_seen_ = true;
        rx_epoch_ = epoch;
    }

    if (state_ == LinkState::kHandshaking) {
        state_ = LinkState::kUp;
        if (sink_.on_up) {
            sink_.on_up();
        }
    }
}

void InterServerLink::handle_ack(const std::uint8_t* body, std::size_t size) {
    if (size < 4) {
        finish_down(LinkDownReason::kProtocolError);
        return;
    }
    drain_acked(get32(body));
}

void InterServerLink::handle_business(const Frame& frame) {
    if (state_ != LinkState::kUp) {
        // 握手未毕的业务帧 = 对端握手纪律破坏，内核从严断连。
        finish_down(LinkDownReason::kProtocolError);
        return;
    }
    if (frame.payload.size() < 2) {
        finish_down(LinkDownReason::kProtocolError);
        return;
    }
    const std::uint16_t msg_id = get16(frame.payload.data());
    const std::uint8_t* data = frame.payload.data() + 2;
    const std::size_t size = frame.payload.size() - 2;

    const bool reliable = (frame.flags & kFlagReliable) != 0;
    if (reliable && frame.seq <= last_reliable_seq_) {
        // 重复帧（重发撞上去重水位）：丢弃计数，但仍推进回执水位——
        // 重连收敛靠它（对端重发的旧帧换来更高 ack，出队随之）。
        ++stats_.dup_dropped;
        ack_watermark_ = std::max(ack_watermark_, frame.seq);
        ack_dirty_ = true;
        return;
    }

    if (dispatcher_.dispatch(msg_id, data, size) == DispatchResult::kDispatched) {
        ++stats_.dispatched;
    } else {
        ++stats_.dispatch_rejected;
    }

    if (reliable) {
        last_reliable_seq_ = frame.seq;
        ack_watermark_ = std::max(ack_watermark_, frame.seq);
        ack_dirty_ = true;
    }
}

void InterServerLink::drain_acked(std::uint32_t acked_seq) {
    while (!reliable_queue_.empty() && reliable_queue_.front().seq <= acked_seq) {
        reliable_bytes_ -= reliable_queue_.front().wire_size;
        reliable_queue_.pop_front();
        if (sent_count_ > 0) {
            --sent_count_;
        }
        ++stats_.reliable_acked;
    }
}

void InterServerLink::flush(std::uint64_t /*now_ms*/) {
    if (!transport_) {
        return;
    }
    if (ack_dirty_) {
        const std::uint8_t ack[4] = {
            static_cast<std::uint8_t>(ack_watermark_ & 0xFF),
            static_cast<std::uint8_t>((ack_watermark_ >> 8) & 0xFF),
            static_cast<std::uint8_t>((ack_watermark_ >> 16) & 0xFF),
            static_cast<std::uint8_t>((ack_watermark_ >> 24) & 0xFF),
        };
        enqueue_control(kCtrlAck, ack, sizeof ack);
        ack_dirty_ = false;
    }

    for (const auto& payload : control_queue_) {
        append_frame(tx_buffer_, 0, kFlagControl, payload);
    }
    control_queue_.clear();
    control_bytes_ = 0;

    if (state_ == LinkState::kUp) {
        // 业务帧待握手完成才上线：重启检出（读对端 Hello → 清队）必然先于
        // 本连接任何业务帧过线，老一代排队帧不会漏进新一代。
        // reliable：只发本连接未发过的；已发未 ack 的留队列（重连全量重发）。
        while (sent_count_ < reliable_queue_.size()) {
            const QueuedFrame& q = reliable_queue_[sent_count_];
            append_frame(tx_buffer_, q.seq, kFlagReliable, q.payload);
            ++sent_count_;
        }

        // droppable：序列化即出队（fire-and-forget；丢失不作数）。
        for (const QueuedFrame& q : droppable_queue_) {
            append_frame(tx_buffer_, q.seq, 0, q.payload);
        }
        droppable_queue_.clear();
        droppable_bytes_ = 0;
    }

    write_transport();
}

bool InterServerLink::write_transport() {
    while (tx_offset_ < tx_buffer_.size()) {
        const int n = transport_->write(tx_buffer_.data() + tx_offset_,
                                        tx_buffer_.size() - tx_offset_);
        if (n < 0) {
            finish_down(LinkDownReason::kTransportError);
            return false;
        }
        if (n == 0) {
            break;  // 缝暂时不可写：余量下个泵周期续写
        }
        tx_offset_ += static_cast<std::size_t>(n);
    }
    if (tx_offset_ >= tx_buffer_.size()) {
        tx_buffer_.clear();
        tx_offset_ = 0;
    }
    return true;
}

void InterServerLink::finish_down(LinkDownReason reason) {
    if (state_ != LinkState::kHandshaking && state_ != LinkState::kUp) {
        return;
    }
    stats_.droppable_dropped += droppable_queue_.size();
    droppable_queue_.clear();
    droppable_bytes_ = 0;
    control_queue_.clear();
    control_bytes_ = 0;
    tx_buffer_.clear();
    tx_offset_ = 0;
    if (transport_) {
        transport_->close();
        transport_.reset();
    }
    parser_.reset();
    cut_sustainer_.reset();
    ack_dirty_ = false;
    // 保留：reliable_queue_（同 epoch 重发，收端去重水位兜底恰一次）、
    // rx 去重水位、peer_epoch 记忆（重启判定基准）、ack_watermark_（新帧
    // 到达后回执从既有水位续计）。清零它们只该发生在对端重启（handle_hello
    // epoch 分支）或 Link 实例重建。
    state_ = LinkState::kDown;
    if (sink_.on_down) {
        sink_.on_down(reason);
    }
}

}  // namespace apollo::net::link
