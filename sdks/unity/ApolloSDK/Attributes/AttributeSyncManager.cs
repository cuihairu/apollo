using System;
using System.Collections.Generic;
using System.Linq;

namespace ApolloSDK.Attributes
{
    /// <summary>
    /// 属性同步管理器
    /// 负责属性与服务端的同步，包括增量同步和全量同步
    /// </summary>
    public class AttributeSyncManager
    {
        private static AttributeSyncManager _instance;
        private static readonly object _lock = new object();

        public static AttributeSyncManager Instance
        {
            get
            {
                if (_instance == null)
                {
                    lock (_lock)
                    {
                        if (_instance == null)
                            _instance = new AttributeSyncManager();
                    }
                }
                return _instance;
            }
        }

        // 同步间隔（秒）
        private float _syncInterval = 0.1f;

        // 上次同步时间
        private float _lastSyncTime;

        // 是否启用自动同步
        private bool _autoSyncEnabled = true;

        // 容器集合
        private readonly Dictionary<ulong, AttributeContainer> _containers;

        private AttributeSyncManager()
        {
            _containers = new Dictionary<ulong, AttributeContainer>();
        }

        /// <summary>
        /// 注册属性容器
        /// </summary>
        public void RegisterContainer(AttributeContainer container)
        {
            if (container == null) return;

            lock (_containers)
            {
                _containers[container.ObjectId] = container;
            }
        }

        /// <summary>
        /// 注销属性容器
        /// </summary>
        public void UnregisterContainer(ulong objectId)
        {
            lock (_containers)
            {
                _containers.Remove(objectId);
            }
        }

        /// <summary>
        /// 获取属性容器
        /// </summary>
        public AttributeContainer GetContainer(ulong objectId)
        {
            lock (_containers)
            {
                _containers.TryGetValue(objectId, out var container);
                return container;
            }
        }

        /// <summary>
        /// 创建属性容器
        /// </summary>
        public AttributeContainer CreateContainer(ulong objectId)
        {
            var container = new AttributeContainer(objectId);
            RegisterContainer(container);
            return container;
        }

        /// <summary>
        /// 同步间隔
        /// </summary>
        public float SyncInterval
        {
            get => _syncInterval;
            set => _syncInterval = Math.Max(0.01f, value);
        }

        /// <summary>
        /// 是否启用自动同步
        /// </summary>
        public bool AutoSyncEnabled
        {
            get => _autoSyncEnabled;
            set => _autoSyncEnabled = value;
        }

        /// <summary>
        /// 每帧更新，处理自动同步
        /// </summary>
        public void Update(float deltaTime)
        {
            if (!_autoSyncEnabled) return;

            _lastSyncTime += deltaTime;

            if (_lastSyncTime >= _syncInterval)
            {
                _lastSyncTime = 0f;
                SyncAllDirtyAttributes();
            }
        }

        /// <summary>
        /// 同步所有容器的脏属性
        /// </summary>
        public void SyncAllDirtyAttributes()
        {
            List<AttributeContainer> containers;

            lock (_containers)
            {
                containers = _containers.Values.Where(c => c.HasDirtyAttributes).ToList();
            }

            foreach (var container in containers)
            {
                SyncDirtyAttributes(container);
            }
        }

        /// <summary>
        /// 同步指定容器的脏属性
        /// </summary>
        public void SyncDirtyAttributes(AttributeContainer container)
        {
            if (container == null) return;

            var dirtyAttrs = container.GetDirtyAttributeValues();
            if (dirtyAttrs.Count == 0) return;

            // 构建同步包
            byte[] packet = BuildDeltaPacket(container.ObjectId, dirtyAttrs);

            // 发送到服务端（需要依赖网络管理器）
            OnSyncPacketReady?.Invoke(packet);

            // 清除脏标志
            container.ClearDirtyFlags();
        }

        /// <summary>
        /// 同步包准备就绪事件
        /// </summary>
        public event Action<byte[]> OnSyncPacketReady;

        #region 数据包构建

        /// <summary>
        /// 构建增量同步数据包
        /// 格式: [对象ID(8)] [属性数量(2)] [属性ID(4) 属性值长度(2) 属性值数据...]
        /// </summary>
        public byte[] BuildDeltaPacket(AttributeContainer container)
        {
            if (container == null) return null;

            var dirtyAttrs = container.GetDirtyAttributeValues();
            return BuildDeltaPacket(container.ObjectId, dirtyAttrs);
        }

        /// <summary>
        /// 构建增量同步数据包
        /// </summary>
        public byte[] BuildDeltaPacket(ulong objectId, Dictionary<uint, AttributeValue> attributes)
        {
            if (attributes == null || attributes.Count == 0) return null;

            using (var ms = new System.IO.MemoryStream())
            using (var writer = new System.IO.BinaryWriter(ms))
            {
                // 对象ID
                writer.Write(objectId);

                // 属性数量
                writer.Write((ushort)attributes.Count);

                // 每个属性
                foreach (var kvp in attributes.OrderBy(a => a.Key))
                {
                    byte[] valueBytes = kvp.Value.ToBytes();

                    writer.Write(kvp.Key);           // 属性ID (4字节)
                    writer.Write((ushort)valueBytes.Length); // 值长度 (2字节)
                    writer.Write(valueBytes);        // 值数据
                }

                return ms.ToArray();
            }
        }

