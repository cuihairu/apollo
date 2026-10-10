// 结构化行解析实现（写侧 = modules/core/log/src/structured.cpp 的镜像；
// 引号/转义规则两件同源 structured.h 头注，改须同步）。

#include "logger/structured_line.hpp"

#include <cstdlib>

namespace logger {
namespace {

// 数字键（ts/tick）：全数字才算命中——ts=abc 判非结构化行
bool parse_u64(const std::string& text, std::uint64_t* out) {
    if (text.empty()) {
        return false;
    }
    std::uint64_t value = 0;
    for (char c : text) {
        if (c < '0' || c > '9') {
            return false;
        }
        value = value * 10 + static_cast<std::uint64_t>(c - '0');
    }
    *out = value;
    return true;
}

} // namespace

std::string StructuredLine::src() const {
    for (const auto& kv : kv) {
        if (kv.first == "src") {
            return kv.second;
        }
    }
    return {};
}

bool parse_structured_line(const std::string& line, StructuredLine* out) {
    if (out == nullptr) {
        return false;
    }
    *out = StructuredLine{};

    bool have[6] = {false, false, false, false, false, false};
    std::size_t pos = 0;
    const std::size_t n = line.size();
    while (true) {
        while (pos < n && line[pos] == ' ') {
            ++pos;
        }
        if (pos >= n) {
            break;
        }
        // key 至 '='（key 内空白 = 非法形态）
        const std::size_t key_start = pos;
        while (pos < n && line[pos] != '=' && line[pos] != ' ') {
            ++pos;
        }
        if (pos >= n || line[pos] != '=') {
            return false;
        }
        const std::string key = line.substr(key_start, pos - key_start);
        ++pos;  // 过 '='

        std::string value;
        if (pos < n && line[pos] == '"') {
            ++pos;
            bool closed = false;
            while (pos < n) {
                const char c = line[pos];
                if (c == '\\') {
                    ++pos;
                    if (pos >= n) {
                        return false;  // 悬空转义（半行）
                    }
                    switch (line[pos]) {
                        case '"': value.push_back('"'); break;
                        case '\\': value.push_back('\\'); break;
                        case 'n': value.push_back('\n'); break;
                        case 'r': value.push_back('\r'); break;
                        case 't': value.push_back('\t'); break;
                        default: value.push_back(line[pos]); break;  // 宽容读
                    }
                    ++pos;
                } else if (c == '"') {
                    ++pos;
                    closed = true;
                    break;
                } else {
                    value.push_back(c);
                    ++pos;
                }
            }
            if (!closed) {
                return false;  // 未闭合引号（半行）
            }
        } else {
            while (pos < n && line[pos] != ' ') {
                value.push_back(line[pos]);
                ++pos;
            }
        }

        if (key == "ts") {
            if (!parse_u64(value, &out->ts)) {
                return false;
            }
            have[0] = true;
        } else if (key == "level") {
            out->level = value;
            have[1] = true;
        } else if (key == "proc") {
            out->proc = value;
            have[2] = true;
        } else if (key == "tick") {
            if (!parse_u64(value, &out->tick)) {
                return false;
            }
            have[3] = true;
        } else if (key == "cat") {
            out->cat = value;
            have[4] = true;
        } else if (key == "msg") {
            out->msg = value;
            have[5] = true;
        } else {
            out->kv.emplace_back(key, value);
        }
    }

    // 六键齐备才算结构化行（写侧恒产六键；缺任何一键 = foreign/半行）
    for (bool ok : have) {
        if (!ok) {
            return false;
        }
    }
    return true;
}

} // namespace logger
