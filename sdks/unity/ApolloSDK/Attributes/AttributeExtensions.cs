using System;
using System.Collections.Generic;

namespace ApolloSDK.Attributes
{
    /// <summary>
    /// AttributeContainer 扩展方法
    /// 提供便捷的属性访问接口
    /// </summary>
    public static class AttributeExtensions
    {
        #region 快捷获取方法

        /// <summary>
        /// 快捷获取 int 值
        /// </summary>
        public static int GetInt(this AttributeContainer container, uint attributeId, int defaultValue = 0)
        {
            return container?.GetInt(attributeId, defaultValue) ?? defaultValue;
        }

        /// <summary>
        /// 快捷获取 long 值
        /// </summary>
        public static long GetLong(this AttributeContainer container, uint attributeId, long defaultValue = 0)
        {
            return container?.GetLong(attributeId, defaultValue) ?? defaultValue;
        }

        /// <summary>
        /// 快捷获取 float 值
        /// </summary>
        public static float GetFloat(this AttributeContainer container, uint attributeId, float defaultValue = 0f)
        {
            return container?.GetFloat(attributeId, defaultValue) ?? defaultValue;
        }

        /// <summary>
        /// 快捷获取 double 值
        /// </summary>
        public static double GetDouble(this AttributeContainer container, uint attributeId, double defaultValue = 0d)
        {
            return container?.GetDouble(attributeId, defaultValue) ?? defaultValue;
        }

        /// <summary>
        /// 快捷获取 bool 值
        /// </summary>
        public static bool GetBool(this AttributeContainer container, uint attributeId, bool defaultValue = false)
        {
            return container?.GetBool(attributeId, defaultValue) ?? defaultValue;
        }

        /// <summary>
        /// 快捷获取 string 值
        /// </summary>
        public static string GetString(this AttributeContainer container, uint attributeId, string defaultValue = null)
        {
            return container?.GetString(attributeId, defaultValue) ?? defaultValue;
        }

        #endregion

        #region 快捷设置方法

        /// <summary>
        /// 快捷设置 int 值
        /// </summary>
        public static bool SetInt(this AttributeContainer container, uint attributeId, int value)
        {
            return container?.SetAttribute(attributeId, value) ?? false;
        }

        /// <summary>
        /// 快捷设置 long 值
        /// </summary>
        public static bool SetLong(this AttributeContainer container, uint attributeId, long value)
        {
            return container?.SetAttribute(attributeId, value) ?? false;
        }

        /// <summary>
        /// 快捷设置 float 值
        /// </summary>
        public static bool SetFloat(this AttributeContainer container, uint attributeId, float value)
        {
            return container?.SetAttribute(attributeId, value) ?? false;
        }

        /// <summary>
        /// 快捷设置 double 值
        /// </summary>
        public static bool SetDouble(this AttributeContainer container, uint attributeId, double value)
        {
            return container?.SetAttribute(attributeId, value) ?? false;
        }

        /// <summary>
        /// 快捷设置 bool 值
        /// </summary>
        public static bool SetBool(this AttributeContainer container, uint attributeId, bool value)
        {
            return container?.SetAttribute(attributeId, value) ?? false;
        }

        /// <summary>
        /// 快捷设置 string 值
        /// </summary>
        public static bool SetString(this AttributeContainer container, uint attributeId, string value)
        {
            return container?.SetAttribute(attributeId, value) ?? false;
        }

        #endregion

        #region 数值操作

        /// <summary>
        /// 增加数值（仅对数值类型有效）
        /// </summary>
        public static int AddInt(this AttributeContainer container, uint attributeId, int delta)
        {
            if (container == null) return 0;

            int current = container.GetInt(attributeId);
            int newValue = current + delta;
            container.SetAttribute(attributeId, newValue);
            return newValue;
        }

        /// <summary>
        /// 增加数值（long 版本）
        /// </summary>
        public static long AddLong(this AttributeContainer container, uint attributeId, long delta)
        {
            if (container == null) return 0;

            long current = container.GetLong(attributeId);
            long newValue = current + delta;
            container.SetAttribute(attributeId, newValue);
            return newValue;
        }

        /// <summary>
        /// 减少数值
        /// </summary>
        public static int SubInt(this AttributeContainer container, uint attributeId, int delta)
        {
            return AddInt(container, attributeId, -delta);
        }

        /// <summary>
        /// 减少数值（long 版本）
        /// </summary>
        public static long SubLong(this AttributeContainer container, uint attributeId, long delta)
        {
            return AddLong(container, attributeId, -delta);
        }

        /// <summary>
        /// 乘法运算
        /// </summary>
        public static int MulInt(this AttributeContainer container, uint attributeId, int multiplier)
        {
            if (container == null) return 0;

            int current = container.GetInt(attributeId);
            int newValue = current * multiplier;
            container.SetAttribute(attributeId, newValue);
            return newValue;
        }

