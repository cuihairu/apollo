/**
 * @file crypto_digest.cpp
 * @brief SHA-256/SHA-1/MD5 增量摘要实现（detail 族）
 */

#include "apollo/utils/crypto.h"
#include <algorithm>
#include <cstring>

namespace apollo {
namespace utils {

//==============================================================================
// SHA-256 实现
//==============================================================================

namespace detail {

SHA256::SHA256() : totalSize_(0), bufferLen_(0) {
    // 初始状态
    state[0] = 0x6a09e667;
    state[1] = 0xbb67ae85;
    state[2] = 0x3c6ef372;
    state[3] = 0xa54ff53a;
    state[4] = 0x510e527f;
    state[5] = 0x9b05688c;
    state[6] = 0x1f83d9ab;
    state[7] = 0x5be0cd19;
    std::memset(buffer, 0, sizeof(buffer));
}

void SHA256::update(const void* data, size_t size) {
    const uint8_t* ptr = static_cast<const uint8_t*>(data);
    totalSize_ += size;

    while (size > 0) {
        size_t copy = std::min(size, BLOCK_SIZE - bufferLen_);
        std::memcpy(buffer + bufferLen_, ptr, copy);
        bufferLen_ += copy;
        ptr += copy;
        size -= copy;

        if (bufferLen_ == BLOCK_SIZE) {
            transform(buffer);
            bufferLen_ = 0;
        }
    }
}

void SHA256::finish(uint8_t hash[HASH_SIZE]) {
    // 填充
    uint64_t totalBits = totalSize_ * 8;
    buffer[bufferLen_++] = 0x80;

    if (bufferLen_ > 56) {
        while (bufferLen_ < BLOCK_SIZE) {
            buffer[bufferLen_++] = 0;
        }
        transform(buffer);
        bufferLen_ = 0;
    }

    while (bufferLen_ < 56) {
        buffer[bufferLen_++] = 0;
    }

    // 长度（大端序）
    for (int i = 7; i >= 0; --i) {
        buffer[bufferLen_++] = (totalBits >> (i * 8)) & 0xFF;
    }

    transform(buffer);

    // 输出
    for (int i = 0; i < 8; ++i) {
        hash[i * 4 + 0] = (state[i] >> 24) & 0xFF;
        hash[i * 4 + 1] = (state[i] >> 16) & 0xFF;
        hash[i * 4 + 2] = (state[i] >> 8) & 0xFF;
        hash[i * 4 + 3] = state[i] & 0xFF;
    }
}

std::string SHA256::finish() {
    uint8_t hash[HASH_SIZE];
    finish(hash);
    return Hash::toHexString(hash, HASH_SIZE);
}

void SHA256::transform(const uint8_t block[BLOCK_SIZE]) {
    static const uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ae, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    uint32_t W[64];
    for (int i = 0; i < 16; ++i) {
        W[i] = (block[i * 4] << 24) | (block[i * 4 + 1] << 16) |
               (block[i * 4 + 2] << 8) | block[i * 4 + 3];
    }
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = (W[i - 15] >> 7) | (W[i - 15] << 25);
        s0 ^= (W[i - 15] >> 18) | (W[i - 15] << 14);
        s0 ^= (W[i - 15] >> 3);
        uint32_t s1 = (W[i - 2] >> 17) | (W[i - 2] << 15);
        s1 ^= (W[i - 2] >> 19) | (W[i - 2] << 13);
        s1 ^= (W[i - 2] >> 10);
        W[i] = W[i - 16] + s0 + W[i - 7] + s1;
    }

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];

    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = (e >> 6) | (e << 26);
        S1 ^= (e >> 11) | (e << 21);
        S1 ^= (e >> 25) | (e << 7);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t temp1 = h + S1 + ch + K[i] + W[i];
        uint32_t S0 = (a >> 2) | (a << 30);
        S0 ^= (a >> 13) | (a << 19);
        S0 ^= (a >> 22) | (a << 10);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

//==============================================================================
// SHA-1 实现
//==============================================================================

SHA1::SHA1() : totalSize_(0), bufferLen_(0) {
    state[0] = 0x67452301;
    state[1] = 0xEFCDAB89;
    state[2] = 0x98BADCFE;
    state[3] = 0x10325476;
    state[4] = 0xC3D2E1F0;
    std::memset(buffer, 0, sizeof(buffer));
}

void SHA1::update(const void* data, size_t size) {
    const uint8_t* ptr = static_cast<const uint8_t*>(data);
    totalSize_ += size;

    while (size > 0) {
        size_t copy = std::min(size, BLOCK_SIZE - bufferLen_);
        std::memcpy(buffer + bufferLen_, ptr, copy);
        bufferLen_ += copy;
        ptr += copy;
        size -= copy;

        if (bufferLen_ == BLOCK_SIZE) {
            transform(buffer);
            bufferLen_ = 0;
        }
    }
}

void SHA1::finish(uint8_t hash[HASH_SIZE]) {
    uint64_t totalBits = totalSize_ * 8;
    buffer[bufferLen_++] = 0x80;

    if (bufferLen_ > 56) {
        while (bufferLen_ < BLOCK_SIZE) {
            buffer[bufferLen_++] = 0;
        }
        transform(buffer);
        bufferLen_ = 0;
    }

    while (bufferLen_ < 56) {
        buffer[bufferLen_++] = 0;
    }

    for (int i = 7; i >= 0; --i) {
        buffer[bufferLen_++] = (totalBits >> (i * 8)) & 0xFF;
    }

    transform(buffer);

    for (int i = 0; i < 5; ++i) {
        hash[i * 4 + 0] = (state[i] >> 24) & 0xFF;
        hash[i * 4 + 1] = (state[i] >> 16) & 0xFF;
        hash[i * 4 + 2] = (state[i] >> 8) & 0xFF;
        hash[i * 4 + 3] = state[i] & 0xFF;
    }
}

std::string SHA1::finish() {
    uint8_t hash[HASH_SIZE];
    finish(hash);
    return Hash::toHexString(hash, HASH_SIZE);
}

void SHA1::transform(const uint8_t block[BLOCK_SIZE]) {
    uint32_t W[80];

    for (int i = 0; i < 16; ++i) {
        W[i] = (block[i * 4] << 24) | (block[i * 4 + 1] << 16) |
               (block[i * 4 + 2] << 8) | block[i * 4 + 3];
    }
    for (int i = 16; i < 80; ++i) {
        W[i] = (W[i - 3] ^ W[i - 8] ^ W[i - 14] ^ W[i - 16]);
        W[i] = (W[i] << 1) | (W[i] >> 31);
    }

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];

