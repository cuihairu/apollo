#pragma once

// contract_route.json 装载器（net M1 内核 · 增量 2）。
//
// 单一契约来源纪律：解析只此一处（golden 产物 → RouteTable IR），路由消费
// 面（分发器/链路/测试）一律持 RouteTable，不做第二个解析实现。
// nlohmann_json 不可用的构建下整文件编译为空（APOLLO_NET_LINK_HAS_JSON 门）。

#include "apollo/net/link/route_table.hpp"

#include <string>

#if APOLLO_NET_LINK_HAS_JSON

namespace apollo::net::link {

// 解析 contract_route.json 全文 → 路由表（含头部三 hash 元数据）。
// 结构不符 / routes 非数组 / 条目缺字段 → false（整表拒，不装半张）。
bool load_route_table_json(const std::string& text, RouteTable& out);

}  // namespace apollo::net::link

#endif  // APOLLO_NET_LINK_HAS_JSON
