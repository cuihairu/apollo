using System;
using System.Collections.Generic;
using System.Linq;

namespace ApolloSDK.Attributes
{
    /// <summary>
    /// 属性注册表
    /// 集中管理所有属性定义，提供属性ID到名称的双向映射
    /// </summary>
    public class AttributeRegistry
    {
        private static AttributeRegistry _instance;
        private static readonly object _lock = new object();

        public static AttributeRegistry Instance
        {
            get
            {
                if (_instance == null)
                {
                    lock (_lock)
                    {
                        if (_instance == null)
                            _instance = new AttributeRegistry();
                    }
                }
                return _instance;
            }
        }

        // 属性ID -> 定义
        private readonly Dictionary<uint, AttributeDefinition> _byId;

        // 属性名称 -> ID
        private readonly Dictionary<string, uint> _byName;

        // 按类别分组的属性
        private readonly Dictionary<AttributeCategory, List<uint>> _byCategory;

        private bool _initialized;

        private AttributeRegistry()
        {
            _byId = new Dictionary<uint, AttributeDefinition>();
            _byName = new Dictionary<string, uint>(StringComparer.OrdinalIgnoreCase);
            _byCategory = new Dictionary<AttributeCategory, List<uint>>();
            _initialized = false;
        }

        /// <summary>
        /// 初始化注册表，注册所有内置属性
        /// </summary>
        public void Initialize()
        {
            if (_initialized) return;

            lock (_lock)
            {
                if (_initialized) return;

                RegisterBuiltinAttributes();
                _initialized = true;
            }
        }

