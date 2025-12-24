#pragma once

#include <cstdint>

namespace apollo {
namespace game {

/**
 * @brief 属性ID定义
 *
 * 与服务端保持一致，用于属性系统的统一标识
 * 命名规范：使用英文描述，避免拼音
 */
enum class AttributeId : uint32_t {
    // ========== 公共对象属性 (1-12) ==========
    OBJECT_START = 1,

    // 基础属性
    POINT_ID = 1,           // 对象指针ID
    OBJ_TYPE = 2,           // 对象类型
    SUB_OBJ_TYPE = 3,       // 对象子类型
    DIRECTION = 4,          // 方向
    DISPLAY_NAME = 5,       // 显示名称
    NAME_COLOR = 6,         // 名字颜色
    STATUS = 7,             // 状态 (0=活着 1=死亡)
    APPEARANCE = 8,         // 外观
    ACTION = 9,             // 当前动作
    APPEAR_EFFECT = 10,     // 出现特效
    MASTER_ID = 11,         // 主人ID
    FASHION_ID = 12,        // 时装ID

    OBJECT_END = 12,

    // ========== 生物属性 (101-500) ==========
    CREATURE_START = 101,

    // 基础信息
    SEX = 101,              // 性别 1:男 0:女
    AVATAR = 102,           // 头像
    AVATAR_FRAME = 103,     // 头像框
    TITLE = 104,            // 称号
    LEVEL = 105,            // 等级
    BODY_EFFECT = 106,      // 身体特效
    MOUNT_APPEARANCE = 107, // 骑宠外观
    CLOTHES_APPEARANCE = 108, // 衣服外观

    // 战斗属性 - 基础值
    DC_MAX = 110,           // 最大物理攻击
    AC_MAX = 111,           // 最大物理防御
    CUR_HP = 112,           // 当前血量
    HP_MAX = 113,           // 最大血量
    ADD_ATK_PERCENT = 114,  // 攻击加成百分比(万分比)
    ADD_HP_PERCENT = 115,   // 生命加成百分比(万分比)
    ADD_DEF_PERCENT = 116,  // 防御加成百分比(万分比)

    // 战斗属性 - 数值
    ATK_SPEED_LEVEL = 117,  // 攻击速度等级
    HIT_VALUE = 118,        // 命中值
    EVASION_VALUE = 119,    // 闪避值
    CRIT_LEVEL = 120,       // 暴击等级
    CRIT_RESIST_VALUE = 121,// 暴击抵抗值
    CRIT_DAMAGE_VALUE = 122,// 暴伤值
    CRIT_RESIST_DAMAGE_VALUE = 123, // 暴伤抵抗值

    // 战斗属性 - 增减伤值
    INCREASE_DAMAGE_VALUE = 124,   // 增伤值
    DAMAGE_REDUCTION_VALUE = 125,  // 减伤值
    PHYSIC_DAMAGE_REDUCTION_VALUE = 126, // 物理减伤值
    MAGIC_DAMAGE_REDUCTION_VALUE = 127,  // 魔法减伤值

    // 战斗属性 - 百分比 (万分比)
    HIT = 128,              // 命中
    DODGE = 129,            // 闪避
    CRIT_RATE = 130,        // 暴击概率
    CRIT_DAMAGE = 131,      // 暴击伤害
    CRIT_RESIST = 132,      // 暴击抵抗
    EXPLOSION_RESIST = 133, // 爆伤抵抗
    ADD_DAMAGE_PERCENT = 134,   // 增伤百分比
    REDUCE_DAMAGE_PERCENT = 135,// 减伤百分比
    PHYSIC_REDUCE_DAMAGE = 136, // 物理减伤
    MAGIC_REDUCE_DAMAGE = 137,  // 魔法减伤
    ARMOR_PENETRATE_RATE = 138, // 破甲概率
    HP_RECOVER_RATE = 139,      // 回血速度
    HP_RECOVER_MAX = 140,       // 回血上限
    ARMOR_PENETRATE = 141,      // 破甲值
    ARMOR_PENETRATE_TIME = 142, // 破甲时间
    REFLECT_DAMAGE = 143,       // 反弹伤害
    PHYSIC_INJURY_REDUCE = 144, // 物理伤害减少
    ATK_SPEED_PERCENT = 145,    // 攻速加成
    REFLECT_RATE = 146,         // 反弹概率
    PK_DAMAGE_ADD_PERCENT = 147, // PK增伤
    PK_DAMAGE_CUT_PERCENT = 148, // PK减伤
    LIFESTEAL_PERCENT = 149,    // 吸血
    ADD_RECEIVE_DAMAGE_PERCENT = 150, // 受到的伤害增加

