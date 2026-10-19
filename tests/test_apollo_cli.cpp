// apolloc CLI 脚手架单测（P0.2 L36：项目初始化的唯一 CLI 入口）。
//   1. help/--help/-h 三种形态打印用法并退出 0；无参打印用法并退出 1；
//   2. 未知子命令显式拒绝（不静默回退），init 缺 path 与多余参数退出 1；
//   3. init 在空新目录生成骨架（目录占位 + README + .gitignore + 项目标记）；
//   4. 非空目录、已存在文件、目录穿越路径显式拒绝（退出 2），原有内容不动；
//   5. 空已存在目录受理；已生成目录二次 init 拒收（不覆盖）；
//   6. 相对 path 按调用者 cwd 解析，不依赖启动时 cwd。
//
// 全部用例在临时目录下落盘自清理，无网络无定时器。

#include "apollo_cli/cli.hpp"

#include <chrono>
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

std::filesystem::path temp_root() {
    static const std::filesystem::path root = [] {
        const auto name = std::string("apollo_cli_tests_") +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        return std::filesystem::temp_directory_path() / name;
    }();
    return root;
}

bool cleanup_all() {
    std::error_code ec;
    std::filesystem::remove_all(temp_root(), ec);
    return !ec;
}

// 每个用例一份互不干扰的独立子目录（跨用例可并行性不保证，故编号递增）。
int case_index = 0;

std::filesystem::path fresh_dir() {
    std::error_code ec;
    std::filesystem::create_directories(temp_root(), ec);
    auto path = temp_root() / ("case_" + std::to_string(case_index++));
    std::filesystem::create_directories(path, ec);
    return path;
}

void write_text(const std::filesystem::path& path, const std::string& body) {
    std::ofstream out(path, std::ios::binary);
    out << body;
}

std::string read_text(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

struct CliResult {
    int code = 0;
    std::string out;
    std::string err;
};

// 直调 CLI（不落进程），argv[0] 固定 "apolloc"。
CliResult run(const std::vector<std::string>& args) {
    CliResult r;
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>("apolloc"));
    for (const auto& a : args) {
        argv.push_back(const_cast<char*>(a.c_str()));
    }
    std::ostringstream out, err;
    r.code = apollo_cli::run(static_cast<int>(argv.size()), argv.data(), out, err);
    r.out = out.str();
    r.err = err.str();
    return r;
}

bool test_help_forms() {
    for (const char* form : {"help", "--help", "-h"}) {
        auto r = run({form});
        TEST_ASSERT(r.code == 0, "help 形态退出 0");
        TEST_ASSERT(r.out.find("init") != std::string::npos, "用法列出 init");
        TEST_ASSERT(r.err.empty(), "用法走 stdout，不污染 stderr");
    }
    return true;
}

bool test_no_args_and_unknown() {
    auto noargs = run({});
    TEST_ASSERT(noargs.code == 1, "无参退出 1（用法错误）");
    TEST_ASSERT(noargs.err.find("init") != std::string::npos, "无参 stderr 给用法");

    auto unknown = run({"frobnicate"});
    TEST_ASSERT(unknown.code == 1, "未知子命令退出 1");
    TEST_ASSERT(unknown.err.find("unknown command") != std::string::npos,
                "未知子命令显式报错");

    auto init_no_path = run({"init"});
    TEST_ASSERT(init_no_path.code == 1, "init 缺 path 退出 1");

    auto extra = run({"init", "proj", "extra"});
    TEST_ASSERT(extra.code == 1, "init 多余参数退出 1");
    return true;
}

bool test_init_creates_skeleton() {
    auto root = fresh_dir();
    auto target = root / "proj";

    auto r = run({"init", target.string()});
    TEST_ASSERT(r.code == 0, "init 新建目录成功");
    TEST_ASSERT(r.out.find(target.string()) != std::string::npos, "成功输出目标路径");

    for (const char* dir : {"config", "scripts", "lua", "contract", "data"}) {
        TEST_ASSERT(std::filesystem::is_directory(target / dir),
                    std::string("生成目录 ") + dir);
    }
    TEST_ASSERT(std::filesystem::is_regular_file(target / "README.md"), "生成 README.md");
    TEST_ASSERT(std::filesystem::is_regular_file(target / ".gitignore"), "生成 .gitignore");
    TEST_ASSERT(std::filesystem::is_regular_file(target / ".apollo-project.json"),
                "生成项目标记");

    auto readme = read_text(target / "README.md");
    TEST_ASSERT(readme.find("Apollo") != std::string::npos, "README 标注 Apollo 项目");
    TEST_ASSERT(readme.find("apolloc init") != std::string::npos,
                "README 标注生成来源");

    auto ignore = read_text(target / ".gitignore");
    TEST_ASSERT(ignore.find("build/") != std::string::npos, ".gitignore 忽略 build/");
    TEST_ASSERT(ignore.find("data/") != std::string::npos, ".gitignore 忽略 data/");

    auto marker = read_text(target / ".apollo-project.json");
    TEST_ASSERT(marker.find("scaffold_version") != std::string::npos,
                "项目标记带脚手架版本");
    return true;
}

