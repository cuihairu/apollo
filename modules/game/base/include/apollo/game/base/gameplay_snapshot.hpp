#pragma once

#include <cstdint>

namespace apollo::game::base {

// 只读玩法快照（player-object-model §生命周期：「进入玩法时提供准入数据和
// 只读基础属性快照，cell 创建场景实体」）：Base 长期资料投向 cell 的值载体
// ——纯值类型、单向、无回写。
//
// 跨界纪律（todo L84）：跨 Base/模拟域边界只传快照值，不传 base::Avatar*
// 指针——指针跨界等于把档案宿主内存地址借给模拟域，长期写权随之漏出。
// 换幕重建 cell::Avatar 一律以本类型值投影（cell::Avatar::project_from），
// 不再经 SceneTransferRequest 携带本地 Avatar 指针。
//
// 字段只收「cell 侧有真实消费点」的长期属性；成长面（level/exp 等任务书
// §6 数值）随玩法批接入 cell 后再扩，不在本批空画字段。
//
// 生产面（Base 侧按 PlayerData/Avatar 长期字段构造并经准入链下发）随
// todo L148「Base 校验小队…cell 准入可信绑定各成员原 Base、快照」落地；
// 本类型只钉消费侧契约，不宣称生产链路已完成。
struct GameplaySnapshot {
    // 驻地（term-contract §1.1）：登录分配，永不随场景切换迁移
    std::uint32_t home_zone_id = 0;
    // 基础属性（PlayerData 长期字段 maxHp/hp）：cell::Avatar 战斗血量的
    // 入场播种值；无快照 = Avatar 出厂态（100/100）
    int max_hp = 100;
    int hp = 100;

    [[nodiscard]] bool operator==(const GameplaySnapshot&) const noexcept = default;
};

} // namespace apollo::game::base