    // 计算属性 (最终值)
    TOTAL_AC_MAX = 151,      // 总最大防御
    TOTAL_DC_MAX = 152,      // 总最大攻击
    TOTAL_HP_MAX = 153,      // 总最大血量
    HP_AUTO_RECOVER = 154,   // 每秒自动回血

    // 状态效果属性
    POISON_PERCENT = 155,    // 中毒百分比
    POISON_DAMAGE = 156,     // 中毒伤害
    PARALYSIS_TIME = 157,    // 麻痹时间
    POISON_RATE = 158,       // 中毒概率
    PARALYSIS_RATE = 159,    // 麻痹概率
    SERIOUS_INJURY = 160,    // 重伤

    // 外观
    SHIELD_APPEARANCE = 161, // 护盾外观
    HELMET_APPEARANCE = 162, // 头盔外观

    // 控制效果
    FROZEN_TIME = 163,       // 冰冻时间
    FROZEN_RATE = 164,       // 冰冻概率
    CONTROL_RESIST_VALUE = 165, // 控制抵抗值
    CONTROL_RESIST = 166,    // 控制抵抗
    SILENCE_RATE = 167,      // 沉默概率
    SILENCE_TIME = 168,      // 沉默时间
    CURE_RATE = 169,         // 治疗概率
    BURN_RATE = 170,         // 烧伤概率

    // 元素伤害 - 值
    PHYSIC_DAMAGE_VALUE = 171,   // 物理增伤值
    MAGIC_DAMAGE_VALUE = 172,    // 魔法增伤值
    TEMP_DAMAGE_VALUE = 175,     // 临时攻击
    TEMP_INCREASE_DAMAGE = 176,  // 临时增伤

    // 元素伤害 - 百分比
    PHYSIC_DAMAGE = 173,     // 物理增伤百分比
    MAGIC_DAMAGE = 174,      // 魔法增伤百分比

    // 五行元素 - 冰/火/风/光/电 (伤害值)
    ICE_DAMAGE_VALUE = 177,
    FIRE_DAMAGE_VALUE = 178,
    WIND_DAMAGE_VALUE = 179,
    LIGHT_DAMAGE_VALUE = 180,

    // 五行元素 - 冰/火/风/光 (百分比)
    ICE_DAMAGE = 181,
    FIRE_DAMAGE = 182,
    WIND_DAMAGE = 183,
    LIGHT_DAMAGE = 184,

    // 五行元素 - 冰/火/风/光 (减伤值)
    ICE_REDUCE_VALUE = 185,
    FIRE_REDUCE_VALUE = 186,
    WIND_REDUCE_VALUE = 187,
    LIGHT_REDUCE_VALUE = 188,

    // 五行元素 - 冰/火/风/光 (减伤百分比)
    ICE_REDUCE_DAMAGE = 189,
    FIRE_REDUCE_DAMAGE = 190,
    WIND_REDUCE_DAMAGE = 191,
    LIGHT_REDUCE_DAMAGE = 192,

    // 临时属性 - 百分比
    TEMP_ADD_ATK_PERCENT = 193,    // 临时攻击百分比
    TEMP_ADD_DAMAGE_PERCENT = 194,  // 临时增伤百分比
    TEMP_FINAL_ADD_DAMAGE = 195,    // 临时最终增伤

