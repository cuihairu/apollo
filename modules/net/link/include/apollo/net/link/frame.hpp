#pragma once

// 帧定界（net M1 内核 · 增量 1；net-abstraction §3 L1 帧格式 + §5.6 M1 行）。
//
// 同一套帧语义两路通用（§5.6 M0「同一套语义、同一个层」）：客户端路径与
// 进程间路径只差传输与拓扑，帧格式一份。
//
// 线形（16B 头 + 载荷 + 4B 尾校验，全部小端）：
//   magic u16 (0x5041, 字节序 'A''P')
//   ver   u8  (=1)
//   flags u8  (bit0 = control：链路自身帧 Hello/Ack/Ping/Pong；载荷帧为 0)
//   seq   u32 (链路内单调递增；对端按 epoch 去重)
//   len   u32 (载荷字节数，不含头与尾校验)
//   rsvd  u16 (=0)
//   hcrc  u16 (前 14 字节 CRC32C 截断低 16 位——护住 len，坏头可重同步)
//   payload len B
//   pcrc  u32 (载荷 CRC32C)
//
// 容错分界：头坏（magic/hcrc 不过）→ 逐字节重同步扫下一 magic；len 超上限
// 或尾校验不过 → 协议错误（Parser 结果带 fatal，链路层据此断连——TCP 流上
// 出现载荷级损坏只能是实现错，静默跳过会掩盖分界错位）。
//
// 载荷语义本层不解释：业务帧载荷 = u16 msg id + 载荷（offload_wire 信封
// 纪律同源，id 归契约 internal 域）；control 帧载荷由 InterServerLink 定。

#include <cstddef>
#include <cstdint>
#include <vector>

namespace apollo::net::link {

// CRC32C（Castagnoli，反射多项式 0x82F63B78）。
// 校验值：crc32c("123456789") == 0xE3069283。
std::uint32_t crc32c(const std::uint8_t* data, std::size_t size) noexcept;
std::uint32_t crc32c(const std::vector<std::uint8_t>& data) noexcept;

inline constexpr std::uint16_t kFrameMagic = 0x5041;  // 'A','P'
inline constexpr std::uint8_t kFrameVersion = 1;
inline constexpr std::size_t kFrameHeaderSize = 16;
inline constexpr std::size_t kFrameTailSize = 4;
inline constexpr std::uint32_t kFlagControl = 0x01;

// 载荷上限（进程间消息没有合法的超 1 MiB 单帧；超限按协议错误断连）。
inline constexpr std::uint32_t kMaxPayload = 1024 * 1024;

struct Frame {
    std::uint8_t flags = 0;
    std::uint32_t seq = 0;
    std::vector<std::uint8_t> payload;
};

bool is_control(const Frame& frame) noexcept;

// 编码一帧追加到 out。seq 由调用方（链路层）分配。
void append_frame(std::vector<std::uint8_t>& out, std::uint32_t seq, std::uint8_t flags,
                  const std::uint8_t* payload, std::size_t size);

inline void append_frame(std::vector<std::uint8_t>& out, std::uint32_t seq, std::uint8_t flags,
                         const std::vector<std::uint8_t>& payload) {
    append_frame(out, seq, flags, payload.data(), payload.size());
}

// 流式拆帧。feed 任意长度字节流，攒够整帧即产出；partial 留存到下次 feed。
struct ParseOutcome {
    std::vector<Frame> frames;
    // 协议错误（len 超限 / 尾校验不过）：true 时 frames 作废，链路必须断连。
    bool fatal = false;
    // 重同步丢弃的字节数（坏头扫描；观测面，不构成错误）。
    std::size_t resync_dropped = 0;
};

class FrameParser {
public:
    // 喂字节，吐完整帧。fatal=true 后解析器作废（重建即新链路语义）。
    ParseOutcome feed(const std::uint8_t* data, std::size_t size);
    ParseOutcome feed(const std::vector<std::uint8_t>& data) {
        return feed(data.data(), data.size());
    }

    std::size_t buffered() const { return buf_.size(); }
    void reset();

private:
    std::vector<std::uint8_t> buf_;
};

}  // namespace apollo::net::link
