#include "apollo/net/gameconnection/gameconnection.hpp"

#include <algorithm>
#include <utility>

namespace apollo::net::gameconnection {

namespace {

using apollo::net::link::SendClass;
using apollo::net::link::WatermarkLevel;

constexpr std::size_t ch_index(Channel ch) {
    return static_cast<std::size_t>(ch);
}

}  // namespace

GameConnection::GameConnection(apollo::net::link::LinkConfig config, SessionConfig session_config)
    : link_(std::move(config)), cfg_(session_config) {
    // 通道环水位判据 = 环字节占用 / 环容量（§4.2）——Watermark 分母与环
    // 容量同源（默认阈值 500/750/950‰）。
    const auto init_queue = [this](Channel ch, std::uint64_t cap) {
        auto& q = queues_[ch_index(ch)];
        q.capacity = cap;
        apollo::net::link::WatermarkConfig wc;
        wc.capacity_bytes = cap;
        q.wm = apollo::net::link::Watermark{wc};
    };
    init_queue(Channel::kMovement, cfg_.movement_capacity_bytes);
    init_queue(Channel::kAttributes, cfg_.attributes_capacity_bytes);
    init_queue(Channel::kEvents, cfg_.events_capacity_bytes);
    init_queue(Channel::kControl, cfg_.control_capacity_bytes);
    // 链路事件内收（用户 sink 走 set_session_sink，不与链路 sink 竞争）。
    link_.set_sink(apollo::net::link::LinkEventSink{
        [this]() { on_link_up(); },
        [this](apollo::net::link::LinkDownReason r) { on_link_down(r); },
        nullptr,
    });
}

GameConnection::~GameConnection() = default;

void GameConnection::attach(std::unique_ptr<apollo::net::link::ILinkTransport> transport,
                            std::uint64_t now_ms) {
    last_pump_ms_ = now_ms;
    link_.attach(std::move(transport), now_ms);
}

bool GameConnection::pump(std::uint64_t now_ms) {
    last_pump_ms_ = now_ms;
    flush_channels();
    return link_.pump(now_ms);
}

SendCode GameConnection::send(std::uint16_t msg_id, const std::vector<std::uint8_t>& payload,
                              Channel ch, Priority p) {
    return send(msg_id, payload.data(), payload.size(), ch, p);
}

SendCode GameConnection::send(std::uint16_t msg_id, const std::uint8_t* payload, std::size_t size,
                              Channel ch, Priority p) {
    Pending f;
    f.msg_id = msg_id;
    f.priority = p;
    f.payload.assign(payload, payload + size);
    switch (ch) {
    case Channel::kMovement:
        return admit_movement(msg_id, std::move(f));
    case Channel::kAttributes:
        return admit_attributes(queues_[ch_index(ch)], p, wire_size(size), std::move(f));
    case Channel::kEvents:
        return admit_fifo(queues_[ch_index(ch)], std::move(f), /*freeze_at_cut=*/true);
    case Channel::kControl:
        return admit_fifo(queues_[ch_index(ch)], std::move(f), /*freeze_at_cut=*/false);
    }
    return SendCode::kBackPressured;
}

// movement（§4.1）：不可靠，同 id 只发最新（覆盖队内未发送旧帧）；环满丢
// 最旧；永不拒收——最新帧必收（单帧超容量时清空旧帧仍入队）。
SendCode GameConnection::admit_movement(std::uint16_t msg_id, Pending&& f) {
    auto& q = queues_[ch_index(Channel::kMovement)];
    const std::uint64_t sz = wire_size(f.payload.size());
    for (auto& fr : q.frames) {
        if (fr.msg_id == msg_id) {
            q.bytes -= wire_size(fr.payload.size());
            q.bytes += sz;
            fr.payload = std::move(f.payload);
            ++session_stats_.movement_overwritten;
            return SendCode::kAccepted;
        }
    }
    while (q.bytes + sz > q.capacity && !q.frames.empty()) {
        q.bytes -= wire_size(q.frames.front().payload.size());
        q.frames.pop_front();
        ++session_stats_.movement_dropped;
    }
    q.bytes += sz;
    q.frames.push_back(std::move(f));
    return SendCode::kAccepted;
}

// attributes（§4.2 行为表）：cut 冻结；hard 起 NORM 拒收；腾位 = 丢最旧的
// LOW 帧（§2 TRIMMED_LOW：入队但同通道低优先级帧被丢弃腾位，计数上报）。
SendCode GameConnection::admit_attributes(ChannelQueue& q, Priority p, std::uint64_t sz,
                                          Pending&& f) {
    const auto level = q.wm.level(q.bytes);
    if (level == WatermarkLevel::kCut) {
        return SendCode::kBackPressured;
    }
    if (level >= WatermarkLevel::kHard && p == Priority::kNormal) {
        return SendCode::kBackPressured;
    }
    bool trimmed = false;
    while (q.bytes + sz > q.capacity) {
        auto it = std::find_if(q.frames.begin(), q.frames.end(),
                               [](const Pending& fr) { return fr.priority == Priority::kLow; });
        if (it == q.frames.end()) {
            return SendCode::kBackPressured;
        }
        q.bytes -= wire_size(it->payload.size());
        q.frames.erase(it);
        ++session_stats_.trimmed_low_dropped;
        trimmed = true;
    }
    q.bytes += sz;
    q.frames.push_back(std::move(f));
    return trimmed ? SendCode::kTrimmedLow : SendCode::kAccepted;
}

// events / control FIFO：环满即拒（events 在 cut 冻结——§4.2 reliable 冻结
// 行，control 豁免）。
SendCode GameConnection::admit_fifo(ChannelQueue& q, Pending&& f, bool freeze_at_cut) {
    const std::uint64_t sz = wire_size(f.payload.size());
    if (freeze_at_cut && q.wm.level(q.bytes) == WatermarkLevel::kCut) {
        return SendCode::kBackPressured;
    }
    if (q.bytes + sz > q.capacity) {
        return SendCode::kBackPressured;
    }
    q.bytes += sz;
    q.frames.push_back(std::move(f));
    return SendCode::kAccepted;
}

// 四通道环 → 链路（control 先刷保饿不死；遇链路背压即停留环下拍再刷）。
void GameConnection::flush_channels() {
    const auto drain = [this](ChannelQueue& q, SendClass cls) {
        while (!q.frames.empty() && forward_front(q, cls)) {
        }
    };
    drain(queues_[ch_index(Channel::kControl)], SendClass::kReliable);
    drain(queues_[ch_index(Channel::kAttributes)], SendClass::kReliable);
    drain(queues_[ch_index(Channel::kEvents)], SendClass::kReliable);
    drain(queues_[ch_index(Channel::kMovement)], SendClass::kDroppable);
}

bool GameConnection::forward_front(ChannelQueue& q, SendClass cls) {
    const auto& f = q.frames.front();
    if (link_.send(f.msg_id, f.payload.data(), f.payload.size(), cls) !=
        apollo::net::link::SendCode::kAccepted) {
        return false;
    }
    q.bytes -= wire_size(f.payload.size());
    q.frames.pop_front();
    return true;
}

void GameConnection::subscribe(std::uint16_t msg_id,
                               apollo::net::link::Dispatcher::Handler handler) {
    {
        std::lock_guard<std::mutex> lk(inbound_mutex_);
        handlers_[msg_id] = handler;
    }
    // 链路分发器挂转发器：pump 线程只拷贝入队，handler 由 owning 线程执行。
    link_.dispatcher().set_handler(
        msg_id, [this](std::uint16_t id, const std::uint8_t* data, std::size_t size) {
            std::vector<std::uint8_t> copy(data, data + size);
            std::lock_guard<std::mutex> lk(inbound_mutex_);
            if (inbound_queue_.size() >= cfg_.inbound_queue_depth) {
                inbound_queue_.pop_front();
                ++session_stats_.inbound_dropped;
            }
            inbound_queue_.push_back(Inbound{id, std::move(copy)});
        });
}

void GameConnection::drain_inbound() {
    std::deque<Inbound> batch;
    {
        std::lock_guard<std::mutex> lk(inbound_mutex_);
        batch.swap(inbound_queue_);
    }
    for (auto& in : batch) {
        apollo::net::link::Dispatcher::Handler h;
        {
            std::lock_guard<std::mutex> lk(inbound_mutex_);
            auto it = handlers_.find(in.msg_id);
            if (it == handlers_.end()) {
                continue;
            }
            h = it->second;
        }
        if (h) {
            h(in.msg_id, in.payload.data(), in.payload.size());
        }
    }
}

void GameConnection::set_route_table(const apollo::net::link::RouteTable* table) {
    link_.dispatcher().set_route_table(table);
}

void GameConnection::allow_domain(std::string domain) {
    link_.dispatcher().allow_domain(std::move(domain));
}

void GameConnection::set_session_sink(SessionSink sink) {
    sink_ = std::move(sink);
}

SessionState GameConnection::state() const {
    switch (link_.state()) {
    case apollo::net::link::LinkState::kHandshaking:
        return resume_pending_ ? SessionState::kResuming : SessionState::kHandshaking;
    case apollo::net::link::LinkState::kUp:
        return resuming_now_ ? SessionState::kResuming : SessionState::kActive;
    case apollo::net::link::LinkState::kIdle:
    case apollo::net::link::LinkState::kDown:
    default:
        return SessionState::kClosed;
    }
}

void GameConnection::close(const std::string& /*reason*/) {
    link_.close();
}

std::uint64_t GameConnection::channel_bytes(Channel ch) const {
    return queues_[ch_index(ch)].bytes;
}

// 同 epoch 重连 + resume 窗口内 = 恢复：报 missing_seq 范围后转 kActive；
// 过期/对端重启（epoch 变）/首连 = 全新会话。
void GameConnection::on_link_up() {
    const bool within_ttl = cfg_.resume_ttl_ms > 0 && last_pump_ms_ >= down_ms_ &&
                            last_pump_ms_ - down_ms_ <= cfg_.resume_ttl_ms;
    if (resume_pending_ && within_ttl && link_.peer_epoch() == epoch_at_down_) {
        resume_pending_ = false;
        resuming_now_ = true;
        if (sink_.on_resume) {
            ResumeInfo info;
            info.resumed = true;
            info.first_missing_seq = missing_lo_;
            info.last_missing_seq = missing_hi_;
            sink_.on_resume(info);
        }
        resuming_now_ = false;
        ++session_stats_.resumes;
    }
    resume_pending_ = false;
    ever_up_ = true;
    if (sink_.on_active) {
        sink_.on_active();
    }
}

// 断线快照 resume 候选（仅到过 kUp 的会话）：epoch、断线时刻、missing_seq
// 范围 =（对端已确认, 本端已发出]；无未确认帧则范围归零。
void GameConnection::on_link_down(apollo::net::link::LinkDownReason reason) {
    if (ever_up_) {
        resume_pending_ = true;
        epoch_at_down_ = link_.peer_epoch();
        down_ms_ = last_pump_ms_;
        missing_lo_ = link_.acked_seq() + 1;
        missing_hi_ = link_.last_sent_seq();
        if (missing_hi_ < missing_lo_) {
            missing_lo_ = 0;
            missing_hi_ = 0;
        }
    }
    if (sink_.on_closed) {
        sink_.on_closed(reason);
    }
}

}  // namespace apollo::net::gameconnection