    // 最终属性
    FINAL_HIT = 196,        // 最终命中率
    FINAL_DODGE = 197,      // 最终闪避率
    FINAL_CRIT_RATE = 198,  // 最终暴击率
    FINAL_CRIT_RESIST = 199, // 最终暴击抵抗
    FINAL_CRIT_DAMAGE = 200,  // 最终暴击伤害
    FINAL_EXPLOSION_RESIST = 201, // 最终爆伤抵抗
    FINAL_ADD_DAMAGE = 202,     // 最终增伤
    FINAL_REDUCE_DAMAGE = 203,  // 最终减伤
    FINAL_PHYSIC_DAMAGE = 204,  // 最终物理增伤
    FINAL_PHYSIC_REDUCE = 205,  // 最终物理减伤

    // 最终五行元素 - 增伤
    FINAL_ICE_DAMAGE = 206,
    FINAL_ICE_REDUCE = 207,
    FINAL_FIRE_DAMAGE = 208,
    FINAL_FIRE_REDUCE = 209,
    FINAL_WIND_DAMAGE = 210,
    FINAL_WIND_REDUCE = 211,
    FINAL_LIGHT_DAMAGE = 212,
    FINAL_LIGHT_REDUCE = 213,

    // 宠物属性加成
    PET_TEMP_ADD_ATK = 214,

    // 五行元素 - 电系
    ELECTRIC_DAMAGE_VALUE = 215,
    ELECTRIC_DAMAGE = 216,
    ELECTRIC_REDUCE_VALUE = 217,
    ELECTRIC_REDUCE_DAMAGE = 218,
    FINAL_ELECTRIC_DAMAGE = 219,
    FINAL_ELECTRIC_REDUCE = 220,

    // 免疫属性
    PHYSIC_IMMUNE = 221,    // 物理免疫
    ICE_IMMUNE = 222,       // 冰系免疫
    WIND_IMMUNE = 223,      // 风系免疫
    FIRE_IMMUNE = 224,      // 火系免疫
    LIGHT_IMMUNE = 225,     // 光系免疫
    ELECTRIC_IMMUNE = 226,  // 电系免疫

    // 临时金币加成
    TEMP_ADD_COIN_PERCENT = 227,

    // 扩展属性
    CREATURE_END = 500,

    // 魔法属性
    CUR_MP = 501,           // 当前魔法
    NO_HURT_TIME = 502,     // 未受伤害时长
    NO_HURT_STACK = 503,    // 未受伤害叠加次数

    // 反伤相关
    REFLECT_DAMAGE_PERCENT = 504,  // 反伤百分比
    ADD_MAGIC_DEF_PERCENT = 505,   // 魔防加成百分比
    ADD_MP_PERCENT = 506,          // 魔法值加成百分比
    MP_MAX = 507,                  // 最大魔法
    DC_MIN = 508,                  // 最小物理攻击
    MC_MAX = 509,                  // 最大魔法攻击
    MC_MIN = 510,                  // 最小魔法攻击
    SC_MAX = 511,                  // 最大道术攻击
    SC_MIN = 512,                  // 最小道术攻击
    AC_MIN = 513,                  // 最小物理防御
    MAC_MAX = 514,                 // 最大魔法防御
    MAC_MIN = 515,                 // 最小魔法防御
    MAGIC_DODGE = 516,             // 魔法闪避
    LUCKY = 517,                   // 幸运
    CURSE = 518,                   // 诅咒
    ATTACK_MODE = 519,             // 攻击模式

    // 强攻/守护
    STORM_RATE = 520,       // 强攻概率
    STORM_DAMAGE = 521,     // 强攻伤害
    MP_RECOVER_RATE = 522,  // 回魔速度
    MP_RECOVER_MAX = 523,   // 回魔上限
    PROTECT = 524,          // 守护值
    PROTECT_RATE = 525,     // 守护概率
    PROTECT_LEVEL = 526,    // 守护等级
    PROTECT_PENETRATE = 527, // 守护穿透
    PROTECT_MISS = 528,     // 守护侵蚀
    ARMOR_PENETRATE_COUNT = 529, // 破甲次数
    MONSTER_ADD_DAMAGE = 530,    // 对怪加伤
    MONSTER_REDUCE_DAMAGE = 531, // 被怪攻击减伤
    IGNORE_DEF_PERCENT = 532,    // 忽视防御