        /// <summary>
        /// 除法运算
        /// </summary>
        public static int DivInt(this AttributeContainer container, uint attributeId, int divisor)
        {
            if (container == null || divisor == 0) return 0;

            int current = container.GetInt(attributeId);
            int newValue = current / divisor;
            container.SetAttribute(attributeId, newValue);
            return newValue;
        }

        /// <summary>
        /// 确保数值在指定范围内
        /// </summary>
        public static int ClampInt(this AttributeContainer container, uint attributeId, int min, int max)
        {
            if (container == null) return min;

            int current = container.GetInt(attributeId);
            int clamped = Math.Clamp(current, min, max);

            if (current != clamped)
            {
                container.SetAttribute(attributeId, clamped);
            }

            return clamped;
        }

        /// <summary>
        /// 确保数值在指定范围内（long 版本）
        /// </summary>
        public static long ClampLong(this AttributeContainer container, uint attributeId, long min, long max)
        {
            if (container == null) return min;

            long current = container.GetLong(attributeId);
            long clamped = Math.Min(max, Math.Max(min, current));

            if (current != clamped)
            {
                container.SetAttribute(attributeId, clamped);
            }

            return clamped;
        }

        #endregion

        #region 条件检查

        /// <summary>
        /// 检查属性是否等于指定值
        /// </summary>
        public static bool IsEqual(this AttributeContainer container, uint attributeId, AttributeValue value)
        {
            if (container == null) return false;

            return container.GetAttribute(attributeId, out var current) && current.Equals(value);
        }

        /// <summary>
        /// 检查属性是否大于指定值
        /// </summary>
        public static bool IsGreaterThan(this AttributeContainer container, uint attributeId, AttributeValue value)
        {
            if (container == null) return false;

            return container.GetAttribute(attributeId, out var current) && current.CompareTo(value) > 0;
        }

        /// <summary>
        /// 检查属性是否小于指定值
        /// </summary>
        public static bool IsLessThan(this AttributeContainer container, uint attributeId, AttributeValue value)
        {
            if (container == null) return false;

            return container.GetAttribute(attributeId, out var current) && current.CompareTo(value) < 0;
        }

        /// <summary>
        /// 检查属性是否在指定范围内
        /// </summary>
        public static bool IsInRange(this AttributeContainer container, uint attributeId, AttributeValue min, AttributeValue max)
        {
            if (container == null) return false;

            return container.GetAttribute(attributeId, out var current) &&
                   current.CompareTo(min) >= 0 &&
                   current.CompareTo(max) <= 0;
        }

        #endregion

        #region 批量操作

        /// <summary>
        /// 批量设置属性
        /// </summary>
        public static int SetAttributes(this AttributeContainer container, Dictionary<uint, AttributeValue> attributes)
        {
            return container?.SetAttributes(attributes) ?? 0;
        }

        /// <summary>
        /// 从字典批量设置属性
        /// </summary>
        public static int SetAttributes(this AttributeContainer container, params (uint id, AttributeValue value)[] attributes)
        {
            if (container == null || attributes == null) return 0;

            var dict = new Dictionary<uint, AttributeValue>();
            foreach (var (id, value) in attributes)
            {
                dict[id] = value;
            }

            return container.SetAttributes(dict);
        }

        #endregion

        #region 游戏常用快捷方法

        /// <summary>
        /// 获取当前血量百分比 (0-100)
        /// </summary>
        public static float GetHpPercent(this AttributeContainer container)
        {
            if (container == null) return 0f;

            long curHp = container.GetLong(AttributeIDs.ATTR_CurHp);
            long maxHp = container.GetLong(AttributeIDs.ATTR_TotalHpMax);

            if (maxHp <= 0) return 0f;
            return (float)curHp * 100f / maxHp;
        }

        /// <summary>
        /// 设置血量百分比
        /// </summary>
        public static bool SetHpPercent(this AttributeContainer container, float percent)
        {
            if (container == null) return false;

            long maxHp = container.GetLong(AttributeIDs.ATTR_TotalHpMax);
            if (maxHp <= 0) return false;

            long newHp = (long)(maxHp * Math.Clamp(percent, 0f, 100f) / 100f);
            return container.SetAttribute(AttributeIDs.ATTR_CurHp, newHp);
        }

        /// <summary>
        /// 增加血量（带上限检查）
        /// </summary>
        public static long AddHp(this AttributeContainer container, long amount, bool allowOverMax = false)
        {
            if (container == null) return 0;

            long curHp = container.GetLong(AttributeIDs.ATTR_CurHp);
            long maxHp = container.GetLong(AttributeIDs.ATTR_TotalHpMax);

            long newHp = curHp + amount;
            if (!allowOverMax)
            {
                newHp = Math.Min(newHp, maxHp);
            }

            container.SetAttribute(AttributeIDs.ATTR_CurHp, new (long)Math.Max(0, newHp));
            return newHp;
        }

