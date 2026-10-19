#pragma once

// WebSocket 升级握手（net M1 内核之上的纯编解码件——零 socket 零线程，
// 传输与生命周期归宿主泵；gateway 游戏入口与 interfaces HTTP 升级路由
// 各自驱动本件，不在本件内置连接模型）。
//
// RFC 6455 §4.2 服务端面：增量喂入升级请求字节，攒到头部终止
// （\r\n\r\n）即就绪；解析只提取字段，合法性判据独立
// （is_valid_upgrade）——解析成功 ≠ 升级合法，非法升级由调用方以
// make_reject_response 显式拒绝（升级路由上不降级成普通 HTTP）。
// 应答只产字节：发出去归传输层（TCP/TLS 同一缝）。

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace apollo::net::ws {

// Sec-WebSocket-Accept（RFC 6455 §4.2.2：SHA-1(key + GUID) 的 base64）。
// 空 key 或摘要失败返回空串——不产出假应答。
std::string compute_accept_key(const std::string& client_key);

struct UpgradeRequest {
    std::string method;             // 请求行第一段（须 GET）
    std::string path;               // 请求行第二段（升级路由判据）
    std::string key;                // Sec-WebSocket-Key
    int version = 0;                // Sec-WebSocket-Version（须 13）
    bool upgrade_websocket = false; // Upgrade 头值含 websocket（大小写不敏感）
    bool connection_upgrade = false;  // Connection 头 token 列表含 upgrade
};

class HandshakeParser {
public:
    enum class State { kNeedMore, kReady, kBad };

    // 增量喂入：攒到头部终止即 kReady（后续字节归帧层——本解析器只吃
    // 头部）；超上限或请求行/头部行不可解析 → kBad。
    State feed(const std::uint8_t* data, std::size_t size);
    State feed(const std::vector<std::uint8_t>& data) {
        return feed(data.data(), data.size());
    }

    // 仅 kReady 后有效。
    const UpgradeRequest& request() const { return request_; }

    void reset();

private:
    bool parse_buffer();

    std::vector<std::uint8_t> buf_;
    UpgradeRequest request_;
    bool done_ = false;
};

// 升级合法性判据（RFC 6455 §4.2.1）：GET、24 位 base64 key、版本 13、
// Upgrade: websocket、Connection 含 upgrade token。解析层字段齐全 +
// 本判据通过 = 可受理升级。
bool is_valid_upgrade(const UpgradeRequest& request);

// 101 应答（含算出的 Sec-WebSocket-Accept）。
std::vector<std::uint8_t> make_accept_response(const std::string& client_key);

// 400 拒绝应答（升级路由上不降级：拒绝即断连语义，body 带原因）。
std::vector<std::uint8_t> make_reject_response(const std::string& reason);

}  // namespace apollo::net::ws