    for (int i = 0; i < 80; ++i) {
        uint32_t f, k;
        if (i < 20) {
            f = (b & c) | (~b & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }

        uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + W[i];
        e = d;
        d = c;
        c = (b << 30) | (b >> 2);
        b = a;
        a = temp;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

//==============================================================================
// MD5 实现
//==============================================================================

MD5::MD5() : totalSize_(0), bufferLen_(0) {
    state[0] = 0x67452301;
    state[1] = 0xEFCDAB89;
    state[2] = 0x98BADCFE;
    state[3] = 0x10325476;
    std::memset(buffer, 0, sizeof(buffer));
}

void MD5::update(const void* data, size_t size) {
    const uint8_t* ptr = static_cast<const uint8_t*>(data);
    totalSize_ += size;

    while (size > 0) {
        size_t copy = std::min(size, BLOCK_SIZE - bufferLen_);
        std::memcpy(buffer + bufferLen_, ptr, copy);
        bufferLen_ += copy;
        ptr += copy;
        size -= copy;

        if (bufferLen_ == BLOCK_SIZE) {
            transform(buffer);
            bufferLen_ = 0;
        }
    }
}

void MD5::finish(uint8_t hash[HASH_SIZE]) {
    uint64_t totalBits = totalSize_ * 8;
    buffer[bufferLen_++] = 0x80;

    if (bufferLen_ > 56) {
        while (bufferLen_ < BLOCK_SIZE) {
            buffer[bufferLen_++] = 0;
        }
        transform(buffer);
        bufferLen_ = 0;
    }

    while (bufferLen_ < 56) {
        buffer[bufferLen_++] = 0;
    }

    for (int i = 0; i < 8; ++i) {
        buffer[bufferLen_++] = (totalBits >> (i * 8)) & 0xFF;
    }

    transform(buffer);

    for (int i = 0; i < 4; ++i) {
        hash[i * 4 + 0] = state[i] & 0xFF;
        hash[i * 4 + 1] = (state[i] >> 8) & 0xFF;
        hash[i * 4 + 2] = (state[i] >> 16) & 0xFF;
        hash[i * 4 + 3] = (state[i] >> 24) & 0xFF;
    }
}

std::string MD5::finish() {
    uint8_t hash[HASH_SIZE];
    finish(hash);
    return Hash::toHexString(hash, HASH_SIZE);
}

void MD5::transform(const uint8_t block[BLOCK_SIZE]) {
    static const uint32_t K[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
        0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
        0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
        0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
        0xd62f105d, 0x2441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
        0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
        0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x4881d05,
        0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
        0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
        0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
    };

    static const uint8_t S[64] = {
        7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
        5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
        4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
        6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
    };

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t M[16];

    for (int i = 0; i < 16; ++i) {
        M[i] = (block[i * 4] | (block[i * 4 + 1] << 8) |
               (block[i * 4 + 2] << 16) | (block[i * 4 + 3] << 24));
    }

    for (int i = 0; i < 64; ++i) {
        uint32_t f = 0, g = 0;

        if (i < 16) {
            f = (b & c) | (~b & d);
            g = i;
        } else if (i < 32) {
            f = (d & b) | (~d & c);
            g = (5 * i + 1) % 16;
        } else if (i < 48) {
            f = b ^ c ^ d;
            g = (3 * i + 5) % 16;
        } else {
            f = c ^ (b | ~d);
            g = (7 * i) % 16;
        }

        uint32_t temp = d;
        d = c;
        c = b;
        b = b + ((a + f + K[i] + M[g]) << S[i] | (a + f + K[i] + M[g]) >> (32 - S[i]));
        a = temp;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

} // namespace detail

} // namespace utils
} // namespace apollo
