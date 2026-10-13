#pragma once
//
// AttrDomain — 属性可见域（attribute-sync §2.2）。
//
// 域决定"字段可见性"：同一实体对不同观察者投影不同（快照与 delta 都按域裁剪）。
// 契约批落地后域标记由契约声明生成；本枚举是机制层的载体。
//

#include <cstdint>

namespace apollo::game::attrsync {

enum class AttrDomain : std::uint8_t {
    kPublic = 0,  // AOI 域（默认）：兴趣范围内所有观察者可见
    kSelf,        // 仅 owner 自身（viewer == entity）
    kTeam,        // owner + 同队成员（经 group gate 判定）
    kGuild,       // 公会成员（经 group gate 判定）
    kWorld,       // 全服广播（机制层等同 public；路由差异归上游）
};

}  // namespace apollo::game::attrsync
