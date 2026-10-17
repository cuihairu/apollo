// 契约路由表实现（net M1 内核 · 增量 2）。

#include "apollo/net/link/route_table.hpp"

#include <cstdlib>

namespace apollo::net::link {

bool RouteTable::add(RouteEntry entry) {
    const auto [it, inserted] = entries_.try_emplace(entry.id, std::move(entry));
    return inserted;
}

const RouteEntry* RouteTable::find(std::uint16_t id) const {
    const auto it = entries_.find(id);
    return it == entries_.end() ? nullptr : &it->second;
}

void RouteTable::set_meta(std::string schema_hash, std::string client_hash,
                          std::string internal_hash) {
    schema_hash_ = std::move(schema_hash);
    client_hash_ = std::move(client_hash);
    internal_hash_ = std::move(internal_hash);
}

std::uint64_t route_version_from_hash(const std::string& hash_hex) {
    // 首 16 个 hex 字符 → 大端 u64；短串/非 hex 按前缀尽力解析（失配可检即可）。
    std::uint64_t v = 0;
    std::size_t taken = 0;
    for (char c : hash_hex) {
        if (taken >= 16) {
            break;
        }
        int d;
        if (c >= '0' && c <= '9') {
            d = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            d = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            d = c - 'A' + 10;
        } else {
            break;
        }
        v = (v << 4) | static_cast<std::uint64_t>(d);
        ++taken;
    }
    // taken=0（空/非 hex）时移位量 = 64 = UB（x86 掩码取模侥幸为 0，ARM 不然），
    // 直接返回 0；taken≥1 时移位量 ≤60 安全。
    if (taken == 0) {
        return 0;
    }
    if (taken < 16) {
        v <<= 4 * (16 - taken);
    }
    return v;
}

std::vector<std::uint64_t> compat_route_versions_from_text(const std::string& text) {
    std::vector<std::uint64_t> out;
    std::string token;
    auto flush_token = [&] {
        if (token.empty()) {
            return;  // 空段跳过（连续分隔符 / 尾逗号）
        }
        const std::uint64_t v = route_version_from_hash(token);
        token.clear();
        if (v != 0) {
            out.push_back(v);  // 零值（空/非 hex）跳过：窗对空代无意义
        }
    };
    for (const char c : text) {
        if (c == ',' || c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            flush_token();
        } else {
            token.push_back(c);
        }
    }
    flush_token();
    return out;
}

std::vector<std::uint64_t> compat_route_versions_from_env() {
    if (const char* raw = std::getenv("APOLLO_ROUTE_COMPAT")) {
        return compat_route_versions_from_text(raw);
    }
    return {};
}

}  // namespace apollo::net::link
