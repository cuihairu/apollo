#include "apollo/net/link/link_manager.hpp"

#include <algorithm>

#if !defined(_WIN32)

namespace apollo::net::link {

LinkManager::Managed::Managed(const EndpointConfig& cfg, const LinkManagerConfig& mc)
    : config(cfg) {
    link_config.self_name = mc.self_name;
    link_config.route_version = mc.route_version;
    link_config.compat_route_versions = mc.compat_route_versions;
    link_config.authority_epoch = mc.authority_epoch;
    // active 角色链路在此；passive 的链路族群由 pump 按对端各建（fan-out）。
    if (cfg.role == EndpointRole::kActive) {
        link = std::make_unique<InterServerLink>(link_config);
    }
}

LinkManager::LinkManager(LinkManagerConfig config) : config_(std::move(config)) {}

bool LinkManager::add_endpoint(const EndpointConfig& endpoint) {
    if (endpoint.name.empty() || by_name_.count(endpoint.name) != 0) {
        return false;
    }
    auto m = std::make_unique<Managed>(endpoint, config_);
    if (m->config.role == EndpointRole::kPassive) {
        if (!m->listener.start(m->config.host, m->config.port)) {
            return false;  // 配置错误装配期显形
        }
    }
    wire_sink(*m);
    by_name_.emplace(m->config.name, m.get());
    endpoints_.push_back(std::move(m));
    return true;
}

void LinkManager::wire_sink(Managed& m) {
    if (!m.link) {
        return;  // passive 端点无本端链路（fan-out 面），事件走 wire_peer_sink
    }
    LinkEventSink sink;
    const std::string name = m.config.name;
    sink.on_up = [this, name]() {
        if (up_fn_) {
            up_fn_(name);
        }
    };
    sink.on_down = [this, name](LinkDownReason reason) {
        if (Managed* m = by_name_[name]) {
            m->down_pending = true;  // 重拨排程在泵拍（回调无时钟）
        }
        if (down_fn_) {
            down_fn_(name, reason);
        }
    };
    sink.on_peer_restart = [this, name](std::uint64_t old_epoch, std::uint64_t new_epoch) {
        if (restart_fn_) {
            restart_fn_(name, old_epoch, new_epoch);
        }
    };
    m.link->set_sink(std::move(sink));
}

// peer 链事件（fan-out 面）：上线装配位挂在链路 on_up——handle_hello 置
// kUp 后，同一次 pump 的读批可能紧跟对端补投的业务帧，装配必须在分发前
// 就位（迁移拍再装就晚了）；断连即以对端自报名出账（握手期坏链无名字，
// 不出账，走 handshake_failures 计数）。
void LinkManager::wire_peer_sink(InterServerLink& peer) {
    LinkEventSink sink;
    sink.on_up = [this, &peer]() {
        if (peer_fn_) {
            peer_fn_(peer);
        }
    };
    sink.on_down = [this, &peer](LinkDownReason reason) {
        if (down_fn_ && !peer.peer_name().empty()) {
            down_fn_(peer.peer_name(), reason);
        }
    };
    sink.on_peer_restart = [this, &peer](std::uint64_t old_epoch, std::uint64_t new_epoch) {
        if (restart_fn_ && !peer.peer_name().empty()) {
            restart_fn_(peer.peer_name(), old_epoch, new_epoch);
        }
    };
    peer.set_sink(std::move(sink));
}

void LinkManager::notify_fleet_down(const std::string& name, std::uint64_t /*now_ms*/) {
    auto it = by_name_.find(name);
    if (it == by_name_.end()) {
        return;
    }
    Managed& m = *it->second;
    m.fleet_up = false;  // 死了的进程不拨；在途连接器一并取消
    if (m.dialing) {
        m.connector.cancel();
        m.dialing = false;
    }
}

void LinkManager::notify_fleet_up(const std::string& name, std::uint64_t now_ms) {
    auto it = by_name_.find(name);
    if (it == by_name_.end()) {
        return;
    }
    Managed& m = *it->second;
    m.fleet_up = true;
    m.consecutive_failures = 0;  // 复活即抢跑：退避复位，立即重拨
    m.next_dial_ms = now_ms;
}

void LinkManager::pump(std::uint64_t now_ms) {
    for (auto& ep : endpoints_) {
        Managed& m = *ep;
        switch (m.config.role) {
            case EndpointRole::kActive:
                // 链路恒泵（对端死亡要靠 EOF/心跳显形）；fleet 闸只管拨号。
                pump_active(m, now_ms);
                break;
            case EndpointRole::kPassive: {
                // accept 出的每条传输独立成链（fan-out；对端重拨 = 同名新链
                // 替换旧链，语义见 pump_passive）。
                while (auto transport = m.listener.accept()) {
                    auto peer = std::make_unique<InterServerLink>(m.link_config);
                    wire_peer_sink(*peer);
                    peer->attach(std::move(transport), now_ms);
                    m.staging.push_back(std::move(peer));
                }
                pump_passive(m, now_ms);
                break;
            }
        }
    }
}

void LinkManager::pump_passive(Managed& m, std::uint64_t now_ms) {
    // staging 推进：握手完迁入 peers（消费者装配分发在迁移前——链路对象
    // 地址自此稳定）；握手期断连 = 装配级故障计数（魔数/版本失配/握手超时）。
    for (auto it = m.staging.begin(); it != m.staging.end();) {
        auto& peer = *it;
        if (!peer->pump(now_ms)) {
            ++m.handshake_failures;
            it = m.staging.erase(it);
            continue;
        }
        if (peer->state() == LinkState::kUp) {
            const std::string name = peer->peer_name();
            if (m.peers.erase(name) != 0 && down_fn_) {
                // 同名替换：旧链退役出账（对端重拨竞速或配置重名——旧链上
                // 未确认 reliable 不再续传，上层按断连对待）。
                down_fn_(name, LinkDownReason::kLocalClose);
            }
            if (up_fn_) {
                up_fn_(name);  // 装配（peer_fn_）已在链路 on_up 当拍完成
            }
            m.peers.emplace(name, std::move(peer));
            it = m.staging.erase(it);
        } else {
            ++it;
        }
    }
    // 已上线 peers：断连即移除（down 事件已由链路 sink 出账）。
    for (auto it = m.peers.begin(); it != m.peers.end();) {
        if (it->second->pump(now_ms)) {
            ++it;
        } else {
            it = m.peers.erase(it);
        }
    }
}

void LinkManager::pump_active(Managed& m, std::uint64_t now_ms) {
    if (m.down_pending) {
        m.down_pending = false;
        m.next_dial_ms = now_ms + config_.backoff_initial_ms;  // 断连重拨从 initial 起
    }

    // 在途拨号收尾。
    if (m.dialing) {
        const auto r = m.connector.pump(now_ms);
        if (r == TcpConnector::PumpResult::kConnected) {
            m.link->attach(m.connector.take_transport(), now_ms);
            m.dialing = false;
            m.consecutive_failures = 0;  // 连上即复位退避
        } else if (r == TcpConnector::PumpResult::kFailed) {
            m.connector.cancel();
            m.dialing = false;
            if (m.consecutive_failures < ~std::uint32_t{0}) {
                ++m.consecutive_failures;
            }
            schedule_backoff(m, now_ms);  // 指数退避（封顶），到点由下述放行
        }
    }

    const bool link_live =
        m.link->state() == LinkState::kUp || m.link->state() == LinkState::kHandshaking;
    if (m.dialing || link_live) {
        m.link->pump(now_ms);
        return;
    }

    // 未连：退避到点即拨；编队死亡闸住拨号起点（在途收尾不受影响——
    // notify_fleet_down 已取消在途连接器）。
    if (m.fleet_up && now_ms >= m.next_dial_ms) {
        if (m.connector.start(m.config.host, m.config.port, m.config.connect_timeout_ms,
                              now_ms)) {
            m.dialing = true;
            ++m.attempts;
        } else {
            // 装配级失败（名字解析）：按一次失败计，走同一退避。
            if (m.consecutive_failures < ~std::uint32_t{0}) {
                ++m.consecutive_failures;
            }
            schedule_backoff(m, now_ms);
        }
    }
}

void LinkManager::schedule_backoff(Managed& m, std::uint64_t now_ms) {
    const std::uint64_t shift = m.consecutive_failures - 1;
    const std::uint64_t backoff =
        std::min<std::uint64_t>(config_.backoff_initial_ms << (shift < 16 ? shift : 16),
                                config_.backoff_max_ms);
    m.next_dial_ms = now_ms + backoff;
}

InterServerLink* LinkManager::find_staging_peer(Managed& m, const std::string& name) {
    // 握手完毕（kUp）但尚未迁入 peers 的链（读批竞速窗：HELLO 后同一读批
    // 紧跟业务帧——分发发生在 staging 泵内，此时按名回执查 peers 落空，
    // wire_peer_sink 注记）。链路对象地址随 unique_ptr 迁移不挪，返回语义
    // 与已注册 peer 同；仍在握手（无名）的不算。
    for (auto& s : m.staging) {
        if (s->state() == LinkState::kUp && s->peer_name() == name) {
            return s.get();
        }
    }
    return nullptr;
}

InterServerLink* LinkManager::link(const std::string& name) {
    // 端点名命中的：active → 本端链路；passive → 先按对端自报名直查（fan-out
    // 面：端点名与对端名同名时，多 peer 场景须按名消歧而非拒答——否则第二
    // 条对端链上线即吞掉全部按端点名回执），再单 peer 兜底（1:1 场景端点名
    // = 对端名）。
    auto it = by_name_.find(name);
    if (it != by_name_.end()) {
        Managed& m = *it->second;
        if (m.config.role == EndpointRole::kActive) {
            return m.link.get();
        }
        if (auto p = m.peers.find(name); p != m.peers.end()) {
            return p->second.get();  // 对端自报名与端点名同链路名：按名精确消歧
        }
        if (m.peers.size() == 1) {
            return m.peers.begin()->second.get();
        }
        if (auto* s = find_staging_peer(m, name)) {
            return s;  // 竞速窗兜底（1:1 场景读批同拍分发）
        }
        return nullptr;
    }
    // 对端自报名直查（fan-out 面：peer 名不是端点名，逐 passive 端点找）。
    for (auto& ep : endpoints_) {
        if (ep->config.role != EndpointRole::kPassive) {
            continue;
        }
        if (auto p = ep->peers.find(name); p != ep->peers.end()) {
            return p->second.get();
        }
        if (auto* s = find_staging_peer(*ep, name)) {
            return s;  // fan-out 同拍竞速窗兜底
        }
    }
    return nullptr;
}

std::uint16_t LinkManager::endpoint_port(const std::string& name) const {
    auto it = by_name_.find(name);
    return it == by_name_.end() ? 0 : it->second->listener.port();
}

std::uint64_t LinkManager::dial_attempts(const std::string& name) const {
    auto it = by_name_.find(name);
    return it == by_name_.end() ? 0 : it->second->attempts;
}

std::size_t LinkManager::peer_count(const std::string& endpoint) const {
    auto it = by_name_.find(endpoint);
    return it == by_name_.end() ? 0 : it->second->peers.size();
}

std::uint64_t LinkManager::handshake_failures(const std::string& endpoint) const {
    auto it = by_name_.find(endpoint);
    return it == by_name_.end() ? 0 : it->second->handshake_failures;
}

}  // namespace apollo::net::link

#endif  // !_WIN32
