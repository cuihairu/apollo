#include "gateway/m1_egress.hpp"

#if !defined(_WIN32)

// Gateway M1 出口真实现（M1 上层接线批 · 2b-iii）：InterServerLink active
// 端点 + legacy 世界帧 → 契约 wire 翻译 + pending 应答关联。机制零内部线程
// ——链路收发/翻译/关联回收全在宿主 pump 一拍内推进；forwardIntent 仅供
// 泵线程外的调用面入互斥投递箱（受理线程与心跳线程都可达）。
//
// 翻译纪律（与 cell 接收端 build_legacy_intent 镜像互逆）：契约 924-928
// 字段序 = 契约表序，头部两字段恒为 request_id（应答关联键，自 1 起配——
// 0 = legacy 无应答哨兵）+ session_id（源会话透传）。

#include "apollo/core/log/log_manager.h"
#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/linktrace/envelope.hpp"
#include "apollo/net/linktrace/span_emitter.hpp"
#include "apollo/net/linktrace/trace_sampler.hpp"
#include "apollo/net/wire/contract_wire.hpp"
#include "apollo_contract.h"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"

#include <atomic>
#include <chrono>
#include <fstream>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

#endif // !_WIN32

namespace gateway {

#if !defined(_WIN32)



namespace {

namespace link = apollo::net::link;
namespace trace = apollo::net::trace;
namespace wire = apollo::net::wire;

using apollo::contract::gen::MsgId;

constexpr std::uint16_t id_of(MsgId id) {
    return static_cast<std::uint16_t>(id);
}

// 库面日志出口：cat=gateway_m1_egress（§5.1 cat= 检索键）
apollo::core::log::Logger& m1log() {
    return *apollo::core::log::global_log_manager().createLogger("gateway_m1_egress");
}

// 头部采样器（logging.md §5.1：入口进程裁决一次，下游不重采）。装配
// 优先级：configure 期 -DAPOLLO_TRACE_SAMPLE_RATE=<N>（生产配置常量，
// 编译期定死压过 env）> APOLLO_TRACE_SAMPLE_RATE env（dev 覆写）>
// 缺省 1/10⁴ 常量
trace::HeadSampler& head_sampler() {
#if defined(APOLLO_TRACE_SAMPLE_RATE_CONST)
    static trace::HeadSampler sampler{APOLLO_TRACE_SAMPLE_RATE_CONST};
#else
    static trace::HeadSampler sampler{trace::sample_rate_from_env()};
#endif
    return sampler;
}

// legacy 消息型 → 契约意图 id（五族白名单；0 = 不在 M1 前递面）。
std::uint16_t contract_id_for(apollo::protocol::MessageType type) {
    using MT = apollo::protocol::MessageType;
    switch (type) {
        case MT::CELL_CREATE_ENTITY: return id_of(MsgId::cell_create_entity);
        case MT::CELL_DESTROY_ENTITY: return id_of(MsgId::cell_destroy_entity);
        case MT::CELL_ENTITY_MOVE: return id_of(MsgId::cell_entity_move);
        case MT::CELL_CROSS_BORDER: return id_of(MsgId::cell_cross_border);
        case MT::COMBAT_SKILL_CAST: return id_of(MsgId::cell_skill_cast);
        default: return 0;
    }
}

// 投递箱项：受理线程入箱的原始世界帧（翻译延至泵拍，链路单写者）。
// enqueued_ms = 入箱时刻（gateway span 计时起点——受理→发出全程）。
struct InboxItem {
    std::uint64_t session_id;
    std::uint16_t contract_id;
    std::vector<std::uint8_t> frame;
    std::uint64_t enqueued_ms = 0;
};

class M1EgressImpl final : public M1Egress {
public:
    explicit M1EgressImpl(M1EgressConfig config)
        : config_(std::move(config)) {}

    bool start() override {
        if (started_.load(std::memory_order_acquire)) {
            return true;
        }
        route_table_ = std::make_unique<link::RouteTable>();
        if (!load_route_table(*route_table_)) {
            route_table_.reset();
            return false;  // 装配错误显形（空路径/坏 JSON = 出口降级，不 throw）
        }

        link::LinkManagerConfig mc;
        mc.self_name = "gateway";  // 对端 cell 装配的端点名（握手身份）
        mc.route_version = link::route_version_from_hash(route_table_->schema_hash());
        manager_ = std::make_unique<link::LinkManager>(mc);

        link::EndpointConfig ep;
        ep.name = "cell";
        ep.host = config_.host;
        ep.port = config_.port;
        ep.role = link::EndpointRole::kActive;
        if (!manager_->add_endpoint(ep)) {
            manager_.reset();
            route_table_.reset();
            return false;
        }

        // 链路对象随 add_endpoint 即存在且跨重连稳定——handler/表/白名单
        // 装配一次，重连不复装。
        install_dispatching(*manager_->link(ep.name));

        started_.store(true, std::memory_order_release);
        return true;
    }

