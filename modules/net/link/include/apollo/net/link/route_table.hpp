#pragma once

// 契约路由表（net M1 内核 · 增量 2；sdk-contract §10.6 附节 contract_route
// 产物 + §11.3 域分段：client 1-899 / internal 900+）。
//
// 路由表是按 id 分发、上行白名单、域过滤的唯一查询面——单一来源 = 生成器
// 产物 contract_route.json（golden 入库），本表不做第二个契约解析实现的
// 数据面（解析只在 route_json 装载器，IR 即本表）。

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace apollo::net::link {

struct RouteEntry {
    std::uint16_t id = 0;
    std::string name;
    std::string dir;      // C2S / S2C / P2P
    std::string channel;  // movement / attributes / events / control
    std::string domain;   // client / internal（sdk-contract §11.3）
    std::string binding;  // native / reflect
};

class RouteTable {
public:
    // 重复 id → false（契约不变量：id 唯一，构建期拦）。
    bool add(RouteEntry entry);
    const RouteEntry* find(std::uint16_t id) const;
    std::size_t size() const { return entries_.size(); }

    // 元数据（contract_route.json 头部；装载器填写，缺省空）。
    const std::string& schema_hash() const { return schema_hash_; }
    const std::string& client_hash() const { return client_hash_; }
    const std::string& internal_hash() const { return internal_hash_; }
    void set_meta(std::string schema_hash, std::string client_hash, std::string internal_hash);

private:
    std::unordered_map<std::uint16_t, RouteEntry> entries_;
    std::string schema_hash_;
    std::string client_hash_;
    std::string internal_hash_;
};

// schema_hash 十六进制串 → 握手 route_version（首 8 字节大端 u64）。
// 握手比对只求失配可检，不承载完整性（完整性 = golden 与 bin 的 hash 互检，
// sdk-contract §10.6 装载期三件互检，不在链路层）。
std::uint64_t route_version_from_hash(const std::string& hash_hex);

}  // namespace apollo::net::link
