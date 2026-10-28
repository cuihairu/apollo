// world 二进制分角色解析单测（L27）：
//   1. 缺省/无参数 = manager（管理端缺省）；
//   2. --role cell 与 --role=cell 两形态受理；
//   3. 未知角色显式拒绝（不静默回退）；
//   4. 缺值（末位 --role / 后随选项）显式拒绝；
//   5. 重复指定后到覆盖；其余选项共存不干扰。
//
// 纯解析头直测，无网络无定时器。

#include "world_app/role_options.hpp"

#include <iostream>
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

int run(const std::vector<const char*>& args, std::string& role, std::string& err) {
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>("world"));
    for (const char* a : args) {
        argv.push_back(const_cast<char*>(a));
    }
    return world_app::parse_world_role(static_cast<int>(argv.size()), argv.data(), role, err);
}

bool test_default_is_manager() {
    std::string role, err;
    TEST_ASSERT(run({}, role, err), "no args parses");
    TEST_ASSERT(role == "manager" && err.empty(), "无参数 = manager（管理端缺省）");

    const char* others1[] = {"--world-id", "1", "--m1-port", "9101"};
    TEST_ASSERT(run({others1[0], others1[1], others1[2], others1[3]}, role, err),
                "other flags parse");
    TEST_ASSERT(role == "manager", "未指定角色仍 = manager");

    const char* others2[] = {"--role", "--m1-port", "9101"};
    TEST_ASSERT(!run({others2[0], others2[1], others2[2]}, role, err),
                "--role followed by option rejected");
    return true;
}

bool test_cell_role_forms() {
    std::string role, err;
    const char* a[] = {"--role", "cell"};
    TEST_ASSERT(run({a[0], a[1]}, role, err), "space form parses");
    TEST_ASSERT(role == "cell" && err.empty(), "--role cell = 执行端");

    const char* b[] = {"--role=cell"};
    TEST_ASSERT(run({b[0]}, role, err), "equals form parses");
    TEST_ASSERT(role == "cell", "--role=cell = 执行端");

    const char* c[] = {"--role=manager"};
    TEST_ASSERT(run({c[0]}, role, err), "explicit manager parses");
    TEST_ASSERT(role == "manager", "--role=manager 显式管理端");

    const char* d[] = {"--m1-port", "9101", "--role=cell", "--world-id", "2"};
    TEST_ASSERT(run({d[0], d[1], d[2], d[3], d[4]}, role, err), "mixed flags parse");
    TEST_ASSERT(role == "cell", "与其他选项共存不干扰");
    return true;
}

bool test_unknown_role_rejected() {
    std::string role, err;
    const char* a[] = {"--role", "worker"};
    TEST_ASSERT(!run({a[0], a[1]}, role, err), "unknown value rejected");
    TEST_ASSERT(err.find("worker") != std::string::npos, "报错带出未知值");

    const char* b[] = {"--role=managerx"};
    TEST_ASSERT(!run({b[0]}, role, err), "equals-form unknown rejected");
    TEST_ASSERT(err.find("managerx") != std::string::npos, "equals 报错带出未知值");

    const char* c[] = {"--role"};
    TEST_ASSERT(!run({c[0]}, role, err), "trailing --role rejected");
    TEST_ASSERT(err.find("requires a value") != std::string::npos, "缺值报错明示");
    return true;
}

bool test_repeated_flag_last_wins() {
    std::string role, err;
    const char* a[] = {"--role=cell", "--role", "manager"};
    TEST_ASSERT(run({a[0], a[1], a[2]}, role, err), "repeated parses");
    TEST_ASSERT(role == "manager", "重复指定后到覆盖");

    const char* b[] = {"--role", "manager", "--role=cell"};
    TEST_ASSERT(run({b[0], b[1], b[2]}, role, err), "repeated parses");
    TEST_ASSERT(role == "cell", "重复指定后到覆盖（等号形态）");
    return true;
}


bool test_manager_only_flag_set() {
    // 委托路径剔除集须覆盖旧内联路径「接受并忽略」的全部 manager 专用 flag。
    const char* flags[] = {
        "--assign-spaces", "--world-journal", "--cell-expiry-ms",
        "--max-instances", "--base-business-host", "--base-business-port",
        "--settlement-credential-dir", "--service-register-host",
        "--service-register-port",
    };
    for (const char* f : flags) {
        TEST_ASSERT(world_app::is_manager_only_world_flag(f),
                    std::string("manager-only flag listed: ") + f);
    }
    // cell 相关 flag 不得混入剔除集（委托后须原样到达执行端）。
    TEST_ASSERT(!world_app::is_manager_only_world_flag("--m1-port"),
                "cell flag not manager-only");
    TEST_ASSERT(!world_app::is_manager_only_world_flag("--register-host"),
                "cell flag not manager-only");
    TEST_ASSERT(!world_app::is_manager_only_world_flag("--m1-authority-epoch"),
                "cell flag not manager-only");
    return true;
}


} // namespace

int main() {
    struct Case {
        const char* name;
        bool (*fn)();
    } cases[] = {
        {"default_is_manager", test_default_is_manager},
        {"cell_role_forms", test_cell_role_forms},
        {"unknown_role_rejected", test_unknown_role_rejected},
        {"repeated_flag_last_wins", test_repeated_flag_last_wins},
        {"manager_only_flag_set", test_manager_only_flag_set},
    };
    int passed = 0, failed = 0;
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
    std::cout << passed << " passed, " << failed << " failed" << std::endl;
    return failed == 0 ? 0 : 1;
}
