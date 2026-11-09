/**
 * @file crypto_encode.cpp
 * @brief Base64/Hex/URL 编解码
 */

#include "apollo/utils/crypto.h"

namespace apollo {
namespace utils {

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

} // namespace utils
} // namespace apollo
