#include "apollo/net/ws/handshake.hpp"

#include <openssl/evp.h>
#include <openssl/sha.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace apollo::net::ws {

namespace {

// 头部上限：升级请求头部很小，超上限即判坏（不无界攒缓冲）。
inline constexpr std::size_t kMaxHeaderBytes = 8 * 1024;

inline std::string to_lower(std::string_view text) {
    std::string out(text);
    for (auto& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

inline std::string trim(const std::string& text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && (text[begin] == ' ' || text[begin] == '\t')) {
        ++begin;
    }
    while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\t')) {
        --end;
    }
    return text.substr(begin, end - begin);
}

// key + magic GUID 的 SHA-1 base64（RFC 6455 §4.2.2）。
std::string base64_of_sha1(const std::string& client_key) {
    static const char kGuid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::vector<std::uint8_t> input;
    input.reserve(client_key.size() + sizeof kGuid - 1);
    input.insert(input.end(), client_key.begin(), client_key.end());
    input.insert(input.end(), kGuid, kGuid + sizeof kGuid - 1);

    std::uint8_t digest[EVP_MAX_MD_SIZE] = {0};
    std::uint32_t digest_len = 0;
    if (EVP_Digest(input.data(), input.size(), digest, &digest_len, EVP_sha1(),
                   nullptr) != 1) {
        return {};
    }

    const std::size_t out_size = 4 * ((digest_len + 2) / 3);
    std::string out(out_size, '\0');
    EVP_EncodeBlock(reinterpret_cast<std::uint8_t*>(out.data()), digest,
                    static_cast<int>(digest_len));
    return out;
}

}  // namespace

std::string compute_accept_key(const std::string& client_key) {
    if (client_key.empty()) {
        return {};
    }
    return base64_of_sha1(client_key);
}

bool is_valid_upgrade(const UpgradeRequest& request) {
    if (request.method != "GET" || request.version != 13 ||
        !request.upgrade_websocket || !request.connection_upgrade) {
        return false;
    }
    // key 须为 24 位 base64（16 字节原始值）。
    if (request.key.size() != 24) {
        return false;
    }
    for (const char c : request.key) {
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                        (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=';
        if (!ok) {
            return false;
        }
    }
    return true;
}

std::vector<std::uint8_t> make_accept_response(const std::string& client_key) {
    const std::string accept = compute_accept_key(client_key);
    std::string text = "HTTP/1.1 101 Switching Protocols\r\n"
                       "Upgrade: websocket\r\n"
                       "Connection: Upgrade\r\n"
                       "Sec-WebSocket-Accept: " +
                       accept + "\r\n"
                       "\r\n";
    return std::vector<std::uint8_t>(text.begin(), text.end());
}

std::vector<std::uint8_t> make_reject_response(const std::string& reason) {
    std::string text = "HTTP/1.1 400 Bad Request\r\n"
                       "Connection: close\r\n"
                       "Content-Type: text/plain; charset=utf-8\r\n"
                       "Content-Length: " +
                       std::to_string(reason.size()) + "\r\n"
                       "\r\n" + reason;
    return std::vector<std::uint8_t>(text.begin(), text.end());
}

// 头部终止与行分隔的模式（显式长度——std::search 不收字符串字面量）。
static constexpr char kTerminator[] = "\r\n\r\n";
static constexpr char kLineBreak[] = "\r\n";

void HandshakeParser::reset() {
    buf_.clear();
    request_ = UpgradeRequest{};
    done_ = false;
}

HandshakeParser::State HandshakeParser::feed(const std::uint8_t* data,
                                             std::size_t size) {
    if (done_) {
        return State::kReady;  // 只吃头部；头部之后的字节归帧层
    }
    buf_.insert(buf_.end(), data, data + size);
    if (buf_.size() > kMaxHeaderBytes) {
        return State::kBad;
    }
    if (std::search(buf_.begin(), buf_.end(), kTerminator, kTerminator + 4) ==
        buf_.end()) {
        return State::kNeedMore;
    }
    if (!parse_buffer()) {
        return State::kBad;
    }
    buf_.clear();
    done_ = true;
    return State::kReady;
}

bool HandshakeParser::parse_buffer() {
    const std::string_view raw(reinterpret_cast<const char*>(buf_.data()),
                               buf_.size());
    // 请求行终止是 2 字节 \r\n（4 字节终止符属最后一个头部行）。
    const auto line_end = raw.find(kLineBreak);
    if (line_end == std::string_view::npos) {
        return false;
    }
    const std::string_view first_line = raw.substr(0, line_end);
    const std::size_t first_space = first_line.find(' ');
    const std::size_t second_space = first_line.find(' ', first_space + 1);
    if (first_space == std::string_view::npos ||
        second_space == std::string_view::npos ||
        second_space == first_line.size() - 1) {
        return false;  // 请求行三段（method path version）缺一即判坏
    }
    request_.method = std::string(first_line.substr(0, first_space));
    request_.path = std::string(first_line.substr(first_space + 1,
                                                  second_space - first_space - 1));

    const auto after_first = raw.substr(line_end + 2);
    for (auto it = after_first.begin(); it != after_first.end();) {
        auto nl = std::search(it, after_first.end(), kLineBreak, kLineBreak + 2);
        if (nl == after_first.end()) {
            break;  // 尾部无终止：头部段已完整
        }
        const std::string_view line(it, nl);
        it = nl + 2;
        if (line.empty()) {
            continue;
        }
        const auto colon = line.find(':');
        if (colon == std::string_view::npos) {
            return false;  // 头部行不可解析
        }
        const std::string name = to_lower(line.substr(0, colon));
        const std::string value = trim(std::string(line.substr(colon + 1)));

        if (name == "sec-websocket-key") {
            request_.key = value;
        } else if (name == "sec-websocket-version") {
            request_.version = std::atoi(value.c_str());
        } else if (name == "upgrade") {
            request_.upgrade_websocket = to_lower(value).find("websocket") != std::string::npos;
        } else if (name == "connection") {
            const std::string lowered = to_lower(value);
            for (std::size_t start = 0; start <= lowered.size();) {
                const std::size_t comma = lowered.find(',', start);
                const std::string_view token(lowered.data() + start,
                                             (comma == std::string::npos ? lowered.size() : comma) - start);
                if (trim(std::string(token)) == "upgrade") {
                    request_.connection_upgrade = true;
                }
                start = comma == std::string::npos ? lowered.size() + 1 : comma + 1;
            }
        }
    }
    return true;
}

}  // namespace apollo::net::ws
