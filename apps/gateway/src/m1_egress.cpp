#include "gateway/m1_egress.hpp"
#include "gateway/self_name.hpp"

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
#include <iterator>
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

// World 意图应答码：2001 = 「simulation not hosted」（路由型拒绝——无可用
// cell 承接）。与 apps/world/include/world_app/acceptance_queue.hpp
// kIntentAckInternal 对齐；2101 背压不作路由失效（非路由因）。
constexpr std::int32_t kWorldNotHostedAck = 2001;

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

// legacy 消息型 → 契约意图 id（白名单；0 = 不在 M1 前递面）。
std::uint16_t contract_id_for(apollo::protocol::MessageType type) {
    using MT = apollo::protocol::MessageType;
    switch (type) {
        case MT::CELL_CREATE_ENTITY: return id_of(MsgId::cell_create_entity);
        case MT::CELL_DESTROY_ENTITY: return id_of(MsgId::cell_destroy_entity);
        case MT::CELL_ENTITY_MOVE: return id_of(MsgId::cell_entity_move);
        case MT::CELL_CROSS_BORDER: return id_of(MsgId::cell_cross_border);
        case MT::COMBAT_SKILL_CAST: return id_of(MsgId::cell_skill_cast);
        // 队伍与小队副本族（P2 真实链路）：sessionId 由头部透传（协议帧头
        // session_id 即源会话，受理面入箱前已带）。
        case MT::PARTY_CREATE_REQUEST: return id_of(MsgId::party_create_intent);
        case MT::PARTY_JOIN_REQUEST: return id_of(MsgId::party_join_intent);
        case MT::SQUAD_LAUNCH: return id_of(MsgId::squad_launch_intent);
        case MT::SQUAD_ENTER: return id_of(MsgId::squad_enter_intent);
        case MT::SQUAD_START: return id_of(MsgId::squad_start_intent);
        case MT::SQUAD_KILL: return id_of(MsgId::squad_kill_intent);
        default: return 0;
    }
}

// 投递箱项：受理线程入箱的原始世界帧（翻译延至泵拍，链路单写者）。
// enqueued_ms = 入箱时刻（gateway span 计时起点——受理→发出全程）。
// space_id/cell_id = 路由提示（多 cell 批；0 = 未带）——受理面快照，入箱
// 即定格（路由事实以入箱时刻为准，不随泵拍晚到改写）。
// cell_host/cell_port = 归属 cell 的 M1 直连端点（L122；空/0 = 未带——出口
// 走 World 前递回退面），同定格纪律。
struct InboxItem {
    std::uint64_t session_id;
    std::uint16_t contract_id;
    std::vector<std::uint8_t> frame;
    std::uint64_t enqueued_ms = 0;
    std::uint64_t space_id = 0;
    std::uint64_t cell_id = 0;
    std::string cell_host;
    std::uint16_t cell_port = 0;
};

class M1EgressImpl final : public M1Egress {
public:
    explicit M1EgressImpl(M1EgressConfig config)
        : config_(std::move(config))
        // 请求号基址含网关身份域（多网关：两网关各从 1 起会在 World 请求关联键
        // 互撞——ack 按单键寻路无歧义需跨网关唯一；单网关基址 1 兼容）。
        , next_request_id_(gateway_id_base(config_.gateway_id)) {}

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
        // 对端 cell 装配的端点名（握手身份 = 本网关进程唯一 self_name——多网关
        // 同面共存 peer 键区分，gateway_self_name 单网关缺省同旧值）。
        mc.self_name = gateway_self_name(config_.gateway_id);
        mc.route_version = link::route_version_from_hash(route_table_->schema_hash());
        mc.compat_route_versions = link::compat_route_versions_from_env();
        manager_ = std::make_unique<link::LinkManager>(mc);

        link::EndpointConfig ep;
        ep.name = "world";
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

