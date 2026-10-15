#pragma once

// §3.2 Phase A 工程面：patch 目录读 + hash 校验（控制面装载前置）。
//
// patch 目录 = 构建管线（§3.5）产物，形态：
//   <name>.lua     源码（必需——prepare 的输入，也是 hash 校验对象）
//   <name>.luac    预编译字节码（可选——版本头匹配才随产物带出）
//   manifest       三元组清单（§3.5 lua_bytecode_cache 同格式：
//                  <name>\t<src_hash:016x>\t<bc_hash:016x>\t<format_sig>）
//
// 校验纪律（控制面在调 prepare 之前先过这一层）：
//   - 源 hash 不符 = 篡改/损坏 → 该模块**拒载**（进 errors，不抛）；
//   - 字节码 hash 不符 = 产物损坏 → **丢字节码留源码**（产物回退仍可用）；
//   - format_sig 不符 = 跨 Lua 小版本（§3.5 版本头纪律）→ **丢字节码留源码**；
//   - 源码缺失 = 拒载。manifest 缺失/目录不可读 = 整体错误。
//
// 分工：本面只负责「读 + 校验 + 带出产物」，编译/冒烟/换表归
// ScriptRuntime::prepare_module（Phase A 主线）；控制面按本面返回的清单
// 串行调用 prepare/commit（含 deps 拓扑序——归控制面编排）。

#include <cstdint>
#include <string>
#include <vector>

namespace apollo::script {

struct PatchModule {
    std::string name;
    std::string source;                   // 校验过的源码（prepare 输入）
    std::vector<std::uint8_t> bytecode;   // 可能为空（无产物 / hash 或版本头不符）
    std::uint64_t source_hash = 0;        // manifest 记录的源 hash（已比对一致）
    std::uint64_t bytecode_hash = 0;      // manifest 记录的字节码 hash
};

struct PatchLoadResult {
    std::vector<PatchModule> modules;  // 校验通过的模块（清单序）
    std::vector<std::string> errors;   // "<name>: <原因>" 行（拒载/整体错误）
};

// 读 patch 目录（manifest 驱动逐条目校验）。目录不存在 / manifest 缺失 /
// manifest 坏行跳过——坏条目不整体拒载（同 §3.5 清单面纪律）。
PatchLoadResult load_patch_directory(const std::string& dir);

}  // namespace apollo::script
