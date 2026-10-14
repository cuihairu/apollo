#pragma once

// Lua 字节码缓存面（scripting-lua §3.5）：构建期预编译产物 + manifest
// 三元组（源 hash / bytecode hash / 版本格式签名字符串）校验的只读装载路径。
//
// 分工：构建管线（lua_precompile 工具）产 `<name>.luac` + manifest；运行期
// （ScriptRuntime）按 manifest 校验后走 luaL_loadbufferx 的 "b" 分支。
// 运行期只读不写——失效规则（Lua 版本变 / 源变 / 装载器逻辑变）全部在
// 构建期重跑管线处理，运行期三重校验任一不过即回退源码编译（mode "t"）。
//
// 红线不变（§2）：`load` 对运行期字符串仍拒 bytecode（sandbox_load mode
// "t"）；本路径只服务自家构建产物，不经 `load`。
//
// manifest 格式（行式，TAB 分隔；'#' 注释行）：
//   <name>\t<src_hash:016x>\t<bc_hash:016x>\t<format_sig>
// name = chunk 名（文件 stem，与装载方传入的 chunk 名一致）；format_sig =
// 字节码格式签名（LUA_VERSION_RELEASE_NUM + 四个尺寸字段，见
// bytecode_format_signature——版本头判据的等价形态：不依赖字节码头部布局，
// 工具与运行期从同一宏集取同值）。

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace apollo::script {

// 缓存开关 env（空/未设 = 关，装载面恒走源码编译；非空 = 缓存目录）。
inline constexpr const char* kBytecodeCacheEnv = "APOLLO_LUA_BYTECODE_CACHE";

// 字节码格式签名（工具与运行期同宏同值；跨 Lua 小版本必不同）。
std::string bytecode_format_signature();

// FNV-1a 64（篡改/损坏检测用，非加密口径——自家构建产物防撞库足够）。
std::uint64_t fnv1a64(const std::uint8_t* data, std::size_t size);

struct ManifestEntry {
    std::string name;
    std::uint64_t source_hash = 0;
    std::uint64_t bytecode_hash = 0;
    std::string format_sig;
};

// 行解析（格式不符/字段缺失/hex 非法 = false）。
bool parse_manifest_line(const std::string& line, ManifestEntry& out);

// manifest 全读（文件缺失 = 空清单——调用方按未命中处理；坏行跳过）。
std::vector<ManifestEntry> load_manifest(const std::string& path);

std::string to_hex(std::uint64_t v);

}  // namespace apollo::script
