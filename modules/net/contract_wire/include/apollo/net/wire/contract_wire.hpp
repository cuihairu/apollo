#pragma once

// 契约 wire codec（M1 上层接线批 · 增量 i；net-abstraction §5.7 信封纪律：
// 业务帧载荷 = u16 LE msg id + 载荷，载荷语义本层之前不解释——本类是首个
// 解释层：按生成头 MsgMeta 的字段序与型做平铺 LE 编解码）。
//
// 线形（无 tag，schema 静态——双方从同一生成头取字段表，帧内不带布局）：
//   Bool        1B 0/1
//   Int32       4B LE（编码校验值域，越界即编码失败）
//   Int64       8B LE
//   Float       4B IEEE754 LE
//   Double      8B IEEE754 LE
//   String      u32 LE 长度 + utf8 字节
//   Bytes       u32 LE 长度 + 原始字节
//   Int64List   u32 LE 元素数 + n×8B LE
//   StringList  u32 LE 元素数 + 逐元素（u32 LE 长度 + 字节）
//
// 机制/事实源分界：字段布局的唯一事实源 = sdks/cpp/generated 生成物
// （kMessages 表）——契约变更 codec 自动跟随，无第二份布局知识；调用方
// 按 id 给字段值序列（顺序 = 表序）。编码失败（id 不在表 / 字段数或型
// 不符）不产帧；解码失败（字节流与布局不符）= 字段级非法，归 handler
// 拒收计数（dispatcher.hpp 口径——帧级 fatal 才断连）。

#include "apollo_contract.h"

#include <cstdint>
#include <string>
#include <vector>

namespace apollo::net::wire {

// 九型 WireType 的统一字段值（按型取用对应成员；编码时按表的型校验）。
struct FieldValue {
    std::int64_t integer = 0;              // Bool/Int32/Int64
    double real = 0;                       // Float/Double
    std::string text;                      // String
    std::vector<std::uint8_t> bytes;       // Bytes
    std::vector<std::int64_t> integers;    // Int64List
    std::vector<std::string> strings;      // StringList
};

// id → 生成头字段表查询（不在契约 = nullptr）。观测/预检共用一口。
const apollo::contract::gen::MsgMeta* find_msg(std::uint16_t msg_id);

// 按字段表编码载荷。成功置 out（无字段消息 = 空载荷）；失败返回 false
// 且 out 清空。
bool encode_payload(std::uint16_t msg_id, const std::vector<FieldValue>& fields,
                    std::vector<std::uint8_t>& out);

// 载荷解码（id 已剥出后的部分；归一于 Dispatcher Handler 的入参视图）。
// false = id 不在契约 / 字段级非法 / 尾部余字节。
bool decode_payload(std::uint16_t msg_id, const std::uint8_t* data, std::size_t size,
                    std::vector<FieldValue>& fields);

// u16 LE id + 载荷（业务帧载荷信封的线形视图，与 decode_envelope 对偶）。
// 注意：InterServerLink::send(msg_id, payload, …) 自封此信封——传裸载荷，
// 勿把本函数产物再喂给 send（双信封 = 对端剥一层后仍见 id 前缀）。
std::vector<std::uint8_t> append_envelope(std::uint16_t msg_id,
                                          const std::vector<std::uint8_t>& payload);

// 信封解出（id 剥出 + 按字段表解码载荷）。false = 信封残缺 / id 不在
// 契约 / 字段级非法。
bool decode_envelope(const std::uint8_t* data, std::size_t size, std::uint16_t& msg_id,
                     std::vector<FieldValue>& fields);

}  // namespace apollo::net::wire
