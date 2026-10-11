#include "apollo/net/link/link_manager.hpp"

#include <algorithm>

#if !defined(_WIN32)

namespace apollo::net::link {

LinkManager::Managed::Managed(const EndpointConfig& cfg, const LinkManagerConfig& mc)
    : config(cfg), link([&] {
          LinkConfig lc;
          lc.self_name = mc.self_name;
          lc.route_version = mc.route_version;
          lc.authority_epoch = mc.authority_epoch;
          return lc;
      }()) {}

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
    m.link.set_sink(std::move(sink));
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
                // accept 出的新传输直接接管（对端重拨场景：attach 内部完成
                // 旧连接收尾——同 epoch 保可靠队列，新连接重握手）。
                while (auto transport = m.listener.accept()) {
                    m.link.attach(std::move(transport), now_ms);
                }
                m.link.pump(now_ms);
                break;
            }
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
            m.link.attach(m.connector.take_transport(), now_ms);
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
        m.link.state() == LinkState::kUp || m.link.state() == LinkState::kHandshaking;
    if (m.dialing || link_live) {
        m.link.pump(now_ms);
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

InterServerLink* LinkManager::link(const std::string& name) {
    auto it = by_name_.find(name);
    return it == by_name_.end() ? nullptr : &it->second->link;
}

std::uint16_t LinkManager::endpoint_port(const std::string& name) const {
    auto it = by_name_.find(name);
    return it == by_name_.end() ? 0 : it->second->listener.port();
}

std::uint64_t LinkManager::dial_attempts(const std::string& name) const {
    auto it = by_name_.find(name);
    return it == by_name_.end() ? 0 : it->second->attempts;
}

}  // namespace apollo::net::link

#endif  // !_WIN32
