# Q64: 如何优化网络带宽？

## 问题分析

本题考察对网络带宽优化的理解：
- 消息压缩
- 增量同步
- 批量发送
- 协议优化
- KBEngine 消息处理

---

## 一、带宽优化策略

### 1.1 优化层次

```
┌─────────────────────────────────────────────────────────────┐
│                    带宽优化层次                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 协议层 (Protocol Layer):                                │
│  ├── 二进制协议 (Protobuf)                                  │
│  ├── 字段压缩 (Varint)                                      │
│  ├── 枚举替代字符串                                         │
│  └── 消息合并                                               │
│                          │                                  │
│  2. 数据层 (Data Layer):                                    │
│  ├── 增量更新 (Delta)                                       │
│  ├── 过滤不必要数据                                         │
│  ├── 位置同步优化                                           │
│  └── 属性同步策略                                           │
│                          │                                  │
│  3. 传输层 (Transport Layer):                              │
│  ├── 批量发送                                               │
│  ├── 消息优先级                                             │
│  ├── 拥塞控制                                               │
│  └── 速率限制                                               │
│                          │                                  │
│  4. 压缩层 (Compression Layer):                             │
│  ├── Zlib 压缩                                              │
│  ├── LZ4 快速压缩                                           │
│  ├── Snappy 均衡压缩                                        │
│  └── 自适应压缩                                             │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 带宽占用分析

```
┌─────────────────────────────────────────────────────────────┐
│                    网络消息分类                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  高频消息 (每秒多次):                                        │
│  ├── 位置同步 (30-50次/秒)                                   │
│  ├── 动画状态 (10-20次/秒)                                   │
│  └── 战斗数值 (5-10次/秒)                                    │
│  → 需要重点优化                                              │
│                                                             │
│  中频消息 (每秒几次):                                        │
│  ├── 技能释放                                                │
│  ├── Buff 变化                                              │
│  └── 状态变化                                               │
│  → 适度优化                                                  │
│                                                             │
│  低频消息 (偶尔):                                            │
│  ├── 聊天消息                                                │
│  ├── 系统通知                                                │
│  └── 交易请求                                               │
│  → 正常处理                                                  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、消息优化

### 2.1 位置同步优化

```cpp
// 位置同步优化

struct PositionMessage {
    float x, y, z;
    uint8_t flags;  // 压缩标志位

    // 压缩位置 (减少精度)
    void compress(float originalX, float originalY, float originalZ) {
        // 使用 16.16 定点数 (2字节)
        x = std::round(originalX * 256) / 256;
        y = std::round(originalY * 256) / 256;
        z = std::round(originalZ * 256) / 256;
    }

    // 增量编码
    void encodeDelta(const PositionMessage& prev, PositionMessage& curr) {
        curr.x = curr.x - prev.x;
        curr.y = curr.y - prev.y;
        curr.z = curr.z - prev.z;
    }
};

// 批量位置同步
class PositionBatcher {
public:
    void addPosition(uint64_t entityId, const Vector3& pos) {
        pending_[entityId] = pos;
    }

    // 每 50ms 发送一次
    void flush() {
        if (pending_.empty()) return;

        PositionBatchMessage msg;
        msg.positions.reserve(pending_.size());

        for (const auto& [id, pos] : pending_) {
            msg.positions.push_back({id, pos});
        }

        broadcast(msg);
        pending_.clear();
    }

private:
    std::unordered_map<uint64_t, Vector3> pending_;
};
```

### 2.2 属性同步优化

```cpp
// 属性同步优化

class PropertySync {
public:
    // 使用位标记同步变化的属性
    struct PropertyFlags {
        uint32_t flags = 0;

        enum Flag : uint32_t {
            HP = 1 << 0,
            MP = 1 << 1,
            LEVEL = 1 << 2,
            EXP = 1 << 3,
            // ... 更多属性
        };

        void markChanged(Flag flag) {
            flags |= flag;
        }

        bool isChanged(Flag flag) const {
            return flags & flag;
        }

        void clear() {
            flags = 0;
        }
    };

    // 只同步变化的属性
    void syncProperties(Entity* entity, PropertyFlags flags) {
        PropertyUpdateMessage msg;
        msg.entityId = entity->getId();
        msg.flags = flags.flags;

        if (flags.isChanged(PropertyFlags::HP)) {
            msg.hp = entity->getHP();
        }
        if (flags.isChanged(PropertyFlags::MP)) {
            msg.mp = entity->getMP();
        }
        // ... 其他属性

        broadcastToAOI(entity, msg);
        flags.clear();
    }
};
```

---

## 三、KBEngine 消息优化

