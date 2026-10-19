#include "apollo/net/ws/frame.hpp"

#include <cstring>

namespace apollo::net::ws {

namespace {

// 帧总字节数：header 已含 2B 头与 [2|8]B 扩展长 + 4B 掩码 + 载荷。
std::size_t frame_total_size(std::size_t header, std::uint64_t declared_len) {
    return header + 4 + static_cast<std::size_t>(declared_len);
}

}  // namespace

void append_frame(std::vector<std::uint8_t>& out, OpCode opcode,
                  const std::uint8_t* payload, std::size_t size) {
    out.push_back(0x80 | static_cast<std::uint8_t>(opcode));  // FIN=1
    if (size < 126) {
        out.push_back(static_cast<std::uint8_t>(size));
    } else if (size <= 0xFFFF) {
        out.push_back(126);
        out.push_back(static_cast<std::uint8_t>((size >> 8) & 0xFF));
        out.push_back(static_cast<std::uint8_t>(size & 0xFF));
    } else {
        out.push_back(127);
        for (int shift = 56; shift >= 0; shift -= 8) {
            out.push_back(static_cast<std::uint8_t>((size >> shift) & 0xFF));
        }
    }
    out.insert(out.end(), payload, payload + size);
}

void append_close(std::vector<std::uint8_t>& out, std::uint16_t code,
                  const char* reason) {
    const std::size_t reason_len = reason != nullptr ? std::strlen(reason) : 0;
    const std::size_t payload_len = 2 + reason_len;
    out.push_back(0x88);  // FIN=1 + close
    out.push_back(static_cast<std::uint8_t>(payload_len));
    out.push_back(static_cast<std::uint8_t>((code >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(code & 0xFF));
    if (reason_len > 0) {
        out.insert(out.end(), reason, reason + reason_len);
    }
}

void FrameParser::reset() {
    buf_.clear();
    in_fragment_ = false;
    first_opcode_ = OpCode::kBinary;
    fragment_.clear();
}

FrameParser::Outcome FrameParser::feed(const std::uint8_t* data, std::size_t size) {
    Outcome out;
    buf_.insert(buf_.end(), data, data + size);

    std::size_t offset = 0;
    while (!out.fatal && !out.peer_closed && buf_.size() - offset >= 2) {
        const std::uint8_t b0 = buf_[offset];
        const std::uint8_t b1 = buf_[offset + 1];
        if ((b0 & 0x70) != 0) {
            out.fatal = true;  // RSV 位非零：未协商扩展
            break;
        }
        const bool fin = (b0 & 0x80) != 0;
        const auto opcode = static_cast<OpCode>(b0 & 0x0F);
        const bool masked = (b1 & 0x80) != 0;
        const std::size_t len7 = b1 & 0x7F;
        const bool is_control = (b0 & 0x08) != 0;

        if (!masked) {
            out.fatal = true;  // 客户端→服务端帧必须掩码（RFC §5.1）
            break;
        }
        if (is_control && (!fin || len7 > 125)) {
            out.fatal = true;  // 控制帧不碎片化且载荷 ≤ 125（RFC §5.5）
            break;
        }

        std::uint64_t declared_len = len7;
        std::size_t header = 2;
        if (len7 == 126) {
            if (buf_.size() - offset < 4) {
                break;  // 扩展长未齐
            }
            declared_len = (static_cast<std::uint64_t>(buf_[offset + 2]) << 8) |
                           buf_[offset + 3];
            header = 4;
        } else if (len7 == 127) {
            if (buf_.size() - offset < 10) {
                break;
            }
            declared_len = 0;
            for (int i = 0; i < 8; ++i) {
                declared_len = (declared_len << 8) | buf_[offset + 2 + i];
            }
            header = 10;
            if ((declared_len >> 63) != 0) {
                out.fatal = true;  // 最高位必须为 0（RFC §5.2）
                break;
            }
        }
        if (declared_len > max_message_) {
            out.fatal = true;  // 声明超限：拒绝在读入前（不无界攒缓冲）
            break;
        }

        const std::size_t total = frame_total_size(header, declared_len);
        if (buf_.size() - offset < total) {
            break;  // 载荷未齐
        }

        std::uint8_t* payload = buf_.data() + offset + header + 4;
        const std::uint8_t* mask = payload - 4;
        for (std::uint64_t i = 0; i < declared_len; ++i) {
            payload[i] ^= mask[i & 3];  // 原地解掩码
        }
        const std::size_t payload_size = static_cast<std::size_t>(declared_len);

        if (is_control) {
            switch (opcode) {
                case OpCode::kPing:
                    // 回执 pong：无掩码原载荷回（RFC §5.5.3）。
                    append_frame(out.pending_control, OpCode::kPong, payload, payload_size);
                    break;
                case OpCode::kPong:
                    break;  // 静默受理
                case OpCode::kClose:
                    if (payload_size == 1) {
                        out.fatal = true;  // close 载荷恰 1 字节非法
                        break;
                    }
                    // echo close（code 与原因原样回）。
                    out.pending_control.push_back(0x88);
                    out.pending_control.push_back(static_cast<std::uint8_t>(payload_size));
                    out.pending_control.insert(out.pending_control.end(), payload,
                                               payload + payload_size);
                    out.peer_closed = true;
                    break;
                default:
                    out.fatal = true;  // 保留控制 opcode
                    break;
            }
        } else {
            const bool valid_data =
                opcode == OpCode::kText || opcode == OpCode::kBinary ||
                opcode == OpCode::kContinuation;
            if (!valid_data) {
                out.fatal = true;  // 保留 data opcode
                break;
            }
            if (in_fragment_) {
                if (opcode != OpCode::kContinuation) {
                    out.fatal = true;  // 分片中途出现新 data 帧
                    break;
                }
                fragment_.insert(fragment_.end(), payload, payload + payload_size);
            } else {
                if (opcode == OpCode::kContinuation) {
                    out.fatal = true;  // 孤立 continuation
                    break;
                }
                fragment_.assign(payload, payload + payload_size);
                first_opcode_ = opcode;
            }
            if (fin) {
                Message message;
                message.opcode = first_opcode_;
                message.payload = std::move(fragment_);
                fragment_.clear();
                in_fragment_ = false;
                out.messages.push_back(std::move(message));
            } else {
                in_fragment_ = true;
            }
        }
        offset += total;
    }

    if (offset > 0) {
        buf_.erase(buf_.begin(), buf_.begin() + static_cast<std::ptrdiff_t>(offset));
    }
    if (out.fatal) {
        // 语义 = 断连：帧流不可信，残留缓冲作废。
        buf_.clear();
    }
    return out;
}

}  // namespace apollo::net::ws