    void stop() override {
        started_.store(false, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lock(inbox_mutex_);
            inbox_.clear();
        }
        manager_.reset();  // 链路随管理器析构拆线
        route_table_.reset();
    }

    void pump(std::uint64_t now_ms) override {
        if (!manager_) {
            return;
        }
        drain_inbox(now_ms);
        manager_->pump(now_ms);
        sweep_timeouts(now_ms);
    }

    bool forwardIntent(std::uint64_t session_id,
                       const std::vector<std::uint8_t>& legacy_frame) override {
        if (!started_.load(std::memory_order_acquire)) {
            return false;
        }
        // 仅在调用线程验帧头（五族白名单筛选；残帧/非意图族即刻落地），
        // 帧体翻译留泵拍——parseHeader 纯读，跨线程安全。
        if (legacy_frame.size() < sizeof(apollo::protocol::MessageHeader)) {
            ++unforwardable_;
            return false;
        }
        const auto header = apollo::protocol::MessageCodec::parseHeader(legacy_frame);
        const auto type = static_cast<apollo::protocol::MessageType>(header.type);
        const auto contract_id = contract_id_for(type);
        if (contract_id == 0) {
            ++unforwardable_;
            return false;
        }
        {
            std::lock_guard<std::mutex> lock(inbox_mutex_);
            if (inbox_.size() >= config_.inbox_cap) {
                ++unforwardable_;  // 箱满落地：legacy 链路死观察面同型
                return false;
            }
            inbox_.push_back(
                InboxItem{session_id, contract_id, legacy_frame, trace::now_ms()});
        }
        return true;
    }

    bool cellConnected() const override {
        auto* link = const_cast<M1EgressImpl*>(this)->gateway_link();
        return link != nullptr && link->state() == link::LinkState::kUp;
    }

    std::uint64_t sentIntents() const override {
        return sent_.load(std::memory_order_relaxed);
    }

    std::uint64_t ackedIntents() const override {
        return acked_.load(std::memory_order_relaxed);
    }

    std::uint64_t failedAcks() const override {
        return failed_.load(std::memory_order_relaxed);
    }

    std::uint64_t timedOutIntents() const override {
        return timed_out_.load(std::memory_order_relaxed);
    }

    std::uint64_t unforwardableFrames() const override {
        return unforwardable_.load(std::memory_order_relaxed);
    }

    std::uint64_t droppedSends() const override {
        return dropped_.load(std::memory_order_relaxed);
    }

    std::size_t pendingAcks() const override {
        return pending_.size();
    }

private:
    link::InterServerLink* gateway_link() {
        return manager_ ? manager_->link("cell") : nullptr;
    }

    bool load_route_table(link::RouteTable& table) {
        if (config_.route_json.empty()) {
            return false;
        }
        std::ifstream in(config_.route_json, std::ios::binary);
        if (!in) {
            return false;
        }
        std::ostringstream buffer;
        buffer << in.rdbuf();
#if APOLLO_NET_LINK_HAS_JSON
        return link::load_route_table_json(buffer.str(), table);
#else
        (void)table;
        return false;
#endif
    }

    void install_dispatching(link::InterServerLink& link) {
        auto& dispatcher = link.dispatcher();
        dispatcher.set_route_table(route_table_.get());
        dispatcher.allow_domain("internal");  // 929 应答族入闸（同意图族域）
        dispatcher.set_handler(id_of(MsgId::cell_intent_ack),
                               [this](std::uint16_t, const std::uint8_t* data,
                                      std::size_t size) {
                                   handle_ack(data, size);
                               });
    }

    void drain_inbox(std::uint64_t now_ms) {
        std::vector<InboxItem> items;
        {
            std::lock_guard<std::mutex> lock(inbox_mutex_);
            items = std::move(inbox_);
            inbox_.clear();
        }
        for (auto& item : items) {
            translate_and_send(item, now_ms);
        }
    }

