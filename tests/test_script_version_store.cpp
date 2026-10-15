// §3.2 current 版本指针持久化测试（scripting-lua.md §3.2「回滚要持久化
// current 版本指针」——防坏版本崩溃循环）。
//
// 覆盖：
//   - 清单行解析/整表原子写/缺文件与坏行容错（store 函数面）
//   - 换表成功/commit 成功/回滚成功三处回写落盘（runtime 集成面）
//   - 空路径 = 关闭（不写盘、读空表）
//   - prepare 不写（版本尚未激活）
//   - 崩溃循环防护：回滚 → 进程重启（新 runtime 同路径）→ current 指针
//     仍为回滚版本（已判坏版本不再被默认装载）
//   - 跨 runtime 持久性：A 写入、B 读取

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "apollo/script/lua_bytecode_cache.hpp"
#include "apollo/script/module_version_store.hpp"
#include "apollo/script/script_runtime.hpp"

using namespace apollo::script;

namespace {

// 测试工作目录（每测试独享，结束即清）。临时目录路径随平台。
class VersionStore : public ::testing::Test {
  protected:
    void SetUp() override {
        dir_ = std::filesystem::temp_directory_path() / "apollo_vs_store_test";
        std::filesystem::remove_all(dir_);
        std::filesystem::create_directories(dir_);
        path_ = (dir_ / "current_versions").string();
    }
    void TearDown() override {
        std::filesystem::remove_all(dir_);
    }
    std::filesystem::path dir_;
    std::string path_;
};

ScriptConfig store_config(const std::string& path) {
    ScriptConfig cfg;
    cfg.instruction_budget = 100000;
    cfg.memory_limit_bytes = 1 << 20;
    cfg.version_store_path = path;
    return cfg;
}

constexpr const char* kBody = R"(
return {
    add = function(a, b) return a + b end,
    smoke = function() return true end,
}
)";

}  // namespace

// ── store 函数面 ──────────────────────────────────────────

TEST_F(VersionStore,ParseWriteLoadRoundTrip) {
    const std::vector<VersionEntry> in = {{"math", 1}, {"physics", 7}, {"net", 42}};
    ASSERT_TRUE(write_version_manifest(path_, in));
    const auto out = load_version_manifest(path_);
    ASSERT_EQ(out.size(), in.size());
    for (std::size_t i = 0; i < in.size(); ++i) {
        EXPECT_EQ(out[i].name, in[i].name);
        EXPECT_EQ(out[i].version, in[i].version);
    }
}

TEST_F(VersionStore,LoadMissingReturnsEmpty) {
    EXPECT_TRUE(load_version_manifest("/nonexistent/path/nowhere").empty());
}

TEST_F(VersionStore,LoadSkipsBadLines) {
    // 注释行/空行/字段缺失/hex 非法/尾随字段 全跳过，好行保留。
    std::ofstream out(path_, std::ios::binary);
    out << "# comment\n"
        << "\n"
        << "math\t" << to_hex(5) << "\n"
        << "physics\n"
        << "bad\tZZZZZZZZZZZZZZZZ\n"
        << "net\t1\textra\n"
        << "logic\t" << to_hex(9) << "\n";
    out.close();
    const auto entries = load_version_manifest(path_);
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].name, "math");
    EXPECT_EQ(entries[0].version, 5u);
    EXPECT_EQ(entries[1].name, "logic");
    EXPECT_EQ(entries[1].version, 9u);
}

TEST_F(VersionStore,ParseLineRejectsMalformed) {
    VersionEntry e;
    EXPECT_FALSE(parse_version_line("", e));
    EXPECT_FALSE(parse_version_line("# comment", e));
    EXPECT_FALSE(parse_version_line("math", e));
    EXPECT_FALSE(parse_version_line("math\t1", e));
    EXPECT_FALSE(parse_version_line("math\tZZZZZZZZZZZZZZZZ", e));
    EXPECT_FALSE(parse_version_line("math\t" + to_hex(1) + "\textra", e));
    EXPECT_FALSE(parse_version_line("\t" + to_hex(1), e));
}

TEST_F(VersionStore,WriteAtomicNoTmpLeftover) {
    ASSERT_TRUE(write_version_manifest(path_, {{"a", 1}}));
    EXPECT_FALSE(std::filesystem::exists(path_ + ".tmp"));
    // 二次写覆盖，仍无临时残留。
    ASSERT_TRUE(write_version_manifest(path_, {{"a", 2}}));
    EXPECT_FALSE(std::filesystem::exists(path_ + ".tmp"));
    const auto entries = load_version_manifest(path_);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].version, 2u);
}

// ── runtime 集成面 ────────────────────────────────────────

TEST_F(VersionStore, PersistOnSwap) {
    ScriptRuntime rt(store_config(path_));
    ASSERT_TRUE(rt.swap_module({"math", 1, {}}, kBody));
    auto m = rt.current_version_manifest();
    ASSERT_EQ(m.size(), 1u);
    EXPECT_EQ(m["math"], 1u);
}

