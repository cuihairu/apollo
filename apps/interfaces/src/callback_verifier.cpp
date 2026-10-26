#include "interfaces/callback_verifier.hpp"

#include "apollo/utils/crypto.h"

#include <algorithm>
#include <cstdint>
#include <cctype>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>

namespace interfaces {
namespace {

constexpr std::string_view kSignatureScheme = "hmac-sha256=";
constexpr std::string_view kContentLength = "content-length";
constexpr int kUnauthorized = 401;
constexpr int kForbidden = 403;

std::string lower_hex(std::string_view value) {
    std::string out(value);
    std::transform(value.begin(), value.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

bool is_lower_hex(std::string_view value) {
    return value.size() == 64 &&
           std::all_of(value.begin(), value.end(), [](unsigned char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}

std::pair<std::optional<std::string>, bool> header(const HttpRequest& request,
                                                   std::string_view name) {
    std::optional<std::string> value;
    bool ambiguous = false;
    for (const auto& [header_name, header_value] : request.headers) {
        if (header_name == name) {
            if (value) {
                ambiguous = true;
            } else {
                value = header_value;
            }
        }
    }
    return {value, ambiguous};
}

std::optional<std::size_t> exact_size(std::string_view value) {
    if (value.empty()) {
        return std::nullopt;
    }
    const auto limit = std::numeric_limits<std::size_t>::max();
    std::size_t result = 0;
    for (unsigned char c : value) {
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
        const std::size_t digit = static_cast<std::size_t>(c - '0');
        // 溢出防护：位数超体长表示域直接判 malformed。不防回绕会产生
        // 小于原值的结果，声明体长与实收体的比对可被超长声明绕过。
        if (result > (limit - digit) / 10) {
            return std::nullopt;
        }
        result = result * 10 + digit;
    }
    return result;
}

bool constant_time_equal(std::string_view a, std::string_view b) {
    // OpenSSL CRYPTO_memcmp 口径：长度先比（digest 定长 64，不泄密），
    // 再全量异或累积、不早退——std::equal 会随首字节差异改变比较时长。
    bool equal = a.size() == b.size();
    unsigned char mismatch = 0;
    const std::size_t span = a.size() < b.size() ? a.size() : b.size();
    for (std::size_t i = 0; i < span; ++i) {
        mismatch |= static_cast<unsigned char>(a[i]) ^
                    static_cast<unsigned char>(b[i]);
    }
    return equal && mismatch == 0;
}

std::string signing_frame(std::string_view raw_request) {
    std::string_view raw = raw_request;
    std::size_t line_start = raw.find("\r\n");
    if (line_start == std::string_view::npos) {
        return std::string(raw);
    }
    line_start += 2;
    while (line_start < raw.size()) {
        const std::size_t line_end = raw.find("\r\n", line_start);
        if (line_end == std::string_view::npos) {
            break;
        }
        if (line_end == line_start) {
            return std::string(raw);
        }
        const std::string_view line = raw.substr(line_start, line_end - line_start);
        const std::size_t colon = line.find(':');
        if (colon != std::string_view::npos) {
            std::string name(line.substr(0, colon));
            std::transform(name.begin(), name.end(), name.begin(),
                           [](unsigned char c) {
                               return static_cast<char>(std::tolower(c));
                           });
            if (name == "x-signature") {
                std::string signed_frame(raw);
                signed_frame.erase(line_start, line_end + 2 - line_start);
                return signed_frame;
            }
        }
        line_start = line_end + 2;
    }
    return std::string(raw);
}

CallbackVerifier::Result malformed(CallbackVerifier::Reason reason) {
    return {false, reason, kForbidden};
}

} // namespace

CallbackVerifier::CallbackVerifier(std::string secret)
    : secret_(std::move(secret)) {}

CallbackVerifier::Result CallbackVerifier::verify(const HttpRequest& request) const {
    if (secret_.empty()) {
        return {false, Reason::SecretNotConfigured, kForbidden};
    }
    if (request.raw_request.empty()) {
        return malformed(Reason::RawFrameMismatch);
    }

    const auto [signature_header, signature_ambiguous] =
        header(request, "x-signature");
    if (signature_ambiguous) {
        return malformed(Reason::SignatureHeaderAmbiguous);
    }
    if (!signature_header) {
        return {false, Reason::SignatureHeaderMissing, kUnauthorized};
    }

    if (signature_header->size() <= kSignatureScheme.size() ||
        lower_hex(signature_header->substr(0, kSignatureScheme.size())) !=
            kSignatureScheme) {
        return malformed(Reason::SignatureHeaderMalformed);
    }
    const std::string provided =
        lower_hex(signature_header->substr(kSignatureScheme.size()));
    if (!is_lower_hex(provided)) {
        return malformed(Reason::SignatureMalformed);
    }

    const auto [content_length, content_length_ambiguous] =
        header(request, kContentLength);
    if (!content_length || content_length_ambiguous) {
        return malformed(Reason::ContentLengthMissing);
    }
    const auto declared = exact_size(*content_length);
    if (!declared || *declared != request.body.size()) {
        return malformed(Reason::ContentLengthMismatch);
    }

    const std::string raw(request.raw_request.begin(), request.raw_request.end());
    const std::string_view raw_view(raw);
    if (raw.size() < request.body.size() ||
        raw_view.substr(raw.size() - request.body.size()) !=
            std::string_view(reinterpret_cast<const char*>(request.body.data()),
                             request.body.size())) {
        return malformed(Reason::RawFrameMismatch);
    }
    const std::string frame = signing_frame(raw);
    const std::string expected =
        apollo::utils::HMAC::sha256(secret_, frame.data(), frame.size());
    return constant_time_equal(expected, provided)
               ? CallbackVerifier::Result{true, Reason::SignatureMismatch, 202}
               : CallbackVerifier::Result{false, Reason::SignatureMismatch,
                                          kUnauthorized};
}

} // namespace interfaces
