// apolloc CLI 脚手架单测（P0.2 L36：项目初始化的唯一 CLI 入口）。
//   1. help/--help/-h 三种形态打印用法并退出 0；无参打印用法并退出 1；
//   2. 未知子命令显式拒绝（不静默回退），init 缺 path 与多余参数退出 1；
//   3. init 在空新目录生成模板（五目录占位 + 配置 + Lua 入口 +
//      契约入口 + README + .gitignore + 项目标记），内容逐项断言；
//   4. 模板禁物：不生成认证材料、无 CRLF、启动/停止脚本无 sleep；
//   5. 非空目录、已存在文件、目录穿越路径显式拒绝（退出 2），原有内容不动；
//   6. 空已存在目录受理；已生成目录二次 init 拒收（不覆盖）；
//   7. 相对 path 按调用者 cwd 解析，不依赖启动时 cwd。
//   8. 非法项目名（空格/分号/数字或点开头）拒收退出 2 且不落盘；合法形态
//      （字母或下划线开头，含数字/下划线/连字符/点）照常受理（L41 验收）。
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
                             "scripts/start.sh", "scripts/stop.sh", "scripts/status.sh",
                             "scripts/verify.sh", "CMakeLists.txt",
                             ".github/workflows/ci.yml", "lua/boot.lua",
                             "lua/contract_handlers.lua", "contract/README.md",
                             "contract/version", "contract/errors.xml",
                             "contract/attrs.xml", "contract/entities.xml",
                             "contract/messages.xml", "run/.gitkeep"}) {
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
                               "cell_dir()", "require_bin_dir()", "bin_of()",
                               "ready_line()", "member_ready()", "wait_ready()",
                               "stop_one()"}) {
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
    TEST_ASSERT(lib.find("database ready port=") != std::string::npos &&
                    lib.find("Server is running") != std::string::npos,
                "就绪标记取自各服务真实启动行，不虚构");
    TEST_ASSERT(lib.find("kill ") != std::string::npos,
                "TERM 面集中在 lib.sh，start/stop 共用");

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
    TEST_ASSERT(start.find("wait_ready") != std::string::npos,
                "启动在真实就绪后推进下一成员");
    TEST_ASSERT(start.find("rollback") != std::string::npos &&
                    start.find("STARTED") != std::string::npos,
                "失败只回滚本次新启动成员，保留已有实例");
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
    TEST_ASSERT(lib.find("READY_TIMEOUT_SECONDS") != std::string::npos &&
                    lib.find("sleep 1") != std::string::npos,
                "就绪轮询有界且按秒 tick，不用固定 sleep 冒充就绪");
    const char* order[] = {"start_member database", "start_member login",
                           "start_member base", "start_member world",
                           "start_member gateway"};
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
    TEST_ASSERT(stop.find("stop_one") != std::string::npos,
                "停止脚本消费 lib.sh 的 PID 面");
    TEST_ASSERT(lib.find("kill ") != std::string::npos, "停止脚本发 TERM");
    TEST_ASSERT(stop.find("start_one gateway") == std::string::npos &&
                    stop.find("stop_one gateway") < stop.find("stop_one database"),
                "停止顺序与启动反向");
    TEST_ASSERT(stop.find("sleep") == std::string::npos, "无等待循环");

    // 状态脚本：只读，逐成员报 PID 文件状态与真实 readiness，不启停。
    auto status = read_text(target / "scripts/status.sh");
    TEST_ASSERT(status.find(". \"$(dirname \"$0\")/lib.sh\"") !=
                    std::string::npos,
                "状态脚本源 lib.sh");
    TEST_ASSERT(status.find("member_ready") != std::string::npos,
                "状态读真实 readiness 面");
    TEST_ASSERT(status.find("kill -0") != std::string::npos,
                "状态只观测存活");
    TEST_ASSERT(status.find("start_one") == std::string::npos &&
                    status.find("stop_one") == std::string::npos,
                "状态脚本无启停动作");
    TEST_ASSERT(status.find("sleep") == std::string::npos,
                "状态脚本无等待循环");

    // 目录说明：README 列出模板布局、调用方式与未实现状态。
    auto readme = read_text(target / "README.md");
    TEST_ASSERT(readme.find("## Layout") != std::string::npos, "README 有目录说明");
    TEST_ASSERT(readme.find("sh scripts/start.sh") != std::string::npos,
                "README 给出启动方式");
    TEST_ASSERT(readme.find("sh scripts/status.sh") != std::string::npos &&
                    readme.find("rollback") != std::string::npos,
                "README 给出状态命令与失败回滚口径");
    TEST_ASSERT(readme.find("sh scripts/verify.sh") != std::string::npos,
                "README 给出开发验证入口");
    TEST_ASSERT(readme.find(".github/workflows/ci.yml") != std::string::npos,
                "README 指向 CI 配置");
    TEST_ASSERT(readme.find("cmake --build") != std::string::npos,
                "README 说明 CMake 门禁目标入口");
    TEST_ASSERT(readme.find("scaffold acceptance pass") != std::string::npos &&
                    readme.find("Still pending") != std::string::npos,
                "README 声明验收已过并标注仍未交付项");
    TEST_ASSERT(readme.find("No auth material is generated") != std::string::npos,
                "README 声明不生成认证材料");
    TEST_ASSERT(readme.find("run/<role>") != std::string::npos ||
                    readme.find("run/role") != std::string::npos,
                "README 说明逐角色运行目录");
    TEST_ASSERT(readme.find("cell") != std::string::npos,
                "README 说明 cell 目录不共写");
    TEST_ASSERT(readme.find("BUILD_VERSION") != std::string::npos,
                "README 说明构建版本");

    // 契约入口：说明 apollo_gen 生成流程、漂移门禁与 handler 同步纪律。
    auto contract = read_text(target / "contract/README.md");
    TEST_ASSERT(contract.find("apollo_gen") != std::string::npos,
                "契约入口指向生成器");
    TEST_ASSERT(contract.find("--contract contract") != std::string::npos,
                "契约入口给出生成命令");
    TEST_ASSERT(contract.find("verify.sh") != std::string::npos &&
                    contract.find("--check") != std::string::npos,
                "契约入口指向验证入口与漂移门禁");

    // L40 构建面：可审查的 CMake 配置，只暴露门禁目标，不复制门禁逻辑。
    auto cmk = read_text(target / "CMakeLists.txt");
    TEST_ASSERT(cmk.find("cmake_minimum_required(VERSION 3.20)") !=
                    std::string::npos,
                "CMake 声明最低版本");
    TEST_ASSERT(cmk.find("LANGUAGES NONE") != std::string::npos,
                "CMake 不编译 C++，契约与文档是唯一构建物");
    TEST_ASSERT(cmk.find("APOLLO_ENGINE_DIR") != std::string::npos &&
                    cmk.find("APOLLO_GEN") != std::string::npos,
                "CMake 显式声明引擎与生成器输入，不静默回退");
    TEST_ASSERT(cmk.find("APOLLO_BIN_DIR") == std::string::npos,
                "CMake 不掺运行时启动面");
    for (const char* gate : {"verify-build", "verify-contract", "verify-test",
                             "verify-docs", "verify"}) {
        TEST_ASSERT(cmk.find(gate) != std::string::npos,
                    std::string("CMake 暴露门禁目标 ") + gate);
    }
    TEST_ASSERT(cmk.find("scripts/verify.sh") != std::string::npos,
                "CMake 门禁目标是验证入口的薄封装，不复制门禁逻辑");
    TEST_ASSERT(cmk.find("add_subdirectory") == std::string::npos,
                "CMake 不拉引擎源码，只消费引擎产物");
    TEST_ASSERT(cmk.find("publish") == std::string::npos &&
                    cmk.find("create-release") == std::string::npos &&
                    cmk.find("gh release") == std::string::npos,
                "CMake 不含发布动作");

    // L40 CI 面：仓库 workflow 走与本地同一入口，不装进业务处理循环，
    // 不做任何发布动作。
    auto ci = read_text(target / ".github/workflows/ci.yml");
    TEST_ASSERT(ci.find("scripts/verify.sh") != std::string::npos,
                "CI 走与本地同一开发验证入口");
    TEST_ASSERT(ci.find("APOLLO_ENGINE_DIR") != std::string::npos,
                "CI 显式给出引擎路径");
    TEST_ASSERT(ci.find("apollo_gen") != std::string::npos,
                "CI 自建生成器，不依赖本机路径");
    TEST_ASSERT(ci.find("push") != std::string::npos &&
                    ci.find("pull_request") != std::string::npos,
                "CI 覆盖 push 与 PR 触发");
    TEST_ASSERT(ci.find("workflow_dispatch") != std::string::npos,
                "CI 支持手动触发");
    TEST_ASSERT(ci.find("create-release") == std::string::npos &&
                    ci.find("publish") == std::string::npos &&
                    ci.find("gh release") == std::string::npos,
                "CI 不做任何发布动作，版本号不来自 CI");

    // L40 开发验证入口：build/contract/test/docs 四道门禁，失败即停。
    auto verify = read_text(target / "scripts/verify.sh");
    TEST_ASSERT(verify.find("set -eu") != std::string::npos,
                "验证入口失败即停");
    TEST_ASSERT(verify.find(". \"$(dirname \"$0\")/lib.sh\"") !=
                    std::string::npos,
                "验证入口源 lib.sh 取项目根");
    for (const char* stage : {"gate_build", "gate_contract", "gate_test",
                              "gate_docs"}) {
        TEST_ASSERT(verify.find(stage) != std::string::npos,
                    std::string("验证入口提供阶段 ") + stage);
    }
    TEST_ASSERT(verify.find("xmllint --noout --schema") != std::string::npos,
                "契约门禁一：schema 校验");
    TEST_ASSERT(verify.find("--check") != std::string::npos &&
                    verify.find("--contract") != std::string::npos &&
                    verify.find("--out") != std::string::npos,
                "构建与契约门禁：生成产物并做漂移比对");
    TEST_ASSERT(verify.find("sh -n") != std::string::npos &&
                    verify.find("luac") != std::string::npos &&
                    verify.find(" -p ") != std::string::npos,
                "测试门禁：脚本与 Lua 语法检查");
    TEST_ASSERT(verify.find("README.md") != std::string::npos,
                "文档门禁：说明文档存在");
    TEST_ASSERT(verify.find("APOLLO_ENGINE_DIR") != std::string::npos &&
                    verify.find("apollo.xsd") != std::string::npos,
                "验证入口要求显式引擎路径（schema 来源）");
    TEST_ASSERT(verify.find("sleep") == std::string::npos,
                "验证入口无等待循环");

    // L40 starter 契约：从可校验的最小 errors 表起步，门禁有真实输入。
    TEST_ASSERT(read_text(target / "contract/version") == "1\n",
                "契约版本从 1 起步");
    auto errors = read_text(target / "contract/errors.xml");
    TEST_ASSERT(errors.find("<errors version=\"1\">") != std::string::npos,
                "starter 契约版本属性与 version 文件一致");
    TEST_ASSERT(errors.find("code=\"0\"") != std::string::npos &&
                    errors.find("name=\"ok\"") != std::string::npos,
                "starter 契约含 0/ok 基线");
    TEST_ASSERT(read_text(target / "contract/attrs.xml").find(
                        "<attrs version=\"1\">") != std::string::npos &&
                    read_text(target / "contract/entities.xml").find(
                        "<entities version=\"1\">") != std::string::npos &&
                    read_text(target / "contract/messages.xml").find(
                        "<messages version=\"1\">") != std::string::npos,
                "starter 契约空根齐全且根版本与 version 文件一致");
    bool baseline_leaked = false;
    for (const auto& f : apollo_cli::scaffold_files("proj")) {
        if (f.first.rfind("contract/generated", 0) == 0) baseline_leaked = true;
    }
    TEST_ASSERT(!baseline_leaked,
                "契约生成基线不在清单内，由验证入口产出后提交");

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

    // 清单本身：20 个文件、全为相对路径、无重复。
    auto files = apollo_cli::scaffold_files("proj");
    TEST_ASSERT(files.size() == 20, "模板清单 20 个文件");
    TEST_ASSERT(read_text(target / ".gitignore").find("contract/generated/") ==
                        std::string::npos,
                "契约生成基线是提交物，不被忽略");
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

bool test_init_rejects_unsafe_project_name() {
    auto root = fresh_dir();

    // 项目名进 README 首行与 CMake project()：空格会让生成的 CMake 无法
    // configure，分号/引号还能注入。名字不安全即拒收，不生成坏文件。
    auto spaced = root / "my proj";
    auto r1 = run({"init", spaced.string()});
    TEST_ASSERT(r1.code == 2, "含空格项目名拒收（退出 2）");
    TEST_ASSERT(r1.err.find("not safe") != std::string::npos,
                "项目名不安全原因明确");
    TEST_ASSERT(!std::filesystem::exists(spaced), "拒收后不落任何目录");

    auto semicolon = root / "bad;name";
    auto r2 = run({"init", semicolon.string()});
    TEST_ASSERT(r2.code == 2, "含分号项目名拒收（退出 2）");

    auto digit = root / "7zip";
    auto r3 = run({"init", digit.string()});
    TEST_ASSERT(r3.code == 2, "数字开头项目名拒收（退出 2）");

    auto dot = root / ".hidden";
    auto r4 = run({"init", dot.string()});
    TEST_ASSERT(r4.code == 2, "点开头项目名拒收（退出 2）");

    // 合法形态不受影响：字母开头，允许数字/下划线/连字符/点。
    for (const char* ok : {"proj", "my-proj", "my_proj", "proj.v2", "Proj1"}) {
        auto r = run({"init", (root / ok).string()});
        TEST_ASSERT(r.code == 0, std::string("合法项目名受理 ") + ok);
    }
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
        test_init_rejects_unsafe_project_name,
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
