#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <random>

namespace apollo {
namespace utils {

//==============================================================================
// 哈希工具
//==============================================================================

class Hash {
public:
    // MD5 (RFC 1321)
    static std::string md5(const void* data, size_t size);
    static std::string md5(const std::string& str) {
        return md5(str.data(), str.size());
    }

    // SHA-256
    static std::string sha256(const void* data, size_t size);
    static std::string sha256(const std::string& str) {
        return sha256(str.data(), str.size());
    }

    // SHA-1
    static std::string sha1(const void* data, size_t size);
    static std::string sha1(const std::string& str) {
        return sha1(str.data(), str.size());
    }

    // FNV-1a 32-bit
    static uint32_t fnv1a32(const void* data, size_t size, uint32_t seed = 2166136261u) {
        const uint8_t* ptr = static_cast<const uint8_t*>(data);
        const uint32_t prime = 16777619u;
        uint32_t hash = seed;

        for (size_t i = 0; i < size; ++i) {
            hash ^= ptr[i];
            hash *= prime;
        }

        return hash;
    }

    // FNV-1a 64-bit
    static uint64_t fnv1a64(const void* data, size_t size, uint64_t seed = 14695981039346656037ull) {
        const uint8_t* ptr = static_cast<const uint8_t*>(data);
        const uint64_t prime = 1099511628211ull;
        uint64_t hash = seed;

        for (size_t i = 0; i < size; ++i) {
            hash ^= ptr[i];
            hash *= prime;
        }

        return hash;
    }

    // MurmurHash3 32-bit
    static uint32_t murmur3_32(const void* data, size_t size, uint32_t seed = 0);

    // MurmurHash3 128-bit
    static void murmur3_128(const void* data, size_t size, uint32_t seed, uint64_t out[2]);

    // xxHash (32-bit)
    static uint32_t xxhash32(const void* data, size_t size, uint32_t seed = 0);

    // CityHash64
    static uint64_t cityHash64(const void* data, size_t size);

    // 组合哈希（用于 uint64_t）
    static uint64_t combineHashes(uint64_t h1, uint64_t h2) {
        return h1 ^ (h2 + 0x9e3779b97f4a7c15ull + (h1 << 6) + (h1 >> 2));
    }

    // 将哈希转换为十六进制字符串
    static std::string toHexString(const void* data, size_t size);
    static std::string toHexString(const std::vector<uint8_t>& data) {
        return toHexString(data.data(), data.size());
    }
};

//==============================================================================
// Base64 编解码
//==============================================================================

class Base64 {
public:
    static std::string encode(const void* data, size_t size);
    static std::string encode(const std::string& str) {
        return encode(str.data(), str.size());
    }
    static std::string encode(const std::vector<uint8_t>& data) {
        return encode(data.data(), data.size());
    }

    static std::vector<uint8_t> decode(const std::string& str);
    static std::string decodeToString(const std::string& str) {
        auto bytes = decode(str);
        return std::string(bytes.begin(), bytes.end());
    }

    // 获取编码后的长度
    static size_t encodedLength(size_t inputLength) {
        return ((inputLength + 2) / 3) * 4;
    }

    // 获取解码后的长度
    static size_t decodedLength(const std::string& encoded);
};

//==============================================================================
// Hex 编解码
//==============================================================================

class Hex {
public:
    // 编码为十六进制字符串
    static std::string encode(const void* data, size_t size);
    static std::string encode(const std::string& str) {
        return encode(str.data(), str.size());
    }
    static std::string encode(const std::vector<uint8_t>& data) {
        return encode(data.data(), data.size());
    }

    // 从十六进制字符串解码
    static std::vector<uint8_t> decode(const std::string& hex);
    static std::string decodeToString(const std::string& hex) {
        auto bytes = decode(hex);
        return std::string(bytes.begin(), bytes.end());
    }

    // 转大写
    static std::string toUpper(const std::string& hex);
    // 转小写
    static std::string toLower(const std::string& hex);
};

//==============================================================================
// 密码哈希（用于存储密码）
//==============================================================================

class PasswordHash {
public:
    // 使用 PBKDF2-HMAC-SHA256
    static std::string pbkdf2(const std::string& password,
                              const std::string& salt,
                              int iterations = 10000,
                              size_t keyLength = 32);

    // 生成随机盐
    static std::string generateSalt(size_t length = 16);

    // 生成密码哈希（包含盐）
    struct HashedPassword {
        std::string algorithm;
        int iterations;
        std::string salt;
        std::string hash;
    };

    static HashedPassword hash(const std::string& password,
                               int iterations = 10000);

    // 验证密码
    static bool verify(const std::string& password,
                       const HashedPassword& hashed);

    static bool verify(const std::string& password,
                       const std::string& storedHash);

    // 格式化存储
    static std::string format(const HashedPassword& hashed);
    static HashedPassword parse(const std::string& formatted);
};

//==============================================================================
// 简单加密（XOR）
//==============================================================================

class SimpleCipher {
public:
    // XOR 加密/解密
    static void xorCipher(const void* input, void* output, size_t size,
                         const std::string& key);
    static void xorCipher(const void* input, void* output, size_t size,
                         const std::vector<uint8_t>& key);
    static void xorCipher(const void* input, void* output, size_t size,
                         uint64_t key);