        // 已发但未确认的请求结果不明，不能跨 World 进程代次自动重放。
        // 旧代 reliable 由链路清除；pending 保留到超时，供观测与业务恢复。
        // 尚未发送的帧留在有界 inbox，握手成功后按原受理顺序投递。
        started_.store(true, std::memory_order_release);
        return true;
    }

    void setClientDeliver(ClientDeliverFn fn) override {
        client_deliver_ = std::move(fn);
    }

    void setRouteInvalidate(RouteInvalidateFn fn) override {
        on_route_invalidate_ = std::move(fn);
    }

    void stop() override {
        started_.store(false, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lock(inbox_mutex_);
            inbox_.clear();
        }
        retry_queue_.clear();  // 停机不落重试（原请求已无 pending 账面，请求方按超时收敛）
        route_health_.clear();
        manager_.reset();  // 链路随管理器析构拆线
        route_table_.reset();
    }

    void pump(std::uint64_t now_ms) override {
        if (!manager_) {
            return;
        }
        manager_->pump(now_ms);
        // 逐项就绪门（L122 多端点）：意图各自等各自的目标链（World 前递链
        // 或归属 cell 直连链）——就绪的当拍发出，未就绪的留箱/留队重排，
        // 不因单条链路未起而堵死其余端点的投递。
        process_due_retries(now_ms);  // 旧意图先走（近似保序）
        drain_inbox(now_ms);
        sweep_timeouts(now_ms);
    }

    bool forwardIntent(std::uint64_t session_id,
                       const std::vector<std::uint8_t>& legacy_frame,
                       std::uint64_t space_id, std::uint64_t cell_id,
                       const std::string& cell_host,
                       std::uint16_t cell_port) override {
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
            inbox_.push_back(InboxItem{session_id, contract_id, legacy_frame,
                                       trace::now_ms(), space_id, cell_id,
                                       cell_host, cell_port});
        }
        return true;
    }

    bool worldConnected() const override {
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

    std::uint64_t retriedIntents() const override {
        return retried_.load(std::memory_order_relaxed);
    }

    std::uint64_t redirectLimitHits() const override {
        return redirect_limit_hits_.load(std::memory_order_relaxed);
    }

    std::uint64_t directCellIntents() const override {
        return direct_cell_.load(std::memory_order_relaxed);
    }

    std::uint64_t deliveredClients() const override {
        return deliver_count_.load(std::memory_order_relaxed);
    }

    std::uint64_t undeliverableClients() const override {
        return undeliverable_.load(std::memory_order_relaxed);
    }

    std::size_t pendingAcks() const override {
        return pending_.size();
    }

private:
    // 发送结果（L122 多端点就绪门）：kSent 已交链路；kRequeue 目标链未就绪
    //（拨号/退避窗）——留箱/留队等下一拍，不落地不丢；kDrop 不可恢复（翻译
    // 失败/背压/端点未装配）落地计数。
    enum class SendOutcome { kSent, kRequeue, kDrop };

    link::InterServerLink* gateway_link() {
        return manager_ ? manager_->link("world") : nullptr;
    }

    // 目标链路（L122 网关→cell 直连）：带归属 cell 端点 = 直连该 cell 的 M1
    // 面（首见即建 active 端点，端点名按 host:port 定名——地址换则链换名，
    // 重连不换名，链路对象跨重连稳定）；未带端点 = World 前递链（既有回退
    // 面，端点未声明/旧端未上报时）。装配一次（同 world 端点纪律）。
    link::InterServerLink* target_link(const InboxItem& item) {
        if (manager_ == nullptr) {
            return nullptr;
        }
        if (item.cell_host.empty() || item.cell_port == 0) {
            return gateway_link();
        }
        const std::string name = "cell-" + item.cell_host + ":"
                                + std::to_string(item.cell_port);
        if (manager_->link(name) == nullptr) {
            link::EndpointConfig ep;
            ep.name = name;
            ep.host = item.cell_host;
            ep.port = item.cell_port;
            ep.role = link::EndpointRole::kActive;
            if (!manager_->add_endpoint(ep)) {
                ++dropped_;  // 装配失败（同名已存/坏配置）= 静默面
                return nullptr;
            }
            install_dispatching(*manager_->link(name));
        }
        return manager_->link(name);
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
        dispatcher.allow_domain("internal");  // 929 应答 / 939 下行族入闸（同域）
        dispatcher.set_handler(id_of(MsgId::cell_intent_ack),
                               [this](std::uint16_t, const std::uint8_t* data,
                                      std::size_t size) {
                                   handle_ack(data, size);
                               });
        // 客户端帧下行（P2 真实下行）：939 携客户端 wire 全帧回网关，
        // 按 session_id 投递；gateway_id 非零且不匹配本网关时落地不投
        //（多网关防串投——本批单网关恒 0，字段面为多网关批留）。
        dispatcher.set_handler(id_of(MsgId::client_deliver),
                               [this](std::uint16_t, const std::uint8_t* data,
                                      std::size_t size) {
                                   handle_client_deliver(data, size);
                               });
    }

    void handle_client_deliver(const std::uint8_t* data, std::size_t size) {
        std::vector<wire::FieldValue> fields;
        if (!wire::decode_payload(id_of(MsgId::client_deliver), data, size, fields) ||
            fields.size() < 3) {
            ++undeliverable_;
            return;
        }
        const auto session_id = static_cast<std::uint64_t>(fields[0].integer);
        const auto gateway_id = static_cast<std::uint32_t>(fields[1].integer);
        auto payload = fields[2].bytes;
        if (session_id == 0 || payload.empty() || payload.size() <
                sizeof(apollo::protocol::MessageHeader)) {
            ++undeliverable_;  // 残帧/空投递落地
            return;
        }
        // 身份校验：本网关身份已配（非 0）且回带不匹配 = 串投，落地不投；
        // 身份未配（0，单网关批缺省）或回带 0（World 不指定）均放行。
        if (config_.gateway_id != 0 && gateway_id != 0 &&
            gateway_id != config_.gateway_id) {
            ++undeliverable_;
            m1log().warning("client_deliver gateway mismatch: got "
                            + std::to_string(gateway_id) + " own "
                            + std::to_string(config_.gateway_id) + " session "
                            + std::to_string(session_id));
            return;
        }
        deliver_count_.fetch_add(1, std::memory_order_relaxed);
        if (client_deliver_) {
            client_deliver_(session_id, gateway_id, payload);
        }
    }

    // 透明重试到期拍（L122 末批）：延迟窗给 World/cell 就绪机会；到期项走与
    // 新意图同一翻译/发送路径（新 request_id、新 pending 账面）。目标链未
    // 就绪（拨号/退避窗）= 原样留队等下一拍；发不出去（背压/拆线）= 终局
    // 落地（dropped 计数，translate_and_send 内）。到期项先整体摘出再处理
    //（处理中留队回填不能与迭代同拍——push_back 扩容会使未完成迭代器失效）。
    void process_due_retries(std::uint64_t now_ms) {
        std::vector<RetryItem> due;
        std::vector<RetryItem> deferred;
        for (auto& entry : retry_queue_) {
            if (now_ms >= entry.retry_at_ms) {
                due.push_back(std::move(entry));
            } else {
                deferred.push_back(std::move(entry));  // 未到期（乱序到期时间）
            }
        }
        retry_queue_ = std::move(deferred);
        std::vector<RetryItem> requeue;
        for (auto& entry : due) {
            InboxItem item = std::move(entry.item);
            try {
                if (translate_and_send(item, now_ms, /*is_retry=*/true)
                    == SendOutcome::kRequeue) {
                    // 目标链未就绪：留队，下一拍原样重试（不落地不续时窗）
                    requeue.push_back(RetryItem{now_ms, std::move(item)});
                }
            } catch (const std::exception&) {
                ++unforwardable_;
            } catch (...) {
                ++unforwardable_;
            }
        }
        for (auto& entry : requeue) {
            retry_queue_.push_back(std::move(entry));
        }
    }

    void drain_inbox(std::uint64_t now_ms) {
        std::vector<InboxItem> items;
        {
            std::lock_guard<std::mutex> lock(inbox_mutex_);
            items = std::move(inbox_);
            inbox_.clear();
        }
        std::vector<InboxItem> requeue;
        for (auto& item : items) {
            // 泵线程推进帧体翻译：受理面只过帧头筛选，客户端残帧的 body 解码
            // 会抛（长度校验）——落地计数不扩散，不终止宿主泵线程。
            try {
                if (translate_and_send(item, now_ms)
                    == SendOutcome::kRequeue) {
                    // 目标链未就绪（L122 逐项就绪门）：留箱等下一拍
                    requeue.push_back(std::move(item));
                }
            } catch (const std::exception&) {
                ++unforwardable_;
            } catch (...) {
                ++unforwardable_;
            }
        }
        if (!requeue.empty()) {
            // 未就绪项回填箱首（保受理序：本轮未发出的项仍排在后续入箱项前）
            std::lock_guard<std::mutex> lock(inbox_mutex_);
            inbox_.insert(inbox_.begin(), std::make_move_iterator(requeue.begin()),
                           std::make_move_iterator(requeue.end()));
        }
    }

    // legacy 帧 → 契约字段（序 = 契约表序，WorldApp 侧 build_legacy_intent 镜像
    // 互逆）；帧体非法抛（drain_inbox 捕获落地）——受理面已过帧头筛选，此处兜底。
    // is_retry = 透明重试发送（新 pending 条目标记 retried，不再续试）。
    // 返回 SendOutcome（见枚举注）：kSent / kRequeue（目标链未就绪，留箱）/
    // kDrop（不可恢复，落地计数）。
    SendOutcome translate_and_send(const InboxItem& item, std::uint64_t now_ms,
                                   bool is_retry = false) {
        auto& frame = item.frame;
        if (frame.size() < sizeof(apollo::protocol::MessageHeader)) {
            ++unforwardable_;  // 残帧：body 切片越界，泵拍侧再验一次（不占关联键）
            return SendOutcome::kDrop;
        }
        // 目标链就绪门前置（L122 逐项就绪门）：链未就绪时先判后建载荷——
        // request_id 关联键只为真发送的帧配发（未就绪留箱/留队的帧不占键，
        // 补投时按当拍序号重新配发，键序与到达序一致）。
        auto* link = target_link(item);
        if (link == nullptr) {
            ++dropped_;  // 端点未装配（同名已存/坏配置）= 静默面
            return SendOutcome::kDrop;
        }
        if (link->state() != link::LinkState::kUp) {
            return SendOutcome::kRequeue;  // 拨号/退避窗：留箱等下一拍
        }
        const auto request_id = next_request_id_++;

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
            // 队伍与小队副本族（P2 真实链路）：字段序 = 契约表序，头部两字段
            // （request_id/session_id）出 switch 统一填。
            case id_of(MsgId::party_create_intent): {
                fields.resize(2);  // 空 body：仅头部
                break;
            }
            case id_of(MsgId::party_join_intent): {
                const auto msg = apollo::protocol::MessageCodec::decodeBody<
                    apollo::protocol::PartyJoinRequest>(body);
                fields.resize(3);
                fields[2].integer = static_cast<std::int64_t>(msg.partyId);
                break;
            }
            case id_of(MsgId::squad_launch_intent): {
                const auto msg = apollo::protocol::MessageCodec::decodeBody<
                    apollo::protocol::SquadLaunch>(body);
                fields.resize(4);
                fields[2].integer = static_cast<std::int64_t>(msg.partyId);
                fields[3].integer = static_cast<std::int64_t>(msg.mapId);
                break;
            }
            case id_of(MsgId::squad_enter_intent): {
                const auto msg = apollo::protocol::MessageCodec::decodeBody<
                    apollo::protocol::SquadEnter>(body);
                fields.resize(3);
                fields[2].integer = static_cast<std::int64_t>(msg.partyId);
                break;
            }
            case id_of(MsgId::squad_start_intent): {
                const auto msg = apollo::protocol::MessageCodec::decodeBody<
                    apollo::protocol::SquadStart>(body);
                fields.resize(3);
                fields[2].integer = static_cast<std::int64_t>(msg.partyId);
                break;
            }
            case id_of(MsgId::squad_kill_intent): {
                const auto msg = apollo::protocol::MessageCodec::decodeBody<
                    apollo::protocol::SquadKill>(body);
                fields.resize(4);
                fields[2].integer = static_cast<std::int64_t>(msg.partyId);
                fields[3].integer = static_cast<std::int64_t>(msg.monsterEntityId);
                break;
            }
            default:
                ++unforwardable_;
                return SendOutcome::kDrop;
        }

        // 头部两字段：request_id（应答关联键）+ session_id（源会话透传）
        fields[0].integer = static_cast<std::int64_t>(request_id);
        fields[1].integer = static_cast<std::int64_t>(item.session_id);

        std::vector<std::uint8_t> payload;
        if (!wire::encode_payload(item.contract_id, fields, payload)) {
            ++unforwardable_;  // 帧体翻译失败（decodeBody 不抛但字节面可能错位）
            return SendOutcome::kDrop;
        }

        // trace 子集（P3-3 语义层批）：意图恒走 932 internal_envelope——
        // trace 字段恒带（ids 恒传播，未采样不发射 span 行）；头采样在本
        // 进程（链根）裁决，下游按旗标传播不再重采。发端 span id 恒生成
        // 进信封 parent 位——WorldApp span 以此挂链（gateway span 记录仅采样
        // 命中时发射）。
        trace::HeadSampler& sampler = head_sampler();
        trace::EnvelopeFields env;
        env.message_type = trace::EnvelopeKind::EntityMethodCall;
        env.request_id = request_id;
        env.source_app = "gateway";
        env.target_app = "world";
        env.trace.trace_id = trace::generate_trace_id();
        env.trace.parent_span_id = trace::generate_span_id();
        env.trace.sampled = sampler.should_sample();
        env.payload = wire::append_envelope(item.contract_id, payload);
        // 路由提示（多 cell 批）：受理面快照的当前 cell/空间入信封——World 管理端
        // 据空间归属把模拟输入路由到当前 cell，不逐条经目录或 World 管理端代理。
        // 0 = 未带（单 cell 批/未解析路由），World 按首就绪回退。
        env.cell_id = item.cell_id;
        env.space_id = item.space_id;

        std::vector<std::uint8_t> packed;
        if (!trace::pack_envelope(env, packed)) {
            ++unforwardable_;  // 信封装配失败 = 契约面失配（正常不发生）
            return SendOutcome::kDrop;
        }

        // Reliable：意图不可丢；链路断连期间入队，重连恰一次补投。
        //（InterServerLink::send 自封 u16 id 信封——传裸载荷，勿再包一层）
        const auto code = link->send(id_of(MsgId::internal_envelope), packed.data(),
                                     packed.size(), link::SendClass::kReliable);
        if (code != link::SendCode::kAccepted) {
            ++dropped_;  // 背压/拒收：legacy 链路死观察面同型，落地不重试
            return SendOutcome::kDrop;
        }
        if (!item.cell_host.empty() && item.cell_port != 0) {
            ++direct_cell_;  // L122 直连归属 cell 发出（观测面）
        }
        // 首发条目保留原帧（2001 透明重试的载荷源）；重试条目不再续试，帧不
        // 复留（pending 生命周期由 ack 超时清扫封顶，帧容量随箱顶格同界）。
        // 路由身份随条目定格（D08 #4 限次键），发送同时刷新会话路由账：提示
        // 与既有账不同 = 宿主已重解析换路 → 预算重置；同路照常累计。
        auto& health = route_health_[item.session_id];
        if (health.space_id != item.space_id || health.cell_id != item.cell_id ||
            health.cell_host != item.cell_host ||
            health.cell_port != item.cell_port) {
            health = RouteHealth{item.space_id, item.cell_id, item.cell_host,
                                 item.cell_port, 0, now_ms};
        } else {
            health.last_ms = now_ms;
        }
        if (is_retry) {
            pending_.emplace(request_id,
                             Pending{item.session_id, now_ms, {}, true, item.space_id,
                                     item.cell_id, item.cell_host, item.cell_port});
            ++retried_;
        } else {
            pending_.emplace(request_id,
                             Pending{item.session_id, now_ms, item, false, item.space_id,
                                     item.cell_id, item.cell_host, item.cell_port});
        }
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
        return SendOutcome::kSent;
    }

    void handle_ack(const std::uint8_t* data, std::size_t size) {
        std::vector<wire::FieldValue> fields;
        if (!wire::decode_payload(id_of(MsgId::cell_intent_ack), data, size, fields) ||
            fields.size() < 2) {
            return;
        }
        const auto request_id = static_cast<std::uint64_t>(fields[0].integer);
        const auto code = static_cast<std::int32_t>(fields[1].integer);
        auto it = pending_.find(request_id);
        if (it == pending_.end()) {
            // 超时清扫后迟到的应答 / 未知关联键——无挂账可消，观察面记 debug
            m1log().debug("WorldApp intent ack without pending entry: request_id="
                          + std::to_string(request_id));
            return;
        }
        if (code == 0) {
            // 路由证明可用（D08 #4）：清该会话限次账（迟到应答落在新路由上
            // 不清旧账——身份不符保留计数）。
            const auto health_it = route_health_.find(it->second.session_id);
            if (health_it != route_health_.end() &&
                health_it->second.space_id == it->second.space_id &&
                health_it->second.cell_id == it->second.cell_id &&
                health_it->second.cell_host == it->second.cell_host &&
                health_it->second.cell_port == it->second.cell_port) {
                route_health_.erase(health_it);
            }
            pending_.erase(it);
            ++acked_;
            return;
        }
        const auto session_id = it->second.session_id;
        const auto retried = it->second.retried;
        ++failed_;
        m1log().warning("WorldApp intent rejected by remote: request_id="
                        + std::to_string(request_id) + " code="
                        + std::to_string(code));
        // 路由型拒绝（2001 = World「simulation not hosted」，契约对齐
        // acceptance_queue.hpp kIntentAckInternal）：会话路由缓存作废，
        // 下一意图经 ensureRouteSnapshot 重解析——迟到的旧 cell/空间提示
        // 不因缓存残留反复命中。背压（2101）/其余非 0 不作废（非路由因）。
        if (code == kWorldNotHostedAck && on_route_invalidate_) {
            on_route_invalidate_(session_id);
        }
        // 会话级重定向限次（D08 #4）：同一路由身份上的连续 2001 计数（含重
        // 试应答——重试本身也是重定向）；达上限后不再透明重试，但每次 2001
        // 照常作废缓存（宿主回退查目录重解析）。0 = 不限次（旧语义）。
        bool retry_allowed = false;
        if (code == kWorldNotHostedAck) {
            auto& health = route_health_[session_id];
            if (health.space_id != it->second.space_id ||
                health.cell_id != it->second.cell_id ||
                health.cell_host != it->second.cell_host ||
                health.cell_port != it->second.cell_port) {
                // 账面身份与本条不符（账被换路重置/回收后重建）——按本条
                // 路由重建账面后计数。
                health = RouteHealth{it->second.space_id, it->second.cell_id,
                                     it->second.cell_host, it->second.cell_port,
                                     0, trace::now_ms()};
            }
            ++health.rejects;
            health.last_ms = trace::now_ms();
            if (!retried && config_.intent_retry_enabled) {
                retry_allowed = config_.max_redirects_per_route == 0 ||
                                health.rejects <= config_.max_redirects_per_route;
                if (!retry_allowed) {
                    ++redirect_limit_hits_;
                    m1log().info("WorldApp redirect limit reached: session="
                                 + std::to_string(session_id) + " rejects="
                                 + std::to_string(health.rejects) + " on route "
                                 + std::to_string(health.space_id) + "/"
                                 + std::to_string(health.cell_id));
                }
            }
        }
        // 被拒意图透明重试一次（L122 末批 · 用户拍板「网关内透明重试一
        // 次」）：2001 为路由型拒绝（暂无可服务 cell，多为瞬时——cell 注
        // 册/重注册窗口），网关在链路内延迟重发同一条意图一次，不重写协
        // 议、无客户端可见同步面（请求方按既有超时收敛）。已重试过 / 重试
        // 关闭 / 会话限次命中 / 重试队列顶格 = 终局失败（failed_ 已计，不
        // 再续试）。
        if (code == kWorldNotHostedAck && retry_allowed) {
            if (retry_queue_.size() < config_.inbox_cap) {
                retry_queue_.push_back(
                    RetryItem{trace::now_ms() + config_.intent_retry_delay_ms,
                              std::move(it->second.item)});
            } else {
                m1log().warning("WorldApp intent retry skipped (retry queue full): "
                                "request_id=" + std::to_string(request_id));
            }
        }
        pending_.erase(it);
    }

    void sweep_timeouts(std::uint64_t now_ms) {
        for (auto it = pending_.begin(); it != pending_.end();) {
            if (now_ms - it->second.sent_at_ms >= config_.ack_timeout_ms) {
                m1log().warning("WorldApp intent ack timeout: request_id="
                                + std::to_string(it->first) + " session="
                                + std::to_string(it->second.session_id));
                it = pending_.erase(it);
                ++timed_out_;
            } else {
                ++it;
            }
        }
        // 限次账回收（D08 #4）：会话消亡无通知面，出口侧以闲置窗界定账面
        // 规模——idle 超 4×ack_timeout 清除（换路/成功应答本就重置，回收
        // 陈旧账只影响长闲置后的一次预算，属可接受退化）。
        const std::uint64_t health_idle_ms = 4 * config_.ack_timeout_ms;
        for (auto it = route_health_.begin(); it != route_health_.end();) {
            if (now_ms - it->second.last_ms > health_idle_ms) {
                it = route_health_.erase(it);
            } else {
                ++it;
            }
        }
    }

    struct Pending {
        std::uint64_t session_id;
        std::uint64_t sent_at_ms;
        InboxItem item;   // 首发载荷（2001 透明重试源；重试条目为空帧 + retried）
        bool retried;     // 已重试过（不再续试）
        // 路由身份定格（D08 #4 限次键，与受理入箱同纪律）：本条发送所用
        // 的路由提示四元组——2001 计数按此归属会话的路由账（提示变化 =
        // 宿主已重解析换路 = 预算重置）。
        std::uint64_t space_id = 0;
        std::uint64_t cell_id = 0;
        std::string cell_host;
        std::uint16_t cell_port = 0;
    };

    // 透明重试到期项（L122 末批）：retry_at_ms = 到期拍（延迟窗），item = 原帧
    // + 路由提示（入箱时刻定格）。
    struct RetryItem {
        std::uint64_t retry_at_ms;
        InboxItem item;
    };

    M1EgressConfig config_;
    std::unique_ptr<link::RouteTable> route_table_;
    std::unique_ptr<link::LinkManager> manager_;

    std::mutex inbox_mutex_;               // forwardIntent（受理/心跳线程）↔ 泵拍
    std::vector<InboxItem> inbox_;

    std::vector<RetryItem> retry_queue_;   // 透明重试到期队列（泵线程独占，同 pending_）
    std::unordered_map<std::uint64_t, Pending> pending_;  // 泵线程独占
    std::uint64_t next_request_id_ = 1;    // 网关身份域基址（ctor 按 gateway_id 设）；0 = legacy 无应答哨兵，不配发

    // D08 #4 重定向限次账（泵线程独占，同 pending_）：会话在「同一路由身份」
    // （space/cell/直连端点四元组，受理定格）上的连续 2001 计数。宿主重解析
    // 换路（提示变化）或 code 0 证明路由可用 → 账面重置；会话消亡无通知面
    // ——idle 超 4×ack_timeout 的陈旧账在 sweep_timeouts 回收（界内重入续账）。
    struct RouteHealth {
        std::uint64_t space_id = 0;
        std::uint64_t cell_id = 0;
        std::string cell_host;
        std::uint16_t cell_port = 0;
        std::uint32_t rejects = 0;
        std::uint64_t last_ms = 0;
    };
    std::unordered_map<std::uint64_t, RouteHealth> route_health_;

    ClientDeliverFn client_deliver_;       // 泵线程回调（939 下行出口）
    RouteInvalidateFn on_route_invalidate_;  // 泵线程回调（2001 路由失效出口）

    std::atomic<bool> started_{false};
    std::atomic<std::uint64_t> sent_{0};
    std::atomic<std::uint64_t> acked_{0};
    std::atomic<std::uint64_t> failed_{0};
    std::atomic<std::uint64_t> timed_out_{0};
    std::atomic<std::uint64_t> unforwardable_{0};
    std::atomic<std::uint64_t> dropped_{0};
    std::atomic<std::uint64_t> retried_{0};
    std::atomic<std::uint64_t> redirect_limit_hits_{0};  // D08 #4 限次命中（不再重试）
    std::atomic<std::uint64_t> direct_cell_{0};  // L122 直连归属 cell 发出
    std::atomic<std::uint64_t> deliver_count_{0};
    std::atomic<std::uint64_t> undeliverable_{0};
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
