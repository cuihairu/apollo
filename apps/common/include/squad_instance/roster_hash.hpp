#pragma once

// 名册摘要（序无关 FNV-1a 64 → 16 位十六进制串）：签发侧与准入侧同算法
// 同口径的单一事实源。从 admission_credential.cpp 迁出为 inline 头（D11
// issuer=Base 批③）——Base 侧资格盖章（972→973/976 roster_hash）与 cell
// 侧 enter 核验共用同函数；Base 目标不链 apollo_squad_instance（重游戏
// 库），inline 头让两侧零链接依赖同值。
//
// 成员作集合处理（排序后逐字节折叠），与入队次序无关——名册变更即摘要
// 变化。

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace squad_instance {
namespace roster_hash_detail {

constexpr std::uint64_t kFnvOffset = 1469598103934665603ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

inline const std::array<char, 16>& hex_digits() {
    static const std::array<char, 16> digits = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f',
    };
    return digits;
}

inline std::string to_hex(std::uint64_t value) {
    const auto& digits = hex_digits();
    std::string out(16, '\0');
    for (std::size_t i = 16; i-- > 0;) {
        out[i] = digits[value & 0xfULL];
        value >>= 4;
    }
    return out;
}

}  // namespace roster_hash_detail

// 名册摘要：受理快照成员的可复算指纹。members 顺序不影响结果。
inline std::string roster_hash_of(const std::vector<std::uint64_t>& members) {
    std::vector<std::uint64_t> sorted(members);
    std::sort(sorted.begin(), sorted.end());
    std::uint64_t hash = roster_hash_detail::kFnvOffset;
    for (const auto member : sorted) {
        for (int shift = 0; shift < 8; ++shift) {
            hash ^= static_cast<std::uint8_t>((member >> (shift * 8)) & 0xffULL);
            hash *= roster_hash_detail::kFnvPrime;
        }
    }
    return roster_hash_detail::to_hex(hash);
}

}  // namespace squad_instance