    // 重复密钥 XOR
    static std::vector<uint8_t> xorRepeatingKey(
        const std::vector<uint8_t>& data,
        const std::vector<uint8_t>& key);

    // 旋转加密
    static void rotateCipher(void* data, size_t size, int8_t amount);
    static void rotateCipher(std::string& str, int8_t amount);

    // 简单替换加密
    static std::string substitutionCipher(const std::string& input,
                                         const std::string& alphabet,
                                         const std::string& key);
    static std::string substitutionDecipher(const std::string& input,
                                         const std::string& key,
                                         const std::string& alphabet);
};

//==============================================================================
// 随机数生成
//==============================================================================

class SecureRandom {
public:
    static void initialize();

    // 生成随机字节
    static void nextBytes(void* buffer, size_t size);

    // 生成随机数
    static uint8_t nextUint8();
    static uint16_t nextUint16();
    static uint32_t nextUint32();
    static uint64_t nextUint64();

    // 生成范围内的随机数
    static uint32_t nextUint32(uint32_t min, uint32_t max);
    static uint64_t nextUint64(uint64_t min, uint64_t max);

    // 生成随机字符串
    static std::string nextString(size_t length,
                                  const std::string& charset =
                                  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");

    // 生成 UUID
    static std::string generateUUID();
    static std::string generateUUIDv4();

    // 生成 GUID (Microsoft 格式)
    static std::string generateGUID();
};

//==============================================================================
// HMAC
//==============================================================================

class HMAC {
public:
    // HMAC-MD5
    static std::string md5(const std::string& key, const void* data, size_t size);
    static std::string md5(const std::string& key, const std::string& str) {
        return md5(key, str.data(), str.size());
    }

    // HMAC-SHA1
    static std::string sha1(const std::string& key, const void* data, size_t size);
    static std::string sha1(const std::string& key, const std::string& str) {
        return sha1(key, str.data(), str.size());
    }

    // HMAC-SHA256
    static std::string sha256(const std::string& key, const void* data, size_t size);
    static std::string sha256(const std::string& key, const std::string& str) {
        return sha256(key, str.data(), str.size());
    }
};

//==============================================================================
// 数据校验
//==============================================================================

class Checksum {
public:
    // Adler-32
    static uint32_t adler32(const void* data, size_t size, uint32_t initial = 1);

    // CRC-32
    static uint32_t crc32(const void* data, size_t size, uint32_t initial = 0);

    // CRC-16
    static uint16_t crc16(const void* data, size_t size, uint16_t polynomial = 0x8005);

    // 简单校验和
    static uint8_t sum8(const void* data, size_t size);
    static uint16_t sum16(const void* data, size_t size);
    static uint32_t sum32(const void* data, size_t size);
};

//==============================================================================
// URL 编码
//==============================================================================

class URLCodec {
public:
    // URL 编码
    static std::string encode(const std::string& str);
    static std::string decode(const std::string& str);

    // 查询字符串构建
    static std::string buildQuery(const std::vector<std::pair<std::string, std::string>>& params);
    static std::vector<std::pair<std::string, std::string>> parseQuery(const std::string& query);
};

//==============================================================================
// 实现细节
//==============================================================================

namespace detail {

// SHA-256 实现
class SHA256 {
public:
    static constexpr size_t HASH_SIZE = 32;
    static constexpr size_t BLOCK_SIZE = 64;

    SHA256();
    void update(const void* data, size_t size);
    void finish(uint8_t hash[HASH_SIZE]);
    std::string finish();

private:
    void transform(const uint8_t block[BLOCK_SIZE]);

    uint64_t totalSize_;
    uint8_t buffer[BLOCK_SIZE];
    size_t bufferLen_;
    uint32_t state[8];
};

// SHA-1 实现
class SHA1 {
public:
    static constexpr size_t HASH_SIZE = 20;
    static constexpr size_t BLOCK_SIZE = 64;

    SHA1();
    void update(const void* data, size_t size);
    void finish(uint8_t hash[HASH_SIZE]);
    std::string finish();

private:
    void transform(const uint8_t block[BLOCK_SIZE]);

    uint64_t totalSize_;
    uint8_t buffer[BLOCK_SIZE];
    size_t bufferLen_;
    uint32_t state[5];
};

// MD5 实现
class MD5 {
public:
    static constexpr size_t HASH_SIZE = 16;
    static constexpr size_t BLOCK_SIZE = 64;

    MD5();
    void update(const void* data, size_t size);
    void finish(uint8_t hash[HASH_SIZE]);
    std::string finish();

private:
    void transform(const uint8_t block[BLOCK_SIZE]);

    uint64_t totalSize_;
    uint8_t buffer[BLOCK_SIZE];
    size_t bufferLen_;
    uint32_t state[4];
};

} // namespace detail

} // namespace utils
} // namespace apollo
