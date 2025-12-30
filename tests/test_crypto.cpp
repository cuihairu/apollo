/**
 * @file test_crypto.cpp
 * @brief Crypto/Hash/Checksum/URLCodec 单元测试
 */

#include "apollo/utils/crypto.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

using namespace apollo::utils;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

static bool test_hash_vectors() {
    std::cout << "Running: test_hash_vectors..." << std::endl;

    TEST_ASSERT(Hash::md5("abc") == "900150983cd24fb0d6963f7d28e17f72", "MD5 abc");
    TEST_ASSERT(Hash::sha1("abc") == "a9993e364706816aba3e25717850c26c9cd0d89d", "SHA1 abc");
    TEST_ASSERT(Hash::sha256("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA256 abc");

    TEST_ASSERT(Hash::fnv1a32("hello", 5) == 0x4f9f2cabu, "FNV1a32 hello");
    TEST_ASSERT(Hash::fnv1a64("hello", 5) == 0xa430d84680aabd0bull, "FNV1a64 hello");

    // Sanity: same input => same hash, different seed => usually different
    TEST_ASSERT(Hash::murmur3_32("hello", 5, 0) == Hash::murmur3_32("hello", 5, 0), "Murmur3 stable");
    TEST_ASSERT(Hash::murmur3_32("hello", 5, 1) != Hash::murmur3_32("hello", 5, 0), "Murmur3 seed affects");

    // Use aligned input to avoid unaligned loads inside implementation.
    alignas(8) const uint64_t blocks[2] = {0x0123456789abcdefull, 0xfedcba9876543210ull};
    uint64_t out0[2] = {0, 0};
    uint64_t out1[2] = {0, 0};
    Hash::murmur3_128(blocks, sizeof(blocks), 0, out0);
    Hash::murmur3_128(blocks, sizeof(blocks), 1, out1);
    TEST_ASSERT(out0[0] != 0 || out0[1] != 0, "Murmur3_128 non-zero");
    TEST_ASSERT(out0[0] != out1[0] || out0[1] != out1[1], "Murmur3_128 seed affects");

    // CityHash64 is currently implemented as FNV-1a 64-bit.
    TEST_ASSERT(Hash::cityHash64("hello", 5) == Hash::fnv1a64("hello", 5), "CityHash64 fallback");

    std::cout << "  PASSED" << std::endl;
    return true;
}

static bool test_base64_hex() {
    std::cout << "Running: test_base64_hex..." << std::endl;

    TEST_ASSERT(Base64::encode("") == "", "Base64 encode empty");
    TEST_ASSERT(Base64::encode("hello") == "aGVsbG8=", "Base64 encode hello");
    TEST_ASSERT(Base64::decodeToString("aGVsbG8=") == "hello", "Base64 decode hello");
    TEST_ASSERT(Base64::decodeToString("aGVsbG8=\n") == "hello", "Base64 decode with newline");
    TEST_ASSERT(Base64::decode("not-base64!") .empty(), "Base64 invalid returns empty");
    TEST_ASSERT(Base64::decodedLength("aGVsbG8=") == 5, "Base64 decodedLength");

    TEST_ASSERT(Hex::encode("hello") == "68656c6c6f", "Hex encode hello");
    TEST_ASSERT(Hex::decodeToString("68656c6c6f") == "hello", "Hex decode hello");
    TEST_ASSERT(Hex::decode("68656c6c6fzz").empty(), "Hex invalid returns empty");
    TEST_ASSERT(Hex::toUpper("a1b2c3def") == "A1B2C3DEF", "Hex toUpper");
    TEST_ASSERT(Hex::toLower("A1B2C3DEF") == "a1b2c3def", "Hex toLower");

    std::cout << "  PASSED" << std::endl;
    return true;
}

static bool test_simple_cipher() {
    std::cout << "Running: test_simple_cipher..." << std::endl;

    const std::string input = "ApolloFramework";
    std::string out(input.size(), '\0');
    std::string back(input.size(), '\0');

    SimpleCipher::xorCipher(input.data(), out.data(), out.size(), std::string("key"));
    TEST_ASSERT(out != input, "XOR changes content");
    SimpleCipher::xorCipher(out.data(), back.data(), back.size(), std::string("key"));
    TEST_ASSERT(back == input, "XOR reversible");

    // Empty key should keep data unchanged (and allow in-place).
    std::string inPlace = input;
    SimpleCipher::xorCipher(inPlace.data(), inPlace.data(), inPlace.size(), std::string());
    TEST_ASSERT(inPlace == input, "XOR empty key no-op");

    std::vector<uint8_t> bytes = {1, 2, 3, 4, 5};
    auto xored = SimpleCipher::xorRepeatingKey(bytes, {0xFF});
    auto unxored = SimpleCipher::xorRepeatingKey(xored, {0xFF});
    TEST_ASSERT(unxored == bytes, "xorRepeatingKey reversible");

    std::string rotated = "abcXYZ";
    SimpleCipher::rotateCipher(rotated, 3);
    SimpleCipher::rotateCipher(rotated, -3);
    TEST_ASSERT(rotated == "abcXYZ", "rotateCipher reversible");

    const std::string alphabet = "abcdefghijklmnopqrstuvwxyz";
    const std::string key =      "zyxwvutsrqponmlkjihgfedcba";
    auto cipher = SimpleCipher::substitutionCipher("abc xyz", alphabet, key);
    auto plain = SimpleCipher::substitutionDecipher(cipher, key, alphabet);
    TEST_ASSERT(plain == "abc xyz", "substitution cipher roundtrip");

    std::cout << "  PASSED" << std::endl;
    return true;
}

static bool test_secure_random_uuid_format() {
    std::cout << "Running: test_secure_random_uuid_format..." << std::endl;

    auto uuid = SecureRandom::generateUUID();
    TEST_ASSERT(uuid.size() == 36, "UUID length");
    TEST_ASSERT(uuid[8] == '-' && uuid[13] == '-' && uuid[18] == '-' && uuid[23] == '-', "UUID hyphens");
    TEST_ASSERT(uuid[14] == '4', "UUIDv4 version nibble");
    TEST_ASSERT(uuid[19] == '8' || uuid[19] == '9' || uuid[19] == 'a' || uuid[19] == 'b', "UUID variant nibble");

    auto guid = SecureRandom::generateGUID();
    TEST_ASSERT(guid.size() == 38, "GUID length");
    TEST_ASSERT(guid.front() == '{' && guid.back() == '}', "GUID braces");

    std::cout << "  PASSED" << std::endl;
    return true;
}

static bool test_hmac_checksum_urlcodec() {
    std::cout << "Running: test_hmac_checksum_urlcodec..." << std::endl;

    // HMAC-SHA256 test vector
    const std::string msg = "The quick brown fox jumps over the lazy dog";
    const std::string expected = "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8";
    TEST_ASSERT(HMAC::sha256("key", msg.data(), msg.size()) == expected, "HMAC-SHA256");

    TEST_ASSERT(Checksum::adler32("Wikipedia", 9) == 0x11e60398u, "Adler32 Wikipedia");
    TEST_ASSERT(Checksum::crc32("123456789", 9) == 0xcbf43926u, "CRC32 123456789");
    TEST_ASSERT(Checksum::crc16("123456789", 9) == 0xfee8u, "CRC16 123456789");

    const uint8_t sums[] = {1, 2, 3, 4};
    TEST_ASSERT(Checksum::sum8(sums, sizeof(sums)) == 10, "sum8");
    TEST_ASSERT(Checksum::sum16(sums, sizeof(sums)) == 0x0102 + 0x0304, "sum16");

    auto encoded = URLCodec::encode("a b&c=d");
    TEST_ASSERT(encoded == "a+b%26c%3Dd", "URL encode");
    TEST_ASSERT(URLCodec::decode(encoded) == "a b&c=d", "URL decode");

    std::vector<std::pair<std::string, std::string>> params = {{"a b", "c=d"}, {"k", "v"}};
    auto query = URLCodec::buildQuery(params);
    auto parsed = URLCodec::parseQuery(query);
    TEST_ASSERT(parsed.size() == 2, "Query parse size");
    TEST_ASSERT(parsed[0].first == "a b" && parsed[0].second == "c=d", "Query parse kv1");
    TEST_ASSERT(parsed[1].first == "k" && parsed[1].second == "v", "Query parse kv2");

    std::cout << "  PASSED" << std::endl;
    return true;
}

int main() {
    std::cout << "=== Crypto Unit Tests ===" << std::endl;
    int passed = 0;
    int total = 0;

    auto run = [&](const char* name, bool (*test)()) {
        std::cout << std::endl;
        std::cout << "== " << name << " ==" << std::endl;
        ++total;
        if (test()) {
            ++passed;
        }
    };

    run("test_hash_vectors", test_hash_vectors);
    run("test_base64_hex", test_base64_hex);
    run("test_simple_cipher", test_simple_cipher);
    run("test_secure_random_uuid_format", test_secure_random_uuid_format);
    run("test_hmac_checksum_urlcodec", test_hmac_checksum_urlcodec);

    std::cout << std::endl;
    std::cout << "=== Results: " << passed << "/" << total << " passed ===" << std::endl;
    return (passed == total) ? 0 : 1;
}