    // 麻痹抗性
    PARALYSIS_RESIST_RATE = 533,  // 麻痹概率抗性
    PARALYSIS_RESIST_TIME = 534,  // 麻痹时间抗性
    PARALYSIS_CUT_RATE = 535,      // 麻痹概率减免
    PARALYSIS_CUT_TIME = 536,      // 麻痹时间减免

    // 冰冻抗性
    ANTI_FROZEN_RATE = 537,  // 冰冻概率抗性
    ANTI_FROZEN_TIME = 538,  // 冰冻时间抗性
    CUT_FROZEN_RATE = 539,   // 冰冻概率减免
    CUT_FROZEN_TIME = 540,   // 冰冻时间减免

    // 魔法/道术加成
    ADD_MC_PERCENT = 541,    // 魔法加成百分比
    ADD_SC_PERCENT = 542,    // 道术加成百分比

    // 计算属性 - 最小值
    TOTAL_DC_MIN = 543,      // 总最小物理攻击
    TOTAL_MC_MAX = 544,      // 总最大魔法攻击
    TOTAL_MC_MIN = 545,      // 总最小魔法攻击
    TOTAL_SC_MAX = 546,      // 总最大道术攻击
    TOTAL_SC_MIN = 547,      // 总最小道术攻击
    TOTAL_AC_MIN = 548,      // 总最小物理防御
    TOTAL_MAC_MAX = 549,     // 总最大魔法防御
    TOTAL_MAC_MIN = 550,     // 总最小魔法防御

    CAMP = 551,             // 阵营
    ANTI_ARMOR_PENETRATE = 552, // 抗破甲率
    MP_AUTO_RECOVER = 553,   // 每秒自动回蓝
    TOTAL_MP_MAX = 554,      // 总最大魔法量
    BUFF_MOVE_SPEED = 555,   // 移速加成(百分比)

    CREATURE_END_EXTENDED = 600,

    // ========== 玩家专属属性 (301-700) ==========
    PLAYER_START = 301,

    CURRENT_STAGE = 301,     // 当前关卡
    CURRENT_HERO = 302,      // 当前主角
    EXP_MAX = 303,           // 升级所需经验
    EXIT_FLAG = 304,         // 退出标记
    BIND_CASH = 305,         // 绑定元宝
    CASH = 306,              // 元宝
    DIAMOND = 307,           // 钻石
    GOLD = 308,              // 金币
    CURRENT_ENERGY = 309,    // 当前体力
    TALENT_EXP = 310,        // 天赋经验
    TALENT_MAX_POINT = 311,  // 最大天赋点
    TALENT_REMAIN_POINT = 312, // 剩余天赋点
    MAX_ENERGY = 313,        // 体力上限

    // 家园属性
    HOME_LEVEL = 314,        // 家园等级
    HOME_ATTACK_LEVEL = 315, // 家园攻击等级
    HOME_DEFENSE_LEVEL = 316, // 家园防御等级
    HOME_HP_LEVEL = 317,     // 家园生命等级

    // 背包相关
    BAG_DATA = 318,          // 背包数据
    MAX_BAG = 326,           // 最大背包数

    // 玩家标识
    PLAYER_ID = 327,         // 玩家唯一ID
    FIRST_LOGIN = 328,       // 首次登录
    REGISTER_TIME = 329,     // 注册时间
    NORMAL_SKILL = 330,      // 普通技能

    CREATE_ROLE_FLAG = 331,  // 创建角色标记

    // 位置记录
    LAST_MAP = 332,          // 上次地图
    LAST_MAP_X = 333,        // 上次X坐标
    LAST_MAP_Y = 334,        // 上次Y坐标

    TOTAL_LOGIN_DAYS = 335,  // 累计登录天数

    // 引导相关
    GUIDE_INFO = 337,        // 引导信息

