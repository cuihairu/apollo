using System;
using System.Collections.Generic;
using System.Linq;

namespace ApolloSDK.Attributes
{
    /// <summary>
    /// 属性容器
    /// 对应服务端的 ProperValue 结构，存储对象的所有属性
    /// </summary>
    public class AttributeContainer
    {
        // 对象ID
        private ulong _objectId;

        // 属性存储: 属性ID -> (值, 是否需要同步标志)
        private readonly Dictionary<uint, (AttributeValue value, bool dirty)> _attributes;

        // 读写锁
        private readonly object _lock = new object();

        // 事件
        public event EventHandler<AttributeChangeEventArgs> OnAttributeChanged;
        public event EventHandler<AttributeBatchChangeEventArgs> OnBatchAttributeChanged;

        /// <summary>
        /// 构造函数
        /// </summary>
        public AttributeContainer(ulong objectId)
        {
            _objectId = objectId;
            _attributes = new Dictionary<uint, (AttributeValue, bool)>();
        }

        /// <summary>
        /// 对象ID
        /// </summary>
        public ulong ObjectId => _objectId;

        /// <summary>
        /// 属性数量
        /// </summary>
        public int Count
        {
            get
            {
                lock (_lock)
                {
                    return _attributes.Count;
                }
            }
        }

        /// <summary>
        /// 是否有需要同步的属性
        /// </summary>
        public bool HasDirtyAttributes
        {
            get
            {
                lock (_lock)
                {
                    return _attributes.Values.Any(v => v.dirty);
                }
            }
        }

        #region 设置属性

        /// <summary>
        /// 设置属性
        /// </summary>
        /// <param name="attributeId">属性ID</param>
        /// <param name="value">新值</param>
        /// <param name="fromServer">是否来自服务端</param>
        /// <returns>是否设置成功</returns>
        public bool SetAttribute(uint attributeId, AttributeValue value, bool fromServer = false)
        {
            if (value.Type == AttributeType.None) return false;

            lock (_lock)
            {
                // 获取旧值
                bool exists = _attributes.TryGetValue(attributeId, out var oldValue);
                AttributeValue oldVal = exists ? oldValue.value : new AttributeValue();

                // 值未变化
                if (exists && oldValue.value.Equals(value))
                    return false;

                // 设置新值
                _attributes[attributeId] = (value, !fromServer); // 来自服务端的值不需要同步回去

                // 触发事件
                var evt = new AttributeChangeEventArgs(_objectId, attributeId, oldVal, value,
                    fromServer ? AttributeChangeSource.Server : AttributeChangeSource.Local);
                OnAttributeChanged?.Invoke(this, evt);

                // 触发全局事件
                AttributeEventManager.Instance.TriggerEvent(evt);

                return true;
            }
        }

        /// <summary>
        /// 批量设置属性
        /// </summary>
        /// <param name="attributes">属性列表</param>
        /// <param name="fromServer">是否来自服务端</param>
        /// <returns>实际变更的属性数量</returns>
        public int SetAttributes(Dictionary<uint, AttributeValue> attributes, bool fromServer = false)
        {
            if (attributes == null || attributes.Count == 0) return 0;

            var changes = new List<AttributeChangeEventArgs>();

            lock (_lock)
            {
                foreach (var kvp in attributes)
                {
                    uint attrId = kvp.Key;
                    AttributeValue newVal = kvp.Value;

                    if (newVal.Type == AttributeType.None) continue;

                    bool exists = _attributes.TryGetValue(attrId, out var oldVal);
                    AttributeValue oldValValue = exists ? oldVal.value : new AttributeValue();

                    // 值未变化
                    if (exists && oldVal.value.Equals(newVal)) continue;

                    // 设置新值
                    _attributes[attrId] = (newVal, !fromServer);

                    changes.Add(new AttributeChangeEventArgs(_objectId, attrId, oldValValue, newVal,
                        fromServer ? AttributeChangeSource.Server : AttributeChangeSource.Local));
                }
            }

            // 触发批量事件
            if (changes.Count > 0)
            {
                var batchEvt = new AttributeBatchChangeEventArgs
                {
                    ObjectId = _objectId,
                    Changes = changes,
                    Source = fromServer ? AttributeChangeSource.Server : AttributeChangeSource.Local
                };

                OnBatchAttributeChanged?.Invoke(this, batchEvt);

                // 触发单个事件
                foreach (var evt in changes)
                {
                    OnAttributeChanged?.Invoke(this, evt);
                    AttributeEventManager.Instance.TriggerEvent(evt);
                }
            }

            return changes.Count;
        }

