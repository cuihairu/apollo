// lua_precompile — §3.5 构建期预编译工具：把 .lua 源编译为 .luac 产物 +
// manifest 三元组（源 hash / bytecode hash / 格式签名）。
//
// 与运行期同链 vcpkg lua（bytecode 格式同源）；manifest 由本工具独占写
// （运行期只读）。用法：
//   lua_precompile <cache_dir> <file.lua>...
// 任一文件编译失败 = 非零退出且不写 manifest（构建闸）。

#include "apollo/script/lua_bytecode_cache.hpp"

#include <lua.hpp>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

int dump_writer(lua_State* /*L*/, const void* p, std::size_t sz, void* ud) {
    auto* out = static_cast<std::vector<std::uint8_t>*>(ud);
    const auto* bytes = static_cast<const std::uint8_t*>(p);
    out->insert(out->end(), bytes, bytes + sz);
    return 0;
}

std::string stem_of(const std::string& path) {
    const auto slash = path.find_last_of("/\\");
    std::string base = slash == std::string::npos ? path : path.substr(slash + 1);
    const auto dot = base.find_last_of('.');
    return dot == std::string::npos ? base : base.substr(0, dot);
}

bool read_file(const std::string& path, std::vector<std::uint8_t>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    const std::string s = buffer.str();
    out.assign(s.begin(), s.end());
    return true;
}

bool write_file(const std::string& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(out);
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "usage: lua_precompile <cache_dir> <file.lua>..." << std::endl;
        return 2;
    }
    const std::string cache_dir = argv[1];

    std::vector<apollo::script::ManifestEntry> entries;
    for (int i = 2; i < argc; ++i) {
        const std::string path = argv[i];
        std::vector<std::uint8_t> source;
        if (!read_file(path, source)) {
            std::cerr << "lua_precompile: cannot read " << path << std::endl;
            return 1;
        }
        const std::string name = stem_of(path);

        lua_State* L = luaL_newstate();
        if (L == nullptr) {
            std::cerr << "lua_precompile: lua state alloc failed" << std::endl;
            return 1;
        }
        const int rc = luaL_loadbufferx(
            L, reinterpret_cast<const char*>(source.data()), source.size(),
            name.c_str(), "t");
        if (rc != LUA_OK) {
            const char* msg = lua_tostring(L, -1);
            std::cerr << "lua_precompile: compile failed " << path << ": "
                      << (msg ? msg : "unknown error") << std::endl;
            lua_close(L);
            return 1;
        }
        std::vector<std::uint8_t> bytecode;
        lua_dump(L, dump_writer, &bytecode, 0);
        lua_close(L);

        apollo::script::ManifestEntry e;
        e.name = name;
        e.source_hash = apollo::script::fnv1a64(source.data(), source.size());
        e.bytecode_hash = apollo::script::fnv1a64(bytecode.data(), bytecode.size());
        // 格式签名与运行期同宏同值（版本 + 尺寸集）；跨 Lua 小版本必不同。
        e.format_sig = apollo::script::bytecode_format_signature();

        if (!write_file(cache_dir + "/" + name + ".luac", bytecode)) {
            std::cerr << "lua_precompile: cannot write " << cache_dir << "/"
                      << name << ".luac" << std::endl;
            return 1;
        }
        entries.push_back(std::move(e));
        std::cout << "lua_precompile: " << path << " -> " << cache_dir << "/"
                  << name << ".luac (" << entries.back().bytecode_hash << ")"
                  << std::endl;
    }

    // manifest 全量重写（构建管线独占；全成功才落盘 = 构建闸）
    std::ostringstream manifest;
    for (const auto& e : entries) {
        manifest << e.name << '\t' << apollo::script::to_hex(e.source_hash)
                 << '\t' << apollo::script::to_hex(e.bytecode_hash) << '\t'
                 << e.format_sig << '\n';
    }
    // str() 只取一次（两次调用返回两份独立拷贝，跨拷贝迭代器区间 = UB，
    // 实测写串：manifest 尾部黏出千余字节堆垃圾）。
    const std::string manifest_text = manifest.str();
    if (!write_file(cache_dir + "/manifest",
                    std::vector<std::uint8_t>(manifest_text.begin(),
                                              manifest_text.end()))) {
        std::cerr << "lua_precompile: cannot write " << cache_dir
                  << "/manifest" << std::endl;
        return 1;
    }
    return 0;
}
