#pragma once
//
// LoggerApp——日志收集进程壳（ADR-013 立项；term-contract §1.3 契约代码
// 标识，不随目录名变）。
//
// v1 = 只读汇聚（tail 拉，logger-app.md §3(a)）：
//   scan   一发检索：读目录 *.log → 解析 → 过滤（proc/level/ts/cat）→
//          stdout 原行透传；
//   follow 跟随模式：周期轮询目录，增量读新行同过滤输出（编队聚合面，
//          dev_fleet roster 行）。
// 职责边界：归档/滚动沿用 FileAppender 既有（logger 不接管）；三禁 =
// 不开任何端口（无 UDP/TCP 监听）。

#include "logger/push_sink.hpp"
#include "logger/structured_line.hpp"

#include <cstdint>
#include <map>
#include <string>

namespace logger {

// 级别序（检索阈值用）：DEBUG < INFO < WARN < ERROR < CRITICAL；
// 未知名（含 "-"）返回 -1
int level_rank(const std::string& name);

struct RetrievalFilter {
    std::string proc;            // 空 = 不过滤
    std::string cat;             // 空 = 不过滤
    int min_level = -1;          // -1 = 不过滤；否则 level_rank 阈值（含）
    std::uint64_t since_ms = 0;  // 0 = 不限；ts >= since_ms
    std::uint64_t until_ms = 0;  // 0 = 不限；ts <= until_ms

    bool passes(const StructuredLine& line) const;
};

// 目录摄取统计
struct IngestStats {
    std::size_t files = 0;
    std::size_t parsed = 0;
    std::size_t skipped = 0;  // 非结构化行（半行/人读行/foreign 行）
};

// 读 dir 下全部 *.log（文件名排序），逐行解析过 sink；目录不存在 = 空统计。
IngestStats ingest_dir(const std::string& dir, IPushSink& sink);

class LoggerApp {
public:
    // 命令行入口：解析参数 → scan/follow；返回进程退出码
    int run(int argc, char* argv[]);

private:
    int run_scan(const std::string& dir, const RetrievalFilter& filter);
    int run_follow(const std::string& dir, const RetrievalFilter& filter,
                   std::uint32_t interval_ms);
    // 增量读一轮（follow 用）：按文件记偏移，只处理自上次以来的完整行
    void poll_once(const std::string& dir, const RetrievalFilter& filter,
                   std::map<std::string, std::streamoff>* offsets);
};

} // namespace logger
