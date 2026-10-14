// Lua 字节码缓存测试（scripting-lua.md §3.5）。
//
// 覆盖：
//   - manifest 行解析（合法/坏 hex/字段缺失/注释行）
//   - 缓存命中（"b" 装载 + cache_hits 计数）
//   - 三失效规则回退：源 hash 变 / 格式签名变 / 产物损坏
//   - 产物过 hash 闸但 "b" 装载失败 → 兜底回退源码
//   - 清单缺失 / 清单无该条目 → 回退（计 miss）
//   - env 未设：缓存面零计数（统计只反映缓存面本身）
//   - load_contract 走缓存（§3.4 × §3.5 交汇）
//
// 测试内自产 .luac + manifest（luaL_loadbufferx + lua_dump，与 lua_precompile
// 工具同路径）；cache 目录每用例独立，落盘全程 binary 模式（Windows 文本
// 模式 CRLF 坑），remove_all 前所有句柄已闭合。

#include <lua.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "apollo/script/lua_bytecode_cache.hpp"
#include "apollo/script/script_runtime.hpp"

using namespace apollo::script;

namespace {

namespace fs = std::filesystem;

void set_env(const char* name, const char* value) {
#ifdef _WIN32
    ::_putenv_s(name, value);
#else
    ::setenv(name, value, 1);
#endif
}

void unset_env(const char* name) {
#ifdef _WIN32
    ::_putenv_s(name, "");
#else
    ::unsetenv(name);
#endif
}

fs::path make_cache_dir(const std::string& tag) {
    fs::path dir = fs::temp_directory_path() / ("apollo_bc_tests_" + tag);
    fs::create_directories(dir);
    return dir;
}

void write_bytes(const fs::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(static_cast<bool>(out));
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    ASSERT_TRUE(static_cast<bool>(out));
}

void write_manifest(const fs::path& dir, const std::string& text) {
    std::vector<std::uint8_t> bytes(text.begin(), text.end());
    ASSERT_NO_FATAL_FAILURE(write_bytes(dir / "manifest", bytes));
}

// 与 lua_precompile 工具同路径的自产产物：源编译 → dump。
std::vector<std::uint8_t> compile_chunk(const char* name, const std::string& src) {
    lua_State* L = luaL_newstate();
    EXPECT_NE(L, nullptr);
    EXPECT_EQ(luaL_loadbufferx(L, src.data(), src.size(), name, "t"), LUA_OK);
    std::vector<std::uint8_t> bytes;
    lua_dump(L, [](lua_State*, const void* p, std::size_t sz, void* ud) -> int {
        auto* out = static_cast<std::vector<std::uint8_t>*>(ud);
        const auto* b = static_cast<const std::uint8_t*>(p);
        out->insert(out->end(), b, b + sz);
        return 0;
    }, &bytes, 0);
    lua_close(L);
    return bytes;
}

// 合法 manifest 条目行（三元组全对 → 命中面）。
std::string manifest_line(const char* name, const std::string& src,
                          const std::vector<std::uint8_t>& bc) {
    std::string line = std::string(name) + '\t';
    line += to_hex(fnv1a64(reinterpret_cast<const std::uint8_t*>(src.data()),
                           src.size()));
    line += '\t';
    line += to_hex(fnv1a64(bc.data(), bc.size()));
    line += '\t';
    line += bytecode_format_signature();
    return line;
}

void cleanup_dir(const fs::path& dir) {
    std::error_code ec;
    fs::remove_all(dir, ec);  // 句柄全闭合后调用（Windows 存活期坑）
}

ScriptConfig make_config() {
    ScriptConfig cfg;
    cfg.instruction_budget = 100000;
    cfg.memory_limit_bytes = 1 << 20;
    return cfg;
}

}  // namespace

// ── manifest 解码面 ──────────────────────────────────────

TEST(BytecodeCache, ParseManifestLine) {
    ManifestEntry e;
    EXPECT_TRUE(parse_manifest_line("boot\t0123456789abcdef\tfedcba9876543210\t55/4/8/8/8", e));
    EXPECT_EQ(e.name, "boot");
    EXPECT_EQ(e.source_hash, 0x0123456789abcdefULL);
    EXPECT_EQ(e.bytecode_hash, 0xfedcba9876543210ULL);
    EXPECT_EQ(e.format_sig, "55/4/8/8/8");

    EXPECT_FALSE(parse_manifest_line("boot\tzzzz\t0123456789abcdef\tsig", e));   // 坏 hex
    EXPECT_FALSE(parse_manifest_line("boot\t0123456789abcdef\tsig", e));         // 3 字段
    EXPECT_FALSE(parse_manifest_line("\t0123456789abcdef\t0123456789abcdef\tsig", e)); // 空名
    EXPECT_FALSE(parse_manifest_line("boot\t0123456789abcdef\t0123456789abcdef\t", e)); // 空 sig
    EXPECT_FALSE(parse_manifest_line("# comment", e));                            // 注释行

    EXPECT_EQ(to_hex(0xdeadbeefULL), "00000000deadbeef");
}

