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
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <random>
#include <array>
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

//==============================================================================
// Base64 实现
//==============================================================================

static const char g_base64Chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const uint8_t g_base64Lookup[256] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x3E, 0xFF, 0x3E, 0xFF, 0x3F,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
    0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

std::string Base64::encode(const void* data, size_t size) {
    const uint8_t* input = static_cast<const uint8_t*>(data);
    std::string result;
    result.reserve(encodedLength(size));

    for (size_t i = 0; i < size; i += 3) {
        uint32_t triplet = (i + 0 < size ? input[i + 0] : 0) << 16 |
                          (i + 1 < size ? input[i + 1] : 0) << 8 |
                          (i + 2 < size ? input[i + 2] : 0);

        result += g_base64Chars[(triplet >> 18) & 0x3F];
        result += g_base64Chars[(triplet >> 12) & 0x3F];
        result += (i + 1 < size) ? g_base64Chars[(triplet >> 6) & 0x3F] : '=';
        result += (i + 2 < size) ? g_base64Chars[triplet & 0x3F] : '=';
    }

    return result;
}

std::vector<uint8_t> Base64::decode(const std::string& str) {
    std::vector<uint8_t> result;

    // 跳过空白
    size_t len = str.size();
    while (len > 0 && (str[len - 1] == '\r' || str[len - 1] == '\n' ||
                       str[len - 1] == ' ' || str[len - 1] == '\t')) {
        --len;
    }

    if (len % 4 != 0) {
        return result; // 无效的 Base64
    }

    result.reserve((len / 4) * 3);

    for (size_t i = 0; i < len; i += 4) {
        uint8_t indices[4];
        for (int j = 0; j < 4; ++j) {
            uint8_t c = static_cast<uint8_t>(str[i + j]);
            indices[j] = g_base64Lookup[c];
            if (indices[j] == 0xFF && str[i + j] != '=') {
                return {}; // 无效字符
            }
        }

        uint32_t triplet = (indices[0] << 18) | (indices[1] << 12) |
                          (indices[2] << 6) | indices[3];

        result.push_back(static_cast<uint8_t>((triplet >> 16) & 0xFF));
        if (str[i + 2] != '=') {
            result.push_back(static_cast<uint8_t>((triplet >> 8) & 0xFF));
        }
        if (str[i + 3] != '=') {
            result.push_back(static_cast<uint8_t>(triplet & 0xFF));
        }
    }

    return result;
}

size_t Base64::decodedLength(const std::string& encoded) {
    size_t len = encoded.size();
    size_t padding = 0;

    if (len >= 2 && encoded[len - 1] == '=') {
        ++padding;
        if (encoded[len - 2] == '=') {
            ++padding;
        }
    }

    return (len / 4) * 3 - padding;
}

//==============================================================================
// Hex 实现
//==============================================================================

std::string Hex::encode(const void* data, size_t size) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    std::string result;
    result.reserve(size * 2);

    const char* hex = "0123456789abcdef";
    for (size_t i = 0; i < size; ++i) {
        result += hex[bytes[i] >> 4];
        result += hex[bytes[i] & 0x0F];
    }

    return result;
}

std::vector<uint8_t> Hex::decode(const std::string& hex) {
    std::vector<uint8_t> result;

    auto charToValue = [](char c) -> int8_t {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };

    size_t len = hex.size();
    if (len % 2 != 0) {
        return result;
    }

    result.reserve(len / 2);

    for (size_t i = 0; i < len; i += 2) {
        int8_t high = charToValue(hex[i]);
        int8_t low = charToValue(hex[i + 1]);

        if (high < 0 || low < 0) {
            return {}; // 无效字符
        }

        result.push_back(static_cast<uint8_t>((high << 4) | low));
    }

    return result;
}

