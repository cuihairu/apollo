// Lua 字节码缓存面实现（scripting-lua §3.5）——manifest 三元组编解码 +
// 格式签名（工具 lua_precompile 与运行期 ScriptRuntime 共用）。

#include "apollo/script/lua_bytecode_cache.hpp"

#include <lua.hpp>

#include <cstdio>
#include <fstream>
#include <sstream>

namespace apollo::script {
namespace {

constexpr std::uint64_t kFnvOffset = 1469598103934665603ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

bool parse_hex(const std::string& s, std::uint64_t& out) {
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

}  // namespace

std::string bytecode_format_signature() {
    // 版本 + 类型尺寸集 = 字节码格式的全部决定项（官方 format 字节恒 0；
    // 端序随构建面一致——工具与运行期同链同 vcpkg lua）。TAB 禁入（manifest
    // 分隔符）。
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%u/%u/%u/%u/%u",
                  static_cast<unsigned>(LUA_VERSION_RELEASE_NUM),
                  static_cast<unsigned>(sizeof(int)),
                  static_cast<unsigned>(sizeof(std::size_t)),
                  static_cast<unsigned>(sizeof(lua_Integer)),
                  static_cast<unsigned>(sizeof(lua_Number)));
    return std::string(buf);
}

std::uint64_t fnv1a64(const std::uint8_t* data, std::size_t size) {
    std::uint64_t h = kFnvOffset;
    for (std::size_t i = 0; i < size; ++i) {
        h ^= data[i];
        h *= kFnvPrime;
    }
    return h;
}

std::string to_hex(std::uint64_t v) {
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx",
                  static_cast<unsigned long long>(v));
    return std::string(buf, 16);
}

bool parse_manifest_line(const std::string& line, ManifestEntry& out) {
    if (line.empty() || line[0] == '#') {
        return false;
    }
    std::vector<std::string> f;
    std::stringstream ss(line);
    std::string field;
    while (std::getline(ss, field, '\t')) {
        f.push_back(field);
    }
    if (f.size() != 4 || f[0].empty()) {
        return false;
    }
    ManifestEntry e;
    e.name = f[0];
    if (!parse_hex(f[1], e.source_hash) || !parse_hex(f[2], e.bytecode_hash)) {
        return false;
    }
    // format_sig：非空、无 TAB（分隔符冲突禁入）；内容合法性由比对裁决
    // （不匹配 = 未命中回退，无解析风险）。
    if (f[3].empty()) {
        return false;
    }
    e.format_sig = f[3];
    out = std::move(e);
    return true;
}

std::vector<ManifestEntry> load_manifest(const std::string& path) {
    std::vector<ManifestEntry> entries;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return entries;  // 缺失 = 空清单（未命中面）
    }
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        ManifestEntry e;
        if (parse_manifest_line(line, e)) {
            entries.push_back(std::move(e));
        }
        // 坏行跳过（构建期产物，运行期不因单行坏损整体拒载）
    }
    return entries;
}

}  // namespace apollo::script
