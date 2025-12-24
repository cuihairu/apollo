using System;
using System.Collections.Generic;

namespace ApolloSDK.Attributes
{
    /// <summary>
    /// 属性变更事件参数
    /// </summary>
    public class AttributeChangeEventArgs : EventArgs
    {
        /// <summary>
        /// 对象ID
        /// </summary>
        public ulong ObjectId { get; set; }

        /// <summary>
        /// 属性ID
        /// </summary>
        public uint AttributeId { get; set; }

        /// <summary>
        /// 旧值
        /// </summary>
        public AttributeValue OldValue { get; set; }

        /// <summary>
        /// 新值
        /// </summary>
        public AttributeValue NewValue { get; set; }

        /// <summary>
        /// 变更来源（服务端同步/客户端本地修改）
        /// </summary>
        public AttributeChangeSource Source { get; set; }

        public AttributeChangeEventArgs() { }

        public AttributeChangeEventArgs(ulong objectId, uint attributeId,
            AttributeValue oldValue, AttributeValue newValue,
            AttributeChangeSource source = AttributeChangeSource.Local)
        {
            ObjectId = objectId;
            AttributeId = attributeId;
            OldValue = oldValue;
            NewValue = newValue;
            Source = source;
        }

        public override string ToString()
        {
            return $"[AttrChange] Obj:{ObjectId} Attr:{AttributeId} {OldValue} -> {NewValue} Src:{Source}";
        }
    }

    /// <summary>
    /// 属性变更来源
    /// </summary>
    public enum AttributeChangeSource : byte
    {
        /// <summary>
        /// 客户端本地修改
        /// </summary>
        Local = 0,

        /// <summary>
        /// 服务端同步
        /// </summary>
        Server = 1,

        /// <summary>
        /// 系统计算（如计算属性重算）
        /// </summary>
        System = 2
    }

    /// <summary>
    /// 属性批量变更事件参数
    /// </summary>
    public class AttributeBatchChangeEventArgs : EventArgs
    {
        /// <summary>
        /// 对象ID
        /// </summary>
        public ulong ObjectId { get; set; }

        /// <summary>
        /// 变更的属性列表
        /// </summary>
        public List<AttributeChangeEventArgs> Changes { get; set; }

        /// <summary>
        /// 变更来源
        /// </summary>
        public AttributeChangeSource Source { get; set; }

        public AttributeBatchChangeEventArgs()
        {
            Changes = new List<AttributeChangeEventArgs>();
        }

        public int Count => Changes?.Count ?? 0;
    }

    /// <summary>
    /// 属性事件管理器
    /// </summary>
    public class AttributeEventManager
    {
        private static AttributeEventManager _instance;
        private static readonly object _lock = new object();

        public static AttributeEventManager Instance
        {
            get
            {
                if (_instance == null)
                {
                    lock (_lock)
                    {
                        if (_instance == null)
                            _instance = new AttributeEventManager();
                    }
                }
                return _instance;
            }
        }

        private readonly Dictionary<uint, List<EventHandler<AttributeChangeEventArgs>>> _listeners;
        private readonly Dictionary<ulong, EventHandler<AttributeChangeEventArgs>> _objectListeners;
        private readonly List<EventHandler<AttributeBatchChangeEventArgs>> _batchListeners;

        private AttributeEventManager()
        {
            _listeners = new Dictionary<uint, List<EventHandler<AttributeChangeEventArgs>>>();
            _objectListeners = new Dictionary<ulong, EventHandler<AttributeChangeEventArgs>>();
            _batchListeners = new List<EventHandler<AttributeBatchChangeEventArgs>>();
        }

        /// <summary>
        /// 注册属性变更监听器
        /// </summary>
        /// <param name="attributeId">属性ID，0 表示监听所有属性</param>
        /// <param name="handler">事件处理器</param>
        public void RegisterListener(uint attributeId, EventHandler<AttributeChangeEventArgs> handler)
        {
            if (handler == null) return;

            lock (_listeners)
            {
                if (!_listeners.TryGetValue(attributeId, out var list))
                {
                    list = new List<EventHandler<AttributeChangeEventArgs>>();
                    _listeners[attributeId] = list;
                }
                list.Add(handler);
            }
        }

