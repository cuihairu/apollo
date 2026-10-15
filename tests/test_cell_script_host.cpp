// Cell 脚本宿主接线批测试（admin attach transport 批 A）：
//   - boot 源加载：chunk 副作用对 scene 的 runtime 可见（经 §7.1 eval 通道读回）
//   - boot 文件缺失：不停机（fault 走 runtime 事件面，cell 继续服务）
//   - 未挂 scene 幂等补挂（gameLoop 每拍 attach，新 scene 下一拍生效）
//   - debug eval 入队 → Scene tick 首拍固定调试点执行 → 结果按 id 回传
// 观察面纪律：ScriptRuntime 归 gameThread_ 单写者（scripting-lua §2），故不断言
// 其指针；一切跨线程观察走 Scene §7.1 队列（投递/回传两端自带互斥）。
// 无 GTest，plain-assert + main 汇总（cell 族直编目标惯例）。
#include "cell/cell_server.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

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

const char* kRouteJson = APOLLO_CELL_CONTRACT_ROUTE_JSON;

using cell::CellConfig;
using cell::CellServer;

// 轮询谓词直到成立（每拍 sleep ≥2ms——macOS 裸自旋 flake 纪律）。
template <typename Pred>
bool poll_until(Pred&& pred, int timeout_ms) {
    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        if (pred()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return pred();
}

// 服务端：boot 路径按需；m1Port 0 = 内核分配（Windows 桩 listenPort 0，不断言）。
struct ServerEnd {
    CellConfig config;
    std::unique_ptr<CellServer> server;

    bool start(int tick_rate_ms) {
        config.tickRateMs = tick_rate_ms;
        config.m1Port = 0;
        config.m1RouteJson = kRouteJson;
        try {
            server = std::make_unique<CellServer>(config);
            server->start();
        } catch (const std::exception& e) {
            std::cout << "  server start failed: " << e.what() << std::endl;
            return false;
        }
        return server->isRunning();
    }
    void stop() {
        if (server) {
            server->stop();
            server.reset();
        }
    }
};

// 临时脚本源（binary 模式 + 先关句柄再删——Windows 落盘两坑纪律）。
struct BootFile {
    std::string path;
    BootFile(const std::string& source,
             const char* name = "cell_script_host_boot.lua")
        : path(name) {
        std::ofstream out(path, std::ios::binary);
        out << source;
    }
    ~BootFile() {
        if (!path.empty()) {
            std::remove(path.c_str());
        }
    }
};

// 默认 scene（world 首建 = id 1）已挂上脚本宿主。
bool default_scene_attached(CellServer& server) {
    auto* scene = server.find_scene(1);
    return scene != nullptr && scene->script_attached();
}

// 投一条 eval 并等回传结果（走 §7.1 队列：投递/回传自带互斥，跨线程安全）。
bool eval_and_await(CellServer& server, std::uint64_t id, const std::string& source,
                    apollo::game::world::Scene::DebugEvalResult& out) {
    auto* scene = server.find_scene(1);
    if (scene == nullptr || !scene->enqueue_debug_eval(id, source)) {
        return false;
    }
    bool done = poll_until([&] {
        auto batch = scene->drain_debug_eval_results();
        for (auto& r : batch) {
            if (r.id == id) {
                out = std::move(r);
                return true;
            }
        }
        return false;
    }, 5000);
    return done;
}

// ---------- boot 执行 + 默认 scene 幂等 attach ----------

bool test_boot_executes_and_scenes_attached() {
    std::cout << "Running: test_boot_executes_and_scenes_attached..." << std::endl;
    BootFile boot("cell_boot_ok = true\n");

    ServerEnd s;
    s.config.scriptBootFile = boot.path;
    TEST_ASSERT(s.start(10), "server started");

    // 默认 scene 下一拍自动挂上（attach_script_to_scenes 每拍幂等补挂）
    TEST_ASSERT(poll_until([&] { return default_scene_attached(*s.server); }, 5000),
                "default scene attached");

    // boot chunk 副作用对同一 runtime 可见（§2：runtime 建于 gameThread 开头，
    // boot 先于首拍 attach，故 attach 后即可读）
    apollo::game::world::Scene::DebugEvalResult r{};
    TEST_ASSERT(eval_and_await(*s.server, 1, "tostring(cell_boot_ok)", r),
                "boot chunk visible through eval channel");
    TEST_ASSERT(r.ok, "boot global read ok");
    TEST_ASSERT(r.result == "true", "boot chunk executed");

    s.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- boot 缺失不停机 ----------

bool test_missing_boot_file_keeps_cell_alive() {
    std::cout << "Running: test_missing_boot_file_keeps_cell_alive..." << std::endl;

    ServerEnd s;
    s.config.scriptBootFile = "cell_script_host_no_such_boot.lua";
    TEST_ASSERT(s.start(10), "server started with missing boot file");

    // runtime 仍照常建（boot 加载失败只发 warning），scene 照常挂上
    TEST_ASSERT(poll_until([&] { return default_scene_attached(*s.server); }, 5000),
                "scene still attached after boot load failure");
    TEST_ASSERT(s.server->isRunning(), "cell alive after boot load failure");

    // 数拍后仍运行（gameLoop 未因加载失败退出）
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    TEST_ASSERT(s.server->isRunning(), "cell still running after several ticks");

    s.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- debug eval 经宿主 tick 窗口回传 ----------

bool test_eval_reaches_scene_tick_window() {
    std::cout << "Running: test_eval_reaches_scene_tick_window..." << std::endl;

    ServerEnd s;
    TEST_ASSERT(s.start(10), "server started");
    TEST_ASSERT(poll_until([&] { return default_scene_attached(*s.server); }, 5000),
                "scene attached");

    apollo::game::world::Scene::DebugEvalResult r{};
    TEST_ASSERT(eval_and_await(*s.server, 7, "1+1", r), "eval result drained");
    TEST_ASSERT(r.ok, "eval ok");
    TEST_ASSERT(r.result == "2", "eval result value");

    s.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- eval 错误面：语法/运行错回 ok=false，不打断宿主 ----------

bool test_eval_error_keeps_host_alive() {
    std::cout << "Running: test_eval_error_keeps_host_alive..." << std::endl;

    ServerEnd s;
    TEST_ASSERT(s.start(10), "server started");
    TEST_ASSERT(poll_until([&] { return default_scene_attached(*s.server); }, 5000),
                "scene attached");

    apollo::game::world::Scene::DebugEvalResult bad{};
    TEST_ASSERT(eval_and_await(*s.server, 9, "error('boom')", bad),
                "failing eval still returns a result");
    TEST_ASSERT(!bad.ok, "failing eval reports not-ok");

    // 失败后宿主继续服务（后续 eval 仍通）
    apollo::game::world::Scene::DebugEvalResult after{};
    TEST_ASSERT(eval_and_await(*s.server, 10, "'alive'", after),
                "eval accepted after failure");
    TEST_ASSERT(after.ok && after.result == "alive", "host still serving");

    TEST_ASSERT(s.server->isRunning(), "cell alive after eval failure");
    s.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- 宿主启动装载（§3.4）：handler 面 → 契约 → boot 源 ----------

bool test_contract_loads_at_boot_with_handlers() {
    std::cout << "Running: test_contract_loads_at_boot_with_handlers..." << std::endl;
    // 启动序断言一体三用：boot 源能读 contract 全局 = 契约先于 boot 落地；
    // 契约能过装载闸 = handler 面先于契约落地（routes 引用的全局须已在位）。
    BootFile handlers("alpha_probe = function() end\n"
                      "beta_probe = function() end\n",
                      "cell_script_host_handlers.lua");
    BootFile contract(
        "return {\n"
        "  routes = {\n"
        "    [42001] = 'alpha_probe',\n"
        "    [42002] = 'beta_probe',\n"
        "  },\n"
        "  write_whitelist = {},\n"
        "  version = 7,\n"
        "}\n",
        "cell_script_host_contract.lua");
    BootFile boot("boot_seen_contract = tostring(contract and contract.version)\n");

    ServerEnd s;
    s.config.scriptHandersFile = handlers.path;
    s.config.scriptContractFile = contract.path;
    s.config.scriptBootFile = boot.path;
    TEST_ASSERT(s.start(10), "server started");

    TEST_ASSERT(poll_until([&] { return default_scene_attached(*s.server); }, 5000),
                "scene attached");

    apollo::game::world::Scene::DebugEvalResult r{};
    TEST_ASSERT(eval_and_await(*s.server, 21, "boot_seen_contract", r),
                "boot source ran");
    TEST_ASSERT(r.ok && r.result == "7", "contract loaded before boot source");

    apollo::game::world::Scene::DebugEvalResult route{};
    TEST_ASSERT(eval_and_await(*s.server, 22, "tostring(contract.routes[42001])", route),
                "route readable through eval");
    TEST_ASSERT(route.ok && route.result == "alpha_probe", "routes data face live");

    s.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- 装载闸：缺 handler 拒载 + cell 不停机 ----------

bool test_contract_gate_rejects_missing_handler() {
    std::cout << "Running: test_contract_gate_rejects_missing_handler..." << std::endl;
    // routes 引用未定义 handler → 闸拒载（原子提交不发生）→ contract 全局
    // 不存在；cell 照常服务（同 boot 源纪律）。
    BootFile contract(
        "return { routes = { [42003] = 'gamma_probe' }, version = 1 }\n",
        "cell_script_host_contract_bad.lua");

    ServerEnd s;
    s.config.scriptContractFile = contract.path;
    TEST_ASSERT(s.start(10), "server started");
    TEST_ASSERT(poll_until([&] { return default_scene_attached(*s.server); }, 5000),
                "scene still attached after contract rejection");

    apollo::game::world::Scene::DebugEvalResult r{};
    TEST_ASSERT(eval_and_await(*s.server, 23, "tostring(contract)", r),
                "eval served after contract rejection");
    TEST_ASSERT(r.ok && r.result == "nil", "rejected contract not committed");

    TEST_ASSERT(s.server->isRunning(), "cell alive after contract rejection");
    s.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- 契约文件缺失：warn + 继续 ----------

bool test_missing_contract_file_keeps_cell_alive() {
    std::cout << "Running: test_missing_contract_file_keeps_cell_alive..." << std::endl;

    ServerEnd s;
    s.config.scriptContractFile = "cell_script_host_no_such_contract.lua";
    TEST_ASSERT(s.start(10), "server started with missing contract file");
    TEST_ASSERT(poll_until([&] { return default_scene_attached(*s.server); }, 5000),
                "scene attached after missing contract file");

    apollo::game::world::Scene::DebugEvalResult r{};
    TEST_ASSERT(eval_and_await(*s.server, 24, "tostring(contract)", r),
                "eval served after missing contract file");
    TEST_ASSERT(r.ok && r.result == "nil", "no contract global");
    TEST_ASSERT(s.server->isRunning(), "cell alive after missing contract file");

    s.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

} // namespace

int main() {
    int failed = 0;
    if (!test_boot_executes_and_scenes_attached()) ++failed;
    if (!test_missing_boot_file_keeps_cell_alive()) ++failed;
    if (!test_eval_reaches_scene_tick_window()) ++failed;
    if (!test_eval_error_keeps_host_alive()) ++failed;
    if (!test_contract_loads_at_boot_with_handlers()) ++failed;
    if (!test_contract_gate_rejects_missing_handler()) ++failed;
    if (!test_missing_contract_file_keeps_cell_alive()) ++failed;
    if (failed == 0) {
        std::cout << "ALL CELL SCRIPT HOST TESTS PASSED" << std::endl;
        return 0;
    }
    std::cerr << failed << " cell script host test(s) failed" << std::endl;
    return 1;
}