/**
 * @file crypto.cpp
 * @brief 加密和哈希工具实现
 */

// ========== 平台相关头文件（必须最先包含）==========
#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX       // 防止 min/max 宏冲突
    #endif
    #include <windows.h>
    #include <bcrypt.h>
    #pragma comment(lib, "bcrypt.lib")
#endif
#include "apollo/utils/crypto.h"
#include <iomanip>
#include <random>
#include <sstream>
#include <openssl/evp.h>

namespace apollo {
namespace utils {

//==============================================================================
// SecureRandom 实现
//==============================================================================

thread_local std::random_device g_randomDevice;
thread_local std::mt19937_64 g_generator(g_randomDevice());

void SecureRandom::initialize() {
    // 已在 thread_local 初始化
}

void SecureRandom::nextBytes(void* buffer, size_t size) {
#ifdef _WIN32
    NTSTATUS status = BCryptGenRandom(
        nullptr,
        static_cast<PUCHAR>(buffer),
        static_cast<ULONG>(size),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG
    );
    if (status != 0) {
        // 回退到软件随机
        uint8_t* ptr = static_cast<uint8_t*>(buffer);
        std::uniform_int_distribution<int> dist(0, 255);
        for (size_t i = 0; i < size; ++i) {
            ptr[i] = static_cast<uint8_t>(dist(g_generator));
        }
    }
#else
    uint8_t* ptr = static_cast<uint8_t*>(buffer);
    std::uniform_int_distribution<int> dist(0, 255);
    for (size_t i = 0; i < size; ++i) {
        ptr[i] = static_cast<uint8_t>(dist(g_generator));
    }
#endif
}

uint8_t SecureRandom::nextUint8() {
    uint8_t value;
    nextBytes(&value, 1);
    return value;
}

uint16_t SecureRandom::nextUint16() {
    uint16_t value;
    nextBytes(&value, 2);
    return value;
}

uint32_t SecureRandom::nextUint32() {
    uint32_t value;
    nextBytes(&value, 4);
    return value;
}

uint64_t SecureRandom::nextUint64() {
    uint64_t value;
    nextBytes(&value, 8);
    return value;
}

uint32_t SecureRandom::nextUint32(uint32_t min, uint32_t max) {
    std::uniform_int_distribution<uint32_t> dist(min, max);
    return dist(g_generator);
}

uint64_t SecureRandom::nextUint64(uint64_t min, uint64_t max) {
    std::uniform_int_distribution<uint64_t> dist(min, max);
    return dist(g_generator);
}

std::string SecureRandom::nextString(size_t length, const std::string& charset) {
    std::string result;
    result.reserve(length);
    std::uniform_int_distribution<size_t> dist(0, charset.size() - 1);

    for (size_t i = 0; i < length; ++i) {
        result += charset[dist(g_generator)];
    }
    return result;
}

std::string SecureRandom::generateUUID() {
    // UUID v4 格式: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    std::string uuid;
    uuid.reserve(36);

    // 生成 16 字节随机数据
    uint8_t data[16];
    nextBytes(data, 16);

    // 设置版本和变体位
    data[6] = (data[6] & 0x0F) | 0x40; // 版本 4
    data[8] = (data[8] & 0x3F) | 0x80; // 变体

    // 格式化为字符串
    const char* hex = "0123456789abcdef";
    for (int i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            uuid += '-';
        }
        uuid += hex[data[i] >> 4];
        uuid += hex[data[i] & 0x0F];
    }

    return uuid;
}

std::string SecureRandom::generateUUIDv4() {
    return generateUUID();
}

std::string SecureRandom::generateGUID() {
    // GUID 格式: {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
    return "{" + generateUUID() + "}";
}

//==============================================================================
// Hash 实现
//==============================================================================

std::string Hash::md5(const void* data, size_t size) {
    detail::MD5 md5;
    md5.update(data, size);
    return md5.finish();
}

std::string Hash::sha256(const void* data, size_t size) {
    uint8_t digest[32];
    unsigned int digestLen = 0;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return {};
    }

