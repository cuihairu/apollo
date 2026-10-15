// §3.2 Phase A 工程面：patch 目录读 + hash 校验（控制面装载前置）。
// patch 目录形态（§3.5 构建管线产物）：<name>.lua + 可选 <name>.luac + manifest
// 三元组清单。校验纪律：源 hash 不符 = 拒载该模块；字节码 hash / format_sig
// 不符 = 丢字节码留源码（产物仍可用）。

#include "apollo/script/lua_bytecode_cache.hpp"  // load_manifest / fnv1a64 / to_hex / bytecode_format_signature
#include "apollo/script/patch_loader.hpp"
#include "apollo/script/script_runtime.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace apollo::script;

namespace {

class PatchLoader : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = std::filesystem::temp_directory_path() / "apollo_patch_loader_test";
        std::filesystem::remove_all(dir_);
        std::filesystem::create_directories(dir_);
    }
    void TearDown() override { std::filesystem::remove_all(dir_); }

    std::filesystem::path dir_;

    static std::string read_file(const std::filesystem::path& p) {
        std::ifstream in(p, std::ios::binary);
        std::string s;
        if (in) {
            s.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        }
        return s;
    }
    static void write_file(const std::filesystem::path& p, const std::string& s) {
        std::ofstream out(p, std::ios::binary | std::ios::trunc);
        out << s;
    }

    // 以源内容写完整 patch 目录（lua + 三元组 manifest）；luac 可选。
    void install_patch(const std::vector<std::pair<std::string, std::string>>& modules,
                       const std::string& format_sig = "sig-current") {
        for (const auto& [name, source] : modules) {
            write_file(dir_ / (name + ".lua"), source);
        }
        std::string manifest;
        for (const auto& [name, source] : modules) {
            const std::uint64_t src_hash = hash_bytes(source);
            manifest += name + '\t' + apollo::script::to_hex(src_hash) + "\t" +
                        apollo::script::to_hex(src_hash) + '\t' + format_sig + '\n';
        }
        write_file(dir_ / "manifest", manifest);
    }

    static std::uint64_t hash_bytes(const std::string& s) {
        return apollo::script::fnv1a64(
            reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
    }

    ScriptConfig cfg(const std::string& store_path) {
        ScriptConfig c;
        c.instruction_budget = 100000;
        c.memory_limit_bytes = 1 << 20;
        c.version_store_path = store_path;
        return c;
    }
};

// ── 装载面 ────────────────────────────────────────

TEST_F(PatchLoader, LoadsModulesFromManifest) {
    install_patch({{"math", "return { add = function(a,b) return a+b end }"},
                   {"net", "return { send = function() end }"}});
    auto result = apollo::script::load_patch_directory(dir_.string());
    ASSERT_TRUE(result.errors.empty()) << (result.errors.empty() ? "" : result.errors[0]);
    ASSERT_EQ(result.modules.size(), 2u);
    EXPECT_EQ(result.modules[0].name, "math");
    EXPECT_EQ(result.modules[0].source, "return { add = function(a,b) return a+b end }");
    EXPECT_EQ(result.modules[0].source_hash,
              hash_bytes("return { add = function(a,b) return a+b end }"));
    EXPECT_TRUE(result.modules[0].bytecode.empty());  // 无 luac = 纯源码
}

TEST_F(PatchLoader, MissingManifestYieldsError) {
    write_file(dir_ / "math.lua", "return {}");
    auto result = apollo::script::load_patch_directory(dir_.string());
    EXPECT_TRUE(result.modules.empty());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_NE(result.errors[0].find("manifest"), std::string::npos);
}

TEST_F(PatchLoader, SourceHashMismatchRejectsModule) {
    // manifest 记录 hash ≠ 实际源内容（篡改/损坏）。
    install_patch({{"math", "return { v = 1 }"}});
    std::string manifest = "math\t" + apollo::script::to_hex(hash_bytes("return { v = 2 }")) +
                           "\t0000000000000000\tsig-current\n";
    write_file(dir_ / "manifest", manifest);
    auto result = apollo::script::load_patch_directory(dir_.string());
    EXPECT_TRUE(result.modules.empty());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_NE(result.errors[0].find("math"), std::string::npos);
    EXPECT_NE(result.errors[0].find("source"), std::string::npos);
}

TEST_F(PatchLoader, MissingSourceRejectsModule) {
    install_patch({{"math", "return {}"}});
    std::filesystem::remove(dir_ / "math.lua");
    auto result = apollo::script::load_patch_directory(dir_.string());
    EXPECT_TRUE(result.modules.empty());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_NE(result.errors[0].find("math"), std::string::npos);
}

TEST_F(PatchLoader, MissingBytecodeKeepsSource) {
    install_patch({{"math", "return {}"}});
    std::string manifest = "math\t" + apollo::script::to_hex(hash_bytes("return {}")) +
                           "\t0000000000000000\tsig-current\n";
    write_file(dir_ / "manifest", manifest);
    auto result = apollo::script::load_patch_directory(dir_.string());
    ASSERT_TRUE(result.errors.empty());
    ASSERT_EQ(result.modules.size(), 1u);
    EXPECT_TRUE(result.modules[0].bytecode.empty());
    EXPECT_EQ(result.modules[0].source, "return {}");
}