        /// <summary>
        /// 扣除血量（带下限检查）
        /// </summary>
        public static long SubHp(this AttributeContainer container, long amount)
        {
            if (container == null) return 0;

            long curHp = container.GetLong(AttributeIDs.ATTR_CurHp);
            long newHp = Math.Max(0, curHp - amount);

            container.SetAttribute(AttributeIDs.ATTR_CurHp, newHp);
            return newHp;
        }

        /// <summary>
        /// 检查是否死亡
        /// </summary>
        public static bool IsDead(this AttributeContainer container)
        {
            if (container == null) return true;

            int status = container.GetInt(AttributeIDs.ATTR_Status);
            return status != 0 || container.GetLong(AttributeIDs.ATTR_CurHp) <= 0;
        }

        /// <summary>
        /// 检查是否存活
        /// </summary>
        public static bool IsAlive(this AttributeContainer container)
        {
            return !IsDead(container);
        }

        /// <summary>
        /// 增加金币
        /// </summary>
        public static long AddJinBi(this AttributeContainer container, long amount)
        {
            return container.AddLong(AttributeIDs.ATTR_JinBi, amount);
        }

        /// <summary>
        /// 增加元宝
        /// </summary>
        public static long AddYuanBao(this AttributeContainer container, long amount)
        {
            return container.AddLong(AttributeIDs.ATTR_YuanBao, amount);
        }

        /// <summary>
        /// 增加钻石
        /// </summary>
        public static long AddDiamond(this AttributeContainer container, long amount)
        {
            return container.AddLong(AttributeIDs.ATTR_Diamond, amount);
        }

        /// <summary>
        /// 扣除金币（带检查）
        /// </summary>
        public static bool CostJinBi(this AttributeContainer container, long amount)
        {
            if (container == null) return false;

            long current = container.GetLong(AttributeIDs.ATTR_JinBi);
            if (current < amount) return false;

            container.SetLong(AttributeIDs.ATTR_JinBi, current - amount);
            return true;
        }

        /// <summary>
        /// 扣除元宝（带检查）
        /// </summary>
        public static bool CostYuanBao(this AttributeContainer container, long amount)
        {
            if (container == null) return false;

            long current = container.GetLong(AttributeIDs.ATTR_YuanBao);
            if (current < amount) return false;

            container.SetLong(AttributeIDs.ATTR_YuanBao, current - amount);
            return true;
        }

        /// <summary>
        /// 扣除钻石（带检查）
        /// </summary>
        public static bool CostDiamond(this AttributeContainer container, long amount)
        {
            if (container == null) return false;

            long current = container.GetLong(AttributeIDs.ATTR_Diamond);
            if (current < amount) return false;

            container.SetLong(AttributeIDs.ATTR_Diamond, current - amount);
            return true;
        }

        /// <summary>
        /// 增加经验值
        /// </summary>
        public static int AddExp(this AttributeContainer container, int amount)
        {
            // 这里假设经验存储在自定义属性中，具体根据游戏设计调整
            return container.AddInt(AttributeIDs.ATTR_CurField, amount);
        }

        /// <summary>
        /// 增加等级
        /// </summary>
        public static int AddLevel(this AttributeContainer container, int amount = 1)
        {
            if (container == null) return 0;

            int currentLevel = container.GetInt(AttributeIDs.ATTR_Level);
            int newLevel = currentLevel + amount;

            if (newLevel > currentLevel)
            {
                container.SetAttribute(AttributeIDs.ATTR_Level, newLevel);
                return newLevel;
            }

            return currentLevel;
        }

        #endregion

        #region 计算属性

        /// <summary>
        /// 计算所有计算属性的值并更新到容器
        /// </summary>
        /// <param name="changedAttributes">变更的基础属性ID集合（为空则重新计算所有计算属性）</param>
        public static int RecomputeComputed(this AttributeContainer container, HashSet<uint> changedAttributes = null)
        {
            if (container == null) return 0;

            var manager = ComputedAttributeManager.Instance;
            List<uint> toRecompute;

            if (changedAttributes == null || changedAttributes.Count == 0)
            {
                // 重新计算所有计算属性
                toRecompute = new List<uint>();
                lock (manager)
                {
                    foreach (var attr in manager.GetAllDefinitions())
                    {
                        if (manager.IsComputedAttribute(attr.Id))
                        {
                            toRecompute.Add(attr.Id);
                        }
                    }
                }
            }
            else
            {
                // 只重新计算受影响的计算属性
                toRecompute = new List<uint>();
                foreach (var attrId in changedAttributes)
                {
                    var dependent = manager.GetAttributesToRecalculate(attrId);
                    toRecompute.AddRange(dependent);
                }
            }

            int updatedCount = 0;
            foreach (var attrId in toRecompute)
            {
                var computedAttr = manager.GetComputedAttribute(attrId);
                if (computedAttr != null)
                {
                    var newValue = computedAttr.Compute(container);
                    if (container.SetAttribute(attrId, newValue))
                    {
                        updatedCount++;
                    }
                }
            }

            return updatedCount;
        }

        #endregion
    }
}
