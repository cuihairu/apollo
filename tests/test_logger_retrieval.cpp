// LoggerApp 检索面测试（ADR-013 批 L3，logger-app.md §6）：
//   结构化行读侧（写侧契约镜像）、检索过滤（proc/level/ts/cat）、
//   目录摄取（FileAppender structuredOutput → 落盘 → ingest 全链）、
//   push 接缝空推（v1 留接口不实现）。
// 直编惯例同 scene_tests（无 GTest，plain-assert + main 汇总）。
#include "logger/logger_app.hpp"
#include "logger/push_sink.hpp"
#include "logger/structured_line.hpp"

#include "apollo/core/log/file_appender.h"
#include "apollo/core/log/log_level.h"
#include "apollo/core/log/log_record.h"
#include "apollo/core/log/structured.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#define TEST_ASSERT(cond, msg)                                                          \
    do {                                                                                \
        if (!(cond)) {                                                                  \
            std::cout << "FAIL: " << (msg) << " (" << #cond << ") at "                  \
                      << __LINE__ << std::endl;                                         \
            ++g_failures;                                                               \
            return false;                                                               \
        }                                                                               \
    } while (0)

namespace {

int g_failures = 0;

using logger::IngestStats;
using logger::IPushSink;
using logger::level_rank;
using logger::LoggerApp;
using logger::NullPushSink;
using logger::parse_structured_line;
using logger::RetrievalFilter;
using logger::StructuredLine;

using apollo::core::log::FileAppender;
using apollo::core::log::FileAppenderConfig;
using apollo::core::log::LogContext;
using apollo::core::log::LogLevel;
using apollo::core::log::LogRecord;
using apollo::core::log::to_structured_line;

// ---------- 读侧：写侧行解析回读 ----------

bool test_parse_round_trip_plain() {
    LogContext::set_process("cell");
    LogContext::set_tick(7);
    LogRecord record(LogLevel::Info, "starting server", "cell");
    const std::string line = to_structured_line(record);

    StructuredLine parsed;
    TEST_ASSERT(parse_structured_line(line, &parsed), "plain line parses");
    TEST_ASSERT(parsed.level == "INFO", "level round trip");
    TEST_ASSERT(parsed.proc == "cell", "proc round trip");
    TEST_ASSERT(parsed.tick == 7, "tick round trip");
    TEST_ASSERT(parsed.cat == "cell", "cat round trip");
    TEST_ASSERT(parsed.msg == "starting server", "msg round trip");
    TEST_ASSERT(parsed.ts > 0, "ts positive");
    TEST_ASSERT(parsed.kv.empty(), "no kv without src");
    return true;
}

bool test_parse_round_trip_quoted_and_kv() {
    // 值含空白/=/引号/中文 → 写侧带引号 + 转义；kv 扩展段含 src
    LogContext::set_process("manager");
    LogContext::set_tick(0);
    LogRecord record(LogLevel::Warning,
                     "收敛开放：reported=2/3 (note a=\"q\" b=\\c)", "recovery");
    record.setLocation("src/main.cpp", 42, "fn");
    record.setKv("extra key", "x y");
    const std::string line = to_structured_line(record);

    StructuredLine parsed;
    TEST_ASSERT(parse_structured_line(line, &parsed), "quoted line parses");
    TEST_ASSERT(parsed.level == "WARN", "warn level");
    TEST_ASSERT(parsed.proc == "manager", "proc");
    TEST_ASSERT(parsed.cat == "recovery", "cat");
    TEST_ASSERT(parsed.msg == "收敛开放：reported=2/3 (note a=\"q\" b=\\c)",
                "msg escapes round trip");
    TEST_ASSERT(parsed.src() == "src/main.cpp:42", "src kv round trip");
    TEST_ASSERT(parsed.kv.size() == 2, "two kv pairs");
    // 键由写侧清洗（[A-Za-z0-9_.] 外替 _，§5.1 键集稳定契约）；读侧保真读
    TEST_ASSERT(parsed.kv.back().first == "extra_key", "kv key sanitized");
    TEST_ASSERT(parsed.kv.back().second == "x y", "kv value round trip");
    return true;
}

bool test_parse_control_and_empty_values() {
    // 控制符降级占位（\n → 占位后读侧原样）、空值 key=、
    // 制表符转义还原
    LogRecord record(LogLevel::Error, "a\tb", "t");
    const std::string line = to_structured_line(record);
    StructuredLine parsed;
    TEST_ASSERT(parse_structured_line(line, &parsed), "tab-escaped parses");
    TEST_ASSERT(parsed.msg == "a\tb", "tab round trip");

    // 空值：手工行（msg= 紧跟空白）——写侧空值不加引号
    const std::string empty_msg =
        "ts=1 level=INFO proc=p tick=0 cat=c msg= src=f:1";
    TEST_ASSERT(parse_structured_line(empty_msg, &parsed), "empty msg parses");
    TEST_ASSERT(parsed.msg.empty(), "empty msg value");
    TEST_ASSERT(parsed.src() == "f:1", "src after empty msg");
    return true;
}

bool test_parse_malformed_lines() {
    StructuredLine parsed;
    TEST_ASSERT(!parse_structured_line("", &parsed), "empty line rejected");
    TEST_ASSERT(!parse_structured_line("not structured at all", &parsed),
                "plain text rejected");
    TEST_ASSERT(!parse_structured_line("ts=1 level=INFO proc=p tick=0 cat=c",
                                       &parsed),
                "five keys rejected");
    TEST_ASSERT(!parse_structured_line(
                    "ts=abc level=INFO proc=p tick=0 cat=c msg=m", &parsed),
                "non-numeric ts rejected");
    TEST_ASSERT(
        !parse_structured_line("ts=1 level=INFO proc=p tick=0 cat=c msg=\"open",
                               &parsed),
        "unterminated quote rejected");
    TEST_ASSERT(!parse_structured_line("ts=", &parsed), "key only rejected");
    TEST_ASSERT(!parse_structured_line(
                    "ts=1 level=INFO proc=p tick=0 msg=m tick=1", &parsed),
                "missing cat rejected");  // cat 缺席 = 六键不齐（tick 二现不算数）
    return true;
}

// ---------- 检索过滤 ----------

bool test_level_rank() {
    TEST_ASSERT(level_rank("DEBUG") == 0, "debug rank");
    TEST_ASSERT(level_rank("INFO") == 1, "info rank");
    TEST_ASSERT(level_rank("WARN") == 2, "warn rank");
    TEST_ASSERT(level_rank("ERROR") == 3, "error rank");
    TEST_ASSERT(level_rank("CRITICAL") == 4, "critical rank");
    TEST_ASSERT(level_rank("UNKNOWN") == -1, "unknown rank");
    return true;
}

StructuredLine make_line(std::uint64_t ts, const char* level, const char* proc,
                         const char* cat) {
    StructuredLine line;
    line.ts = ts;
    line.level = level;
    line.proc = proc;
    line.tick = 0;
    line.cat = cat;
    line.msg = "m";
    return line;
}

bool test_filter_dimensions() {
    RetrievalFilter filter;
    TEST_ASSERT(filter.passes(make_line(100, "INFO", "cell", "cell")),
                "empty filter passes all");

    filter.min_level = level_rank("WARN");
    TEST_ASSERT(!filter.passes(make_line(100, "INFO", "cell", "cell")),
                "info below warn threshold");
    TEST_ASSERT(filter.passes(make_line(100, "WARN", "cell", "cell")),
                "warn at threshold");
    TEST_ASSERT(filter.passes(make_line(100, "ERROR", "cell", "cell")),
                "error above threshold");

    RetrievalFilter proc_filter;
    proc_filter.proc = "cell";
    TEST_ASSERT(proc_filter.passes(make_line(1, "INFO", "cell", "x")),
                "proc match");
    TEST_ASSERT(!proc_filter.passes(make_line(1, "INFO", "base", "x")),
                "proc mismatch");

    RetrievalFilter cat_filter;
    cat_filter.cat = "recovery";
    TEST_ASSERT(cat_filter.passes(make_line(1, "INFO", "x", "recovery")),
                "cat match");
    TEST_ASSERT(!cat_filter.passes(make_line(1, "INFO", "x", "mirror")),
                "cat mismatch");

    RetrievalFilter time_filter;
    time_filter.since_ms = 100;
    time_filter.until_ms = 200;
    TEST_ASSERT(time_filter.passes(make_line(100, "INFO", "x", "y")),
                "since inclusive");
    TEST_ASSERT(time_filter.passes(make_line(200, "INFO", "x", "y")),
                "until inclusive");
    TEST_ASSERT(!time_filter.passes(make_line(99, "INFO", "x", "y")),
                "before since rejected");
    TEST_ASSERT(!time_filter.passes(make_line(201, "INFO", "x", "y")),
                "after until rejected");
    return true;
}

// ---------- 目录摄取全链：FileAppender 落盘 → ingest ----------

class CollectSink final : public IPushSink {
public:
    std::vector<StructuredLine> records;
    void on_record(const StructuredLine& record) override {
        records.push_back(record);
    }
};

bool test_ingest_dir_from_real_appender() {
    namespace fs = std::filesystem;
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path dir =
        fs::temp_directory_path() / ("logger_test_" + std::to_string(suffix));
    fs::remove_all(dir);
    fs::create_directories(dir);

    FileAppenderConfig config;
    config.directory = dir.string();
    config.baseName = "cell";
    config.structuredOutput = true;
    LogContext::set_process("cell");
    LogContext::set_tick(3);
    {
        FileAppender appender(config);
        LogRecord info(LogLevel::Info, "first line", "cell");
        appender.append(info);
        LogRecord warn(LogLevel::Warning, "second line", "recovery");
        warn.setLocation("a.cpp", 9, "fn");
        appender.append(warn);
    }  // appender 析构关流——读侧在真相源落定后开始

    CollectSink sink;
    const IngestStats stats = ingest_dir(dir.string(), sink);
    TEST_ASSERT(stats.files == 1, "one file ingested");
    TEST_ASSERT(stats.parsed == 2, "two lines parsed");
    TEST_ASSERT(stats.skipped == 0, "nothing skipped");
    TEST_ASSERT(sink.records.size() == 2, "both records sunk");
    TEST_ASSERT(sink.records[0].level == "INFO" && sink.records[0].msg == "first line",
                "first record fields");
    TEST_ASSERT(sink.records[0].proc == "cell" && sink.records[0].tick == 3,
                "proc/tick context");
    TEST_ASSERT(sink.records[1].level == "WARN", "second record level");
    TEST_ASSERT(sink.records[1].src() == "a.cpp:9", "second record src");

    // 非结构化行计数：混入人读行
    const fs::path stray = dir / "human.log";
    std::ofstream out(stray);
    out << "plain human line" << std::endl;
    out.close();
    CollectSink sink2;
    const IngestStats stats2 = ingest_dir(dir.string(), sink2);
    TEST_ASSERT(stats2.files == 2, "two files seen");
    TEST_ASSERT(stats2.parsed == 2, "structured lines still parsed");
    TEST_ASSERT(stats2.skipped == 1, "human line skipped");

    // 缺失目录 = 空统计不炸
    const IngestStats none = ingest_dir((dir / "nope").string(), sink2);
    TEST_ASSERT(none.files == 0 && none.parsed == 0, "missing dir empty stats");

    fs::remove_all(dir);
    return true;
}

bool test_null_push_sink_seam() {
    // v1 接缝：空推实现零副作用（ADR-013 push 留接口不实现）
    NullPushSink sink;
    sink.on_record(make_line(1, "INFO", "p", "c"));
    return true;
}

bool test_logger_app_help_runs() {
    // run(--help) 走完参数面：exit 0（stdout 由 ctest 捕获）
    LoggerApp app;
    char argv0[] = "logger";
    char argv1[] = "--help";
    char* argv[] = {argv0, argv1, nullptr};
    TEST_ASSERT(app.run(2, argv) == 0, "help exits zero");
    return true;
}

} // namespace

int main() {
    struct Case {
        const char* name;
        bool (*fn)();
    };
    const Case cases[] = {
        {"parse_round_trip_plain", test_parse_round_trip_plain},
        {"parse_round_trip_quoted_and_kv", test_parse_round_trip_quoted_and_kv},
        {"parse_control_and_empty_values", test_parse_control_and_empty_values},
        {"parse_malformed_lines", test_parse_malformed_lines},
        {"level_rank", test_level_rank},
        {"filter_dimensions", test_filter_dimensions},
        {"ingest_dir_from_real_appender", test_ingest_dir_from_real_appender},
        {"null_push_sink_seam", test_null_push_sink_seam},
        {"logger_app_help_runs", test_logger_app_help_runs},
    };
    for (const auto& c : cases) {
        const int before = g_failures;
        const bool ok = c.fn();
        const bool passed = ok && g_failures == before;
        std::cout << (passed ? "[ PASS ] " : "[ FAIL ] ") << c.name << std::endl;
    }
    if (g_failures > 0) {
        std::cout << g_failures << " assertion(s) failed" << std::endl;
        return 1;
    }
    std::cout << "all logger retrieval tests passed" << std::endl;
    return 0;
}
