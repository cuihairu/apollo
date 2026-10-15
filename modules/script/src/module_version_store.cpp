// §3.2 current 版本指针持久化实现——清单行解析 + 整表原子写。
// 格式与 §3.5 字节码缓存 manifest 同族（行式 TAB 分隔、'#' 注释、坏行跳过），
// 但访问面相反：本清单运行期写、控制面读。

#include "apollo/script/module_version_store.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "apollo/script/lua_bytecode_cache.hpp"  // to_hex（同命名空间复用）

namespace apollo::script {
namespace {

// 版本字段 = 16 位 hex（与 §3.5 hash 同宽，to_hex 同口径）。
bool parse_version_hex(const std::string& s, std::uint64_t& out) {
    if (s.size() != 16) {
        return false;
    }
    std::uint64_t v = 0;
    for (const char c : s) {
        v <<= 4;
        if (c >= '0' && c <= '9') {
            v |= static_cast<std::uint64_t>(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            v |= static_cast<std::uint64_t>(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            v |= static_cast<std::uint64_t>(c - 'A' + 10);
        } else {
            return false;
        }
    }
    out = v;
    return true;
}

// rename 覆盖已存在目标在部分 Windows CRT 上返回非零（不替换）；先删目标再 rename。
// tmp 已写完才删——目标位置永不出现半截文件，旧清单在 rename 成功前保持完整。
bool place_atomic_(const std::string& tmp, const std::string& path) {
    if (std::rename(tmp.c_str(), path.c_str()) == 0) {
        return true;  // 目标不存在，或 CRT 支持替换
    }
    (void)std::remove(path.c_str());
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}

}  // namespace

bool parse_version_line(const std::string& line, VersionEntry& out) {
    if (line.empty() || line[0] == '#') {
        return false;
    }
    std::vector<std::string> f;
    std::stringstream ss(line);
    std::string field;
    while (std::getline(ss, field, '\t')) {
        f.push_back(field);
    }
    if (f.size() != 2 || f[0].empty()) {
        return false;
    }
    VersionEntry e;
    e.name = f[0];
    if (!parse_version_hex(f[1], e.version)) {
        return false;
    }
    out = std::move(e);
    return true;
}

std::vector<VersionEntry> load_version_manifest(const std::string& path) {
    std::vector<VersionEntry> entries;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return entries;  // 缺失 = 空清单（未命中面）
    }
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        VersionEntry e;
        if (parse_version_line(line, e)) {
            entries.push_back(std::move(e));
        }
        // 坏行跳过（运行期写、控制面读——单行坏损不整体拒载）
    }
    return entries;
}

bool write_version_manifest(const std::string& path,
                            const std::vector<VersionEntry>& entries) {
    // 整表原子写：先写临时文件，再 rename 覆盖——防写一半崩溃留半截清单
    // （半截清单会让重启误读 current 指针，正是本机制要防的崩溃循环）。
    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return false;
        }
        out << "# apollo current module versions (written by ScriptRuntime)\n";
        for (const auto& e : entries) {
            out << e.name << '\t' << to_hex(e.version) << '\n';
        }
        out.flush();
        if (!out) {
            return false;
        }
    }
    return place_atomic_(tmp, path);
}

}  // namespace apollo::script
