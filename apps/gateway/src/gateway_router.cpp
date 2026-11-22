#include "gateway/gateway_server.hpp"
#include "gateway_server_detail.hpp"
#include <chrono>

//==============================================================================
// MessageRouter 实现（gateway_server 拆分单元：M1 出口装配/世界与 Base 前递/
// 939 客户端下行出口）
//==============================================================================

namespace gateway {

namespace {

// 库面日志出口（server_detail 单定义，本单元转发——定义在 gateway_server.cpp）
apollo::core::log::Logger& glog() {
    return server_detail::glog();
}

} // namespace

MessageRouter::MessageRouter(const GatewayConfig& config)
    : config_(config) {
}

MessageRouter::~MessageRouter() {
    stop();
}

void MessageRouter::start() {
    if (running_) return;

    // cell 面换装 M1 出口（2b-iii）：路由表缺位 = 出口降级（警告 + 空件，
    // world 前递落地），起进程不拦——处方①同口径。cell 不在线不在此刻判断，
    // active 端点自带退避重连。
    M1EgressConfig egressConfig;
    egressConfig.host = config_.worldM1Host;
    egressConfig.port = config_.worldM1Port;
    egressConfig.route_json = config_.worldM1RouteJson;
    m1Egress_ = makeM1Egress(std::move(egressConfig));
    if (!m1Egress_->start()) {
        glog().warning("gateway WorldApp M1 egress degraded: route table unavailable ("
                       + config_.worldM1RouteJson + "), WorldApp forwarding disabled");
        m1Egress_.reset();
    }

    if (m1Egress_ && deliverToClient_) {
        setDeliverToClient(deliverToClient_);
    }
    // 路由失效出口（start 前装配 → egress 晚建后重装，同 setDeliverToClient 序）。
    if (m1Egress_ && routeInvalidate_) {
        setRouteInvalidate(routeInvalidate_);
    }

    running_ = true;
}

void MessageRouter::stop() {
    running_ = false;
    if (m1Egress_) {
        m1Egress_->stop();
        m1Egress_.reset();
    }
    // 摘除宿主回调（audit M1 stop 排空面）：Base 前递/应答面经宿主网关的
    // 会话表与入口线程落地面，停机后晚到的 forwardToBaseApp 调用必须惰性
    // （face absent 丢弃），不得再进宿主引用面。deliverToClient_ 不在此摘——
    // 它只由本路由器自有 M1 出口泵触发（stop 已随 m1Egress_ 拆除），且
    // start 契约保留宿主 start 前装配的下行出口。
    forwardToBase_ = nullptr;
    baseResponse_ = nullptr;
}

void MessageRouter::pump() {
    if (!m1Egress_) {
        return;
    }
    m1Egress_->pump(std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch())
                        .count());
}

void MessageRouter::forwardToWorld(
    SessionID sessionId,
    const RouteSnapshot& routeSnapshot,
    const std::vector<uint8_t>& message
) {
    if (!m1Egress_) {
        glog().warning("Gateway dropped world message for session "
                       + std::to_string(sessionId) + " (M1 egress degraded)");
        return;
    }
    // 路由提示（多 cell 批）：RouteSnapshot 的当前 cell/空间映入 932 信封——
    // World 管理端据空间归属把模拟输入路由到当前 cell（0 = 未带，按首就绪回退）。
    if (!m1Egress_->forwardIntent(sessionId, message, routeSnapshot.spaceId,
                                  routeSnapshot.cellId, routeSnapshot.cellHost,
                                  routeSnapshot.cellPort)) {
        // 不可前递（非五族意图——如断连通知，cell 感知面随契约后续批收口）
        glog().info("Gateway dropped unforwardable world message for session "
                    + std::to_string(sessionId));
    }
}

void MessageRouter::forwardToBaseApp(SessionID sessionId, const std::vector<uint8_t>& message) {
    if (!forwardToBase_) {
        glog().info("Gateway dropped base message for session "
                    + std::to_string(sessionId) + " (base forwarding face absent)");
        return;
    }
    if (forwardToBase_(sessionId, message) == 0) {
        glog().info("Gateway dropped base message for session "
                    + std::to_string(sessionId) + " (base transport unavailable)");
    }
}

void MessageRouter::setBaseForwarding(
    std::function<std::uint64_t(SessionID, const std::vector<std::uint8_t>&)> forward,
    BaseResponseFn onResponse) {
    forwardToBase_ = std::move(forward);
    baseResponse_ = std::move(onResponse);
}

void MessageRouter::setDeliverToClient(
    std::function<void(SessionID, const std::vector<uint8_t>&)> fn) {
    // 保留宿主在 start 前设置的出口，重启路由器后也需重新装配。
    deliverToClient_ = std::move(fn);
    // 939 下行出口：M1 出口解字段 → 本回调（宿主泵线程）→ 入口投递队列。
    // egress 降级时闭包不装——World 回带只落 observability 计数。
    if (!m1Egress_) {
        return;
    }
    m1Egress_->setClientDeliver(
        [fn = deliverToClient_](std::uint64_t session_id, std::uint32_t /*gateway_id*/,
                             const std::vector<std::uint8_t>& payload) {
            if (fn) fn(session_id, payload);
        });
}

void MessageRouter::setRouteInvalidate(RouteInvalidateFn fn) {
    // 路由失效出口（2001 应答 → 宿主作废会话路由缓存）。egress 降级时闭包
    // 不装——无 M1 链路即无 2001 来源，宿主侧缓存重解析面不受影响。
    routeInvalidate_ = std::move(fn);
    if (!m1Egress_) {
        return;
    }
    m1Egress_->setRouteInvalidate(
        [fn = routeInvalidate_](std::uint64_t session_id) {
            if (fn) fn(session_id);
        });
}

RouteSnapshot MessageRouter::buildDefaultRoute() const {
    RouteSnapshot routeSnapshot;
    routeSnapshot.worldId = 1;
    routeSnapshot.mapId = 1;
    routeSnapshot.instanceId = 1;
    routeSnapshot.spaceId = 1;
    routeSnapshot.routeVersion = 1;
    // 路由名目留存（路由解析面与快照消费方仍按 url 标认 world 落点）；
    // M1 出口单端点直连（cellId=0 = 未带提示）——World 管理端按空间归属
    // 路由（spaceId 提示入 932 信封），无可服务归属时首就绪回退。
    routeSnapshot.worldServerUrl = config_.worldUrl;
    return routeSnapshot;
}

} // namespace gateway
