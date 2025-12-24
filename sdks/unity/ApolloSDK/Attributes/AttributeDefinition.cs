using System;

namespace ApolloSDK.Attributes
{
    /// <summary>
    /// 属性定义类
    /// 描述一个属性的基本信息，用于类型检查和验证
    /// </summary>
    public class AttributeDefinition
    {
        /// <summary>
        /// 属性ID
        /// </summary>
        public uint Id { get; set; }

        /// <summary>
        /// 属性名称
        /// </summary>
        public string Name { get; set; }

        /// <summary>
        /// 属性类型
        /// </summary>
        public AttributeType Type { get; set; }

        /// <summary>
        /// 默认值
        /// </summary>
        public AttributeValue DefaultValue { get; set; }

        /// <summary>
        /// 最小值（可选）
        /// </summary>
        public AttributeValue MinValue { get; set; }

        /// <summary>
        /// 最大值（可选）
        /// </summary>
        public AttributeValue MaxValue { get; set; }

        /// <summary>
        /// 是否需要同步到客户端
        /// </summary>
        public bool SyncToClient { get; set; }

        /// <summary>
        /// 是否需要持久化到数据库
        /// </summary>
        public bool Persistent { get; set; }

        /// <summary>
        /// 属性类别（用于分组管理）
        /// </summary>
        public AttributeCategory Category { get; set; }

        /// <summary>
        /// 属性描述
        /// </summary>
        public string Description { get; set; }

        /// <summary>
        /// 验证属性值是否在有效范围内
        /// </summary>
        public bool ValidateValue(AttributeValue value)
        {
            // 类型检查
            if (value.Type != Type && value.Type != AttributeType.None)
                return false;

            // 范围检查
            if (MinValue.Type != AttributeType.None)
            {
                int cmp = value.CompareTo(MinValue);
                if (cmp < 0) return false;
            }

            if (MaxValue.Type != AttributeType.None)
            {
                int cmp = value.CompareTo(MaxValue);
                if (cmp > 0) return false;
            }

            return true;
        }

        /// <summary>
        /// 创建属性定义的便捷方法
        /// </summary>
        public static AttributeDefinition Create(uint id, string name, AttributeType type,
            AttributeValue defaultValue = default,
            bool syncToClient = true,
            bool persistent = false,
            AttributeCategory category = AttributeCategory.Common)
        {
            return new AttributeDefinition
            {
                Id = id,
                Name = name,
                Type = type,
                DefaultValue = defaultValue,
                SyncToClient = syncToClient,
                Persistent = persistent,
                Category = category
            };
        }
    }

    /// <summary>
    /// 属性类别
    /// </summary>
    public enum AttributeCategory : byte
    {
        // 公共对象属性 (1-12)
        Object = 1,

        // 生物属性 (101-223)
        Creature = 2,

        // 玩家专属属性 (301-378)
        Player = 3,

        // 宠物属性 (3001-3100)
        Pet = 4,

        // 怪物属性 (5001-6000)
        Monster = 5,

        // NPC属性 (7001-8000)
        NPC = 6,

        // 物品属性 (8000-9000)
        Item = 7,

        // 地上对象 (11001-12000)
        FloorObj = 8
    }

    /// <summary>
    /// 属性ID常量定义
    /// 与服务端 ObjProperty_enum.h 保持一致
    /// </summary>
    public static class AttributeIDs
    {
        // ========== 公共对象属性 (1-12) ==========
        public const uint ATTR_PointId = 1;        // 对象指针ID
        public const uint ATTR_ObjType = 2;        // 对象类型
        public const uint ATTR_SubObjType = 3;     // 对象子类型
        public const uint ATTR_Direction = 4;      // 方向
        public const uint ATTR_DisplayName = 5;    // 显示名称
        public const uint ATTR_NameColor = 6;      // 名字颜色
        public const uint ATTR_Status = 7;         // 状态 (0=活着 1=死亡)
        public const uint ATTR_Appr = 8;           // 外观
        public const uint ATTR_Action = 9;         // 当前动作
        public const uint ATTR_AppearEffect = 10;  // 出现特效
        public const uint ATTR_MasterID = 11;      // 主人ID
        public const uint ATTR_FashionId = 12;     // 时装ID

        public const uint OBJECT_START = 1;
        public const uint OBJECT_END = 12;

        // ========== 生物属性 (101-223) ==========
        public const uint ATTR_Sex = 101;              // 性别 1:男 0:女
        public const uint ATTR_CurHead = 102;          // 头像
        public const uint ATTR_CurHeadFrame = 103;     // 头像框
        public const uint ATTR_CurTitle = 104;         // 称号
        public const uint ATTR_Level = 105;            // 等级
        public const uint ATTR_BodyEffect = 106;       // 身体特效
        public const uint ATTR_HorseAppr = 107;        // 骑宠外观
        public const uint ATTR_ClothesAppr = 108;      // 衣服外观

        // 战斗属性
        public const uint ATTR_DcMax = 110;            // 最大攻击
        public const uint ATTR_AcMax = 111;            // 最大防御
        public const uint ATTR_CurHp = 112;            // 当前血量
        public const uint ATTR_HpMax = 113;            // 最大血量
        public const uint ATTR_AddAtkPer = 114;        // 攻击万分比
        public const uint ATTR_AddHpMaxPer = 115;      // 生命万分比
        public const uint ATTR_AddDefPer = 116;        // 增加防御万分比