    // 聊天限制
    BAN_CHAT_TIME = 338,     // 禁言时间
    CHAT_COUNT = 339,        // 聊天次数

    // 宠物相关
    PET_ADD_RATE = 341,      // 宠物属性加成

    // 队伍相关
    TEAM_ID = 372,           // 队伍ID
    TEAM_STATE = 373,        // 队伍状态

    PLAYER_END = 700,

    // ========== 宠物属性 (3001-3100) ==========
    PET_START = 3001,
    PET_INDEX = 3002,        // 宠物索引
    PET_STATE = 3003,        // 宠物状态
    PET_FIGHT_TIME = 3004,   // 出战时间
    PET_DEAD_TIME = 3005,    // 死亡时间
    PET_REBORN = 3006,       // 转生数
    PET_PHASE = 3007,        // 阶数
    PET_AUTO_FIGHT = 3008,   // 自动出战
    PET_DEAD_PARAM = 3009,   // 死亡参数
    PET_MONSTER_ID = 3010,   // 怪物ID
    PET_END = 3100,

    // ========== 怪物属性 (5001-6000) ==========
    MONSTER_START = 5001,
    MONSTER_MOVE_INTERVAL = 5001, // 移动间隔
    MONSTER_STEP = 5002,     // 移动步伐
    MONSTER_ATTACK_RANGE = 5003, // 攻击范围
    MONSTER_ATTACK_INTERVAL = 5004, // 攻击间隔
    MONSTER_SKILL_CD = 5005,  // 技能CD
    MONSTER_ID = 5006,       // 怪物ID
    MONSTER_TYPE = 5008,     // 怪物类型
    MONSTER_RACE = 5010,     // 怪物种族
    MONSTER_GRADE = 5011,    // 怪物等级(普通/精英/BOSS)
    MONSTER_MTYPE = 5012,    // 怪物系别
    MONSTER_EXP = 5013,      // 怪物经验
    MONSTER_COOL_EYE = 5014, // 视觉方式
    MONSTER_ALERT_RANGE = 5015, // 警戒范围
    MONSTER_HOME_RANGE = 5016, // 橡皮筋范围
    MONSTER_ESCAPE_HP = 5017, // 逃跑条件
    MONSTER_ESCAPE_TYPE = 5018, // 逃跑方式
    MONSTER_BEHAVIOR = 5019, // 行为类型
    MONSTER_DROP_GROUP = 5020, // 掉落组
    MONSTER_CORPSE_TIME = 5021, // 尸体时间
    MONSTER_TASK = 5022,     // 任务怪
    MONSTER_CLASS = 5023,    // 类型(怪/宠/召唤/NPC)
    MONSTER_END = 6000,

    // ========== NPC属性 (7001-8000) ==========
    NPC_START = 7001,
    NPC_TASK_FLAG = 7001,    // 任务冒泡
    NPC_END = 8000,

    // ========== 物品属性 (8001-9000) ==========
    ITEM_START = 8001,

    // 藏品/品质
    COLLECTION_LEVEL = 8050, // 藏品等级
    COLLECTION_STAR = 8051,  // 藏品星级

    // 物品基础信息
    ITEM_TYPE = 8052,        // 物品类型
    ITEM_ID = 8053,          // 物品ID
    ITEM_DURA = 8054,        // 堆叠数
    ITEM_QUALITY = 8055,     // 品质
    ITEM_PHASE = 8056,       // 阶数
    ITEM_SAVE_POS = 8057,    // 保存位置
    ITEM_GRID_POS = 8058,    // 格子位置

    ITEM_EQUIP_COST = 8059,  // 装备升品消耗
    ITEM_PET_ID = 8060,      // 宠物ID
    ITEM_BOUND = 8061,       // 绑定标记
    ITEM_STACK_LIMIT = 8062, // 可叠加上限
    ITEM_UNIQUE_ID = 8063,   // 流水号
    ITEM_KIND = 8064,        // 大类
    ITEM_SUBTYPE = 8065,     // 小类