        /// <summary>
        /// 构建全量同步数据包
        /// </summary>
        public byte[] BuildFullPacket(AttributeContainer container)
        {
            if (container == null) return null;

            using (var ms = new System.IO.MemoryStream())
            using (var writer = new System.IO.BinaryWriter(ms))
            {
                // 对象ID
                writer.Write(container.ObjectId);

                // 全量数据
                byte[] containerData = container.Serialize();
                writer.Write((uint)containerData.Length);
                writer.Write(containerData);

                return ms.ToArray();
            }
        }

        /// <summary>
        /// 构建请求属性数据包
        /// </summary>
        public byte[] BuildRequestPacket(ulong objectId, params uint[] attributeIds)
        {
            if (attributeIds == null || attributeIds.Length == 0) return null;

            using (var ms = new System.IO.MemoryStream())
            using (var writer = new System.IO.BinaryWriter(ms))
            {
                // 对象ID
                writer.Write(objectId);

                // 请求数量
                writer.Write((ushort)attributeIds.Length);

                // 请求的属性ID列表
                foreach (var attrId in attributeIds)
                {
                    writer.Write(attrId);
                }

                return ms.ToArray();
            }
        }

        #endregion

        #region 数据包解析

        /// <summary>
        /// 解析服务端同步数据包并应用到容器
        /// </summary>
        /// <param name="packet">数据包</param>
        /// <param name="container">目标容器</param>
        /// <returns>成功更新的属性数量</returns>
        public int ApplyServerUpdate(byte[] packet, AttributeContainer container)
        {
            if (packet == null || packet.Length < 10) return 0;

            try
            {
                using (var ms = new System.IO.MemoryStream(packet))
                using (var reader = new System.IO.BinaryReader(ms))
                {
                    ulong objectId = reader.ReadUInt64();

                    if (container == null || container.ObjectId != objectId)
                    {
                        container = GetContainer(objectId);
                        if (container == null)
                        {
                            container = CreateContainer(objectId);
                        }
                    }

                    // 读取属性数量
                    ushort count = reader.ReadUInt16();

                    int updatedCount = 0;
                    for (int i = 0; i < count; i++)
                    {
                        uint attrId = reader.ReadUInt32();
                        ushort len = reader.ReadUInt16();
                        byte[] valueBytes = reader.ReadBytes(len);

                        int offset = 0;
                        var value = AttributeValue.FromBytes(valueBytes, ref offset);
                        if (container.SetAttribute(attrId, value, fromServer: true))
                        {
                            updatedCount++;
                        }
                    }

                    return updatedCount;
                }
            }
            catch (Exception ex)
            {
                UnityEngine.Debug.LogError($"ApplyServerUpdate error: {ex.Message}");
                return 0;
            }
        }

        /// <summary>
        /// 解析服务端全量同步数据包
        /// </summary>
        public int ApplyFullUpdate(byte[] packet, AttributeContainer container)
        {
            if (packet == null || packet.Length < 12) return 0;

            try
            {
                using (var ms = new System.IO.MemoryStream(packet))
                using (var reader = new System.IO.BinaryReader(ms))
                {
                    ulong objectId = reader.ReadUInt64();
                    uint dataLen = reader.ReadUInt32();

                    if (container == null || container.ObjectId != objectId)
                    {
                        container = GetContainer(objectId);
                        if (container == null)
                        {
                            container = CreateContainer(objectId);
                        }
                    }

                    byte[] data = reader.ReadBytes((int)dataLen);
                    container.Deserialize(data);

                    // 清除脏标志，因为这是服务端的数据
                    container.ClearDirtyFlags();

                    return container.Count;
                }
            }
            catch (Exception ex)
            {
                UnityEngine.Debug.LogError($"ApplyFullUpdate error: {ex.Message}");
                return 0;
            }
        }

        /// <summary>
        /// 批量应用服务端更新到多个容器
        /// </summary>
        public Dictionary<ulong, int> ApplyBatchServerUpdate(byte[] packet)
        {
            var result = new Dictionary<ulong, int>();

            if (packet == null) return result;

            // 数据包格式: [容器数量(2)] [每个容器的数据...]
            try
            {
                using (var ms = new System.IO.MemoryStream(packet))
                using (var reader = new System.IO.BinaryReader(ms))
                {
                    ushort containerCount = reader.ReadUInt16();

                    for (int i = 0; i < containerCount; i++)
                    {
                        // 读取容器数据长度
                        uint containerLen = reader.ReadUInt32();

                        // 读取容器数据
                        byte[] containerData = reader.ReadBytes((int)containerLen);

                        // 应用更新
                        int count = ApplyServerUpdate(containerData, null);
                        if (count > 0)
                        {
                            // 从数据中提取对象ID
                            if (containerData.Length >= 8)
                            {
                                ulong objectId = BitConverter.ToUInt64(containerData, 0);
                                result[objectId] = count;
                            }
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                UnityEngine.Debug.LogError($"ApplyBatchServerUpdate error: {ex.Message}");
            }

            return result;
        }

        #endregion

        /// <summary>
        /// 清理所有容器
        /// </summary>
        public void Clear()
        {
            lock (_containers)
            {
                _containers.Clear();
            }
        }
    }
}