        #endregion

        #region 获取属性

        /// <summary>
        /// 获取属性
        /// </summary>
        /// <param name="attributeId">属性ID</param>
        /// <param name="value">输出值</param>
        /// <returns>是否存在该属性</returns>
        public bool GetAttribute(uint attributeId, out AttributeValue value)
        {
            lock (_lock)
            {
                if (_attributes.TryGetValue(attributeId, out var val))
                {
                    value = val.value;
                    return true;
                }

                value = new AttributeValue();
                return false;
            }
        }

        /// <summary>
        /// 获取属性（带默认值）
        /// </summary>
        public AttributeValue GetAttribute(uint attributeId, AttributeValue defaultValue)
        {
            lock (_lock)
            {
                if (_attributes.TryGetValue(attributeId, out var val))
                {
                    return val.value;
                }
                return defaultValue;
            }
        }

        /// <summary>
        /// 类型安全的获取方法
        /// </summary>
        public int GetInt(uint attributeId, int defaultValue = 0)
        {
            lock (_lock)
            {
                if (_attributes.TryGetValue(attributeId, out var val))
                    return val.value.GetInt(defaultValue);
                return defaultValue;
            }
        }

        public long GetLong(uint attributeId, long defaultValue = 0)
        {
            lock (_lock)
            {
                if (_attributes.TryGetValue(attributeId, out var val))
                    return val.value.GetLong(defaultValue);
                return defaultValue;
            }
        }

        public float GetFloat(uint attributeId, float defaultValue = 0f)
        {
            lock (_lock)
            {
                if (_attributes.TryGetValue(attributeId, out var val))
                    return val.value.GetFloat(defaultValue);
                return defaultValue;
            }
        }

        public double GetDouble(uint attributeId, double defaultValue = 0d)
        {
            lock (_lock)
            {
                if (_attributes.TryGetValue(attributeId, out var val))
                    return val.value.GetDouble(defaultValue);
                return defaultValue;
            }
        }

        public bool GetBool(uint attributeId, bool defaultValue = false)
        {
            lock (_lock)
            {
                if (_attributes.TryGetValue(attributeId, out var val))
                    return val.value.GetBool(defaultValue);
                return defaultValue;
            }
        }

        public string GetString(uint attributeId, string defaultValue = null)
        {
            lock (_lock)
            {
                if (_attributes.TryGetValue(attributeId, out var val))
                    return val.value.GetString(defaultValue);
                return defaultValue;
            }
        }

        #endregion

        #region 属性操作

        /// <summary>
        /// 增加属性值
        /// </summary>
        /// <param name="attributeId">属性ID</param>
        /// <param name="delta">增量</param>
        /// <returns>新的属性值</returns>
        public AttributeValue AddValue(uint attributeId, AttributeValue delta)
        {
            lock (_lock)
            {
                if (_attributes.TryGetValue(attributeId, out var current))
                {
                    var newVal = current.value + delta;
                    SetAttribute(attributeId, newVal);
                    return newVal;
                }
                // 属性不存在，直接设置
                SetAttribute(attributeId, delta);
                return delta;
            }
        }

