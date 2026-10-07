// 结构化行格式单测（P3-3 批 C，logging.md §5.1 外采契约）。
//
// 覆盖：
//   1. 稳定键集与序：ts= level= proc= tick= cat= msg= 六键固定序、ts 恒数字；
//   2. LogContext 注入：proc 启动期 / tick 每帧，缺省 "-" / 0；
//   3. 引号规则：安全字符不加引号、空白/=/控制符/UTF-8 进引号、
//      " \ \n \r \t 转义、空值 key=——含「单行保证」（无裸换行）；
//   4. kv 扩展段：键清洗（[A-Za-z0-9_.] 外替 _）、值同引号规则、段序尾随；
//   5. 溯源 src=<file>:<line>（LogRecord 带 file 时首随 kv 段）；
//   6. FileAppender structuredOutput 开关：结构化行落文件、默认人读不破坏；
//   7. logfmt 微解析器全行通过（filebeat/fluent-bit 类采集器可解析契约）。

#include "apollo/core/log/file_appender.h"
#include "apollo/core/log/log_level.h"
#include "apollo/core/log/log_record.h"
#include "apollo/core/log/structured.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#define TEST_ASSERT(cond, msg)                                                               \
    do {                                                                                     \
        if (!(cond)) {                                                                       \
            std::cerr << "FAIL: " << (msg) << " (" << __FILE__ << ":" << __LINE__ << ")"     \
                      << std::endl;                                                          \
            return false;                                                                    \
        }                                                                                    \
    } while (0)