bool test_init_refuses_nonempty_and_file() {
    auto root = fresh_dir();

    auto occupied = root / "occupied";
    std::filesystem::create_directories(occupied);
    write_text(occupied / "keep.txt", "keep-me\n");
    auto r1 = run({"init", occupied.string()});
    TEST_ASSERT(r1.code == 2, "非空目录拒绝（退出 2）");
    TEST_ASSERT(r1.err.find("not empty") != std::string::npos, "非空目录原因明确");
    TEST_ASSERT(read_text(occupied / "keep.txt") == "keep-me\n",
                "拒绝后原有文件未被改动");
    TEST_ASSERT(!std::filesystem::exists(occupied / "README.md"),
                "拒绝后不落任何骨架文件");

    auto as_file = root / "file";
    write_text(as_file, "not a dir\n");
    auto r2 = run({"init", as_file.string()});
    TEST_ASSERT(r2.code == 2, "已存在文件拒绝（退出 2）");
    TEST_ASSERT(r2.err.find("not a directory") != std::string::npos,
                "已存在文件原因明确");

    auto nested = root / "empty_parent" / "proj";
    auto r3 = run({"init", nested.string()});
    TEST_ASSERT(r3.code == 0, "父目录不存在时可自动创建");
    TEST_ASSERT(std::filesystem::is_directory(nested / "contract"),
                "父目录链一并创建");
    return true;
}

bool test_init_rejects_empty_and_traversal() {
    auto root = fresh_dir();

    auto empty_path = run({"init", ""});
    TEST_ASSERT(empty_path.code == 2, "空 path 拒绝（退出 2）");
    TEST_ASSERT(!empty_path.out.empty() || !empty_path.err.empty(),
                "空 path 给出说明");

    auto traversal = root / ".." / "outside";
    auto r = run({"init", traversal.string()});
    TEST_ASSERT(r.code == 2, "目录穿越路径拒绝（退出 2）");
    TEST_ASSERT(r.err.find("..") != std::string::npos, "穿越路径原因明确");
    TEST_ASSERT(!std::filesystem::exists(root / ".." / "outside"), "未创建越界目录");
    return true;
}

bool test_init_empty_existing_and_refuses_rescaffold() {
    auto root = fresh_dir();

    auto empty = root / "empty";
    std::filesystem::create_directories(empty);
    auto r1 = run({"init", empty.string()});
    TEST_ASSERT(r1.code == 0, "空已存在目录受理");
    TEST_ASSERT(std::filesystem::is_directory(empty / "config"), "骨架已生成");

    auto r2 = run({"init", empty.string()});
    TEST_ASSERT(r2.code == 2, "已生成目录二次 init 拒收（不覆盖）");
    TEST_ASSERT(std::filesystem::is_directory(empty / "config"), "拒收后骨架完好");
    return true;
}

bool test_init_relative_from_other_cwd() {
    auto root = fresh_dir();
    const auto old_cwd = std::filesystem::current_path();
    std::filesystem::current_path(root);
    auto r = run({"init", "proj"});
    TEST_ASSERT(r.code == 0, "从其他 cwd 用相对 path 成功");
    TEST_ASSERT(std::filesystem::is_directory(root / "proj" / "scripts"),
                "相对 path 落在调用者 cwd 下");
    std::filesystem::current_path(old_cwd);
    return true;
}

} // namespace

int main() {
    auto cases = {
        test_help_forms,
        test_no_args_and_unknown,
        test_init_creates_skeleton,
        test_init_refuses_nonempty_and_file,
        test_init_rejects_empty_and_traversal,
        test_init_empty_existing_and_refuses_rescaffold,
        test_init_relative_from_other_cwd,
    };
    int failed = 0;
    for (auto& test : cases) {
        if (!test()) {
            ++failed;
        }
    }
    std::error_code ec;
    std::filesystem::remove_all(temp_root(), ec);
    if (failed == 0) {
        std::cout << "All tests passed" << std::endl;
        return 0;
    }
    std::cout << failed << " test(s) failed" << std::endl;
    return 1;
}
