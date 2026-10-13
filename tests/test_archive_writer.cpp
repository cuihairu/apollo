// ArchiveWriter 单元测试（collector 归档滚动批，P3-3 移交余件③）：
// 按日滚动（关旧开新当日本名）、同日重启续写、保留份数清理（只清本名
// <base>_*.log）、env 保留数解析。日期缝注假日期强制跨日，不待真钟。
// 无 GTest，plain-assert + main 汇总。
// Windows 句柄纪律：writer 内层作用域先析构（关句柄）再清目录——句柄
// 存活期 remove_all 删锁定文件抛 filesystem_error，未捕获即 0xc0000409。
#include "logger/archive_writer.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

#define TEST_ASSERT(cond, msg)                                                     \
    do {                                                                           \
        if (!(cond)) {                                                             \
            std::cout << "FAIL: " << (msg) << " (" << #cond << ") at "             \
                      << __LINE__ << std::endl;                                    \
            ++g_failures;                                                          \
            return false;                                                          \
        }                                                                          \
    } while (0)

std::string slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::vector<std::string> log_files(const std::string& dir) {
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".log") {
            names.push_back(entry.path().filename().string());
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

// 临时目录：固定名 + 起手清残留（ctest 串行，无并行碰撞面）
std::string temp_dir(const char* tag) {
    const std::string dir =
        (fs::temp_directory_path()
         / ("apollo_archive_tests_" + std::string(tag))).string();
    std::error_code ec;
    fs::remove_all(dir, ec);
    return dir;
}

// ---------- 开档 + 逐行落盘（flush 即时可见） ----------

bool test_open_and_write() {
    std::cout << "Running: test_open_and_write..." << std::endl;
    const std::string dir = temp_dir("basic");
    {
        logger::ArchiveWriter writer({dir, "collector", 0});
        writer.set_date_provider([] { return "20261013"; });
        TEST_ASSERT(writer.open(), "open succeeded");
        TEST_ASSERT(writer.current_path().find("collector_20261013.log") != std::string::npos,
                    "dated file name");

        writer.write_line("ts=1 level=INFO proc=gw msg=hello");
        writer.write_line("ts=2 level=INFO proc=gw msg=world");

        // 逐行冲刷：无关闭即读得到（scan/follow 即时可见口径）
        const std::string content = slurp(writer.current_path());
        TEST_ASSERT(content == "ts=1 level=INFO proc=gw msg=hello\n"
                               "ts=2 level=INFO proc=gw msg=world\n",
                    "lines flushed per write");
        TEST_ASSERT(log_files(dir).size() == 1, "single dated file");
    }  // 句柄先释放再删目录

    std::error_code ec;
    fs::remove_all(dir, ec);
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- 跨日滚动：关旧开新当日本名，旧文件原样 ----------

bool test_day_rollover() {
    std::cout << "Running: test_day_rollover..." << std::endl;
    const std::string dir = temp_dir("rollover");
    std::string today = "20261013";
    {
        logger::ArchiveWriter writer({dir, "collector", 0});
        writer.set_date_provider([&today] { return today; });
        TEST_ASSERT(writer.open(), "open succeeded");
        writer.write_line("day one");

        today = "20261014";  // 跨日
        writer.write_line("day two");

        const auto files = log_files(dir);
        TEST_ASSERT(files.size() == 2, "two dated files after rollover");
        TEST_ASSERT(files[0] == "collector_20261013.log" && files[1] == "collector_20261014.log",
                    "one file per day");
        TEST_ASSERT(slurp((fs::path(dir) / files[0]).string()) == "day one\n",
                    "old file untouched");
        TEST_ASSERT(slurp((fs::path(dir) / files[1]).string()) == "day two\n",
                    "new file has new line");
        TEST_ASSERT(writer.current_path().find("20261014") != std::string::npos,
                    "current path advanced");
    }  // 句柄先释放再删目录

    std::error_code ec;
    fs::remove_all(dir, ec);
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- 同日重启：app 模式续写不覆写 ----------

bool test_same_day_restart_appends() {
    std::cout << "Running: test_same_day_restart_appends..." << std::endl;
    const std::string dir = temp_dir("restart");
    {
        logger::ArchiveWriter writer({dir, "collector", 0});
        writer.set_date_provider([] { return "20261013"; });
        TEST_ASSERT(writer.open(), "first open");
        writer.write_line("before restart");
    }
    {
        logger::ArchiveWriter writer({dir, "collector", 0});
        writer.set_date_provider([] { return "20261013"; });
        TEST_ASSERT(writer.open(), "restart open");
        writer.write_line("after restart");
    }
    const auto files = log_files(dir);
    TEST_ASSERT(files.size() == 1, "same day = same file");
    TEST_ASSERT(slurp((fs::path(dir) / files[0]).string())
                    == "before restart\nafter restart\n",
                "restart appends without truncation");

    fs::remove_all(dir);
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- 保留份数清理：只清本名最旧，他名文件不动 ----------

bool test_max_files_cleanup() {
    std::cout << "Running: test_max_files_cleanup..." << std::endl;
    const std::string dir = temp_dir("cleanup");
    std::string today = "20261011";
    {
        logger::ArchiveWriter writer({dir, "collector", 2});
        writer.set_date_provider([&today] { return today; });
        TEST_ASSERT(writer.open(), "open day one");

        // 他名文件（旧版归档名 + BI tap 名）必须原样
        { std::ofstream legacy(fs::path(dir) / "collector.log"); legacy << "legacy\n"; }
        { std::ofstream bi(fs::path(dir) / "bi.log"); bi << "bi\n"; }

        for (const char* d : {"20261012", "20261013", "20261014"}) {
            today = d;
            writer.write_line("line " + today);
        }

        const auto files = log_files(dir);
        TEST_ASSERT(files.size() == 4, "2 kept + 2 foreign-name files");
        TEST_ASSERT(files[0] == "bi.log" && files[1] == "collector.log",
                    "foreign-name files untouched");
        TEST_ASSERT(files[2] == "collector_20261013.log" && files[3] == "collector_20261014.log",
                    "oldest two dated files removed");
    }  // 句柄先释放再删目录

    std::error_code ec;
    fs::remove_all(dir, ec);
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- 保留份数 <=0 = 不清理 ----------

bool test_no_cleanup_when_disabled() {
    std::cout << "Running: test_no_cleanup_when_disabled..." << std::endl;
    const std::string dir = temp_dir("noclean");
    std::string today = "20261012";
    {
        logger::ArchiveWriter writer({dir, "collector", 0});
        writer.set_date_provider([&today] { return today; });
        TEST_ASSERT(writer.open(), "open");
        for (const char* d : {"20261013", "20261014", "20261015", "20261016"}) {
            today = d;
            writer.write_line("line " + today);
        }
        TEST_ASSERT(log_files(dir).size() == 5, "all five files kept");
    }  // 句柄先释放再删目录

    std::error_code ec;
    fs::remove_all(dir, ec);
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- env 解析：未设/非法 = 30，数字透传 ----------

bool test_max_files_from_env() {
    std::cout << "Running: test_max_files_from_env..." << std::endl;
    TEST_ASSERT(logger::max_files_from_env(nullptr) == 30, "null = default 30");
    TEST_ASSERT(logger::max_files_from_env("") == 30, "empty = default 30");
    TEST_ASSERT(logger::max_files_from_env("abc") == 30, "garbage = default 30");
    TEST_ASSERT(logger::max_files_from_env("12") == 12, "number passes through");
    TEST_ASSERT(logger::max_files_from_env("0") == 0, "zero = cleanup off");
    TEST_ASSERT(logger::max_files_from_env("-1") == -1, "negative = cleanup off");
    std::cout << "  PASSED" << std::endl;
    return true;
}

} // namespace

int main() {
    int failed = 0;
    if (!test_open_and_write()) ++failed;
    if (!test_day_rollover()) ++failed;
    if (!test_same_day_restart_appends()) ++failed;
    if (!test_max_files_cleanup()) ++failed;
    if (!test_no_cleanup_when_disabled()) ++failed;
    if (!test_max_files_from_env()) ++failed;
    if (failed == 0) {
        std::cout << "ALL ARCHIVE WRITER TESTS PASSED" << std::endl;
        return 0;
    }
    std::cerr << failed << " archive writer test(s) failed" << std::endl;
    return 1;
}
