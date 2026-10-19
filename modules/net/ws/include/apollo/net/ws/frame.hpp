#pragma once

// WebSocket 帧编解码（RFC 6455 §5；服务端面，纯编解码——零 socket 零
// 线程，增量 feed 语义同 link FrameParser：喂任意长度字节流，攒够整帧
// 即产出）。
//
// 方向语义：本解析器解析「客户端 → 服务端」帧，强制掩码（无掩码即
// 协议错误 fatal）；encode 产出「服务端 → 客户端」帧，不掩码（RFC §5.1）。
//
// 错误纪律与链路帧层一致：协议错即 fatal，语义 = 断连，无重同步
// （ws 流无 magic 锚点，静默跳过会永久错位）：
//   RSV 位非零 / 客户端帧无掩码 / 控制帧被分片 / 控制帧载荷 > 125 /
//   保留 opcode / 声明长度超上限 / 分片序列错乱（孤立 continuation、
//   分片中途换新 data 帧）/ close 载荷恰 1 字节。
//
// 控制帧不进入业务消息：ping → 回执 pong 排队（原载荷回）；pong →
// 静默受理；close → peer_closed 置位 + 回执 close 排队（echo code）。
// 业务帧（text/binary，含分片拼装结果）才进 messages——宿主据此把载荷
// 当作上层应用帧字节流喂给链路帧解析器。

#include <cstddef>
#include <cstdint>
#include <vector>

namespace apollo::net::ws {

enum class OpCode : std::uint8_t {
    kContinuation = 0x00,
    kText = 0x01,
    kBinary = 0x02,
    kClose = 0x08,
    kPing = 0x09,
    kPong = 0x0A,
};

struct Message {
    OpCode opcode = OpCode::kBinary;  // 分片消息取首片 opcode
    std::vector<std::uint8_t> payload;
};

// 消息上限（默认 2 MiB：链路帧上限 1 MiB + 20B 头尾余量 + 分片容忍）。
inline constexpr std::size_t kDefaultMaxMessage = 2 * 1024 * 1024;

class FrameParser {
public:
    explicit FrameParser(std::size_t max_message_bytes = kDefaultMaxMessage)
        : max_message_(max_message_bytes) {}

    struct Outcome {
        // 完整业务消息（分片拼装后）。
        std::vector<Message> messages;
        // 协议错误：true 时不得再信任本帧流，宿主必须断连。
        bool fatal = false;
        // 对端发起关闭：宿主 echo 回执（pending_control）后断。
        bool peer_closed = false;
        // 控制帧回执（pong / close echo），宿主原样发回对端。
        std::vector<std::uint8_t> pending_control;
    };

    Outcome feed(const std::uint8_t* data, std::size_t size);
    Outcome feed(const std::vector<std::uint8_t>& data) {
        return feed(data.data(), data.size());
    }

    std::size_t buffered() const { return buf_.size(); }
    void reset();

private:
    std::vector<std::uint8_t> buf_;
    bool in_fragment_ = false;
    OpCode first_opcode_ = OpCode::kBinary;
    std::vector<std::uint8_t> fragment_;
    std::size_t max_message_;
};

// 服务端出帧（FIN=1，无掩码）。支持 len7 / ext16 / ext64 全路径。
void append_frame(std::vector<std::uint8_t>& out, OpCode opcode,
                  const std::uint8_t* payload, std::size_t size);
inline void append_frame(std::vector<std::uint8_t>& out, OpCode opcode,
                         const std::vector<std::uint8_t>& payload) {
    append_frame(out, opcode, payload.data(), payload.size());
}

// close 回执（0x88 + code + 原因，无掩码）。
void append_close(std::vector<std::uint8_t>& out, std::uint16_t code,
                  const char* reason);

}  // namespace apollo::net::ws
