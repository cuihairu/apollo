#include "apollo/game/attrsync/frame_codec.hpp"

#include <utility>

namespace apollo::game::attrsync {

using apollo::game::attrsync::encode_varint;
using apollo::game::attrsync::decode_varint;

namespace {

void append_varint(std::vector<std::uint8_t>& out, std::uint64_t value) {
    auto bytes = encode_varint(value);
    out.insert(out.end(), bytes.begin(), bytes.end());
}

// 单帧解码（游标式）；成功返回消耗字节数
std::optional<std::size_t> decode_frame_into(const std::vector<std::uint8_t>& data,
                                              std::size_t offset,
                                              AttrSyncFrame& out) {
    std::size_t pos = offset;

    // entity_id
    auto r = decode_varint(data, pos);
    if (!r) return std::nullopt;
    out.entity_id = r->first;
    pos += r->second;

    // base_seq
    r = decode_varint(data, pos);
    if (!r) return std::nullopt;
    out.base_seq = r->first;
    pos += r->second;

    // flags
    r = decode_varint(data, pos);
    if (!r) return std::nullopt;
    out.is_snapshot = (r->first & 0x01) != 0;
    pos += r->second;

    // count
    r = decode_varint(data, pos);
    if (!r) return std::nullopt;
    auto count = r->first;
    pos += r->second;

    // deltas
    out.deltas.clear();
    out.deltas.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t i = 0; i < count; ++i) {
        AttrDelta delta;
        auto consumed = AttrDelta::decode_into(data, pos, delta);
        if (!consumed) return std::nullopt;
        pos += *consumed;
        out.deltas.push_back(std::move(delta));
    }

    return pos - offset;
}

}  // namespace

std::vector<std::uint8_t> encode_frame(const AttrSyncFrame& frame) {
    std::vector<std::uint8_t> out;
    append_varint(out, frame.entity_id);
    append_varint(out, frame.base_seq);
    append_varint(out, frame.is_snapshot ? 1 : 0);
    append_varint(out, frame.deltas.size());
    for (const auto& d : frame.deltas) {
        auto bytes = d.encode();
        out.insert(out.end(), bytes.begin(), bytes.end());
    }
    return out;
}

std::optional<AttrSyncFrame> decode_frame(const std::vector<std::uint8_t>& data) {
    if (data.empty()) {
        return std::nullopt;
    }
    AttrSyncFrame frame;
    auto consumed = decode_frame_into(data, 0, frame);
    if (!consumed || *consumed != data.size()) {
        return std::nullopt;  // truncated or trailing garbage
    }
    return frame;
}

std::vector<std::uint8_t> encode_batch(const std::vector<AttrSyncFrame>& frames) {
    std::vector<std::uint8_t> out;
    append_varint(out, frames.size());
    for (const auto& f : frames) {
        auto bytes = encode_frame(f);
        out.insert(out.end(), bytes.begin(), bytes.end());
    }
    return out;
}

std::optional<std::vector<AttrSyncFrame>> decode_batch(const std::vector<std::uint8_t>& data) {
    if (data.empty()) {
        return std::nullopt;
    }
    auto r = decode_varint(data, 0);
    if (!r) return std::nullopt;
    auto count = r->first;
    std::size_t pos = r->second;

    std::vector<AttrSyncFrame> frames;
    frames.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t i = 0; i < count; ++i) {
        AttrSyncFrame frame;
        auto consumed = decode_frame_into(data, pos, frame);
        if (!consumed) return std::nullopt;
        pos += *consumed;
        frames.push_back(std::move(frame));
    }
    if (pos != data.size()) {
        return std::nullopt;  // trailing garbage
    }
    return frames;
}

}  // namespace apollo::game::attrsync
