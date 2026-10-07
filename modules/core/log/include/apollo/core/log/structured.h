#pragma once
//
// 结构化行格式（logging.md §5.1 P3 批定案——「键集稳定、引号规则进
// P3 批」由本批落地）。
//
// 行格式（logfmt 族，filebeat/fluent-bit 类采集器原生解析）：
//   ts=<wall_ms> level=<LEVEL> proc=<proc> tick=<n> cat=<logger> msg=<msg>
//   [<空格 kv 对>...]
//
// 稳定键集（六键、序固定——外采解析契约，键集变更须先改 logging §5.1）：
//   ts    墙钟毫秒（epoch ms，恒数字无引号）
//   level DEBUG/INFO/WARN/ERROR/CRITICAL（log_level.h toString 口径）
//   proc  进程标识（LogManagerConfig::processIdentity 启动期设一次，
//         未设为 "-"）
//   tick  帧号（主循环每帧 LogContext::set_tick，未接为 0）
//   cat   日志器名（空为 "-"）
//   msg   消息（见引号规则）
// 其后为 kv 扩展段（含溯源 src=<file>:<line>，LogRecord 带 file 时首随）。
//
// 引号规则（kv 对通用——本批定案）：
//   - 键经清洗恒合法：出现 [A-Za-z0-9_.] 之外的字符一律替为 '_'
//     （键集稳定是外采索引契约，非法键即污染）；
//   - 值为空串 → `key=` 紧跟；
//   - 值含空白、'='、'"'、'\'、控制字符，或含 [A-Za-z0-9_.:/@+-] 之外
//     字符 → 包双引号；内部 '"'→\"、'\'→\\、换行/回车/制表→\n/\r/\t，
//     其余 C0 控制符替换 '?'（行式 tail 不得被值截断）；
//   - 其余原样不加引号（安全字符集零开销）。
//
// 落地面（本批边界）：FileAppenderConfig::structuredOutput 开关（默认关，
// 现有人读消费零破坏；apps/LoggerApp 接线批打开）；console 保持人读
// （外采 tail 只读文件）；spdlog 分支未接（环境无 spdlog，builtin 是唯一
// 活路径——vcpkg.json 零 spdlog 条目为证）。

#include <cstdint>
#include <string>

#include "apollo/core/log/log_record.h"

namespace apollo {
namespace core {
namespace log {

/**
 * @brief 进程级日志上下文
 *
 * proc 启动期设一次（或经 LogManagerConfig::processIdentity 间接设）；
 * tick 主循环每帧更。原子/加锁读写，任意线程可写可读。
 */
class LogContext {
public:
    static void set_process(std::string proc);
    static std::string process();
    static void set_tick(std::uint64_t tick) noexcept;
    static std::uint64_t tick() noexcept;
};

/**
 * @brief 结构化行格式化（§5.1 键集 + 引号规则）
 */
std::string to_structured_line(const LogRecord& record);

} // namespace log
} // namespace core
} // namespace apollo