TEST_F(PatchLoader, BytecodeHashMismatchDropsBytecodeKeepsSource) {
    install_patch({{"math", "return {}"}});
    // luac 内容 ≠ manifest 字节码 hash（产物损坏）→ 丢字节码留源码。
    write_file(dir_ / "math.luac", "corrupted-bytes");
    std::string manifest = "math\t" + apollo::script::to_hex(hash_bytes("return {}")) +
                           "\t0000000000000000\tsig-current\n";
    write_file(dir_ / "manifest", manifest);
    auto result = apollo::script::load_patch_directory(dir_.string());
    ASSERT_TRUE(result.errors.empty());
    ASSERT_EQ(result.modules.size(), 1u);
    EXPECT_TRUE(result.modules[0].bytecode.empty());
    EXPECT_EQ(result.modules[0].source, "return {}");
}

TEST_F(PatchLoader, FormatSigMismatchDropsBytecodeKeepsSource) {
    // 字节码版本头不符（跨 Lua 小版本）→ 丢字节码留源码（§3.5 版本头纪律）。
    install_patch({{"math", "return {}"}}, "sig-other-version");
    write_file(dir_ / "math.luac", "bytecode-bytes");
    std::string manifest = "math\t" + apollo::script::to_hex(hash_bytes("return {}")) +
                           '\t' + apollo::script::to_hex(hash_bytes("bytecode-bytes")) +
                           "\tsig-other-version\n";
    write_file(dir_ / "manifest", manifest);
    auto result = apollo::script::load_patch_directory(dir_.string());
    ASSERT_TRUE(result.errors.empty());
    ASSERT_EQ(result.modules.size(), 1u);
    EXPECT_TRUE(result.modules[0].bytecode.empty());
}

TEST_F(PatchLoader, ValidBytecodeKept) {
    install_patch({{"math", "return {}"}});
    write_file(dir_ / "math.luac", "real-bytecode");
    std::string manifest = "math\t" + apollo::script::to_hex(hash_bytes("return {}")) +
                           '\t' + apollo::script::to_hex(hash_bytes("real-bytecode")) +
                           '\t' + apollo::script::bytecode_format_signature() + '\n';
    write_file(dir_ / "manifest", manifest);
    auto result = apollo::script::load_patch_directory(dir_.string());
    ASSERT_TRUE(result.errors.empty());
    ASSERT_EQ(result.modules.size(), 1u);
    ASSERT_EQ(result.modules[0].bytecode.size(), 13u);
    EXPECT_EQ(std::string(result.modules[0].bytecode.begin(),
                          result.modules[0].bytecode.end()),
              "real-bytecode");
}

TEST_F(PatchLoader, OrderFollowsManifest) {
    install_patch({{"zeta", "return {}"}, {"alpha", "return {}"}, {"mid", "return {}"}});
    auto result = apollo::script::load_patch_directory(dir_.string());
    ASSERT_TRUE(result.errors.empty());
    ASSERT_EQ(result.modules.size(), 3u);
    EXPECT_EQ(result.modules[0].name, "zeta");
    EXPECT_EQ(result.modules[1].name, "alpha");
    EXPECT_EQ(result.modules[2].name, "mid");
}

TEST_F(PatchLoader, EmptyDirYieldsManifestError) {
    auto result = apollo::script::load_patch_directory(dir_.string());
    EXPECT_TRUE(result.modules.empty());
    ASSERT_EQ(result.errors.size(), 1u);
}

// ── 控制面端到端：patch 目录 → prepare → commit ────────

TEST_F(PatchLoader, EndToEndPrepareAndCommit) {
    install_patch({{"math", "return { add = function(a,b) return a+b end, smoke = function() return true end }"}});
    auto patch = apollo::script::load_patch_directory(dir_.string());
    ASSERT_TRUE(patch.errors.empty());
    ASSERT_EQ(patch.modules.size(), 1u);

    std::string store_path = (dir_ / "current_versions").string();
    ScriptRuntime rt(cfg(store_path));
    auto prepared =
        rt.prepare_module({"math", 1, {}}, patch.modules[0].source);
    ASSERT_TRUE(prepared.error.empty()) << prepared.error;
    ASSERT_TRUE(rt.commit_module(prepared));
    auto m = rt.current_version_manifest();
    ASSERT_EQ(m.size(), 1u);
    EXPECT_EQ(m["math"], 1u);
}

TEST_F(PatchLoader, TamperedPatchBlockedByPrepareFlow) {
    // 源 hash 不符的模块进不了 prepare（装载前置拦截）。
    install_patch({{"math", "return { smoke = function() return true end }"}});
    std::string manifest = "math\t" + apollo::script::to_hex(hash_bytes("tampered")) +
                           "\t0000000000000000\tsig-current\n";
    write_file(dir_ / "manifest", manifest);
    auto patch = apollo::script::load_patch_directory(dir_.string());
    EXPECT_TRUE(patch.modules.empty());
    ASSERT_EQ(patch.errors.size(), 1u);
}

}  // namespace