std::string Hex::toUpper(const std::string& hex) {
    std::string result = hex;
    for (char& c : result) {
        if (c >= 'a' && c <= 'f') {
            c = c - 'a' + 'A';
        }
    }
    return result;
}

std::string Hex::toLower(const std::string& hex) {
    std::string result = hex;
    for (char& c : result) {
        if (c >= 'A' && c <= 'F') {
            c = c - 'A' + 'a';
        }
    }
    return result;
}

//==============================================================================
// SimpleCipher 实现
//==============================================================================

void SimpleCipher::xorCipher(const void* input, void* output, size_t size,
                             const std::string& key) {
    if (key.empty()) {
        if (input != output) {
            std::memcpy(output, input, size);
        }
        return;
    }

    const uint8_t* in = static_cast<const uint8_t*>(input);
    uint8_t* out = static_cast<uint8_t*>(output);
    size_t keyLen = key.size();

    for (size_t i = 0; i < size; ++i) {
        out[i] = in[i] ^ static_cast<uint8_t>(key[i % keyLen]);
    }
}

void SimpleCipher::xorCipher(const void* input, void* output, size_t size,
                             const std::vector<uint8_t>& key) {
    if (key.empty()) {
        if (input != output) {
            std::memcpy(output, input, size);
        }
        return;
    }

    const uint8_t* in = static_cast<const uint8_t*>(input);
    uint8_t* out = static_cast<uint8_t*>(output);
    size_t keyLen = key.size();

    for (size_t i = 0; i < size; ++i) {
        out[i] = in[i] ^ key[i % keyLen];
    }
}

void SimpleCipher::xorCipher(const void* input, void* output, size_t size,
                             uint64_t key) {
    const uint8_t* in = static_cast<const uint8_t*>(input);
    uint8_t* out = static_cast<uint8_t*>(output);
    const uint8_t* keyBytes = reinterpret_cast<const uint8_t*>(&key);

    for (size_t i = 0; i < size; ++i) {
        out[i] = in[i] ^ keyBytes[i % 8];
    }
}

std::vector<uint8_t> SimpleCipher::xorRepeatingKey(
    const std::vector<uint8_t>& data, const std::vector<uint8_t>& key) {

    if (key.empty()) {
        return data;
    }

    std::vector<uint8_t> result(data.size());
    size_t keyLen = key.size();

    for (size_t i = 0; i < data.size(); ++i) {
        result[i] = data[i] ^ key[i % keyLen];
    }

    return result;
}

void SimpleCipher::rotateCipher(void* data, size_t size, int8_t amount) {
    uint8_t* bytes = static_cast<uint8_t*>(data);
    for (size_t i = 0; i < size; ++i) {
        bytes[i] = static_cast<uint8_t>(bytes[i] + amount);
    }
}

void SimpleCipher::rotateCipher(std::string& str, int8_t amount) {
    for (char& c : str) {
        c = static_cast<char>(c + amount);
    }
}

std::string SimpleCipher::substitutionCipher(const std::string& input,
                                             const std::string& alphabet,
                                             const std::string& key) {
    std::string result;
    result.reserve(input.size());

    for (char c : input) {
        size_t pos = alphabet.find(c);
        if (pos != std::string::npos && pos < key.size()) {
            result += key[pos];
        } else {
            result += c;
        }
    }

    return result;
}

std::string SimpleCipher::substitutionDecipher(const std::string& input,
                                               const std::string& key,
                                               const std::string& alphabet) {
    std::string result;
    result.reserve(input.size());

    for (char c : input) {
        size_t pos = key.find(c);
        if (pos != std::string::npos && pos < alphabet.size()) {
            result += alphabet[pos];
        } else {
            result += c;
        }
    }

    return result;
}

//==============================================================================
// HMAC 实现
//==============================================================================

std::string HMAC::md5(const std::string& key, const void* data, size_t size) {
    // 简化实现：使用 SHA-256 替代
    return sha256(key, data, size);
}

