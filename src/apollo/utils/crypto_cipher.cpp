/**
 * @file crypto_cipher.cpp
 * @brief SimpleCipher 对称简化加密与 HMAC-SHA256
 */

#include "apollo/utils/crypto.h"
#include <array>
#include <cstring>
#include <openssl/evp.h>

namespace apollo {
namespace utils {

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

} // namespace utils
} // namespace apollo
