/**
 * @file contract_hash.cpp
 * @brief 自包含 SHA-256（FIPS 180-4）+ schema_hash 组合。
 * 不引 OpenSSL：契约工具链（sdks/gen）保持零加密库依赖（决策 #4 链接图最小化）。
 */

#include "apollo/contract/contract_hash.hpp"
#include "apollo/contract/contract_writer.hpp"

#include <cstring>
#include <iomanip>
#include <sstream>
#include <vector>

namespace apollo::contract {

namespace {

constexpr uint32_t kK[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
    0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
    0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
    0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
    0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
    0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
    0xc67178f2,
};

inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

/// 收尾 → 64 位十六进制（computeSchemaHash/computeDomainHash 共用）
std::string finishHex(Sha256& h) {
    auto digest = h.finish();
    std::ostringstream os;
    for (uint8_t b : digest) {
        os << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    return os.str();
}

}  // namespace

Sha256::Sha256() : totalLen_(0), bufferLen_(0) {
    state_[0] = 0x6a09e667;
    state_[1] = 0xbb67ae85;
    state_[2] = 0x3c6ef372;
    state_[3] = 0xa54ff53a;
    state_[4] = 0x510e527f;
    state_[5] = 0x9b05688c;
    state_[6] = 0x1f83d9ab;
    state_[7] = 0x5be0cd19;
    std::memset(buffer_, 0, sizeof(buffer_));
}

void Sha256::processBlock(const uint8_t* block) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
               (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
               static_cast<uint32_t>(block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
    uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];

    for (int i = 0; i < 64; ++i) {
        uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t temp1 = h + s1 + ch + kK[i] + w[i];
        uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
    state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
}

void Sha256::update(const void* data, size_t len) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    totalLen_ += len;
    while (len > 0) {
        size_t take = std::min(len, sizeof(buffer_) - bufferLen_);
        std::memcpy(buffer_ + bufferLen_, p, take);
        bufferLen_ += take;
        p += take;
        len -= take;
        if (bufferLen_ == sizeof(buffer_)) {
            processBlock(buffer_);
            bufferLen_ = 0;
        }
    }
}

std::array<uint8_t, 32> Sha256::finish() {
    const uint64_t bitLen = totalLen_ * 8;
    // 填充规则：0x80 + 0x00* + 8 字节大端位长，凑到 64 的倍数
    const size_t padded = (bufferLen_ + 9 + 63) / 64 * 64;
    std::vector<uint8_t> padding(padded - bufferLen_, 0);
    padding[0] = 0x80;
    for (int i = 0; i < 8; ++i) {
        padding[padding.size() - 8 + i] = static_cast<uint8_t>(bitLen >> (56 - i * 8));
    }
    // 吸收填充（不经过 update——那会污染 totalLen_）
    for (size_t i = 0; i < padding.size();) {
        size_t take = std::min(sizeof(buffer_) - bufferLen_, padding.size() - i);
        std::memcpy(buffer_ + bufferLen_, padding.data() + i, take);
        bufferLen_ += take;
        i += take;
        if (bufferLen_ == sizeof(buffer_)) {
            processBlock(buffer_);
            bufferLen_ = 0;
        }
    }
    std::array<uint8_t, 32> out{};
    for (int i = 0; i < 8; ++i) {
        out[i * 4] = static_cast<uint8_t>(state_[i] >> 24);
        out[i * 4 + 1] = static_cast<uint8_t>(state_[i] >> 16);
        out[i * 4 + 2] = static_cast<uint8_t>(state_[i] >> 8);
        out[i * 4 + 3] = static_cast<uint8_t>(state_[i]);
    }
    return out;
}

std::string Sha256::hex(std::string_view data) {
    Sha256 h;
    h.update(data);
    auto digest = h.finish();
    std::ostringstream os;
    for (uint8_t b : digest) {
        os << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    return os.str();
}

std::string computeSchemaHash(const Contract& c, const std::string& generatorVersion) {
    Sha256 h;
    h.update(canonicalBundle(c));
    h.update(generatorVersion);
    return finishHex(h);
}

std::string computeDomainHash(const Contract& c, const std::string& generatorVersion,
                              MsgDomain domain) {
    Sha256 h;
    h.update(canonicalDomainBundle(c, domain));
    h.update(generatorVersion);
    return finishHex(h);
}

}  // namespace apollo::contract
