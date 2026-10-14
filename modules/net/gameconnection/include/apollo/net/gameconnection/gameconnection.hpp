#pragma once
//
// GameConnection — L3 逻辑门面（net-abstraction.md §2/§3）。
//
// 游戏逻辑唯一入口：send/subscribe/state/close，隐藏链路建立/握手/帧定界/
// 心跳/seq-ack/水位/跨线程迁移。本批交付 L3 适配层（API 映射 + 泵透传），
// L2 会话增量（四通道队列/TRIMMED_LOW/Resuming/resume 回调/场景线程投递）
// 留后续批。
//

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "apollo/net/link/dispatcher.hpp"
#include "apollo/net/link/interserver_link.hpp"
#include "apollo/net/link/transport.hpp"

namespace apollo::net::gameconnection {

// 发送通道（映射 link SendClass；kControl 链路保留，外部不可用）
enum class Channel {
    kReliable,   // 不可丢（RemoteEntityCall 默认、attributes 族）
    kDroppable,  // 可丢声明（镜像流丢旧等）
};

// 通道内优先级（预留：TRIMMED_LOW 裁剪语义留 L2 批）
enum class Priority {
    kLow,
    kNormal,
    kHigh,
};

// 会话状态（映射 link LinkState；Resuming 留 L2 批）
enum class SessionState {
    kHandshaking,  // 传输已挂，握手未完成
    kActive,       // 握手完成，业务可收发
    kClosed,       // 断连（含 kIdle 未挂传输）
};

// 发送返回码（映射 link SendCode；TRIMMED_LOW 留 L2 批）
enum class SendCode {
    kAccepted,
    kBackPressured,
};

class GameConnection {
public:
    explicit GameConnection(apollo::net::link::LinkConfig config);

    // 挂已连传输（连接器/监听器产出）
    void attach(std::unique_ptr<apollo::net::link::ILinkTransport> transport,
                std::uint64_t now_ms);

    // 宿主泵：读传输 → 拆帧 → 分发；刷发送队列；心跳与 cut 计时
    bool pump(std::uint64_t now_ms);

    // 发送入口。channel 决定可靠/易弃；priority 预留（本批不参与裁剪）。
    SendCode send(std::uint16_t msg_id, const std::vector<std::uint8_t>& payload,
                  Channel ch, Priority p);
    SendCode send(std::uint16_t msg_id, const std::uint8_t* payload, std::size_t size,
                  Channel ch, Priority p);

    // 订阅（handler 在 pump 线程同步调用——场景线程投递留 L2 批）
    void subscribe(std::uint16_t msg_id, apollo::net::link::Dispatcher::Handler handler);

    // 路由表与域白名单（装配期注入；不装 = 默认开放）
    void set_route_table(const apollo::net::link::RouteTable* table);
    void allow_domain(std::string domain);

    SessionState state() const;

    // 主动断连
    void close(const std::string& reason);

    // 内部访问（测试用）
    apollo::net::link::InterServerLink& link() { return link_; }
    const apollo::net::link::InterServerLink::Stats& stats() const { return link_.stats(); }

private:
    apollo::net::link::InterServerLink link_;
};

}  // namespace apollo::net::gameconnection