        /// <summary>
        /// 注册内置属性定义
        /// </summary>
        private void RegisterBuiltinAttributes()
        {
            // ========== 公共对象属性 ==========
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_PointId, "PointId", AttributeType.Long,
                syncToClient: false, category: AttributeCategory.Object));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_ObjType, "ObjType", AttributeType.Int,
                syncToClient: true, category: AttributeCategory.Object));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_SubObjType, "SubObjType", AttributeType.Int,
                syncToClient: true, category: AttributeCategory.Object));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_Direction, "Direction", AttributeType.Int,
                syncToClient: true, category: AttributeCategory.Object));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_DisplayName, "DisplayName", AttributeType.String,
                syncToClient: true, category: AttributeCategory.Object));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_NameColor, "NameColor", AttributeType.Int,
                syncToClient: true, category: AttributeCategory.Object));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_Status, "Status", AttributeType.Int,
                syncToClient: true, category: AttributeCategory.Object));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_Appr, "Appr", AttributeType.Int,
                syncToClient: true, category: AttributeCategory.Object));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_Action, "Action", AttributeType.Int,
                syncToClient: true, category: AttributeCategory.Object));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_AppearEffect, "AppearEffect", AttributeType.Int,
                syncToClient: true, category: AttributeCategory.Object));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_MasterID, "MasterID", AttributeType.Long,
                syncToClient: false, category: AttributeCategory.Object));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_FashionId, "FashionId", AttributeType.Int,
                syncToClient: true, category: AttributeCategory.Object));

            // ========== 生物属性 ==========
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_Sex, "Sex", AttributeType.Int,
                new AttributeValue(1), new AttributeValue(0), new AttributeValue(1),
                syncToClient: true, category: AttributeCategory.Creature));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_CurHead, "CurHead", AttributeType.Int,
                syncToClient: true, category: AttributeCategory.Creature));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_CurHeadFrame, "CurHeadFrame", AttributeType.Int,
                syncToClient: true, category: AttributeCategory.Creature));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_CurTitle, "CurTitle", AttributeType.Int,
                syncToClient: true, category: AttributeCategory.Creature));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_Level, "Level", AttributeType.Int,
                new AttributeValue(1), new AttributeValue(1), new AttributeValue(int.MaxValue),
                syncToClient: true, persistent: true, category: AttributeCategory.Creature));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_BodyEffect, "BodyEffect", AttributeType.Int,
                syncToClient: true, category: AttributeCategory.Creature));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_HorseAppr, "HorseAppr", AttributeType.Int,
                syncToClient: true, category: AttributeCategory.Creature));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_ClothesAppr, "ClothesAppr", AttributeType.Int,
                syncToClient: true, category: AttributeCategory.Creature));

            // 战斗属性
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_DcMax, "DcMax", AttributeType.Long,
                syncToClient: true, category: AttributeCategory.Creature));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_AcMax, "AcMax", AttributeType.Long,
                syncToClient: true, category: AttributeCategory.Creature));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_CurHp, "CurHp", AttributeType.Long,
                new AttributeValue(100), new AttributeValue(0), new AttributeValue(long.MaxValue),
                syncToClient: true, category: AttributeCategory.Creature));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_HpMax, "HpMax", AttributeType.Long,
                new AttributeValue(100), new AttributeValue(1), new AttributeValue(long.MaxValue),
                syncToClient: true, category: AttributeCategory.Creature));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_AddAtkPer, "AddAtkPer", AttributeType.Int,
                syncToClient: false, category: AttributeCategory.Creature));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_AddHpMaxPer, "AddHpMaxPer", AttributeType.Int,
                syncToClient: false, category: AttributeCategory.Creature));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_AddDefPer, "AddDefPer", AttributeType.Int,
                syncToClient: false, category: AttributeCategory.Creature));

            // 万分比属性
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_Hit, "Hit", AttributeType.Int,
                syncToClient: true, category: AttributeCategory.Creature));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_Dodge, "Dodge", AttributeType.Int,
                syncToClient: true, category: AttributeCategory.Creature));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_CritRatio, "CritRatio", AttributeType.Int,
                syncToClient: true, category: AttributeCategory.Creature));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_Crit, "Crit", AttributeType.Int,
                syncToClient: true, category: AttributeCategory.Creature));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_CritMiss, "CritMiss", AttributeType.Int,
                syncToClient: true, category: AttributeCategory.Creature));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_AddHurtPer, "AddHurtPer", AttributeType.Int,
                syncToClient: true, category: AttributeCategory.Creature));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_DecHurtPer, "DecHurtPer", AttributeType.Int,
                syncToClient: true, category: AttributeCategory.Creature));

            // 计算属性（只读）
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_TotalAcMax, "TotalAcMax", AttributeType.Long,
                syncToClient: true, category: AttributeCategory.Creature));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_TotalDcMax, "TotalDcMax", AttributeType.Long,
                syncToClient: true, category: AttributeCategory.Creature));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_TotalHpMax, "TotalHpMax", AttributeType.Long,
                syncToClient: true, category: AttributeCategory.Creature));

            // 最终属性
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_FinalHit, "FinalHit", AttributeType.Int,
                syncToClient: true, category: AttributeCategory.Creature));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_FinalDodge, "FinalDodge", AttributeType.Int,
                syncToClient: true, category: AttributeCategory.Creature));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_FinalCritRatio, "FinalCritRatio", AttributeType.Int,
                syncToClient: true, category: AttributeCategory.Creature));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_FinalCritMiss, "FinalCritMiss", AttributeType.Int,
                syncToClient: true, category: AttributeCategory.Creature));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_FinalCrit, "FinalCrit", AttributeType.Int,
                syncToClient: true, category: AttributeCategory.Creature));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_FinalAddHurtPer, "FinalAddHurtPer", AttributeType.Int,
                syncToClient: true, category: AttributeCategory.Creature));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_FinalDecHurtPer, "FinalDecHurtPer", AttributeType.Int,
                syncToClient: true, category: AttributeCategory.Creature));

            // ========== 玩家专属属性 ==========
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_CurField, "CurField", AttributeType.Int,
                persistent: true, category: AttributeCategory.Player));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_CurHeroId, "CurHeroId", AttributeType.Int,
                persistent: true, category: AttributeCategory.Player));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_ExpMax, "ExpMax", AttributeType.Long,
                persistent: true, category: AttributeCategory.Player));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_BindYuanBao, "BindYuanBao", AttributeType.Long,
                new AttributeValue(0), new AttributeValue(0), new AttributeValue(long.MaxValue),
                syncToClient: true, persistent: true, category: AttributeCategory.Player));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_YuanBao, "YuanBao", AttributeType.Long,
                new AttributeValue(0), new AttributeValue(0), new AttributeValue(long.MaxValue),
                syncToClient: true, persistent: true, category: AttributeCategory.Player));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_Diamond, "Diamond", AttributeType.Long,
                new AttributeValue(0), new AttributeValue(0), new AttributeValue(long.MaxValue),
                syncToClient: true, persistent: true, category: AttributeCategory.Player));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_JinBi, "JinBi", AttributeType.Long,
                new AttributeValue(0), new AttributeValue(0), new AttributeValue(long.MaxValue),
                syncToClient: true, persistent: true, category: AttributeCategory.Player));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_CurPower, "CurPower", AttributeType.Int,
                syncToClient: true, category: AttributeCategory.Player));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_MaxPower, "MaxPower", AttributeType.Int,
                syncToClient: true, category: AttributeCategory.Player));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_PlayerID, "PlayerID", AttributeType.Long,
                syncToClient: false, persistent: true, category: AttributeCategory.Player));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_LastMapCode, "LastMapCode", AttributeType.Int,
                persistent: true, category: AttributeCategory.Player));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_LastMapPosX, "LastMapPosX", AttributeType.Int,
                persistent: true, category: AttributeCategory.Player));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_LastMapPosY, "LastMapPosY", AttributeType.Int,
                persistent: true, category: AttributeCategory.Player));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_TeamId, "TeamId", AttributeType.Long,
                syncToClient: true, category: AttributeCategory.Player));
            Register(AttributeDefinition.Create(AttributeIDs.ATTR_TeamState, "TeamState", AttributeType.Int,
                syncToClient: true, category: AttributeCategory.Player));
        }

        /// <summary>
        /// 注册属性定义
        /// </summary>
        public void Register(AttributeDefinition definition)
        {
            if (definition == null) throw new ArgumentNullException(nameof(definition));

            lock (_lock)
            {
                _byId[definition.Id] = definition;
                _byName[definition.Name] = definition.Id;

                if (!_byCategory.ContainsKey(definition.Category))
                {
                    _byCategory[definition.Category] = new List<uint>();
                }
                if (!_byCategory[definition.Category].Contains(definition.Id))
                {
                    _byCategory[definition.Category].Add(definition.Id);
                }
            }
        }

        /// <summary>
        /// 批量注册属性定义
        /// </summary>
        public void RegisterRange(IEnumerable<AttributeDefinition> definitions)
        {
            if (definitions == null) return;

            foreach (var def in definitions)
            {
                Register(def);
            }
        }

        /// <summary>
        /// 获取属性定义
        /// </summary>
        public AttributeDefinition GetDefinition(uint attributeId)
        {
            lock (_lock)
            {
                _byId.TryGetValue(attributeId, out var def);
                return def;
            }
        }

        /// <summary>
        /// 通过名称获取属性定义
        /// </summary>
        public AttributeDefinition GetDefinition(string name)
        {
            lock (_lock)
            {
                if (_byName.TryGetValue(name, out var id))
                {
                    return _byId[id];
                }
                return null;
            }
        }

        /// <summary>
        /// 通过名称获取属性ID
        /// </summary>
        public uint GetAttributeId(string name)
        {
            lock (_lock)
            {
                if (_byName.TryGetValue(name, out var id))
                {
                    return id;
                }
                return 0;
            }
        }

        /// <summary>
        /// 获取所有属性定义
        /// </summary>
        public IEnumerable<AttributeDefinition> GetAllDefinitions()
        {
            lock (_lock)
            {
                return _byId.Values.ToList();
            }
        }

        /// <summary>
        /// 获取指定类别的属性ID列表
        /// </summary>
        public List<uint> GetByCategory(AttributeCategory category)
        {
            lock (_lock)
            {
                if (_byCategory.TryGetValue(category, out var list))
                {
                    return new List<uint>(list);
                }
                return new List<uint>();
            }
        }

        /// <summary>
        /// 检查属性是否已注册
        /// </summary>
        public bool IsRegistered(uint attributeId)
        {
            lock (_lock)
            {
                return _byId.ContainsKey(attributeId);
            }
        }

        /// <summary>
        /// 获取已注册属性数量
        /// </summary>
        public int Count
        {
            get
            {
                lock (_lock)
                {
                    return _byId.Count;
                }
            }
        }
    }
}