### 3.1 KBEngine 消息定义

```python
# KBEngine 消息定义
# kbengine_defs.xml

<!-- 使用固定大小数值类型 -->
<Properties>
    <!-- ✅ 好: 使用固定大小类型 -->
    <Property>
        <Name>hp</Name>
        <Type>INT32</Type>
        <Flags>CELL_PUBLIC</Flags>
    </Property>

    <!-- ❌ 不好: 使用变长类型 -->
    <Property>
        <Name>name</Name>
        <Type>STRING</Type>
        <Flags>BASE</Flags>
    </Property>
</Properties>

<!-- 使用别名减少重复 -->
<Aliases>
    <Alias>
        <Name>Vector3</Name>
        <Type>FLOAT3</Type>
    </Alias>
</Aliases>
```

### 3.2 KBEngine 远程方法调用优化

```python
# KBEngine 客户端方法优化

class Account(KBEngine.Entity):
    def __init__(self):
        KBEngine.Entity.__init__(self)

    # ❌ 不好: 多次调用
    def updateInventory_bad(self):
        for item in self.items:
            self.client.onItemUpdate(item.id, item.count)

    # ✅ 好: 批量发送
    def updateInventory_good(self):
        items_data = [(item.id, item.count) for item in self.items]
        self.client.onInventoryUpdate(items_data)

    # ✅ 好: 使用延迟发送
    def scheduleInventoryUpdate(self):
        if not self._inventoryDirty:
            self._inventoryDirty = True
            KBEngine.addTimer(0.1, 0, self._flushInventory)

    def _flushInventory(self):
        if self._inventoryDirty:
            self.updateInventory_good()
            self._inventoryDirty = False
```

---

## 四、压缩算法

### 4.1 压缩算法对比

| 算法 | 压缩比 | 速度 | CPU | 适用场景 |
|------|--------|------|-----|----------|
| **LZ4** | 低 | 极快 | 低 | 实时消息 |
| **Snappy** | 中 | 快 | 低 | 一般消息 |
| **Zlib** | 高 | 慢 | 中 | 离线数据 |
| **LZMA** | 极高 | 很慢 | 高 | 资源包 |

### 4.2 自适应压缩

```cpp
// 自适应压缩策略

class AdaptiveCompressor {
public:
    std::string compress(const std::string& data) {
        // 小于 100 字节不压缩
        if (data.size() < 100) {
            return data;
        }

        // 尝试 LZ4 压缩
        auto compressed = lz4_.compress(data);

        // 压缩后更大，返回原数据
        if (compressed.size() >= data.size() * 0.9) {
            return data;
        }

        return compressed;
    }

    std::string decompress(const std::string& data) {
        if (isCompressed(data)) {
            return lz4_.decompress(data);
        }
        return data;
    }

private:
    bool isCompressed(const std::string& data) {
        // 检查压缩标记
        return !data.empty() && (data[0] & 0x80);
    }

    LZ4Compressor lz4_;
};
```

---

## 五、优化效果

### 5.1 优化前后对比

| 消息类型 | 优化前 | 优化后 | 节省 |
|---------|--------|--------|------|
| **位置同步** | 32 bytes | 12 bytes | 62% |
| **属性更新** | 64 bytes | 8 bytes | 87% |
| **战斗信息** | 128 bytes | 48 bytes | 62% |
| **AOI 广播** | N × 50 bytes | 批量合并 | 40% |

### 5.2 带宽估算

```
单玩家带宽估算 (优化后):

- 位置同步: 12 bytes × 30 = 360 bytes/s
- 属性更新: 8 bytes × 5 = 40 bytes/s
- 其他消息: 100 bytes/s
- 协议开销: 20%

总计: (360 + 40 + 100) × 1.2 ≈ 600 bytes/s ≈ 4.8 Kbps

5000 玩家: 4.8 × 5000 = 24 Mbps
```

---

## 六、最佳实践

| 实践 | 说明 |
|------|------|
| **使用 Protobuf** | 二进制序列化 |
| **减少同步频率** | 适度降低精度 |
| **批量发送** | 合并小消息 |
| **AOI 过滤** | 只发可见区域 |
| **增量更新** | 只发变化部分 |

---

## 七、总结

```
带宽优化 = 协议优化 + 数据压缩 + 批量发送 + 智能过滤
- 二进制协议
- 只同步变化
- 批量高频消息
- 过滤不可见
```

---

## 参考资料

- [Protocol Buffers](https://developers.google.com/protocol-buffers)
- [KBEngine Network](https://kbengine.github.io/docs/)
- [LZ4 Compression](https://github.com/lz4/lz4)
