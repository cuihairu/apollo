#pragma once
//
// 结构化行解析（logger 检索面 v1，logging.md §5.1 的读侧镜像）。
//
// 契约真相源 = modules/core/log/include/apollo/core/log/structured.h（写侧，
// 六键固定序 + kv 扩展段 + 引号规则）。本件为读侧：写侧如何编码，读侧就如何
// 还原——两件不允许各自漂移，改契约先改 structured.h 头注与 logging.md §5.1。
//
// 行格式：
//   ts=<wall_ms> level=<LEVEL> proc=<proc> tick=<n> cat=<logger> msg=<msg>
//   [<空格 kv 对>...]
//
// 解析规则（写侧引号规则的逆）：
//   - 空白切 kv 对；key 至 '='；值为空串 → `key=`（长度零）；
//   - 值带双引号 → 内部 \" → "、\\ → \、\n/\r/\t → 控制符还原，
//     其余 \x → x（宽容读：写侧只会产上述五种）；
//   - 未闭合引号 / 悬空转义 / 缺 '=' / 六键不齐 → 非结构化行，判 false
//    （tail 半行、人读行、foreign 行都走这条路）。

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace logger {

struct StructuredLine {
    std::uint64_t ts = 0;   // 墙钟毫秒（epoch ms）
    std::string level;      // DEBUG/INFO/WARN/ERROR/CRITICAL
    std::string proc;       // 进程标识（未设为 "-"，保真不归一）
    std::uint64_t tick = 0;
    std::string cat;        // 日志器名（空为 "-"）
    std::string msg;
    std::vector<std::pair<std::string, std::string>> kv;  // 扩展段（含 src）

    // 溯源便捷视图：src= 扩展值（无则空串）
    std::string src() const;
};

// 解析一行结构化行；out 必非空。失败时 *out 内容无意义。
bool parse_structured_line(const std::string& line, StructuredLine* out);

} // namespace logger
