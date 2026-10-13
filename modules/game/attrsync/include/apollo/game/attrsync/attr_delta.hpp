#pragma once
//
// AttrDelta — 属性差分条目编码（attribute-sync §7.1）。
//
// 线上编码布局（varint/zigzag）：
//   attr_id: varint（u32）
//   value:   zigzag varint（s64）
//   change_n: varint（u32，合并次数，诊断用）
//
// 不引 protobuf——自写最小 varint/zigzag 编解码，零依赖。
//

#include <cstdint>
#include <optional>
#include <vector>

namespace apollo::game::attrsync {

struct AttrDelta {
    std::uint32_t attr_id = 0;
    std::int64_t value = 0;
    std::uint32_t change_n = 0;

    // 编码为字节串
    [[nodiscard]] std::vector<std::uint8_t> encode() const;

    // 从字节串解码（失败返回 nullopt）
    [[nodiscard]] static std::optional<AttrDelta> decode(const std::vector<std::uint8_t>& data);

    // 从字节串解码（游标式，返回消耗字节数；失败返回 nullopt）
    [[nodiscard]] static std::optional<std::size_t> decode_into(const std::vector<std::uint8_t>& data,
                                                                 std::size_t offset,
                                                                 AttrDelta& out);
};

// varint/zigzag 原语（公开供测试与其他模块复用）
[[nodiscard]] std::vector<std::uint8_t> encode_varint(std::uint64_t value);
[[nodiscard]] std::optional<std::pair<std::uint64_t, std::size_t>> decode_varint(
    const std::vector<std::uint8_t>& data, std::size_t offset);

[[nodiscard]] std::vector<std::uint8_t> encode_zigzag(std::int64_t value);
[[nodiscard]] std::int64_t decode_zigzag(std::uint64_t value);

}  // namespace apollo::game::attrsync
