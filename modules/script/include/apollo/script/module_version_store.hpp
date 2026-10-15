#pragma once

// §3.2 current 版本指针持久化（防坏版本崩溃循环）：内存换表只在进程活着时
// 有效——回滚前崩溃会让重启重新装载已判坏版本。回滚/换表成功即回写持久
// current 指针（patch 目录版本清单），重启默认装载「最新已知健康版本」。
//
// 与 §3.5 字节码缓存的分工：缓存 manifest 是**构建期产物、运行期只读**；
// 本清单是**运行期写、控制面读**——两者同处 patch 目录但访问面相反，故分文件。
//
// 清单格式（行式，TAB 分隔；'#' 注释行）：
//   <name>\t<version:016x>
// name = 模块名；version = 当前（最新已知健康）版本号。
//
// 写入纪律：整表读改写 + 临时文件 rename 原子替换（防写一半崩溃留半截清单）。
// 写时机只有两处——换表成功（finish_module_swap_）与回滚成功
// （rollback_module）；prepare 不写（版本尚未激活）。

#include <cstdint>
#include <string>
#include <vector>

namespace apollo::script {

struct VersionEntry {
    std::string name;
    std::uint64_t version = 0;
};

// 行解析（格式不符/字段缺失/hex 非法 = false）。
bool parse_version_line(const std::string& line, VersionEntry& out);

// 清单全读（文件缺失 = 空清单——调用方按未命中处理；坏行跳过）。
std::vector<VersionEntry> load_version_manifest(const std::string& path);

// 整表原子写（临时文件 + rename；目录不可写 = false，不抛）。
bool write_version_manifest(const std::string& path,
                            const std::vector<VersionEntry>& entries);

}  // namespace apollo::script
