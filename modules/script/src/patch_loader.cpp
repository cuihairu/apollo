// §3.2 Phase A 工程面实现——patch 目录读 + hash 校验。
// manifest 驱动逐条目：源 hash 必过（不过 = 拒载）；字节码 hash / format_sig
// 不过 = 丢字节码留源码（产物回退仍可用，同 §3.5 版本头纪律）。

#include "apollo/script/patch_loader.hpp"

#include <fstream>
#include <sstream>

#include "apollo/script/lua_bytecode_cache.hpp"  // load_manifest / fnv1a64 / bytecode_format_signature

namespace apollo::script {
namespace {

// 字节文件读取（同 script_runtime read_bytes_file 形态；文件级私有）。
bool read_bytes(const std::string& path, std::vector<std::uint8_t>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    const std::string& s = buffer.str();
    out.assign(s.begin(), s.end());
    return true;
}

bool read_text(const std::string& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    out = buffer.str();
    return true;
}

}  // namespace

PatchLoadResult load_patch_directory(const std::string& dir) {
    PatchLoadResult result;
    std::vector<ManifestEntry> manifest = load_manifest(dir + "/manifest");
    if (manifest.empty()) {
        // manifest 缺失/全坏行 = 整体错误（控制面须感知目录不可装载）。
        result.errors.push_back("manifest: missing or empty in " + dir);
        return result;
    }
    for (const auto& e : manifest) {
        PatchModule m;
        m.name = e.name;
        m.source_hash = e.source_hash;
        m.bytecode_hash = e.bytecode_hash;
        std::string source;
        if (!read_text(dir + "/" + e.name + ".lua", source)) {
            result.errors.push_back(e.name + ": source file missing");
            continue;
        }
        if (e.source_hash != fnv1a64(reinterpret_cast<const std::uint8_t*>(source.data()),
                                     source.size())) {
            result.errors.push_back(e.name + ": source hash mismatch (tampered or stale)");
            continue;
        }
        m.source = std::move(source);
        std::vector<std::uint8_t> bytes;
        if (read_bytes(dir + "/" + e.name + ".luac", bytes)) {
            // 字节码产物双校验：整块 hash（损坏）与版本头（跨 Lua 小版本）。
            // 任一不符 = 丢字节码留源码（产物回退仍可用，不整体拒载）。
            if (e.bytecode_hash == fnv1a64(bytes.data(), bytes.size()) &&
                e.format_sig == bytecode_format_signature()) {
                m.bytecode = std::move(bytes);
            }
        }
        result.modules.push_back(std::move(m));
    }
    return result;
}

}  // namespace apollo::script