// ── 命中面 ──────────────────────────────────────────────

TEST(BytecodeCache, CacheHitCountsAndRuns) {
    const std::string src = "result = (41 + 1) == 42";
    fs::path dir = make_cache_dir("hit");
    const auto bc = compile_chunk("boot", src);
    ASSERT_FALSE(bc.empty());
    ASSERT_NO_FATAL_FAILURE(write_bytes(dir / "boot.luac", bc));
    ASSERT_NO_FATAL_FAILURE(write_manifest(dir, manifest_line("boot", src, bc) + "\n"));

    set_env(kBytecodeCacheEnv, dir.string().c_str());
    {
        ScriptRuntime rt(make_config());
        EXPECT_TRUE(rt.call_string("boot", src));
        EXPECT_TRUE(rt.get_global_bool("result"));
        EXPECT_EQ(rt.stats().cache_hits, 1u);
        EXPECT_EQ(rt.stats().cache_misses, 0u);
        // 同 chunk 再装载 = 再命中。
        EXPECT_TRUE(rt.call_string("boot", src));
        EXPECT_EQ(rt.stats().cache_hits, 2u);
    }
    unset_env(kBytecodeCacheEnv);
    cleanup_dir(dir);
}

// ── 三失效规则回退 ──────────────────────────────────────

TEST(BytecodeCache, SourceHashMismatchFallsBack) {
    const std::string src = "result = true";
    fs::path dir = make_cache_dir("srch");
    const auto bc = compile_chunk("boot", src);
    ASSERT_NO_FATAL_FAILURE(write_bytes(dir / "boot.luac", bc));
    // 清单源 hash 对应另一份源（源变未重跑管线）。
    ASSERT_NO_FATAL_FAILURE(
        write_manifest(dir, manifest_line("boot", "result = false", bc) + "\n"));

    set_env(kBytecodeCacheEnv, dir.string().c_str());
    {
        ScriptRuntime rt(make_config());
        EXPECT_TRUE(rt.call_string("boot", src));  // 回退源码编译，语义不变
        EXPECT_TRUE(rt.get_global_bool("result"));
        EXPECT_EQ(rt.stats().cache_hits, 0u);
        EXPECT_EQ(rt.stats().cache_misses, 1u);
    }
    unset_env(kBytecodeCacheEnv);
    cleanup_dir(dir);
}

TEST(BytecodeCache, BytecodeCorruptedFallsBack) {
    const std::string src = "result = true";
    fs::path dir = make_cache_dir("corrupt");
    const auto bc = compile_chunk("boot", src);
    ASSERT_NO_FATAL_FAILURE(write_manifest(dir, manifest_line("boot", src, bc) + "\n"));
    // 产物损坏（清单 hash 对，文件不对——落盘面损坏/篡改）。
    const std::string garbage = "\x1bLua\x55" "truncated-garbage";
    ASSERT_NO_FATAL_FAILURE(write_bytes(
        dir / "boot.luac",
        std::vector<std::uint8_t>(garbage.begin(), garbage.end())));

    set_env(kBytecodeCacheEnv, dir.string().c_str());
    {
        ScriptRuntime rt(make_config());
        EXPECT_TRUE(rt.call_string("boot", src));
        EXPECT_TRUE(rt.get_global_bool("result"));
        EXPECT_EQ(rt.stats().cache_hits, 0u);
        EXPECT_EQ(rt.stats().cache_misses, 1u);
    }
    unset_env(kBytecodeCacheEnv);
    cleanup_dir(dir);
}

TEST(BytecodeCache, FormatSignatureMismatchFallsBack) {
    const std::string src = "result = true";
    fs::path dir = make_cache_dir("fmtsig");
    const auto bc = compile_chunk("boot", src);
    ASSERT_NO_FATAL_FAILURE(write_bytes(dir / "boot.luac", bc));
    // 格式签名来自另一 Lua 构建（版本/尺寸集不同）。
    std::string line = manifest_line("boot", src, bc);
    line = line.substr(0, line.rfind('\t') + 1) + "999/1/2/4/8";
    ASSERT_NO_FATAL_FAILURE(write_manifest(dir, line + "\n"));

    set_env(kBytecodeCacheEnv, dir.string().c_str());
    {
        ScriptRuntime rt(make_config());
        EXPECT_TRUE(rt.call_string("boot", src));
        EXPECT_EQ(rt.stats().cache_hits, 0u);
        EXPECT_EQ(rt.stats().cache_misses, 1u);
    }
    unset_env(kBytecodeCacheEnv);
    cleanup_dir(dir);
}