std::string HMAC::sha1(const std::string& key, const void* data, size_t size) {
    return sha256(key, data, size);
}

std::string HMAC::sha256(const std::string& key, const void* data, size_t size) {
    // HMAC-SHA256 per RFC 2104 / FIPS 198-1
    std::array<uint8_t, 64> keyBlock{};
    if (key.size() > keyBlock.size()) {
        uint8_t digest[32];
        unsigned int digestLen = 0;

        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        if (!ctx) {
            return {};
        }

        bool ok = EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1 &&
                  EVP_DigestUpdate(ctx, key.data(), key.size()) == 1 &&
                  EVP_DigestFinal_ex(ctx, digest, &digestLen) == 1 &&
                  digestLen == sizeof(digest);

        EVP_MD_CTX_free(ctx);

        if (!ok) {
            return {};
        }

        std::memcpy(keyBlock.data(), digest, sizeof(digest));
    } else {
        std::memcpy(keyBlock.data(), key.data(), key.size());
    }

    std::array<uint8_t, 64> innerPad{};
    std::array<uint8_t, 64> outerPad{};
    innerPad.fill(0x36);
    outerPad.fill(0x5c);
    for (size_t i = 0; i < keyBlock.size(); ++i) {
        innerPad[i] ^= keyBlock[i];
        outerPad[i] ^= keyBlock[i];
    }

    uint8_t innerDigest[32];
    unsigned int innerLen = 0;
    EVP_MD_CTX* inner = EVP_MD_CTX_new();
    if (!inner) {
        return {};
    }
    bool innerOk = EVP_DigestInit_ex(inner, EVP_sha256(), nullptr) == 1 &&
                   EVP_DigestUpdate(inner, innerPad.data(), innerPad.size()) == 1 &&
                   EVP_DigestUpdate(inner, data, size) == 1 &&
                   EVP_DigestFinal_ex(inner, innerDigest, &innerLen) == 1 &&
                   innerLen == sizeof(innerDigest);
    EVP_MD_CTX_free(inner);
    if (!innerOk) {
        return {};
    }

    uint8_t outDigest[32];
    unsigned int outLen = 0;
    EVP_MD_CTX* outer = EVP_MD_CTX_new();
    if (!outer) {
        return {};
    }
    bool outerOk = EVP_DigestInit_ex(outer, EVP_sha256(), nullptr) == 1 &&
                   EVP_DigestUpdate(outer, outerPad.data(), outerPad.size()) == 1 &&
                   EVP_DigestUpdate(outer, innerDigest, sizeof(innerDigest)) == 1 &&
                   EVP_DigestFinal_ex(outer, outDigest, &outLen) == 1 &&
                   outLen == sizeof(outDigest);
    EVP_MD_CTX_free(outer);

    return outerOk ? Hash::toHexString(outDigest, sizeof(outDigest)) : std::string();
}

//==============================================================================
// Checksum 实现
//==============================================================================

uint32_t Checksum::adler32(const void* data, size_t size, uint32_t initial) {
    const uint8_t* ptr = static_cast<const uint8_t*>(data);
    uint32_t a = initial & 0xFFFF;
    uint32_t b = (initial >> 16) & 0xFFFF;

    while (size > 0) {
        size_t amount = std::min(size, static_cast<size_t>(5552));
        size -= amount;

        while (amount > 0) {
            a += *ptr++;
            b += a;
            --amount;
        }

        a %= 65521;
        b %= 65521;
    }

    return (b << 16) | a;
}

