// apolloc CLI 脚手架单测（P0.2 L36：项目初始化的唯一 CLI 入口）。
//   1. help/--help/-h 三种形态打印用法并退出 0；无参打印用法并退出 1；
//   2. 未知子命令显式拒绝（不静默回退），init 缺 path 与多余参数退出 1；
//   3. init 在空新目录生成模板（五目录占位 + 配置 + Lua 入口 +
//      契约入口 + README + .gitignore + 项目标记），内容逐项断言；
//   4. 模板禁物：不生成认证材料、无 CRLF、启动/停止脚本无 sleep；
//   5. 非空目录、已存在文件、目录穿越路径显式拒绝（退出 2），原有内容不动；
//   6. 空已存在目录受理；已生成目录二次 init 拒收（不覆盖）；
//   7. 相对 path 按调用者 cwd 解析，不依赖启动时 cwd。
//
// 全部用例在临时目录下落盘自清理，无网络无定时器。

#include "apollo_cli/cli.hpp"

#include <cctype>
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

    for (const char* dir : {"config", "scripts", "lua", "contract", "run"}) {
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
    TEST_ASSERT(ignore.find("run/*") != std::string::npos,
                ".gitignore 忽略运行目录内容");
    TEST_ASSERT(ignore.find("!run/.gitkeep") != std::string::npos,
                ".gitignore 保留运行目录占位");

    auto marker = read_text(target / ".apollo-project.json");
    TEST_ASSERT(marker.find("scaffold_version") != std::string::npos,
                "项目标记带脚手架版本");

    for (const char* file : {"config/runtime.conf", "scripts/lib.sh",
                             "scripts/start.sh", "scripts/stop.sh",
                             "lua/boot.lua", "lua/contract_handlers.lua",
                             "contract/README.md", "run/.gitkeep"}) {
        TEST_ASSERT(std::filesystem::is_regular_file(target / file),
                    std::string("生成模板文件 ") + file);
    }
    return true;
}