// ── 未命中面 ──────────────────────────────────────────────

TEST(BytecodeCache, InvalidBytecodeFallsBackAfterHash) {
    const std::string src = "result = true";
    fs::path dir = make_cache_dir("bload");
    // hash 闸自洽（清单 hash = 文件 hash），但文件不是合法 bytecode——
    // "b" 装载失败 → 兜底回退源码（装载永不因缓存失败）。
    const std::string garbage = "this is not bytecode at all";
    const std::vector<std::uint8_t> bytes(garbage.begin(), garbage.end());
    ASSERT_NO_FATAL_FAILURE(write_bytes(dir / "boot.luac", bytes));
    std::string line = std::string("boot") + '\t' +
                       to_hex(fnv1a64(reinterpret_cast<const std::uint8_t*>(src.data()),
                                      src.size())) +
                       '\t' + to_hex(fnv1a64(bytes.data(), bytes.size())) + '\t' +
                       bytecode_format_signature();
    ASSERT_NO_FATAL_FAILURE(write_manifest(dir, line + "\n"));

    set_env(kBytecodeCacheEnv, dir.string().c_str());
    {
        ScriptRuntime rt(make_config());
        EXPECT_TRUE(rt.call_string("boot", src));
        EXPECT_TRUE(rt.get_global_bool("result"));
        EXPECT_EQ(rt.stats().cache_hits, 0u);
        EXPECT_EQ(rt.stats().cache_misses, 1u);
    }
    unset_env(kBytecodeCacheEnv);
    cleanup_dir(dir);
}

TEST(BytecodeCache, ManifestMissingFallsBack) {
    fs::path dir = make_cache_dir("nomanifest");  // 目录在、manifest 不在
    set_env(kBytecodeCacheEnv, dir.string().c_str());
    {
        ScriptRuntime rt(make_config());
        EXPECT_TRUE(rt.call_string("boot", "result = true"));
        EXPECT_EQ(rt.stats().cache_hits, 0u);
        EXPECT_EQ(rt.stats().cache_misses, 1u);
    }
    unset_env(kBytecodeCacheEnv);
    cleanup_dir(dir);
}

TEST(BytecodeCache, EntryMissingCountsMiss) {
    const std::string src = "result = true";
    fs::path dir = make_cache_dir("noentry");
    const auto bc = compile_chunk("other", src);
    ASSERT_NO_FATAL_FAILURE(write_bytes(dir / "other.luac", bc));
    ASSERT_NO_FATAL_FAILURE(write_manifest(dir, manifest_line("other", src, bc) + "\n"));

    set_env(kBytecodeCacheEnv, dir.string().c_str());
    {
        ScriptRuntime rt(make_config());
        EXPECT_TRUE(rt.call_string("boot", src));  // 清单无 "boot" 条目
        EXPECT_EQ(rt.stats().cache_hits, 0u);
        EXPECT_EQ(rt.stats().cache_misses, 1u);
    }
    unset_env(kBytecodeCacheEnv);
    cleanup_dir(dir);
}

TEST(BytecodeCache, EnvUnsetMeansNoCacheStats) {
    unset_env(kBytecodeCacheEnv);
    ScriptRuntime rt(make_config());
    EXPECT_TRUE(rt.call_string("boot", "result = true"));
    EXPECT_TRUE(rt.get_global_bool("result"));
    EXPECT_EQ(rt.stats().cache_hits, 0u);
    EXPECT_EQ(rt.stats().cache_misses, 0u);
}

// ── §3.4 × §3.5 交汇：契约表装载走缓存 ───────────────────

TEST(BytecodeCache, LoadContractUsesCache) {
    const std::string src = R"(
function h_1001() contract_hit = true end
return { routes = { [1001] = 'h_1001' }, version = 7 }
)";
    fs::path dir = make_cache_dir("contract");
    const auto bc = compile_chunk("contract", src);
    ASSERT_NO_FATAL_FAILURE(write_bytes(dir / "contract.luac", bc));
    ASSERT_NO_FATAL_FAILURE(write_manifest(dir, manifest_line("contract", src, bc) + "\n"));

    set_env(kBytecodeCacheEnv, dir.string().c_str());
    {
        ScriptRuntime rt(make_config());
        EXPECT_TRUE(rt.load_contract(src));
        EXPECT_EQ(rt.contract_version(), 7u);
        EXPECT_EQ(rt.stats().cache_hits, 1u);
        EXPECT_TRUE(rt.call_route(1001));
        EXPECT_TRUE(rt.get_global_bool("contract_hit"));
    }
    unset_env(kBytecodeCacheEnv);
    cleanup_dir(dir);
}