    // legacy 帧 → 契约字段（序 = 契约表序，cell 侧 build_legacy_intent 镜像
    // 互逆）；帧体非法 = 落地计数（受理面已过帧头筛选，此处兜底）。
    void translate_and_send(const InboxItem& item, std::uint64_t now_ms) {
        const auto request_id = next_request_id_++;

        auto& frame = item.frame;
        const auto body = std::vector<std::uint8_t>(
            frame.begin() + sizeof(apollo::protocol::MessageHeader), frame.end());

        std::vector<wire::FieldValue> fields;
        switch (item.contract_id) {
            case id_of(MsgId::cell_create_entity): {
                const auto msg = apollo::protocol::MessageCodec::decodeBody<
                    apollo::protocol::CellCreateEntity>(body);
                fields.resize(8);
                fields[2].integer = static_cast<std::int64_t>(msg.entityId);
                fields[3].integer = static_cast<std::int64_t>(
                    static_cast<std::uint8_t>(msg.entityType));
                fields[4].integer = static_cast<std::int64_t>(msg.spaceId);
                fields[5].real = msg.position.x;
                fields[6].real = msg.position.y;
                fields[7].real = msg.position.z;
                break;
            }
            case id_of(MsgId::cell_destroy_entity): {
                const auto msg = apollo::protocol::MessageCodec::decodeBody<
                    apollo::protocol::CellDestroyEntity>(body);
                fields.resize(3);
                fields[2].integer = static_cast<std::int64_t>(msg.entityId);
                break;
            }
            case id_of(MsgId::cell_entity_move): {
                const auto msg = apollo::protocol::MessageCodec::decodeBody<
                    apollo::protocol::CellEntityMove>(body);
                fields.resize(9);
                fields[2].integer = static_cast<std::int64_t>(msg.entityId);
                fields[3].real = msg.oldPos.x;
                fields[4].real = msg.oldPos.y;
                fields[5].real = msg.oldPos.z;
                fields[6].real = msg.newPos.x;
                fields[7].real = msg.newPos.y;
                fields[8].real = msg.newPos.z;
                break;
            }
            case id_of(MsgId::cell_cross_border): {
                const auto msg = apollo::protocol::MessageCodec::decodeBody<
                    apollo::protocol::CellCrossBorder>(body);
                fields.resize(8);
                fields[2].integer = static_cast<std::int64_t>(msg.entityId);
                fields[3].integer = static_cast<std::int64_t>(msg.fromSpace);
                fields[4].integer = static_cast<std::int64_t>(msg.toSpace);
                fields[5].real = msg.position.x;
                fields[6].real = msg.position.y;
                fields[7].real = msg.position.z;
                break;
            }
            case id_of(MsgId::cell_skill_cast): {
                const auto msg = apollo::protocol::MessageCodec::decodeBody<
                    apollo::protocol::CombatSkillCast>(body);
                fields.resize(11);
                fields[2].integer = static_cast<std::int64_t>(msg.casterId);
                fields[3].integer = static_cast<std::int64_t>(msg.targetId);
                fields[4].integer = static_cast<std::int64_t>(msg.skillId);
                fields[5].real = msg.casterPos.x;
                fields[6].real = msg.casterPos.y;
                fields[7].real = msg.casterPos.z;
                fields[8].real = msg.targetPos.x;
                fields[9].real = msg.targetPos.y;
                fields[10].real = msg.targetPos.z;
                break;
            }
            default:
                ++unforwardable_;
                return;
        }

        // 头部两字段：request_id（应答关联键）+ session_id（源会话透传）
        fields[0].integer = static_cast<std::int64_t>(request_id);
        fields[1].integer = static_cast<std::int64_t>(item.session_id);

        std::vector<std::uint8_t> payload;
        if (!wire::encode_payload(item.contract_id, fields, payload)) {
            ++unforwardable_;  // 帧体翻译失败（decodeBody 不抛但字节面可能错位）
            return;
        }

        // trace 子集（P3-3 语义层批）：意图恒走 932 internal_envelope——
        // trace 字段恒带（ids 恒传播，未采样不发射 span 行）；头采样在本
        // 进程（链根）裁决，下游按旗标传播不再重采。发端 span id 恒生成
        // 进信封 parent 位——cell span 以此挂链（gateway span 记录仅采样
        // 命中时发射）。
        trace::HeadSampler& sampler = head_sampler();
        trace::EnvelopeFields env;
        env.message_type = trace::EnvelopeKind::EntityMethodCall;
        env.request_id = request_id;
        env.source_app = "gateway";
        env.target_app = "cell";
        env.trace.trace_id = trace::generate_trace_id();
        env.trace.parent_span_id = trace::generate_span_id();
        env.trace.sampled = sampler.should_sample();
        env.payload = wire::append_envelope(item.contract_id, payload);

        std::vector<std::uint8_t> packed;
        if (!trace::pack_envelope(env, packed)) {
            ++unforwardable_;  // 信封装配失败 = 契约面失配（正常不发生）
            return;
        }

        auto* link = gateway_link();
        if (link == nullptr) {
            ++dropped_;
            return;
        }
        // Reliable：意图不可丢；链路断连期间入队，重连恰一次补投。
        //（InterServerLink::send 自封 u16 id 信封——传裸载荷，勿再包一层）
        const auto code = link->send(id_of(MsgId::internal_envelope), packed.data(),
                                     packed.size(), link::SendClass::kReliable);
        if (code != link::SendCode::kAccepted) {
            ++dropped_;  // 背压/拒收：legacy 链路死观察面同型，落地不重试
            return;
        }
        pending_.emplace(request_id, Pending{item.session_id, now_ms});
        ++sent_;

        // gateway span（根；受理→发出，成对视图——落地意图无 cell 对侧
        // 段，慢链路定位只看已前递面）
        if (env.trace.sampled) {
            trace::SpanRecord span;
            span.name = "gateway_intent_forward";
            span.ctx = env.trace;
            span.ctx.parent_span_id = trace::SpanId{};  // 根
            span.span_id = env.trace.parent_span_id;    // 发端 id 即本 span
            span.start_ms = item.enqueued_ms;
            span.end_ms = trace::now_ms();
            trace::emit_span(span);
        }
    }

