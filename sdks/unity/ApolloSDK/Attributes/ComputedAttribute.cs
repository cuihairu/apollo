using System;
using System.Collections.Generic;

namespace ApolloSDK.Attributes
{
    /// <summary>
    /// 计算属性
    /// 根据其他属性的值动态计算得出，如：总攻击 = 基础攻击 * (1 + 攻击百分比加成)
    /// </summary>
    public class ComputedAttribute
    {
        /// <summary>
        /// 属性ID
        /// </summary>
        public uint AttributeId { get; set; }

        /// <summary>
        /// 依赖的属性ID列表
        /// </summary>
        public uint[] Dependencies { get; set; }

        /// <summary>
        /// 计算函数
        /// </summary>
        public Func<AttributeContainer, AttributeValue> ComputeFunc { get; set; }

        /// <summary>
        /// 是否为只读（不能直接设置值）
        /// </summary>
        public bool ReadOnly { get; set; } = true;

        /// <summary>
        /// 计算属性值
        /// </summary>
        public AttributeValue Compute(AttributeContainer container)
        {
            if (container == null || ComputeFunc == null)
                return new AttributeValue();

            try
            {
                return ComputeFunc(container);
            }
            catch (Exception ex)
            {
                UnityEngine.Debug.LogError($"ComputedAttribute.Compute error for attr {AttributeId}: {ex.Message}");
                return new AttributeValue();
            }
        }

        /// <summary>
        /// 检查是否需要重新计算
        /// </summary>
        public bool NeedsRecalculation(AttributeContainer container, HashSet<uint> changedAttributes)
        {
            if (Dependencies == null || changedAttributes == null) return false;

            foreach (var dep in Dependencies)
            {
                if (changedAttributes.Contains(dep))
                    return true;
            }

            return false;
        }
    }

    /// <summary>
    /// 计算属性管理器
    /// </summary>
    public class ComputedAttributeManager
    {
        private static ComputedAttributeManager _instance;
        private static readonly object _lock = new object();

        public static ComputedAttributeManager Instance
        {
            get
            {
                if (_instance == null)
                {
                    lock (_lock)
                    {
                        if (_instance == null)
                            _instance = new ComputedAttributeManager();
                    }
                }
                return _instance;
            }
        }

        // 计算属性定义: 属性ID -> ComputedAttribute
        private readonly Dictionary<uint, ComputedAttribute> _computedAttributes;

        // 属性依赖关系: 属性ID -> 依赖它的计算属性列表
        private readonly Dictionary<uint, List<uint>> _dependencyMap;

        private ComputedAttributeManager()
        {
            _computedAttributes = new Dictionary<uint, ComputedAttribute>();
            _dependencyMap = new Dictionary<uint, List<uint>>();
        }

        /// <summary>
        /// 注册计算属性
        /// </summary>
        public void Register(ComputedAttribute computedAttr)
        {
            if (computedAttr == null) return;

            lock (_lock)
            {
                _computedAttributes[computedAttr.AttributeId] = computedAttr;

                // 建立依赖映射
                if (computedAttr.Dependencies != null)
                {
                    foreach (var dep in computedAttr.Dependencies)
                    {
                        if (!_dependencyMap.TryGetValue(dep, out var list))
                        {
                            list = new List<uint>();
                            _dependencyMap[dep] = list;
                        }
                        list.Add(computedAttr.AttributeId);
                    }
                }
            }
        }

        /// <summary>
        /// 批量注册计算属性
        /// </summary>
        public void RegisterRange(IEnumerable<ComputedAttribute> computedAttrs)
        {
            if (computedAttrs == null) return;

            foreach (var attr in computedAttrs)
            {
                Register(attr);
            }
        }

        /// <summary>
        /// 获取计算属性定义
        /// </summary>
        public ComputedAttribute GetComputedAttribute(uint attributeId)
        {
            lock (_lock)
            {
                _computedAttributes.TryGetValue(attributeId, out var attr);
                return attr;
            }
        }

        /// <summary>
        /// 检查是否为计算属性
        /// </summary>
        public bool IsComputedAttribute(uint attributeId)
        {
            lock (_lock)
            {
                return _computedAttributes.ContainsKey(attributeId);
            }
        }

        /// <summary>
        /// 获取依赖某属性的所有计算属性ID
        /// </summary>
        public List<uint> GetDependentAttributes(uint attributeId)
        {
            lock (_lock)
            {
                if (_dependencyMap.TryGetValue(attributeId, out var list))
                {
                    return new List<uint>(list);
                }
                return new List<uint>();
            }
        }

        /// <summary>
        /// 当属性变更时，获取需要重新计算的计算属性ID列表
        /// </summary>
        public List<uint> GetAttributesToRecalculate(uint changedAttributeId)
        {
            return GetDependentAttributes(changedAttributeId);
        }