        public const uint ATTR_AtkSpeedLv = 117;       // 攻速等级
        public const uint ATTR_HitValue = 118;         // 命中值
        public const uint ATTR_EvasionValue = 119;     // 闪避值
        public const uint ATTR_CritLevel = 120;        // 暴击等级
        public const uint ATTR_CritMissValue = 121;    // 暴击抵抗值
        public const uint ATTR_ExplosiveDamageValue = 122; // 暴伤值
        public const uint ATTR_BlastResistanceValue = 123;  // 暴伤抵抗值

        // 万分比属性
        public const uint ATTR_Hit = 128;              // 命中(万分比)
        public const uint ATTR_Dodge = 129;            // 闪避(万分比)
        public const uint ATTR_CritRatio = 130;        // 暴击概率(万分比)
        public const uint ATTR_Crit = 131;             // 暴击伤害(万分比)
        public const uint ATTR_CritMiss = 132;         // 暴击抵抗(万分比)
        public const uint ATTR_AddHurtPer = 134;       // 增伤万分比
        public const uint ATTR_DecHurtPer = 135;       // 减伤万分比

        // 计算属性
        public const uint ATTR_TotalAcMax = 151;       // 总最大防御
        public const uint ATTR_TotalDcMax = 152;       // 总最大攻击
        public const uint ATTR_TotalHpMax = 153;       // 总最大血量

        // 最终属性
        public const uint ATTR_FinalHit = 196;         // 命中率
        public const uint ATTR_FinalDodge = 197;       // 闪避率
        public const uint ATTR_FinalCritRatio = 198;   // 暴击率
        public const uint ATTR_FinalCritMiss = 199;    // 暴击抵抗
        public const uint ATTR_FinalCrit = 200;        // 暴击伤害
        public const uint ATTR_FinalAddHurtPer = 202;  // 全局增伤率
        public const uint ATTR_FinalDecHurtPer = 203;  // 全局减伤率

        public const uint CREATURE_START = 101;
        public const uint CREATURE_END = 223;

        // ========== 玩家专属属性 (301-378) ==========
        public const uint ATTR_CurField = 301;         // 当前关卡
        public const uint ATTR_CurHeroId = 302;        // 当前主角id
        public const uint ATTR_ExpMax = 303;           // 升级所需经验
        public const uint ATTR_ReselRole = 304;        // 是否正常退出
        public const uint ATTR_BindYuanBao = 305;      // 绑金
        public const uint ATTR_YuanBao = 306;          // 元宝
        public const uint ATTR_Diamond = 307;           // 钻石
        public const uint ATTR_JinBi = 308;            // 金币
        public const uint ATTR_CurPower = 309;         // 当前体力值
        public const uint ATTR_MaxPower = 313;         // 体力上限
        public const uint ATTR_PlayerID = 327;         // 角色全服唯一ID
        public const uint ATTR_LastMapCode = 332;      // 上一个地图编号
        public const uint ATTR_LastMapPosX = 333;      // 上一个地图X坐标
        public const uint ATTR_LastMapPosY = 334;      // 上一个地图Y坐标
        public const uint ATTR_TeamId = 372;           // 队伍ID
        public const uint ATTR_TeamState = 373;        // 队伍状态

        public const uint PLAYER_START = 301;
        public const uint PLAYER_END = 378;

        // ========== 宠物属性 (3001-3100) ==========
        public const uint PetAttr_Start = 3001;
        public const uint ATTR_PetIdx = 3002;         // 宠物索引
        public const uint ATTR_PetState = 3003;       // 宠物状态
        public const uint PetAttr_End = 3100;

        // ========== 怪物属性 (5001-6000) ==========
        public const uint ATTR_MoveInterval = 5001;    // 移动间隔
        public const uint ATTR_MStep = 5002;           // 移动步伐
        public const uint ATTR_AttackDistance = 5003;  // 攻击范围
        public const uint ATTR_AttackInterval = 5004;  // 攻击间隔
        public const uint ATTR_MagicInterval = 5005;   // 技能CD

        // ========== NPC属性 (7001-8000) ==========
        public const uint NPC_START = 7001;
        public const uint ATTR_TASKFLAG = 7001;        // 任务冒泡

        // ========== 物品属性 (8000-9000) ==========
        public const uint ATTR_CollectionLevel = 8050; // 藏品等级
        public const uint ATTR_CollectionStar = 8051;  // 藏品星级
        public const uint ATTR_ItemType = 8052;        // 物品类别
        public const uint ATTR_ItemIdx = 8053;         // 道具ID
        public const uint ATTR_Dura = 8054;            // 物品堆叠数
        public const uint ATTR_Quality = 8055;         // 品质
        public const uint ATTR_Phase = 8056;           // 阶段
        public const uint ATTR_Bound = 8061;           // 绑定标记
        public const uint ATTR_ItemId = 8063;          // 物品流水号

        // ========== 地上对象 (11001-12000) ==========
        public const uint FLOOROBJ_START = 11001;
        public const uint ATTR_FLOOROBJ_WIDTH = 11001; // 范围X
        public const uint ATTR_FLOOROBJ_LENGTH = 11002; // 范围Y
    }
}