    void handle_ack(const std::uint8_t* data, std::size_t size) {
        std::vector<wire::FieldValue> fields;
        if (!wire::decode_payload(id_of(MsgId::cell_intent_ack), data, size, fields) ||
            fields.size() < 2) {
            return;
        }
        const auto request_id = static_cast<std::uint64_t>(fields[0].integer);
        const auto code = static_cast<std::int32_t>(fields[1].integer);
        const auto it = pending_.find(request_id);
        if (it == pending_.end()) {
            // 超时清扫后迟到的应答 / 未知关联键——无挂账可消，观察面记 debug
            m1log().debug("cell intent ack without pending entry: request_id="
                          + std::to_string(request_id));
            return;
        }
        pending_.erase(it);
        if (code == 0) {
            ++acked_;
        } else {
            ++failed_;
            m1log().warning("cell intent rejected by remote: request_id="
                            + std::to_string(request_id) + " code="
                            + std::to_string(code));
        }
    }

    void sweep_timeouts(std::uint64_t now_ms) {
        for (auto it = pending_.begin(); it != pending_.end();) {
            if (now_ms - it->second.sent_at_ms >= config_.ack_timeout_ms) {
                m1log().warning("cell intent ack timeout: request_id="
                                + std::to_string(it->first) + " session="
                                + std::to_string(it->second.session_id));
                it = pending_.erase(it);
                ++timed_out_;
            } else {
                ++it;
            }
        }
    }

    struct Pending {
        std::uint64_t session_id;
        std::uint64_t sent_at_ms;
    };

    M1EgressConfig config_;
    std::unique_ptr<link::RouteTable> route_table_;
    std::unique_ptr<link::LinkManager> manager_;

    std::mutex inbox_mutex_;               // forwardIntent（受理/心跳线程）↔ 泵拍
    std::vector<InboxItem> inbox_;

    std::unordered_map<std::uint64_t, Pending> pending_;  // 泵线程独占
    std::uint64_t next_request_id_ = 1;    // 0 = legacy 无应答哨兵，不配发

    std::atomic<bool> started_{false};
    std::atomic<std::uint64_t> sent_{0};
    std::atomic<std::uint64_t> acked_{0};
    std::atomic<std::uint64_t> failed_{0};
    std::atomic<std::uint64_t> timed_out_{0};
    std::atomic<std::uint64_t> unforwardable_{0};
    std::atomic<std::uint64_t> dropped_{0};
};

} // namespace

std::unique_ptr<M1Egress> makeM1Egress(M1EgressConfig config) {
    return std::make_unique<M1EgressImpl>(std::move(config));
}

#else // _WIN32

std::unique_ptr<M1Egress> makeM1Egress(M1EgressConfig /*config*/) {
    // link TCP 底座不进 M1 批（POSIX only）——Windows 退空件（forwardIntent
    // 恒 false，world 前递落地），调用面无 #if。
    return std::make_unique<M1Egress>();
}

#endif // !_WIN32

} // namespace gateway