uint32_t Checksum::crc32(const void* data, size_t size, uint32_t initial) {
    static const uint32_t table[256] = {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba,
        0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
        0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
        0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
        0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
        0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
        0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
        0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
        0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
        0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
        0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940,
        0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
        0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116,
        0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
        0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
        0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
        0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a,
        0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
        0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818,
        0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
        0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
        0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
        0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c,
        0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
        0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
        0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
        0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
        0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
        0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086,
        0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
        0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4,
        0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
        0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
        0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
        0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
        0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
        0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe,
        0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
        0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
        0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
        0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252,
        0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
        0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60,
        0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
        0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
        0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
        0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04,
        0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
        0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,
        0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
        0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
        0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
        0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e,
        0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
        0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
        0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
        0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
        0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
        0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0,
        0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
        0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6,
        0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
        0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
        0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
    };

    const uint8_t* ptr = static_cast<const uint8_t*>(data);
    uint32_t crc = ~initial;

    for (size_t i = 0; i < size; ++i) {
        crc = table[(crc ^ ptr[i]) & 0xFF] ^ (crc >> 8);
    }

    return ~crc;
}

uint16_t Checksum::crc16(const void* data, size_t size, uint16_t polynomial) {
    const uint8_t* ptr = static_cast<const uint8_t*>(data);
    uint16_t crc = 0;

    while (size-- > 0) {
        crc ^= *ptr++ << 8;
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ polynomial;
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

uint8_t Checksum::sum8(const void* data, size_t size) {
    const uint8_t* ptr = static_cast<const uint8_t*>(data);
    uint8_t sum = 0;

    for (size_t i = 0; i < size; ++i) {
        sum += ptr[i];
    }

    return sum;
}

uint16_t Checksum::sum16(const void* data, size_t size) {
    const uint8_t* ptr = static_cast<const uint8_t*>(data);
    uint16_t sum = 0;

    for (size_t i = 0; i + 1 < size; i += 2) {
        sum += (ptr[i] << 8) | ptr[i + 1];
    }

    if (size % 2 == 1) {
        sum += ptr[size - 1] << 8;
    }

    return sum;
}

uint32_t Checksum::sum32(const void* data, size_t size) {
    const uint32_t* ptr = static_cast<const uint32_t*>(data);
    uint32_t sum = 0;
    size_t count = size / 4;

    for (size_t i = 0; i < count; ++i) {
        sum += ptr[i];
    }

    return sum;
}

//==============================================================================
// URLCodec 实现
//==============================================================================

std::string URLCodec::encode(const std::string& str) {
    std::string result;
    result.reserve(str.size() * 2);

    for (unsigned char c : str) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' ||
            c == '.' || c == '~') {
            result += c;
        } else if (c == ' ') {
            result += '+';
        } else {
            result += '%';
            result += "0123456789ABCDEF"[c >> 4];
            result += "0123456789ABCDEF"[c & 0x0F];
        }
    }

    return result;
}

std::string URLCodec::decode(const std::string& str) {
    std::string result;
    result.reserve(str.size());

    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '+') {
            result += ' ';
        } else if (str[i] == '%' && i + 2 < str.size()) {
            auto hexToValue = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return 0;
            };
            char value = (hexToValue(str[i + 1]) << 4) | hexToValue(str[i + 2]);
            result += value;
            i += 2;
        } else {
            result += str[i];
        }
    }

    return result;
}

std::string URLCodec::buildQuery(const std::vector<std::pair<std::string, std::string>>& params) {
    std::string result;

    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) {
            result += '&';
        }
        result += encode(params[i].first);
        result += '=';
        result += encode(params[i].second);
    }

    return result;
}

std::vector<std::pair<std::string, std::string>> URLCodec::parseQuery(const std::string& query) {
    std::vector<std::pair<std::string, std::string>> result;
    size_t start = 0;

    while (start < query.size()) {
        size_t sep = query.find('=', start);
        size_t end = query.find('&', start);

        if (sep == std::string::npos) {
            break;
        }

        if (end == std::string::npos) {
            end = query.size();
        }

        std::string key = decode(query.substr(start, sep - start));
        std::string value = decode(query.substr(sep + 1, end - sep - 1));

        result.emplace_back(key, value);
        start = end + 1;
    }

    return result;
}

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