    // 洗练词条
    REFINE_ATTR1 = 8066,     // 洗练词条1属性
    REFINE_ATTR2 = 8067,     // 洗练词条2属性
    REFINE_ATTR3 = 8068,     // 洗练词条3属性
    REFINE_ATTR4 = 8069,     // 洗练词条4属性
    REFINE_VALUE1 = 8070,    // 洗练值1
    REFINE_VALUE2 = 8071,    // 洗练值2
    REFINE_VALUE3 = 8072,    // 洗练值3
    REFINE_VALUE4 = 8073,    // 洗练值4
    REFINE_LIB1 = 8074,      // 洗练库1
    REFINE_LIB2 = 8075,      // 洗练库2
    REFINE_LIB3 = 8076,      // 洗练库3
    REFINE_LIB4 = 8077,      // 洗练库4

    ITEM_EQUIP_POS = 8078,   // 装备部位
    ITEM_STORAGE_TAB = 8079,  // 仓库页
    ITEM_EQUIP_TAB = 8080,   // 装备页
    ITEM_AUTO_USE = 8081,    // 自动使用
    ITEM_PARAM1 = 8082,      // 参数1

    // 宠物装备
    PET_EQUIP_SLOT1 = 8095,  // 宠物装备槽1
    PET_EQUIP_SLOT2 = 8096,  // 宠物装备槽2
    PET_EQUIP_SLOT3 = 8097,  // 宠物装备槽3
    PET_EQUIP_OWNER = 8098,  // 宠物装备所有者

    // 有效期
    ITEM_VALID_PERIOD = 8099, // 有效期(分钟)

    // 回复属性
    ITEM_RENEW_HP = 8100,    // 回血
    ITEM_RENEW_MP = 8101,    // 回蓝
    ITEM_RENEW_CD = 8102,    // 使用冷却
    ITEM_RENEW_TIME = 8103,  // 持续时间

    // 回城石
    TELEPORT_MAP = 8104,     // 回城地图
    TELEPORT_X = 8105,       // 回城X
    TELEPORT_Y = 8106,       // 回城Y

    // 极品属性
    RARE_ATTR = 8107,        // 极品属性
    HAS_RARE_ATTR = 8108,    // 是否有极品属性

    // 强化
    STRENGTHEN_LEVEL = 8109, // 强化等级
    CAN_STRENGTHEN = 8110,   // 可强化
    STRENGTHEN_ORDER = 8111, // 强化顺序

    // 符文
    RUNE_ID = 8112,          // 符文ID
    RUNE_INFO = 8113,        // 符文信息

    // 耐久
    DURABILITY_MAX = 8114,   // 最大耐久
    DURABILITY_CUR = 8115,   // 当前耐久

    // 排序权重
    SORT_WEIGHT = 8116,      // 排序权重

    ITEM_END = 9000,

    // ========== 地上对象 (11001-12000) ==========
    FLOOR_OBJ_START = 11001,
    FLOOR_OBJ_WIDTH = 11001, // 范围X
    FLOOR_OBJ_HEIGHT = 11002, // 范围Y
    FLOOR_OBJ_EFFECT = 11003, // 特效
    FLOOR_OBJ_SKILL = 11004, // 技能ID
    FLOOR_OBJ_SKILL_LEVEL = 11005, // 技能等级
    FLOOR_OBJ_BUFF = 11006,  // BuffID
    FLOOR_OBJ_END = 12000
};

/**
 * @brief 属性类别
 */
enum class AttributeCategory : uint8_t {
    OBJECT = 1,      // 公共对象
    CREATURE = 2,    // 生物
    PLAYER = 3,      // 玩家
    PET = 4,         // 宠物
    MONSTER = 5,     // 怪物
    NPC = 6,         // NPC
    ITEM = 7         // 物品
};

/**
 * @brief 属性类型
 */
enum class AttrValueType : uint8_t {
    INT8 = 0,
    INT16,
    INT32,
    INT64,
    UINT8,
    UINT16,
    UINT32,
    UINT64,
    FLOAT,
    DOUBLE,
    BOOL,
    STRING
};

} // namespace game
} // namespace apollo