TEST_F(VersionStore, PersistOnCommit) {
    ScriptRuntime rt(store_config(path_));
    ASSERT_TRUE(rt.commit_module(rt.prepare_module({"math", 3, {}}, kBody)));
    auto m = rt.current_version_manifest();
    ASSERT_EQ(m.size(), 1u);
    EXPECT_EQ(m["math"], 3u);
}

TEST_F(VersionStore, PersistOnRollback) {
    ScriptRuntime rt(store_config(path_));
    ASSERT_TRUE(rt.swap_module({"math", 1, {}}, kBody));
    ASSERT_TRUE(rt.swap_module({"math", 2, {}}, kBody));
    auto before = rt.current_version_manifest();
    EXPECT_EQ(before["math"], 2u);
    ASSERT_TRUE(rt.rollback_module("math"));
    auto after = rt.current_version_manifest();
    ASSERT_EQ(after.size(), 1u);
    EXPECT_EQ(after["math"], 1u);  // 回滚把 current 改回上一版本
}

TEST_F(VersionStore, PersistUpdatesExistingEntry) {
    ScriptRuntime rt(store_config(path_));
    ASSERT_TRUE(rt.swap_module({"math", 1, {}}, kBody));
    ASSERT_TRUE(rt.swap_module({"net", 4, {}}, kBody));
    auto m1 = rt.current_version_manifest();
    EXPECT_EQ(m1.size(), 2u);
    ASSERT_TRUE(rt.swap_module({"math", 2, {}}, kBody));
    auto m2 = rt.current_version_manifest();
    EXPECT_EQ(m2.size(), 2u);  // 两模块并存，非单模块文件
    EXPECT_EQ(m2["math"], 2u);
    EXPECT_EQ(m2["net"], 4u);
}

TEST_F(VersionStore, DisabledWhenPathEmpty) {
    ScriptConfig cfg;
    cfg.instruction_budget = 100000;
    cfg.memory_limit_bytes = 1 << 20;
    ScriptRuntime rt(cfg);  // version_store_path 默认空
    ASSERT_TRUE(rt.swap_module({"math", 1, {}}, kBody));
    EXPECT_TRUE(rt.current_version_manifest().empty());
    EXPECT_FALSE(std::filesystem::exists(path_));  // 不写盘
}

TEST_F(VersionStore, PrepareDoesNotPersist) {
    ScriptRuntime rt(store_config(path_));
    rt.prepare_module({"math", 1, {}}, kBody);
    EXPECT_TRUE(rt.current_version_manifest().empty());
    EXPECT_FALSE(std::filesystem::exists(path_));  // 预备不写（未激活）
}

TEST_F(VersionStore, PersistOnRejectedSwapNoWrite) {
    ScriptRuntime rt(store_config(path_));
    EXPECT_FALSE(rt.swap_module({"math", 1, {}}, "return { = );\n"));
    EXPECT_TRUE(rt.current_version_manifest().empty());
    EXPECT_FALSE(std::filesystem::exists(path_));
}

// ── 崩溃循环防护（核心场景）───────────────────────────────

TEST_F(VersionStore, CrashLoopPrevention) {
    // 版本 1 健康 → 装载；版本 2 判坏 → 回滚 → 进程崩溃（作用域退出）。
    {
        ScriptRuntime rt1(store_config(path_));
        ASSERT_TRUE(rt1.swap_module({"math", 1, {}}, kBody));
        ASSERT_TRUE(rt1.swap_module({"math", 2, {}}, kBody));
        EXPECT_EQ(rt1.current_version_manifest()["math"], 2u);
        ASSERT_TRUE(rt1.rollback_module("math"));  // 回滚到版本 1
        EXPECT_EQ(rt1.current_version_manifest()["math"], 1u);
    }  // rt1 析构 = 模拟进程崩溃

    // 重启：新 runtime 同路径读清单 → current = 1（健康版本），非 2（判坏）。
    ScriptRuntime rt2(store_config(path_));
    auto m = rt2.current_version_manifest();
    ASSERT_EQ(m.size(), 1u);
    EXPECT_EQ(m["math"], 1u);
}

TEST_F(VersionStore, PersistSurvivesRuntimeRestart) {
    // A 写入 → A 析构 → B 读取（跨进程持久性的进程内等价）。
    {
        ScriptRuntime rt_a(store_config(path_));
        ASSERT_TRUE(rt_a.swap_module({"math", 5, {}}, kBody));
    }  // rt_a 析构

    ScriptRuntime rt_b(store_config(path_));
    auto m = rt_b.current_version_manifest();
    ASSERT_EQ(m.size(), 1u);
    EXPECT_EQ(m["math"], 5u);
}

TEST_F(VersionStore, ManifestReadByFreshRuntimeWithNoWrite) {
    // 预置清单（模拟控制面或前次运行写入），新 runtime 直接可读。
    ASSERT_TRUE(write_version_manifest(path_, {{"math", 8}, {"net", 3}}));
    ScriptRuntime rt(store_config(path_));
    auto m = rt.current_version_manifest();
    ASSERT_EQ(m.size(), 2u);
    EXPECT_EQ(m["math"], 8u);
    EXPECT_EQ(m["net"], 3u);
}