    bool ok = EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1 &&
              EVP_DigestUpdate(ctx, data, size) == 1 &&
              EVP_DigestFinal_ex(ctx, digest, &digestLen) == 1 &&
              digestLen == sizeof(digest);

    EVP_MD_CTX_free(ctx);

    return ok ? Hash::toHexString(digest, sizeof(digest)) : std::string();
}

std::string Hash::sha1(const void* data, size_t size) {
    detail::SHA1 sha1;
    sha1.update(data, size);
    return sha1.finish();
}

uint32_t Hash::murmur3_32(const void* data, size_t size, uint32_t seed) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;
    const uint32_t r1 = 15;
    const uint32_t r2 = 13;
    const uint32_t m = 5;
    const uint32_t n = 0xe6546b64;

    uint32_t hash = seed;

    const int nblocks = static_cast<int>(size / 4);
    const uint32_t* blocks = reinterpret_cast<const uint32_t*>(bytes);

    for (int i = 0; i < nblocks; ++i) {
        uint32_t k = blocks[i];
        k *= c1;
        k = (k << r1) | (k >> (32 - r1));
        k *= c2;

        hash ^= k;
        hash = ((hash << r2) | (hash >> (32 - r2))) * m + n;
    }

    const uint8_t* tail = bytes + nblocks * 4;
    uint32_t k = 0;

    switch (size & 3) {
        case 3: k ^= tail[2] << 16;
        case 2: k ^= tail[1] << 8;
        case 1: k ^= tail[0];
            k *= c1;
            k = (k << r1) | (k >> (32 - r1));
            k *= c2;
            hash ^= k;
    }

    hash ^= size;
    hash ^= hash >> 16;
    hash *= 0x85ebca6b;
    hash ^= hash >> 13;
    hash *= 0xc2b2ae35;
    hash ^= hash >> 16;

    return hash;
}

void Hash::murmur3_128(const void* data, size_t size, uint32_t seed, uint64_t out[2]) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    const uint64_t c1 = 0x87c37b91114253d5ULL;
    const uint64_t c2 = 0x4cf5ad432745937fULL;

    const int nblocks = static_cast<int>(size / 16);
    const uint64_t* blocks = reinterpret_cast<const uint64_t*>(bytes);

    uint64_t h1 = seed;
    uint64_t h2 = seed;

    for (int i = 0; i < nblocks; ++i) {
        uint64_t k1 = blocks[i * 2];
        uint64_t k2 = blocks[i * 2 + 1];

        k1 *= c1; k1 = (k1 << 31) | (k1 >> 33); k1 *= c2; h1 ^= k1;
        h1 = (h1 << 27) | (h1 >> 37); h1 += h2; h1 = h1 * 5 + 0x52dce729;

        k2 *= c2; k2 = (k2 << 33) | (k2 >> 31); k2 *= c1; h2 ^= k2;
        h2 = (h2 << 31) | (h2 >> 33); h2 += h1; h2 = h2 * 5 + 0x38495ab5;
    }

    const uint8_t* tail = bytes + nblocks * 16;
    uint64_t k1 = 0;
    uint64_t k2 = 0;

    switch (size & 15) {
        case 15: k2 ^= static_cast<uint64_t>(tail[14]) << 48;
        case 14: k2 ^= static_cast<uint64_t>(tail[13]) << 40;
        case 13: k2 ^= static_cast<uint64_t>(tail[12]) << 32;
        case 12: k2 ^= static_cast<uint64_t>(tail[11]) << 24;
        case 11: k2 ^= static_cast<uint64_t>(tail[10]) << 16;
        case 10: k2 ^= static_cast<uint64_t>(tail[9]) << 8;
        case  9: k2 ^= static_cast<uint64_t>(tail[8]);
                 k2 *= c2; k2 = (k2 << 33) | (k2 >> 31); k2 *= c1; h2 ^= k2;
        case  8: k1 ^= static_cast<uint64_t>(tail[7]) << 56;
        case  7: k1 ^= static_cast<uint64_t>(tail[6]) << 48;
        case  6: k1 ^= static_cast<uint64_t>(tail[5]) << 40;
        case  5: k1 ^= static_cast<uint64_t>(tail[4]) << 32;
        case  4: k1 ^= static_cast<uint64_t>(tail[3]) << 24;
        case  3: k1 ^= static_cast<uint64_t>(tail[2]) << 16;
        case  2: k1 ^= static_cast<uint64_t>(tail[1]) << 8;
        case  1: k1 ^= static_cast<uint64_t>(tail[0]);
                 k1 *= c1; k1 = (k1 << 31) | (k1 >> 33); k1 *= c2; h1 ^= k1;
    }

    h1 ^= size; h2 ^= size;

    h1 += h2;
    h2 += h1;

    h1 ^= h1 >> 33; h1 *= 0xff51afd7ed558ccdULL; h1 ^= h1 >> 33; h1 *= 0xc4ceb9fe1a85ec53ULL; h1 ^= h1 >> 33;
    h2 ^= h2 >> 33; h2 *= 0xff51afd7ed558ccdULL; h2 ^= h2 >> 33; h2 *= 0xc4ceb9fe1a85ec53ULL; h2 ^= h2 >> 33;

    h1 += h2;
    h2 += h1;

    out[0] = h1;
    out[1] = h2;
}

