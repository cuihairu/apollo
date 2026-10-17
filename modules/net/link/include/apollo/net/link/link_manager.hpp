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
// passive fan-out（L4 collector 首个 1:N）：passive 端点 accept 出的每条
// 传输独立成链——握手前入 staging，握手完按对端自报名（Hello）迁入 peers
// （同名 = 对端重连替换旧链）。1:1 场景（cell 收 gateway）是 peers 恒一条
// 的特例，端点名 = 期望对端名时 link()/事件面与 fan-out 前逐字节同值。
// 消费者经 set_peer_listener 在新 peer 上线时装配分发（路由表/白名单/
// handler 挂到各 peer 链路——链路对象地址此后稳定）。
//
// 零内部线程：pump(now_ms) 一拍驱动全部端点（连接器/监听器/链路共用节拍）。

#include "apollo/net/link/interserver_link.hpp"
#include "apollo/net/link/tcp_transport.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
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
    // N-1 兼容窗口（sdk-contract §6）：逐链下发到各 LinkConfig（active 与
    // passive fan-out 同源）。空 = 无窗。
    std::vector<std::uint64_t> compat_route_versions;
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

    // 管理器事件面（对上层开放，§5.7 三回调的具名端点投影）。active 端点
    // name = 端点名；passive fan-out 后 name = 对端自报名（1:1 同名同值）。
    void on_up(std::function<void(const std::string& name)> fn) { up_fn_ = std::move(fn); }
    void on_down(std::function<void(const std::string& name, LinkDownReason reason)> fn) {
        down_fn_ = std::move(fn);
    }
    void on_peer_restart(
        std::function<void(const std::string& name, std::uint64_t old_epoch,
                           std::uint64_t new_epoch)> fn) {
        restart_fn_ = std::move(fn);
    }

    // 新 peer 上线装配位（passive fan-out 消费面）：管理器泵线程在 peer 链
    // kUp 后、置入 peers 前回调——消费者在此挂路由表/白名单/handler。链路
    // 对象地址自此稳定（同名替换 = 旧对象析构、新对象新回调）。
    void set_peer_listener(std::function<void(InterServerLink& peer)> fn) {
        peer_fn_ = std::move(fn);
    }

    // 编队事件（G-1 联动）。now_ms 显式入参——事件时点归调用方，管理器
    // 不自取时钟（与 pump 注入同纪律）。
    void notify_fleet_down(const std::string& name, std::uint64_t now_ms);
    void notify_fleet_up(const std::string& name, std::uint64_t now_ms);

    // 宿主泵：全部端点一拍（拨号状态机 / accept / 链路 pump）。
    void pump(std::uint64_t now_ms);

    // 链路查取：active 端点名 → 本端链路；passive → 该对端链路（按对端
    // 自报名直查，单 peer 时端点名兜底——1:1 场景二者同值，与 fan-out 前
    // 语义连续）。未上线返回 nullptr。
    InterServerLink* link(const std::string& name);
    std::uint16_t endpoint_port(const std::string& name) const;  // passive 实占端口

    // 观测面：拨号尝试计数（退避测试与运维可见性共用一口）；passive 端点
    // 在册对端数与握手期失败数（魔数/版本失配/握手超时——装配级故障显形）。
    std::uint64_t dial_attempts(const std::string& name) const;
    std::size_t peer_count(const std::string& endpoint) const;
    std::uint64_t handshake_failures(const std::string& endpoint) const;

private:
    struct Managed {
        EndpointConfig config;
        LinkConfig link_config;  // 链路装配参数（self/route_version/epoch 共用）
        // active 面单链路（passive 的链路族群在 staging/peers）
        std::unique_ptr<InterServerLink> link;
        // active 面
        TcpConnector connector;
        bool dialing = false;          // connector 在途
        bool down_pending = false;     // 断连待排程（泵拍取 now 排 initial 退避）
        std::uint64_t next_dial_ms = 0;  // 0 = 立即可拨
        std::uint32_t consecutive_failures = 0;
        std::uint64_t attempts = 0;
        // passive fan-out 面：握手前 staging，握手完按对端名迁入 peers
        std::vector<std::unique_ptr<InterServerLink>> staging;
        std::unordered_map<std::string, std::unique_ptr<InterServerLink>> peers;
        TcpListener listener;
        std::uint64_t handshake_failures = 0;
        // 编队
        bool fleet_up = true;
        Managed(const EndpointConfig& cfg, const LinkManagerConfig& mc);
    };

    void wire_sink(Managed& m);
    void wire_peer_sink(InterServerLink& peer);
    void pump_active(Managed& m, std::uint64_t now_ms);
    void pump_passive(Managed& m, std::uint64_t now_ms);
    void schedule_backoff(Managed& m, std::uint64_t now_ms);
    // 竞速窗兜底：staging 里握手完毕（kUp）未迁册的同名链（link() 用）。
    InterServerLink* find_staging_peer(Managed& m, const std::string& name);

    LinkManagerConfig config_;
    std::vector<std::unique_ptr<Managed>> endpoints_;  // 保序（泵次序确定）
    std::unordered_map<std::string, Managed*> by_name_;

    std::function<void(const std::string&)> up_fn_;
    std::function<void(const std::string&, LinkDownReason)> down_fn_;
    std::function<void(const std::string&, std::uint64_t, std::uint64_t)> restart_fn_;
    std::function<void(InterServerLink&)> peer_fn_;  // 新 peer 上线装配位（fan-out）
};

}  // namespace apollo::net::link

#endif  // !_WIN32
