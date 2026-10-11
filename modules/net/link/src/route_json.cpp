// contract_route.json 装载器实现（net M1 内核 · 增量 2）。

#include "apollo/net/link/route_json.hpp"

#if APOLLO_NET_LINK_HAS_JSON

#include <nlohmann/json.hpp>

namespace apollo::net::link {

namespace {

const nlohmann::json* find_field(const nlohmann::json& obj, const char* key) {
    if (!obj.is_object()) {
        return nullptr;
    }
    const auto it = obj.find(key);
    return it == obj.end() ? nullptr : &(*it);
}

bool read_string(const nlohmann::json& obj, const char* key, std::string& out) {
    const nlohmann::json* f = find_field(obj, key);
    if (f == nullptr || !f->is_string()) {
        return false;
    }
    out = f->get<std::string>();
    return true;
}

}  // namespace

bool load_route_table_json(const std::string& text, RouteTable& out) {
    nlohmann::json doc;
    try {
        doc = nlohmann::json::parse(text);
    } catch (...) {
        return false;
    }
    if (!doc.is_object()) {
        return false;
    }

    std::string schema_hash, client_hash, internal_hash;
    if (!read_string(doc, "schema_hash", schema_hash) ||
        !read_string(doc, "client_hash", client_hash) ||
        !read_string(doc, "internal_hash", internal_hash)) {
        return false;
    }

    const nlohmann::json* routes = find_field(doc, "routes");
    if (routes == nullptr || !routes->is_array() || routes->empty()) {
        return false;
    }

    RouteTable table;
    for (const auto& item : *routes) {
        RouteEntry entry;
        const nlohmann::json* id = find_field(item, "id");
        if (id == nullptr || !id->is_number_integer()) {
            return false;
        }
        const std::int64_t v = id->get<std::int64_t>();
        if (v < 0 || v > 0xFFFF) {
            return false;
        }
        entry.id = static_cast<std::uint16_t>(v);
        if (!read_string(item, "name", entry.name) || !read_string(item, "dir", entry.dir) ||
            !read_string(item, "channel", entry.channel) ||
            !read_string(item, "domain", entry.domain) ||
            !read_string(item, "binding", entry.binding)) {
            return false;
        }
        if (!table.add(std::move(entry))) {
            return false;  // 产物内 id 重复 = 生成器不变量破，整表拒
        }
    }
    table.set_meta(std::move(schema_hash), std::move(client_hash), std::move(internal_hash));
    out = std::move(table);
    return true;
}

}  // namespace apollo::net::link

#endif  // APOLLO_NET_LINK_HAS_JSON