uint32_t Hash::xxhash32(const void* data, size_t size, uint32_t seed) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    const uint8_t* const end = p + size;
    uint32_t h32;

    if (size >= 16) {
        const uint8_t* const limit = end - 16;
        uint32_t v1 = seed + 0x9e3779b1 + 0x62b92165;
        uint32_t v2 = seed + 0x9e3779b1;
        uint32_t v3 = seed + 0x9e3779b1;
        uint32_t v4 = seed - 0x9e3779b1;

        do {
            v1 += reinterpret_cast<const uint32_t*>(p)[0] * 0x9e3779b1;
            v1 = (v1 << 15) | (v1 >> 17);
            v1 *= 0x85ebca6b;

            v2 += reinterpret_cast<const uint32_t*>(p)[1] * 0x9e3779b1;
            v2 = (v2 << 15) | (v2 >> 17);
            v2 *= 0xc2b2ae3d;

            v3 += reinterpret_cast<const uint32_t*>(p)[2] * 0x9e3779b1;
            v3 = (v3 << 15) | (v3 >> 17);
            v3 *= 0x85ebca6b;

            v4 += reinterpret_cast<const uint32_t*>(p)[3] * 0x9e3779b1;
            v4 = (v4 << 15) | (v4 >> 17);
            v4 *= 0xc2b2ae3d;

            p += 16;
        } while (p <= limit);

        h32 = ((v1 << 1) | (v1 >> 31)) +
              ((v2 << 7) | (v2 >> 25)) +
              ((v3 << 12) | (v3 >> 20)) +
              ((v4 << 18) | (v4 >> 14));
    } else {
        h32 = seed + 0x165667b1;
    }

    h32 += static_cast<uint32_t>(size);

    while (p + 4 <= end) {
        h32 += reinterpret_cast<const uint32_t*>(p)[0] * 0x9e3779b1;
        h32 = (h32 << 17) | (h32 >> 15);
        h32 *= 0xc2b2ae3d;
        p += 4;
    }

    while (p < end) {
        h32 += (*p++) * 0x9e3779b1;
        h32 = (h32 << 11) | (h32 >> 21);
        h32 *= 0x85ebca6b;
    }

    h32 ^= h32 >> 15;
    h32 *= 0xc2b2ae3d;
    h32 ^= h32 >> 13;

    return h32;
}

uint64_t Hash::cityHash64(const void* data, size_t size) {
    // 简化实现，使用 FNV-1a 64-bit 作为替代
    return fnv1a64(data, size);
}

std::string Hash::toHexString(const void* data, size_t size) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < size; ++i) {
        oss << std::setw(2) << static_cast<int>(bytes[i]);
    }
    return oss.str();
}

} // namespace utils
} // namespace apollo
