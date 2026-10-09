#include "apollo/runtime/crash_capture.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include "apollo/core/log/log_manager.hpp"
#include "apollo/core/metrics/metric_registry.h"

#if APOLLO_HAS_CRASHPAD
#include <client/crash_report_database.h>
#include <client/crashpad_client.h>
#include <client/settings.h>
#endif

namespace apollo::runtime {
namespace {

// 日志/监控打通（设计 §2.5）：init 行 cat=crash。helper 先于日志系统
// 初始化运行——log 面走 write（LogManager 惰性安全），stderr 兜底恒写一行
// （日志不可用时仍可见）。
void log_crash_line(apollo::core::log::LogLevel lv, const std::string& msg) {
    apollo::core::log::global_log_manager().write(lv, "crash", msg);
    std::fprintf(stderr, "[crash] %s %s\n",
                 apollo::core::log::toString(lv), msg.c_str());
}

// 自扫 --crash-* 参数（app 参数循环零改动）
struct CrashArgs {
    std::string dump_dir;    // --crash-dump-dir 覆盖
    std::string handler_path; // --crash-handler-path 覆盖
};

CrashArgs scan_args(int argc, char* const argv[]) {
    CrashArgs args;
    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        if (arg == "--crash-dump-dir" && i + 1 < argc) {
            args.dump_dir = argv[++i];
        } else if (arg == "--crash-handler-path" && i + 1 < argc) {
            args.handler_path = argv[++i];
        }
    }
    return args;
}

#if APOLLO_HAS_CRASHPAD

// 可执行文件目录（Linux：/proc/self/exe；handler 同级定位用）
std::filesystem::path exe_dir() {
    std::error_code ec;
    const auto self = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (ec) {
        return {};
    }
    return self.parent_path();
}

// handler 三级定位（设计 §2.2）：①--crash-handler-path 显式；
// ②可执行文件同级 crashpad_handler（POST_BUILD 复制保证——产物自带）；
// ③编译期 find_file 缓存路径（APOLLO_CRASHPAD_HANDLER_DEFAULT）。
std::string locate_handler(const std::string& explicit_path) {
    if (!explicit_path.empty() && std::filesystem::exists(explicit_path)) {
        return explicit_path;
    }
    const auto dir = exe_dir();
    if (!dir.empty()) {
        const auto sibling = dir / "crashpad_handler";
        if (std::filesystem::exists(sibling)) {
            return sibling.string();
        }
    }
#if defined(APOLLO_CRASHPAD_HANDLER_DEFAULT)
    const std::string cached = APOLLO_CRASHPAD_HANDLER_DEFAULT;
    if (!cached.empty() && std::filesystem::exists(cached)) {
        return cached;
    }
#endif
    return {};
}

#endif // APOLLO_HAS_CRASHPAD

} // namespace

CrashCaptureStatus init_crash_capture(int argc, char* const argv[], const std::string& proc_name) {
    CrashCaptureStatus status;
#if !APOLLO_HAS_CRASHPAD
    (void)argc;
    (void)argv;
    (void)proc_name;
    // no-op 桩：无 crashpad 环境（非 vcpkg configure），采集面缺位照常起。
    status.compiled_in = false;
    log_crash_line(apollo::core::log::LogLevel::Info, "crash capture not compiled in (no crashpad)");
    return status;
#else
    status.compiled_in = true;
    const CrashArgs args = scan_args(argc, argv);

    // db 目录：crashdumps/<proc>/（cwd 相对；machined roster 拉起子进程继承
    // cwd，编队各进程子目录自然隔离）
    const std::string db_dir =
        args.dump_dir.empty() ? "crashdumps/" + proc_name : args.dump_dir;
    std::error_code fs_ec;
    std::filesystem::create_directories(db_dir, fs_ec);

    // 启动 pending sweep（设计 §2.5）：CrashReportDatabase 只读查 Pending
    // 计数——崩溃是事后事件，活进程的观测面 = 上一次运行的遗留可见化。
    // sweep 在 StartHandler 之前（handler 运行期独占数据库，起前读一次即退）。
    {
        auto db = crashpad::CrashReportDatabase::Initialize(base::FilePath(db_dir));
        if (db) {
            std::vector<crashpad::CrashReportDatabase::Report> pending;
            if (db->GetPendingReports(&pending) ==
                crashpad::CrashReportDatabase::kNoError) {
                status.pending_reports = pending.size();
                if (!pending.empty()) {
                    apollo::core::metrics::MetricRegistry::instance()
                        .counter("crash_reports_pending")
                        .increment(static_cast<std::uint64_t>(pending.size()));
                    log_crash_line(apollo::core::log::LogLevel::Info, "crash pending=" + std::to_string(pending.size()) +
                                               " db=" + db_dir);
                }
            }
        }
        // db 对象即释——数据库句柄交还 handler 进程独占
    }

    const std::string handler = locate_handler(args.handler_path);
    if (handler.empty()) {
        // 三级落空 → fail-open：采集关、服务照常
        log_crash_line(apollo::core::log::LogLevel::Error, "crash handler not found (capture disabled, service continues)");
        return status;
    }

    crashpad::CrashpadClient client;
    // URL 空 = 本地模式：报告滞留 Pending 不外发（上传默认关，数据外发纪律）。
    const bool started = client.StartHandler(
        base::FilePath(handler), base::FilePath(db_dir), base::FilePath(""),
        /*url=*/"", /*annotations=*/{{"proc", proc_name}},
        /*arguments=*/{}, /*restartable=*/false, /*asynchronous_start=*/false);
    if (!started) {
        log_crash_line(apollo::core::log::LogLevel::Error, "crash handler start failed (capture disabled, service continues)");
        return status;
    }

    status.enabled = true;
    status.database_dir = db_dir;
    status.handler_path = handler;
    log_crash_line(apollo::core::log::LogLevel::Info, "crash capture enabled db=" + db_dir + " handler=" + handler +
                               " upload=off");
    return status;
#endif
}

bool crash_test(const std::string& kind) {
    if (kind != "null") {
        return false;
    }
    log_crash_line(apollo::core::log::LogLevel::Info, "crash-test null requested: dereferencing null (acceptance hook)");
    // 空指针解引用——采集面验收样例（volatile 防优化器抹除解引用）
    volatile int* p = nullptr;
    *p = 1;
    return true; // 不可达（进程已崩）；编译器视角的收尾
}

} // namespace apollo::runtime
