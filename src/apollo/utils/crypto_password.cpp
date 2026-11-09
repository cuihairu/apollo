/**
 * @file crypto_password.cpp
 * @brief PasswordHash（PBKDF2-HMAC-SHA256 口令散列）
 */

#include "apollo/utils/crypto.h"
#include <openssl/crypto.h>
#include <openssl/evp.h>

namespace apollo {
namespace utils {

//==============================================================================
// PasswordHash 实现（PBKDF2-HMAC-SHA256；存储格式：
//   $pbkdf2-sha256$<iterations>$<salt-hex>$<hash-hex>
//==============================================================================

std::string PasswordHash::pbkdf2(const std::string& password,
                                 const std::string& salt,
                                 int iterations,
                                 size_t keyLength) {
    if (iterations <= 0 || keyLength == 0) {
        return {};
    }
    std::string derived(keyLength, '\0');
    if (PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()),
                          reinterpret_cast<const unsigned char*>(salt.data()),
                          static_cast<int>(salt.size()),
                          iterations, EVP_sha256(),
                          static_cast<int>(keyLength),
                          reinterpret_cast<unsigned char*>(&derived[0])) != 1) {
        return {};
    }
    return derived;
}

std::string PasswordHash::generateSalt(size_t length) {
    if (length == 0) {
        return {};
    }
    std::string salt(length, '\0');
    SecureRandom::nextBytes(&salt[0], length);
    return salt;
}

PasswordHash::HashedPassword PasswordHash::hash(const std::string& password,
                                                int iterations) {
    HashedPassword out;
    out.algorithm = "pbkdf2-sha256";
    out.iterations = iterations > 0 ? iterations : 10000;
    out.salt = generateSalt(16);
    const std::string derived = pbkdf2(password, out.salt, out.iterations, 32);
    out.hash = Hash::toHexString(derived.data(), derived.size());
    return out;
}

bool PasswordHash::verify(const std::string& password,
                          const HashedPassword& hashed) {
    if (hashed.algorithm != "pbkdf2-sha256" || hashed.iterations <= 0) {
        return false;
    }
    const std::string derived = pbkdf2(password, hashed.salt,
                                       hashed.iterations, 32);
    if (derived.empty()) {
        return false;
    }
    const std::string derivedHex = Hash::toHexString(derived.data(), derived.size());
    return derivedHex.size() == hashed.hash.size() &&
           CRYPTO_memcmp(derivedHex.data(), hashed.hash.data(),
                         derivedHex.size()) == 0;
}

bool PasswordHash::verify(const std::string& password,
                          const std::string& storedHash) {
    const HashedPassword parsed = parse(storedHash);
    if (parsed.hash.empty()) {
        return false;
    }
    return verify(password, parsed);
}

std::string PasswordHash::format(const HashedPassword& hashed) {
    return "$" + hashed.algorithm + "$" + std::to_string(hashed.iterations) +
           "$" + Hash::toHexString(hashed.salt.data(), hashed.salt.size()) +
           "$" + hashed.hash;
}

PasswordHash::HashedPassword PasswordHash::parse(const std::string& formatted) {
    HashedPassword out;
    if (formatted.size() < 2 || formatted[0] != '$') {
        return out;
    }
    std::vector<std::string> parts;
    std::size_t start = 1;
    while (true) {
        const std::size_t pos = formatted.find('$', start);
        if (pos == std::string::npos) {
            parts.push_back(formatted.substr(start));
            break;
        }
        parts.push_back(formatted.substr(start, pos - start));
        start = pos + 1;
    }
    if (parts.size() != 4) {
        return out;
    }
    out.algorithm = parts[0];
    try {
        out.iterations = std::stoi(parts[1]);
    } catch (...) {
        return out;
    }
    out.salt = Hex::decodeToString(parts[2]);
    out.hash = parts[3];
    return out;
}

} // namespace utils
} // namespace apollo