        /// <summary>
        /// 注册对象属性变更监听器
        /// </summary>
        /// <param name="objectId">对象ID，0 表示监听所有对象</param>
        /// <param name="handler">事件处理器</param>
        public void RegisterObjectListener(ulong objectId, EventHandler<AttributeChangeEventArgs> handler)
        {
            if (handler == null) return;

            lock (_objectListeners)
            {
                if (_objectListeners.ContainsKey(objectId))
                    _objectListeners[objectId] += handler;
                else
                    _objectListeners[objectId] = handler;
            }
        }

        /// <summary>
        /// 注册批量变更监听器
        /// </summary>
        /// <param name="handler">事件处理器</param>
        public void RegisterBatchListener(EventHandler<AttributeBatchChangeEventArgs> handler)
        {
            if (handler == null) return;

            lock (_batchListeners)
            {
                _batchListeners.Add(handler);
            }
        }

        /// <summary>
        /// 取消注册属性变更监听器
        /// </summary>
        public void UnregisterListener(uint attributeId, EventHandler<AttributeChangeEventArgs> handler)
        {
            if (handler == null) return;

            lock (_listeners)
            {
                if (_listeners.TryGetValue(attributeId, out var list))
                {
                    list.Remove(handler);
                }
            }
        }

        /// <summary>
        /// 取消注册对象监听器
        /// </summary>
        public void UnregisterObjectListener(ulong objectId, EventHandler<AttributeChangeEventArgs> handler)
        {
            if (handler == null) return;

            lock (_objectListeners)
            {
                if (_objectListeners.TryGetValue(objectId, out var existing))
                {
                    _objectListeners[objectId] = existing - handler;
                }
            }
        }

        /// <summary>
        /// 取消注册批量变更监听器
        /// </summary>
        public void UnregisterBatchListener(EventHandler<AttributeBatchChangeEventArgs> handler)
        {
            if (handler == null) return;

            lock (_batchListeners)
            {
                _batchListeners.Remove(handler);
            }
        }

        /// <summary>
        /// 触发属性变更事件
        /// </summary>
        public void TriggerEvent(AttributeChangeEventArgs evt)
        {
            if (evt == null) return;

            // 触发全局属性监听器
            List<EventHandler<AttributeChangeEventArgs>> globalListeners = null;
            List<EventHandler<AttributeChangeEventArgs>> attrListeners = null;

            lock (_listeners)
            {
                _listeners.TryGetValue(0, out globalListeners);
                _listeners.TryGetValue(evt.AttributeId, out attrListeners);
            }

            globalListeners?.ForEach(h => h?.Invoke(this, evt));
            attrListeners?.ForEach(h => h?.Invoke(this, evt));

            // 触发对象监听器
            EventHandler<AttributeChangeEventArgs> objListener = null;
            EventHandler<AttributeChangeEventArgs> globalObjListener = null;

            lock (_objectListeners)
            {
                _objectListeners.TryGetValue(evt.ObjectId, out objListener);
                _objectListeners.TryGetValue(0, out globalObjListener);
            }

            globalObjListener?.Invoke(this, evt);
            objListener?.Invoke(this, evt);
        }

        /// <summary>
        /// 触发批量属性变更事件
        /// </summary>
        public void TriggerBatchEvent(AttributeBatchChangeEventArgs evt)
        {
            if (evt == null || evt.Changes == null || evt.Changes.Count == 0) return;

            List<EventHandler<AttributeBatchChangeEventArgs>> listeners;

            lock (_batchListeners)
            {
                listeners = new List<EventHandler<AttributeBatchChangeEventArgs>>(_batchListeners);
            }

            foreach (var listener in listeners)
            {
                listener?.Invoke(this, evt);
            }
        }

        /// <summary>
        /// 清空所有监听器
        /// </summary>
        public void Clear()
        {
            lock (_listeners)
            {
                _listeners.Clear();
            }

            lock (_objectListeners)
            {
                _objectListeners.Clear();
            }

            lock (_batchListeners)
            {
                _batchListeners.Clear();
            }
        }

        /// <summary>
        /// 清空指定对象的所有监听器
        /// </summary>
        public void ClearObjectListeners(ulong objectId)
        {
            lock (_objectListeners)
            {
                _objectListeners.Remove(objectId);
            }
        }
    }
}
