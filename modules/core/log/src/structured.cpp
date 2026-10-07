/**
 * @file structured.cpp
 * @brief 结构化行格式化与进程级日志上下文（logging.md §5.1 P3 批）
 */

#include "apollo/core/log/structured.h"
#include "apollo/core/log/log_level.h"

#include <atomic>
#include <chrono>
#include <mutex>

namespace apollo {
namespace core {
namespace log {

namespace {

std::mutex g_process_mutex;
std::string g_process;
std::atomic<std::uint64_t> g_tick{0};

// 值安全字符集：[A-Za-z0-9_.:/@+-]。其外一律需引号（含空白、=、"、\、
// 控制符与多字节 UTF-8 —— UTF-8 进引号内原样保留）。
bool isSafeValueChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '_' || c == '.' || c == ':' || c == '/' || c == '@' || c == '+' || c == '-';
}

bool needsQuote(const std::string& value) {
    if (value.empty()) {
        return false;  // 空值紧跟 key=，不加引号
    }
    for (char c : value) {
        if (!isSafeValueChar(c)) {
            return true;
        }
    }
    return false;
}

std::string escapeQuoted(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 2);
    out.push_back('"');
    for (char c : value) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20 || c == 0x7f) {
                    out.push_back('?');  // 其余控制符降级占位，行式 tail 不被截断
                } else {
                    out.push_back(c);
                }
        }
    }
    out.push_back('"');
    return out;
}

std::string formatValue(const std::string& value) {
    if (value.empty() || !needsQuote(value)) {
        return value;
    }
    return escapeQuoted(value);
}

// 键清洗：[A-Za-z0-9_.] 之外替为 '_'，空键为 '_'（键集稳定是外采索引契约）。
std::string sanitizeKey(const std::string& key) {
    if (key.empty()) {
        return "_";
    }
    std::string out = key;
    for (char& c : out) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '_' || c == '.';
        if (!ok) {
            c = '_';
        }
    }
    return out;
}

} // namespace

//==============================================================================
// LogContext 实现
//==============================================================================

void LogContext::set_process(std::string proc) {
    std::lock_guard<std::mutex> lock(g_process_mutex);
    g_process = std::move(proc);
}

std::string LogContext::process() {
    std::lock_guard<std::mutex> lock(g_process_mutex);
    return g_process;
}

void LogContext::set_tick(std::uint64_t tick) noexcept {
    g_tick.store(tick, std::memory_order_relaxed);
}

std::uint64_t LogContext::tick() noexcept {
    return g_tick.load(std::memory_order_relaxed);
}

//==============================================================================
// 结构化行格式化
//==============================================================================

std::string to_structured_line(const LogRecord& record) {
    const auto wall_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                             record.getTimestamp().time_since_epoch())
                             .count();

    std::string out;
    out.reserve(96 + record.getMessage().size());
    out += "ts=";
    out += std::to_string(wall_ms);
    out += " level=";
    out += toString(record.getLevel());

    const std::string proc = LogContext::process();
    out += " proc=";
    out += formatValue(proc.empty() ? std::string("-") : proc);
    out += " tick=";
    out += std::to_string(LogContext::tick());

    out += " cat=";
    out += formatValue(record.getLoggerName().empty() ? std::string("-")
                                                      : record.getLoggerName());
    out += " msg=";
    out += formatValue(record.getMessage());

    if (!record.getFile().empty()) {
        out += " src=";
        out += formatValue(record.getFile() + ":" + std::to_string(record.getLine()));
    }
    for (const auto& kv : record.getKv()) {
        out.push_back(' ');
        out += sanitizeKey(kv.first);
        out.push_back('=');
        out += formatValue(kv.second);
    }
    return out;
}

} // namespace log
} // namespace core
} // namespace apollo
