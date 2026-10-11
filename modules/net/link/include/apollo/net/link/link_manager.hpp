#pragma once

// Link 管理器（net M1 内核 · 增量 5；net-abstraction §5.7——统一重连面：
// 指数退避 + 编队事件联动，反面教材 BW LoggerEndpoint 每消费者自写硬编码
// 重连——apollo 收敛一处，消费者只收事件不写策略）。
//
// 职责分界：连接器/监听器是机制（失败即类型化回调，不带重试）；本类是
// 策略（何时重拨、退避多长、编队死亡停拨）。每具名端点一条 InterServerLink
// + 一条退避状态；拨号/接受角色配置态写死（§5.7：不做运行期协商）。
//
// 退避：指数（initial ×2 封顶 max），只在连续拨号失败上翻倍；连上即复位；
// 断连后的重拨从 initial 起（断连 ≠ 拨号失败，混算会罚健康链路）。
// 编队联动（G-1）：死亡通知 → 该端点停拨 + 取消在途连接器；复活通知 →
// 复位退避 + 立即重拨（死了的进程不拨，复活即抢跑）。
//
// 零内部线程：pump(now_ms) 一拍驱动全部端点（连接器/监听器/链路共用节拍）。

#include "apollo/net/link/interserver_link.hpp"
#include "apollo/net/link/tcp_transport.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#if !defined(_WIN32)

namespace apollo::net::link {

enum class EndpointRole {
    kActive,   // 拨号方（配置态写死，§5.7）
    kPassive,  // 监听方（bind/listen 在装配期，accept 在泵期）
};

struct EndpointConfig {
    std::string name;  // G-1 组件名（握手身份 = Link self_name）
    std::string host;
    std::uint16_t port = 0;  // passive 可 0 = 内核分配（port() 查实际）
    EndpointRole role = EndpointRole::kActive;
    std::uint32_t connect_timeout_ms = 3000;
};

struct LinkManagerConfig {
    std::string self_name;            // 本进程身份（各 Link 的 self_name 同值）
    std::uint64_t route_version = 0;  // 握手比对（全端点一致）
    std::uint64_t authority_epoch = 1;
    std::uint32_t backoff_initial_ms = 500;
    std::uint32_t backoff_max_ms = 15000;
};

class LinkManager {
public:
    explicit LinkManager(LinkManagerConfig config);
    ~LinkManager() = default;

    LinkManager(const LinkManager&) = delete;
    LinkManager& operator=(const LinkManager&) = delete;

    // 装配（配置态）。passive 即刻 bind/listen——失败返回 false（配置错误
    // 装配期显形，不进运行期）；active 首拨在首个 pump 拍。
    bool add_endpoint(const EndpointConfig& endpoint);

    // 管理器事件面（对上层开放，§5.7 三回调的具名端点投影）。
    void on_up(std::function<void(const std::string& name)> fn) { up_fn_ = std::move(fn); }
    void on_down(std::function<void(const std::string& name, LinkDownReason reason)> fn) {
        down_fn_ = std::move(fn);
    }
    void on_peer_restart(
        std::function<void(const std::string& name, std::uint64_t old_epoch,
                           std::uint64_t new_epoch)> fn) {
        restart_fn_ = std::move(fn);
    }

    // 编队事件（G-1 联动）。now_ms 显式入参——事件时点归调用方，管理器
    // 不自取时钟（与 pump 注入同纪律）。
    void notify_fleet_down(const std::string& name, std::uint64_t now_ms);
    void notify_fleet_up(const std::string& name, std::uint64_t now_ms);

    // 宿主泵：全部端点一拍（拨号状态机 / accept / 链路 pump）。
    void pump(std::uint64_t now_ms);

    InterServerLink* link(const std::string& name);
    std::uint16_t endpoint_port(const std::string& name) const;  // passive 实占端口

    // 观测面：拨号尝试计数（退避测试与运维可见性共用一口）。
    std::uint64_t dial_attempts(const std::string& name) const;

private:
    struct Managed {
        EndpointConfig config;
        InterServerLink link;
        // active 面
        TcpConnector connector;
        bool dialing = false;          // connector 在途
        bool down_pending = false;     // 断连待排程（泵拍取 now 排 initial 退避）
        std::uint64_t next_dial_ms = 0;  // 0 = 立即可拨
        std::uint32_t consecutive_failures = 0;
        std::uint64_t attempts = 0;
        // passive 面
        TcpListener listener;
        // 编队
        bool fleet_up = true;
        Managed(const EndpointConfig& cfg, const LinkManagerConfig& mc);
    };

    void wire_sink(Managed& m);
    void pump_active(Managed& m, std::uint64_t now_ms);
    void schedule_backoff(Managed& m, std::uint64_t now_ms);

    LinkManagerConfig config_;
    std::vector<std::unique_ptr<Managed>> endpoints_;  // 保序（泵次序确定）
    std::unordered_map<std::string, Managed*> by_name_;

    std::function<void(const std::string&)> up_fn_;
    std::function<void(const std::string&, LinkDownReason)> down_fn_;
    std::function<void(const std::string&, std::uint64_t, std::uint64_t)> restart_fn_;
};

}  // namespace apollo::net::link

#endif  // !_WIN32