        /// <summary>
        /// 注册内置计算属性
        /// </summary>
        public void RegisterBuiltinComputedAttributes()
        {
            // 总最大攻击 = 最大攻击 * (1 + 攻击万分比 / 10000)
            Register(new ComputedAttribute
            {
                AttributeId = AttributeIDs.ATTR_TotalDcMax,
                Dependencies = new uint[] { AttributeIDs.ATTR_DcMax, AttributeIDs.ATTR_AddAtkPer },
                ComputeFunc = (container) =>
                {
                    long baseAtk = container.GetLong(AttributeIDs.ATTR_DcMax);
                    int addAtkPer = container.GetInt(AttributeIDs.ATTR_AddAtkPer);
                    long total = (long)(baseAtk * (1.0 + addAtkPer / 10000.0));
                    return new AttributeValue(total);
                }
            });

            // 总最大防御 = 最大防御 * (1 + 防御万分比 / 10000)
            Register(new ComputedAttribute
            {
                AttributeId = AttributeIDs.ATTR_TotalAcMax,
                Dependencies = new uint[] { AttributeIDs.ATTR_AcMax, AttributeIDs.ATTR_AddDefPer },
                ComputeFunc = (container) =>
                {
                    long baseDef = container.GetLong(AttributeIDs.ATTR_AcMax);
                    int addDefPer = container.GetInt(AttributeIDs.ATTR_AddDefPer);
                    long total = (long)(baseDef * (1.0 + addDefPer / 10000.0));
                    return new AttributeValue(total);
                }
            });

            // 总最大血量 = 最大血量 * (1 + 生命万分比 / 10000)
            Register(new ComputedAttribute
            {
                AttributeId = AttributeIDs.ATTR_TotalHpMax,
                Dependencies = new uint[] { AttributeIDs.ATTR_HpMax, AttributeIDs.ATTR_AddHpMaxPer },
                ComputeFunc = (container) =>
                {
                    long baseHp = container.GetLong(AttributeIDs.ATTR_HpMax);
                    int addHpPer = container.GetInt(AttributeIDs.ATTR_AddHpMaxPer);
                    long total = (long)(baseHp * (1.0 + addHpPer / 10000.0));
                    return new AttributeValue(total);
                }
            });

            // 最终命中率 = 基础命中 + 额外命中万分比相关的计算
            Register(new ComputedAttribute
            {
                AttributeId = AttributeIDs.ATTR_FinalHit,
                Dependencies = new uint[] { AttributeIDs.ATTR_HitValue, AttributeIDs.ATTR_Hit },
                ComputeFunc = (container) =>
                {
                    int hitValue = container.GetInt(AttributeIDs.ATTR_HitValue);
                    int hitPer = container.GetInt(AttributeIDs.ATTR_Hit);
                    // 示例计算公式
                    int finalHit = hitValue + (int)(hitValue * hitPer / 10000.0);
                    return new AttributeValue(finalHit);
                }
            });

            // 最终闪避率 = 基础闪避 + 额外闪避万分比相关的计算
            Register(new ComputedAttribute
            {
                AttributeId = AttributeIDs.ATTR_FinalDodge,
                Dependencies = new uint[] { AttributeIDs.ATTR_EvasionValue, AttributeIDs.ATTR_Dodge },
                ComputeFunc = (container) =>
                {
                    int dodgeValue = container.GetInt(AttributeIDs.ATTR_EvasionValue);
                    int dodgePer = container.GetInt(AttributeIDs.ATTR_Dodge);
                    int finalDodge = dodgeValue + (int)(dodgeValue * dodgePer / 10000.0);
                    return new AttributeValue(finalDodge);
                }
            });

            // 最终暴击率
            Register(new ComputedAttribute
            {
                AttributeId = AttributeIDs.ATTR_FinalCritRatio,
                Dependencies = new uint[] { AttributeIDs.ATTR_CritLevel, AttributeIDs.ATTR_CritRatio },
                ComputeFunc = (container) =>
                {
                    int critLevel = container.GetInt(AttributeIDs.ATTR_CritLevel);
                    int critPer = container.GetInt(AttributeIDs.ATTR_CritRatio);
                    int finalCrit = critLevel + (int)(critLevel * critPer / 10000.0);
                    return new AttributeValue(finalCrit);
                }
            });

            // 最终暴击伤害
            Register(new ComputedAttribute
            {
                AttributeId = AttributeIDs.ATTR_FinalCrit,
                Dependencies = new uint[] { AttributeIDs.ATTR_ExplosiveDamageValue, AttributeIDs.ATTR_Crit },
                ComputeFunc = (container) =>
                {
                    int critDmgValue = container.GetInt(AttributeIDs.ATTR_ExplosiveDamageValue);
                    int critPer = container.GetInt(AttributeIDs.ATTR_Crit);
                    int finalCrit = critDmgValue + (int)(critDmgValue * critPer / 10000.0);
                    return new AttributeValue(finalCrit);
                }
            });

            // 最终增伤率
            Register(new ComputedAttribute
            {
                AttributeId = AttributeIDs.ATTR_FinalAddHurtPer,
                Dependencies = new uint[] { AttributeIDs.ATTR_IncreaseDamageValue, AttributeIDs.ATTR_AddHurtPer },
                ComputeFunc = (container) =>
                {
                    int addDmgValue = container.GetInt(AttributeIDs.ATTR_IncreaseDamageValue);
                    int addHurtPer = container.GetInt(AttributeIDs.ATTR_AddHurtPer);
                    int finalAdd = addDmgValue + addHurtPer;
                    return new AttributeValue(finalAdd);
                }
            });

            // 最终减伤率
            Register(new ComputedAttribute
            {
                AttributeId = AttributeIDs.ATTR_FinalDecHurtPer,
                Dependencies = new uint[] { AttributeIDs.ATTR_DamageReductionValue, AttributeIDs.ATTR_DecHurtPer },
                ComputeFunc = (container) =>
                {
                    int decDmgValue = container.GetInt(AttributeIDs.ATTR_DamageReductionValue);
                    int decHurtPer = container.GetInt(AttributeIDs.ATTR_DecHurtPer);
                    int finalDec = decDmgValue + decHurtPer;
                    return new AttributeValue(finalDec);
                }
            });
        }
    }
}