bool test_generated_template_integrity() {
    auto root = fresh_dir();
    auto target = root / "proj";
    auto r = run({"init", target.string()});
    TEST_ASSERT(r.code == 0, "模板生成成功");

    // 配置：五个角色的主机/端口/身份齐全，端口按服务缺省对齐。
    auto conf = read_text(target / "config/runtime.conf");
    for (const char* key : {"HOST=127.0.0.1", "DATABASE_PORT=9200",
                            "BASE_PORT=9201", "LOGIN_PORT=9001",
                            "WORLD_M1_PORT=9101", "GATEWAY_PORT=8888",
                            "DATABASE_ID=database-1", "BASE_ID=base-1",
                            "BUILD_VERSION=local"}) {
        TEST_ASSERT(conf.find(key) != std::string::npos,
                    std::string("配置含 ") + key);
    }

    TEST_ASSERT(conf.find("RUN_DIR=") != std::string::npos,
                "配置可覆盖运行根目录");

    // 布局集中：ROOT/RUN_DIR/逐角色日志·资料·管理记录目录只在 lib.sh 定义。
    auto lib = read_text(target / "scripts/lib.sh");
    TEST_ASSERT(lib.find("ROOT=\"$(cd \"$(dirname \"$0\")/..\" && pwd)\"") !=
                    std::string::npos,
                "lib.sh 从自身位置解析项目根，不依赖启动 cwd");
    TEST_ASSERT(lib.find(": \"${RUN_DIR:=$ROOT/run}\"") != std::string::npos,
                "运行根默认 <root>/run 且配置可覆盖");
    for (const char* helper : {"role_dir()", "log_file()", "data_dir()",
                               "admin_dir()", "pid_file()", "ensure_role_dir()",
                               "cell_dir()", "require_bin_dir()", "bin_of()"}) {
        TEST_ASSERT(lib.find(helper) != std::string::npos,
                    std::string("lib.sh 提供 ") + helper);
    }
    TEST_ASSERT(lib.find("$RUN_DIR/cell-$1") != std::string::npos,
                "cell 目录按编号给出，不共写同一目录");
    TEST_ASSERT(lib.find("$RUN_DIR/$1/logs/$1.log") != std::string::npos,
                "日志按角色隔离");
    TEST_ASSERT(lib.find("$RUN_DIR/$1/data") != std::string::npos,
                "资料按角色隔离");
    TEST_ASSERT(lib.find("$RUN_DIR/$1/admin") != std::string::npos,
                "管理记录按角色隔离");
    TEST_ASSERT(lib.find("APOLLO_BIN_DIR is not set") != std::string::npos &&
                    lib.find("$APOLLO_BIN_DIR/$role/$role") != std::string::npos &&
                    lib.find("$APOLLO_BIN_DIR/$role") != std::string::npos,
                "引擎目录校验与两种布局探测集中在 lib.sh");
    TEST_ASSERT(lib.find("engine binary not found") != std::string::npos,
                "引擎二进制缺失时报错退出");

    // 启动脚本：依赖顺序 database→login→base→world→gateway，flag 均为服务
    // 现有解析项，且不落任何就绪伪装（无 sleep）。
    auto start = read_text(target / "scripts/start.sh");
    TEST_ASSERT(start.find(". \"$(dirname \"$0\")/lib.sh\"") !=
                    std::string::npos,
                "启动脚本源 lib.sh 而非自行解析路径");
    TEST_ASSERT(start.find("config/runtime.conf") != std::string::npos,
                "启动脚本源配置");
    TEST_ASSERT(start.find("require_bin_dir") != std::string::npos,
                "启动脚本校验引擎二进制目录");
    TEST_ASSERT(start.find("config/runtime.conf") != std::string::npos,
                "启动脚本源配置");
    TEST_ASSERT(start.find("--database-host") != std::string::npos,
                "base→database 接线走真实 flag");
    TEST_ASSERT(start.find("--crash-dump-dir") != std::string::npos,
                "管理记录目录逐角色显式给出");
    TEST_ASSERT(start.find("\"$(data_dir database)\"") != std::string::npos,
                "资料目录来自统一布局");
    TEST_ASSERT(start.find("RUN_DIR/engine.version") != std::string::npos,
                "构建版本落盘到运行根");
    TEST_ASSERT(start.find(". \"$ROOT/config/runtime.conf\"") ==
                    std::string::npos || start.find("$ROOT/config/runtime.conf") !=
                    std::string::npos,
                "配置路径明确");
    TEST_ASSERT(start.find("--script-boot") != std::string::npos &&
                    start.find("lua/boot.lua") != std::string::npos,
                "world 接 Lua boot 入口");
    TEST_ASSERT(start.find("--settlement-credential-dir") == std::string::npos,
                "不生成结算凭据目录参数");
    TEST_ASSERT(start.find("sleep") == std::string::npos,
                "启动脚本无 sleep 冒充就绪");
    const char* order[] = {"start_one database", "start_one login",
                           "start_one base", "start_one world",
                           "start_one gateway"};
    for (std::size_t i = 1; i < 5; ++i) {
        TEST_ASSERT(start.find(order[i - 1]) < start.find(order[i]),
                    std::string("启动顺序 ") + order[i - 1] + " 先于 " +
                        order[i]);
    }

    // 停止脚本：只按 start.sh 记录的 PID 反向停止，不猜进程。
    auto stop = read_text(target / "scripts/stop.sh");
    TEST_ASSERT(stop.find(". \"$(dirname \"$0\")/lib.sh\"") !=
                    std::string::npos,
                "停止脚本源 lib.sh");
    TEST_ASSERT(stop.find("pid_file") != std::string::npos,
                "停止脚本只认 lib.sh 记录的 PID 面");
    TEST_ASSERT(stop.find("kill ") != std::string::npos, "停止脚本发 TERM");
    TEST_ASSERT(stop.find("start_one gateway") == std::string::npos &&
                    stop.find("stop_one gateway") < stop.find("stop_one database"),
                "停止顺序与启动反向");
    TEST_ASSERT(stop.find("sleep") == std::string::npos, "无等待循环");

    // 目录说明：README 列出模板布局、调用方式与未实现状态。
    auto readme = read_text(target / "README.md");
    TEST_ASSERT(readme.find("## Layout") != std::string::npos, "README 有目录说明");
    TEST_ASSERT(readme.find("sh scripts/start.sh") != std::string::npos,
                "README 给出启动方式");
    TEST_ASSERT(readme.find("readiness is not asserted") != std::string::npos ||
                    readme.find("does\nnot assert readiness") != std::string::npos,
                "README 声明不断言就绪");
    TEST_ASSERT(readme.find("No auth material is generated") != std::string::npos,
                "README 声明不生成认证材料");
    TEST_ASSERT(readme.find("run/<role>") != std::string::npos ||
                    readme.find("run/role") != std::string::npos,
                "README 说明逐角色运行目录");
    TEST_ASSERT(readme.find("cell") != std::string::npos,
                "README 说明 cell 目录不共写");
    TEST_ASSERT(readme.find("BUILD_VERSION") != std::string::npos,
                "README 说明构建版本");

    // 契约入口：说明 apollo_gen 生成流程与 handler 同步纪律。
    auto contract = read_text(target / "contract/README.md");
    TEST_ASSERT(contract.find("apollo_gen") != std::string::npos,
                "契约入口指向生成器");
    TEST_ASSERT(contract.find("--contract contract") != std::string::npos,
                "契约入口给出生成命令");

    // 全模板禁物扫描：不生成认证材料、不残留 CRLF。
    const char* forbidden[] = {"secret", "password", "credential", "BEGIN RSA"};
    for (const auto& file : apollo_cli::scaffold_files("proj")) {
        const auto body = read_text(target / file.first);
        for (const char* word : forbidden) {
            auto lowered = body;
            for (auto& ch : lowered) {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            TEST_ASSERT(lowered.find(word) == std::string::npos,
                        std::string("模板禁物 ") + file.first + " 含 " + word);
        }
        TEST_ASSERT(body.find('\r') == std::string::npos,
                    std::string("模板无 CRLF ") + file.first);
    }

    // 清单本身：11 个文件、全为相对路径、无重复。
    auto files = apollo_cli::scaffold_files("proj");
    TEST_ASSERT(files.size() == 11, "模板清单 11 个文件");
    for (const auto& file : files) {
        TEST_ASSERT(!file.first.empty() && file.first.front() != '/',
                    "清单路径为相对路径");
        TEST_ASSERT(file.first.find("..") == std::string::npos,
                    "清单路径无穿越分量");
        TEST_ASSERT(!file.second.empty() &&
                        file.second.back() == '\n',
                    std::string("清单内容非空且 LF 收尾 ") + file.first);
    }
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
        test_generated_template_integrity,
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