namespace {

using apollo::core::log::FileAppender;
using apollo::core::log::FileAppenderConfig;
using apollo::core::log::LogContext;
using apollo::core::log::LogLevel;
using apollo::core::log::LogRecord;
using apollo::core::log::LogRotationMode;
using apollo::core::log::to_structured_line;

// logfmt 微解析器：取 key 的值（支持双引号转义），未命中返回 false。
// 顺带充当「可解析性」断言——行无法解析 = 外采契约破。
bool extractValue(const std::string& line, const std::string& key, std::string& out) {
    std::size_t pos = 0;
    while (pos < line.size()) {
        // 跳过空白
        while (pos < line.size() && line[pos] == ' ') {
            ++pos;
        }
        if (pos >= line.size()) {
            return false;
        }
        // 读 key 至 '='
        std::size_t key_start = pos;
        while (pos < line.size() && line[pos] != '=' && line[pos] != ' ') {
            ++pos;
        }
        if (pos >= line.size() || line[pos] != '=') {
            return false;  // 空值键未实现（本契约键恒带值）
        }
        const std::string k = line.substr(key_start, pos - key_start);
        ++pos;  // 跳过 '='
        // 读值
        std::string value;
        if (pos < line.size() && line[pos] == '"') {
            ++pos;
            while (pos < line.size()) {
                const char c = line[pos];
                if (c == '\\' && pos + 1 < line.size()) {
                    const char next = line[pos + 1];
                    if (next == 'n') {
                        value.push_back('\n');
                    } else if (next == 'r') {
                        value.push_back('\r');
                    } else if (next == 't') {
                        value.push_back('\t');
                    } else {
                        value.push_back(next);  // 还原转义对（引号与反斜杠）
                    }
                    pos += 2;
                    continue;
                }
                if (c == '"') {
                    ++pos;
                    break;
                }
                value.push_back(c);
                ++pos;
            }
        } else {
            const std::size_t value_start = pos;
            while (pos < line.size() && line[pos] != ' ') {
                ++pos;
            }
            value = line.substr(value_start, pos - value_start);
        }
        if (k == key) {
            out = value;
            return true;
        }
    }
    return false;
}

bool allDigits(const std::string& s) {
    if (s.empty()) {
        return false;
    }
    for (char c : s) {
        if (c < '0' || c > '9') {
            return false;
        }
    }
    return true;
}

bool test_basic_key_order() {
    LogContext::set_process("");
    LogContext::set_tick(0);
    const LogRecord r(LogLevel::Info, "hello");
    const std::string line = to_structured_line(r);
    // 稳定键序：ts → level → proc → tick → cat → msg（逐键前缀断言）
    TEST_ASSERT(line.rfind("ts=", 0) == 0, "ts 首键");
    std::string v;
    TEST_ASSERT(extractValue(line, "ts", v) && allDigits(v), "ts 恒数字");
    TEST_ASSERT(extractValue(line, "level", v) && v == "INFO", "level 口径");
    TEST_ASSERT(extractValue(line, "proc", v) && v == "-", "proc 缺省 -");
    TEST_ASSERT(extractValue(line, "tick", v) && v == "0", "tick 缺省 0");
    TEST_ASSERT(extractValue(line, "cat", v) && v == "-", "cat 空缺省 -");
    TEST_ASSERT(extractValue(line, "msg", v) && v == "hello", "msg 值");
    // 键序断言：ts 出现于 level 之前，msg 恒尾（无 kv 时）
    const auto ts_pos = line.find("ts=");
    const auto level_pos = line.find(" level=");
    const auto tick_pos = line.find(" tick=");
    const auto cat_pos = line.find(" cat=");
    const auto msg_pos = line.find(" msg=");
    TEST_ASSERT(ts_pos == 0 && ts_pos < level_pos && level_pos < line.find(" proc=") &&
                    line.find(" proc=") < tick_pos && tick_pos < cat_pos && cat_pos < msg_pos,
                "六键固定序");
    TEST_ASSERT(msg_pos + 5 + v.size() == line.size(), "msg 尾键（无 src/kv）");
    return true;
}

bool test_context_injection() {
    LogContext::set_process("cell-app-7");
    LogContext::set_tick(42);
    const LogRecord r(LogLevel::Warning, "frame");
    const std::string line = to_structured_line(r);
    std::string v;
    TEST_ASSERT(extractValue(line, "proc", v) && v == "cell-app-7", "proc 注入");
    TEST_ASSERT(extractValue(line, "tick", v) && v == "42", "tick 注入");
    TEST_ASSERT(extractValue(line, "level", v) && v == "WARN", "level toString 口径");
    LogContext::set_process("");
    LogContext::set_tick(0);
    return true;
}

bool test_quoting_rules() {
    const std::string raw = to_structured_line(LogRecord(LogLevel::Info, "plain_word"));
    TEST_ASSERT(raw.find("msg=plain_word") != std::string::npos, "安全字符不加引号");

    const std::string spaced = to_structured_line(LogRecord(LogLevel::Info, "hello world"));
    TEST_ASSERT(spaced.find("msg=\"hello world\"") != std::string::npos, "空白进引号");

    const std::string eq = to_structured_line(LogRecord(LogLevel::Info, "a=b"));
    TEST_ASSERT(eq.find("msg=\"a=b\"") != std::string::npos, "'=' 进引号（防键值歧义）");

    const std::string empty = to_structured_line(LogRecord(LogLevel::Info, ""));
    TEST_ASSERT(empty.find("msg=") != std::string::npos, "空值 key= 紧跟");

    std::string v;
    TEST_ASSERT(extractValue(empty, "msg", v) && v.empty(), "空值解析回空串");

    // 转义：嵌套引号与反斜杠
    const std::string quoted = to_structured_line(LogRecord(LogLevel::Info, "say \"hi\""));
    TEST_ASSERT(quoted.find("msg=\"say \\\"hi\\\"\"") != std::string::npos, "双引号转义");
    TEST_ASSERT(extractValue(quoted, "msg", v) && v == "say \"hi\"", "解析还原嵌套引号");

    const std::string backslash = to_structured_line(LogRecord(LogLevel::Info, "c:\\path"));
    TEST_ASSERT(backslash.find("msg=\"c:\\\\path\"") != std::string::npos, "反斜杠转义");
    TEST_ASSERT(extractValue(backslash, "msg", v) && v == "c:\\path", "解析还原反斜杠");

    // 单行保证：换行进消息 → 行内无裸 '\n'
    const std::string multiline = to_structured_line(LogRecord(LogLevel::Error, "line1\nline2"));
    TEST_ASSERT(multiline.find('\n') == std::string::npos, "无裸换行（tail 行式不断裂）");
    TEST_ASSERT(multiline.find("\\n") != std::string::npos, "换行转义为 \\n");
    TEST_ASSERT(extractValue(multiline, "msg", v) && v == "line1\nline2", "解析还原换行");

    // UTF-8（CJK）：进引号且原样保留
    const std::string cjk = to_structured_line(LogRecord(LogLevel::Info, "启动完成"));
    TEST_ASSERT(cjk.find("msg=\"启动完成\"") != std::string::npos, "UTF-8 进引号原样");
    TEST_ASSERT(extractValue(cjk, "msg", v) && v == "启动完成", "解析还原 UTF-8");
    return true;
}

bool test_kv_extension_and_src() {
    LogRecord r(LogLevel::Error, "boom", "root", "path/file.cpp", 42, "fn");
    r.setKv("op", "q");
    r.setKv("bad key!", "v");
    r.setKv("note", "a b");
    const std::string line = to_structured_line(r);

    std::string v;
    TEST_ASSERT(extractValue(line, "src", v) && v == "path/file.cpp:42", "src 溯源首随 msg");
    TEST_ASSERT(extractValue(line, "op", v) && v == "q", "kv 原样键值");
    TEST_ASSERT(extractValue(line, "bad_key_", v) && v == "v", "非法键字符清洗为 _");
    TEST_ASSERT(extractValue(line, "note", v) && v == "a b", "kv 值引号规则同 msg");

    // 键序：src 在 msg 之后、kv 尾随
    const auto msg_pos = line.find(" msg=");
    const auto src_pos = line.find(" src=");
    const auto op_pos = line.find(" op=");
    TEST_ASSERT(msg_pos < src_pos && src_pos < op_pos, "msg → src → kv 段序");
    return true;
}

bool test_file_appender_structured_output() {
    namespace fs = std::filesystem;
    const std::string testDir = "test_logs_structured";
    try {
        fs::remove_all(testDir);
    } catch (...) {
    }

    LogContext::set_process("login-app-1");
    LogContext::set_tick(7);

    std::string structured_path;
    {
        FileAppenderConfig cfg;
        cfg.directory = testDir;
        cfg.baseName = "structured";
        cfg.extension = "log";
        cfg.rotationMode = LogRotationMode::None;
        cfg.structuredOutput = true;
        FileAppender appender(cfg);
        appender.append(LogRecord(LogLevel::Info, "started"));
        structured_path = appender.getConfig().directory + "/" +
                          appender.getConfig().baseName + "." +
                          appender.getConfig().extension;
    }
    // 人读路径不受影响（默认关）
    std::string human_path;
    {
        FileAppenderConfig cfg;
        cfg.directory = testDir;
        cfg.baseName = "human";
        cfg.extension = "log";
        cfg.rotationMode = LogRotationMode::None;
        FileAppender appender(cfg);
        appender.append(LogRecord(LogLevel::Info, "started"));
        human_path = appender.getConfig().directory + "/" +
                     appender.getConfig().baseName + "." +
                     appender.getConfig().extension;
    }
    LogContext::set_process("");
    LogContext::set_tick(0);

    auto readFirstLine = [](const std::string& path, std::string& out) -> bool {
        std::ifstream in(path);
        if (!in) {
            return false;
        }
        std::getline(in, out);
        return !out.empty();
    };

    std::string line;
    TEST_ASSERT(readFirstLine(structured_path, line), "结构化文件可读");
    std::string v;
    TEST_ASSERT(extractValue(line, "proc", v) && v == "login-app-1", "落盘 proc 注入");
    TEST_ASSERT(extractValue(line, "tick", v) && v == "7", "落盘 tick 注入");
    TEST_ASSERT(extractValue(line, "msg", v) && v == "started", "落盘 msg");

    std::string human;
    TEST_ASSERT(readFirstLine(human_path, human), "人读文件可读");
    TEST_ASSERT(human.find("ts=") == std::string::npos, "默认人读格式无结构化键");
    TEST_ASSERT(human.find("started") != std::string::npos, "人读消息在行");

    try {
        fs::remove_all(testDir);
    } catch (...) {
    }
    return true;
}

} // namespace

int main() {
    int passed = 0;
    int failed = 0;

    struct Case {
        const char* name;
        bool (*fn)();
    } cases[] = {
        {"basic_key_order", test_basic_key_order},
        {"context_injection", test_context_injection},
        {"quoting_rules", test_quoting_rules},
        {"kv_extension_and_src", test_kv_extension_and_src},
        {"file_appender_structured_output", test_file_appender_structured_output},
    };

    for (const auto& c : cases) {
        std::cout << "[ RUN  ] " << c.name << std::endl;
        if (c.fn()) {
            std::cout << "[  OK  ] " << c.name << std::endl;
            ++passed;
        } else {
            std::cout << "[ FAIL ] " << c.name << std::endl;
            ++failed;
        }
    }

    std::cout << "LogStructuredTests: " << passed << " passed, " << failed << " failed"
              << std::endl;
    return failed == 0 ? 0 : 1;
}
