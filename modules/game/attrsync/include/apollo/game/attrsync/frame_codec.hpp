#pragma once
//
// FrameCodec — AttrSyncFrame ↔ wire bytes（attribute-sync §7.1）。
//
// 编码布局（varint，零依赖）：
//   entity_id: varint(u64)
//   base_seq:  varint(u64)
//   flags:     varint(u8) — bit0 = is_snapshot
//   count:     varint(u32) — delta 条目数
//   deltas:    count × AttrDelta 编码（attr_id varint + value zigzag + change_n varint）
//
// 批量编码：count(varint) + count × frame 编码
//

#include <cstdint>
#include <optional>
#include <vector>

#include "apollo/game/attrsync/attr_delta.hpp"
#include "apollo/game/attrsync/delta_builder.hpp"

namespace apollo::game::attrsync {

[[nodiscard]] std::vector<std::uint8_t> encode_frame(const AttrSyncFrame& frame);
[[nodiscard]] std::optional<AttrSyncFrame> decode_frame(const std::vector<std::uint8_t>& data);

[[nodiscard]] std::vector<std::uint8_t> encode_batch(const std::vector<AttrSyncFrame>& frames);
[[nodiscard]] std::optional<std::vector<AttrSyncFrame>> decode_batch(const std::vector<std::uint8_t>& data);

}  // namespace apollo::game::attrsync
