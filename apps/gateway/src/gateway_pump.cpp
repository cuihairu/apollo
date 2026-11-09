#include "gateway/gateway_server.hpp"
#include "gateway_server_detail.hpp"
#include <chrono>
#include <thread>

//==============================================================================
// 宿主泵面（gateway_server 拆分单元：泵循环/metrics 推进/排空状态机）
//==============================================================================

namespace gateway {

namespace {

// 库面日志出口（server_detail 单定义，本单元转发——定义在 gateway_server.cpp）
apollo::core::log::Logger& glog() {
    return server_detail::glog();
}

// 单调毫秒（排空期限钟——受理点（handleDrainRequest）与推进（drain_pump）
// 都在泵线程，同钟无竞）。
std::uint64_t steady_now_ms() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}
} // namespace

void GatewayServer::acceptNewConnections() {
    // 宿主泵循环（net M1 缝纪律：机制零内部线程）：accept/收帧/发帧/M1 出口
    // 推进全在此线程过拍，经 ingress 投递队列与 send/close 跨线程面解耦。
    while (running_) {
        const auto now_ms = steady_now_ms();
        ingressServer_->pump();
        messageRouter_->pump();
        if (controlServer_) {
            controlServer_->pump(now_ms);
        }
        drain_pump(now_ms);
        if (pumpHook_) {
            pumpHook_(now_ms);
        }
        recordMetrics();
        ++pumpTicks_;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void GatewayServer::recordMetrics() {
    auto& registry = apollo::core::metrics::MetricRegistry::instance();
    registry.gauge("gateway_sessions")
        .store(static_cast<std::int64_t>(sessionManager_->getSessionCount()));
    if (auto* egress = messageRouter_->egress(); egress != nullptr) {
        // pending 关联账面仅限宿主泵线程读（m1_egress 线程纪律）——本拍合法
        registry.gauge("gateway_pending_acks")
            .store(static_cast<std::int64_t>(egress->pendingAcks()));
        // 出口计数为进程期只增原子——增量推进（Prometheus rate() 消费面）
        counter_advance_to(gatewayIntentsSentTotal_, lastIntentsSent_,
                           egress->sentIntents());
        counter_advance_to(gatewayIntentsAckedTotal_, lastIntentsAcked_,
                           egress->ackedIntents());
        counter_advance_to(gatewayIntentsFailedTotal_, lastIntentsFailed_,
                           egress->failedAcks());
        counter_advance_to(gatewayIntentsTimedOutTotal_, lastIntentsTimedOut_,
                           egress->timedOutIntents());
        counter_advance_to(gatewayIntentsUnforwardableTotal_, lastIntentsUnforwardable_,
                           egress->unforwardableFrames());
        counter_advance_to(gatewayIntentsDroppedTotal_, lastIntentsDropped_,
                           egress->droppedSends());
    }
}

DrainStatus GatewayServer::handleDrainRequest(const DrainControlRequest& request) {
    switch (request.op) {
        case 1: {  // begin：关监听停新连接 + 起期限（幂等：已排水期重入只收紧期限）
            if (drain_state_.load(std::memory_order_acquire) == kDrainServing) {
                ingressServer_->beginDrain();
                drain_state_.store(kDrainDraining, std::memory_order_release);
                glog().info("gateway drain begun: new connections refused, "
                            "existing connections served");
            }
            if (request.deadline_s != 0) {
                const auto deadline_ms =
                    steady_now_ms()
                    + static_cast<std::uint64_t>(request.deadline_s) * 1000U;
                drain_deadline_ms_ = drain_deadline_ms_ == 0
                                         ? deadline_ms
                                         : std::min(drain_deadline_ms_, deadline_ms);
            }
            break;
        }
        case 2:
            break;  // status：直落快照
        default:
            return DrainStatus{false, drainState(), 0,
                               "unknown op " + std::to_string(request.op)};
    }
    DrainStatus st;
    st.ok = true;
    st.state = drainState();
    st.active_connections = ingressServer_->activeConnections();
    if (st.state == kDrainDrained) {
        st.detail = drain_detail_;
    } else if (st.state == kDrainDraining) {
        st.detail = "draining: " + std::to_string(st.active_connections)
                    + " active connections served";
    } else {
        st.detail = "serving";
    }
    return st;
}

void GatewayServer::drain_pump(std::uint64_t now_ms) {
    if (drain_state_.load(std::memory_order_acquire) != kDrainDraining) {
        return;
    }
    if (ingressServer_->activeConnections() == 0) {
        enter_drained("drained: all client connections left");
        return;
    }
    if (drain_deadline_ms_ != 0 && now_ms >= drain_deadline_ms_) {
        // 强停：期限到仍在线——全量收线（close 经投递队列，泵线程落 socket）
        // 后落终态。强停结果 = 在途客户端工作丢弃，客户端按断线重连面恢复；
        // 数量显形在日志与 status detail（不静默）。
        const auto ids = ingressServer_->connectionIds();
        for (const auto id : ids) {
            ingressServer_->close(id, DisconnectReason::NormalClose);
        }
        enter_drained("drained: deadline reached, " + std::to_string(ids.size())
                      + " connections force-closed");
    }
}

void GatewayServer::enter_drained(const std::string& detail) {
    drain_detail_ = detail;
    drain_state_.store(kDrainDrained, std::memory_order_release);
    glog().info("gateway " + detail);
}

} // namespace gateway