        /// <summary>
        /// 减少属性值
        /// </summary>
        public AttributeValue SubValue(uint attributeId, AttributeValue delta)
        {
            lock (_lock)
            {
                if (_attributes.TryGetValue(attributeId, out var current))
                {
                    var newVal = current.value - delta;
                    SetAttribute(attributeId, newVal);
                    return newVal;
                }
                return new AttributeValue();
            }
        }

        /// <summary>
        /// 检查属性是否存在
        /// </summary>
        public bool HasAttribute(uint attributeId)
        {
            lock (_lock)
            {
                return _attributes.ContainsKey(attributeId);
            }
        }

        /// <summary>
        /// 移除属性
        /// </summary>
        public bool RemoveAttribute(uint attributeId)
        {
            lock (_lock)
            {
                return _attributes.Remove(attributeId);
            }
        }

        /// <summary>
        /// 清空所有属性
        /// </summary>
        public void Clear()
        {
            lock (_lock)
            {
                _attributes.Clear();
            }
        }

        #endregion

        #region 同步相关

        /// <summary>
        /// 获取需要同步的属性列表
        /// </summary>
        public List<uint> GetDirtyAttributes()
        {
            lock (_lock)
            {
                return _attributes
                    .Where(kvp => kvp.Value.dirty)
                    .Select(kvp => kvp.Key)
                    .ToList();
            }
        }

        /// <summary>
        /// 获取需要同步的属性
        /// </summary>
        public Dictionary<uint, AttributeValue> GetDirtyAttributeValues()
        {
            lock (_lock)
            {
                return _attributes
                    .Where(kvp => kvp.Value.dirty)
                    .ToDictionary(kvp => kvp.Key, kvp => kvp.Value.value);
            }
        }

        /// <summary>
        /// 清除脏标志
        /// </summary>
        public void ClearDirtyFlags()
        {
            lock (_lock)
            {
                var keys = _attributes.Keys.ToList();
                foreach (var key in keys)
                {
                    var val = _attributes[key];
                    _attributes[key] = (val.value, false);
                }
            }
        }

        /// <summary>
        /// 清除指定属性的脏标志
        /// </summary>
        public void ClearDirtyFlag(uint attributeId)
        {
            lock (_lock)
            {
                if (_attributes.TryGetValue(attributeId, out var val))
                {
                    _attributes[attributeId] = (val.value, false);
                }
            }
        }

        #endregion

        #region 序列化

        /// <summary>
        /// 序列化所有属性到字节数组（用于全量同步）
        /// </summary>
        public byte[] Serialize()
        {
            lock (_lock)
            {
                // 格式: [属性数量(2)] [属性ID(4) 属性值...]
                using (var ms = new System.IO.MemoryStream())
                using (var writer = new System.IO.BinaryWriter(ms))
                {
                    writer.Write((ushort)_attributes.Count);

                    foreach (var kvp in _attributes.OrderBy(a => a.Key))
                    {
                        writer.Write(kvp.Key);
                        byte[] valueBytes = kvp.Value.value.ToBytes();
                        writer.Write((ushort)valueBytes.Length);
                        writer.Write(valueBytes);
                    }

                    return ms.ToArray();
                }
            }
        }

        /// <summary>
        /// 从字节数组反序列化
        /// </summary>
        public void Deserialize(byte[] data)
        {
            if (data == null || data.Length < 2) return;

            lock (_lock)
            {
                using (var ms = new System.IO.MemoryStream(data))
                using (var reader = new System.IO.BinaryReader(ms))
                {
                    ushort count = reader.ReadUInt16();

                    for (int i = 0; i < count; i++)
                    {
                        uint attrId = reader.ReadUInt32();
                        ushort len = reader.ReadUInt16();
                        byte[] valueBytes = reader.ReadBytes(len);

                        int offset = 0;
                        var value = AttributeValue.FromBytes(valueBytes, ref offset);
                        SetAttribute(attrId, value, fromServer: true);
                    }
                }
            }
        }

        #endregion
    }
}
