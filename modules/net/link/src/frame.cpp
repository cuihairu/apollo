// 帧定界实现（net M1 内核 · 增量 1）。线形与容错分界见 frame.hpp 头注。

#include "apollo/net/link/frame.hpp"

#include <cstring>

namespace apollo::net::link {

namespace {

// 反射 CRC32C（Castagnoli 0x82F63B78），查表法，表惰性初始化一次。
struct Crc32cTable {
    std::uint32_t v[256];
    Crc32cTable() {
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1) ? (0x82F63B78u ^ (c >> 1)) : (c >> 1);
            }
            v[i] = c;
        }
    }
};

const Crc32cTable& crc_table() {
    static const Crc32cTable t;
    return t;
}

void put_u16(std::vector<std::uint8_t>& out, std::uint16_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
}

void put_u32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
}

std::uint16_t get_u16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0]) | (static_cast<std::uint16_t>(p[1]) << 8);
}

std::uint32_t get_u32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

}  // namespace

std::uint32_t crc32c(const std::uint8_t* data, std::size_t size) noexcept {
    const Crc32cTable& t = crc_table();
    std::uint32_t c = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < size; ++i) {
        c = t.v[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

std::uint32_t crc32c(const std::vector<std::uint8_t>& data) noexcept {
    return crc32c(data.data(), data.size());
}

bool is_control(const Frame& frame) noexcept { return (frame.flags & kFlagControl) != 0; }

void append_frame(std::vector<std::uint8_t>& out, std::uint32_t seq, std::uint8_t flags,
                  const std::uint8_t* payload, std::size_t size) {
    std::uint8_t header[kFrameHeaderSize];
    header[0] = static_cast<std::uint8_t>(kFrameMagic & 0xFF);
    header[1] = static_cast<std::uint8_t>(kFrameMagic >> 8);
    header[2] = kFrameVersion;
    header[3] = flags;
    header[4] = static_cast<std::uint8_t>(seq & 0xFF);
    header[5] = static_cast<std::uint8_t>((seq >> 8) & 0xFF);
    header[6] = static_cast<std::uint8_t>((seq >> 16) & 0xFF);
    header[7] = static_cast<std::uint8_t>((seq >> 24) & 0xFF);
    const std::uint32_t len = static_cast<std::uint32_t>(size);
    header[8] = static_cast<std::uint8_t>(len & 0xFF);
    header[9] = static_cast<std::uint8_t>((len >> 8) & 0xFF);
    header[10] = static_cast<std::uint8_t>((len >> 16) & 0xFF);
    header[11] = static_cast<std::uint8_t>((len >> 24) & 0xFF);
    header[12] = 0;
    header[13] = 0;
    const std::uint32_t hc = crc32c(header, 14);
    header[14] = static_cast<std::uint8_t>(hc & 0xFF);
    header[15] = static_cast<std::uint8_t>((hc >> 8) & 0xFF);

    out.insert(out.end(), header, header + kFrameHeaderSize);
    if (size > 0) {
        out.insert(out.end(), payload, payload + size);
    }
    const std::uint32_t pc = crc32c(payload, size);
    put_u32(out, pc);
}

ParseOutcome FrameParser::feed(const std::uint8_t* data, std::size_t size) {
    ParseOutcome out;
    buf_.insert(buf_.end(), data, data + size);

    std::size_t off = 0;
    while (true) {
        // 头不满：等更多字节。
        if (buf_.size() - off < kFrameHeaderSize) {
            break;
        }
        const std::uint8_t* h = buf_.data() + off;

        // 坏头：magic 或头校验不过 → 逐字节重同步。
        const bool magic_ok = get_u16(h) == kFrameMagic && h[2] == kFrameVersion;
        std::uint16_t hc_expect = 0;
        bool hcrc_ok = false;
        if (magic_ok) {
            hc_expect = static_cast<std::uint16_t>(crc32c(h, 14) & 0xFFFFu);
            hcrc_ok = get_u16(h + 14) == hc_expect;
        }
        if (!magic_ok || !hcrc_ok) {
            ++off;
            ++out.resync_dropped;
            continue;
        }

        const std::uint32_t len = get_u32(h + 8);
        if (len > kMaxPayload) {
            out.fatal = true;
            out.frames.clear();
            return out;
        }
        const std::size_t frame_size = kFrameHeaderSize + len + kFrameTailSize;
        if (buf_.size() - off < frame_size) {
            break;  // 整帧未到齐
        }

        const std::uint8_t* payload = h + kFrameHeaderSize;
        const std::uint32_t pcrc_expect = crc32c(payload, len);
        if (get_u32(payload + len) != pcrc_expect) {
            out.fatal = true;
            out.frames.clear();
            return out;
        }

        Frame f;
        f.flags = h[3];
        f.seq = get_u32(h + 4);
        f.payload.assign(payload, payload + len);
        out.frames.push_back(std::move(f));
        off += frame_size;
    }

    if (off > 0) {
        buf_.erase(buf_.begin(), buf_.begin() + static_cast<std::ptrdiff_t>(off));
    }
    return out;
}

void FrameParser::reset() { buf_.clear(); }

}  // namespace apollo::net::link
