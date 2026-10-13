#include "apollo/game/attrsync/attr_delta.hpp"

#include <utility>

namespace apollo::game::attrsync {

// ── varint / zigzag primitives ─────────────────────────────────────

std::vector<std::uint8_t> encode_varint(std::uint64_t value) {
    std::vector<std::uint8_t> out;
    while (value >= 0x80) {
        out.push_back(static_cast<std::uint8_t>(value & 0x7F) | 0x80);
        value >>= 7;
    }
    out.push_back(static_cast<std::uint8_t>(value));
    return out;
}

std::optional<std::pair<std::uint64_t, std::size_t>> decode_varint(
    const std::vector<std::uint8_t>& data, std::size_t offset) {
    if (offset >= data.size()) {
        return std::nullopt;
    }
    std::uint64_t result = 0;
    std::size_t shift = 0;
    std::size_t pos = offset;
    while (pos < data.size()) {
        auto byte = data[pos++];
        result |= static_cast<std::uint64_t>(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) {
            return std::make_pair(result, pos - offset);
        }
        shift += 7;
        if (shift >= 64) {
            return std::nullopt;  // overflow
        }
    }
    return std::nullopt;  // truncated
}

std::vector<std::uint8_t> encode_zigzag(std::int64_t value) {
    // Zigzag: negative numbers mapped to odd positive, positive to even
    auto encoded = static_cast<std::uint64_t>((value << 1) ^ (value >> 63));
    return encode_varint(encoded);
}

std::int64_t decode_zigzag(std::uint64_t value) {
    return static_cast<std::int64_t>((value >> 1) ^ (~(value & 1) + 1));
}

// ── AttrDelta encode / decode ──────────────────────────────────────

std::vector<std::uint8_t> AttrDelta::encode() const {
    std::vector<std::uint8_t> out;
    auto attr_bytes = encode_varint(attr_id);
    auto value_bytes = encode_zigzag(value);
    auto change_bytes = encode_varint(change_n);
    out.insert(out.end(), attr_bytes.begin(), attr_bytes.end());
    out.insert(out.end(), value_bytes.begin(), value_bytes.end());
    out.insert(out.end(), change_bytes.begin(), change_bytes.end());
    return out;
}

std::optional<AttrDelta> AttrDelta::decode(const std::vector<std::uint8_t>& data) {
    AttrDelta delta;
    auto consumed = decode_into(data, 0, delta);
    if (!consumed.has_value()) {
        return std::nullopt;
    }
    return delta;
}

std::optional<std::size_t> AttrDelta::decode_into(const std::vector<std::uint8_t>& data,
                                                    std::size_t offset,
                                                    AttrDelta& out) {
    // attr_id
    auto attr_result = decode_varint(data, offset);
    if (!attr_result.has_value()) {
        return std::nullopt;
    }
    auto [attr_id, attr_len] = *attr_result;
    out.attr_id = static_cast<std::uint32_t>(attr_id);
    offset += attr_len;

    // value (zigzag)
    auto value_result = decode_varint(data, offset);
    if (!value_result.has_value()) {
        return std::nullopt;
    }
    auto [value_encoded, value_len] = *value_result;
    out.value = decode_zigzag(value_encoded);
    offset += value_len;

    // change_n
    auto change_result = decode_varint(data, offset);
    if (!change_result.has_value()) {
        return std::nullopt;
    }
    auto [change_n, change_len] = *change_result;
    out.change_n = static_cast<std::uint32_t>(change_n);
    offset += change_len;

    return offset;
}

}  // namespace apollo::game::attrsync
