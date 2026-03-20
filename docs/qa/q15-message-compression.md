# Q15: 如何实现消息压缩？

## 问题分析

本题考察对消息压缩技术的理解：
- 压缩算法的选择
- 压缩时机和策略
- KBEngine 的压缩支持
- 性能与压缩率的权衡

---

## 一、压缩算法

### 1.1 常用压缩算法

```
┌─────────────────────────────────────────────────────────────┐
│                    压缩算法对比                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  算法         压缩率   速度    特性                      │
│  ┌─────────────────────────────────────────────────┐       │
│  │ Zlib         │ 高     │ 中     │ 平衡                      │       │
│  │ LZ4         │ 中     │ 极快   │ 实时优先                  │       │
│  │ Snappy       │ 低     │ 极快   │ Google 出品                 │       │
│  │ Zstd        │ 高     │ 快     │ Facebook 出品              │       │
│  │ LZMA        │ 最高   │ 慢     │ 7z 底层                   │       │
│  │ Huffman     │ 中     │ 快     │ 需要字典                  │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 压缩率对比

| 内容类型 | 原始大小 | Zlib | LZ4 | Snappy | Zstd |
|----------|----------|------|-----|--------|------|
| **JSON 文本** | 10KB | 3KB | 4KB | 5KB | 2.5KB |
| **Protobuf** | 5KB | 2KB | 3KB | 4KB | 2KB |
| **重复文本** | 8KB | 1KB | 1.5KB | 2KB | 1KB |
| **随机数据** | 10KB | 10KB | 10KB | 10KB | 9.5KB |

---

## 二、压缩策略

### 2.1 压缩时机

```
┌─────────────────────────────────────────────────────────────┐
│                  压缩时机决策                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  压缩 vs 不压缩：                                            │
│  ┌─────────────────────────────────────────────────┐       │
│  │  不压缩优势：                                        │       │
│  │  ├── 无 CPU 开销                                       │       │
│  │  ├── 低延迟                                            │       │
│  │  └── 简单实现                                          │       │
│  │                                                   │       │
│  │  压缩优势：                                          │       │
│  │  ├── 减少带宽占用                                     │       │
│  │  ├── 提高吞吐量                                       │       │
│  │  └── 可能降低延迟（少包=少RTT）                      │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  决策阈值：                                                │
│  ┌─────────────────────────────────────────────────┐       │
│  │  消息大小 > 512 字节 → 考虑压缩                  │       │
│  │  消息大小 < 128 字节 → 不压缩                      │       │
│  │  128-512 字节 → 根据场景选择                       │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 渐戏场景分析

```
┌─────────────────────────────────────────────────────────────┐
│                  游戏消息压缩决策                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  高优先级消息（不压缩）：                                   │
│  ├── 移动同步 (高频，延迟敏感)                             │
│  ├── 战斗操作 (需要低延迟)                                 │
│  └── 技能释放 (实时响应)                                   │
│                                                             │
│  中优先级消息（条件压缩）：                                 │
│  ├── 聊天消息 (文本压缩率高)                               │
│  │   └── 长度 > 200 字节时压缩                            │
│  ├── 系统公告 (文本，可压缩)                               │
│  └── 位置广播 (坐标可压缩)                               │
│                                                             │
│  低优先级消息（压缩）：                                   │
│  ├── 玩家数据 (大量数据)                                  │
│  ├── 配置更新 (文本数据)                                  │
│  └── 日志上报 (文本数据)                                  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 三、实现方案

### 3.1 压缩器接口

```cpp
// 消息压缩器接口

class MessageCompressor {
public:
    // 压缩消息
    std::vector<uint8_t> compress(const void* data, size_t len) {
        size_t compressedSize = compressBound(len);
        std::vector<uint8_t> compressed(compressedSize);

        // 使用 Zlib 压缩
        uLongf destLen = compressedSize;
        compress2((Bytef*)compressed.data(), &destLen,
                (const Bytef*)data, len,
                Z_DEFAULT_COMPRESSION);

        compressed.resize(destLen);
        return compressed;
    }

    // 解压消息
    std::vector<uint8_t decompress(const void* data, size_t len) {
        // 预估解压后大小（通常为压缩前的 2-5 倍）
        std::vector<uint8_t> decompressed(len * 4);

        uLongf destLen = decompressed.size();
        uncompress((Bytef*)decompressed.data(), &destLen,
                  (const Bytef*)data, len);

        decompressed.resize(destLen);
        return decompressed;
    }

    // 压缩级别选择
    int getCompressionLevel(size_t dataSize) const {
        if (dataSize < 128) return 0;      // 不压缩
        if (dataSize < 512) return 3;      // 快速压缩
        return 6;                         // 默认压缩
    }
};

// 压缩级别参考 Zlib:
// 0 = 不压缩
// 1 = 最快速度 (压缩率低)
// 3 = 快速
// 6 = 默认
// 9 = 最大压缩 (慢)
```

### 3.2 消息封装

```cpp
// 带压缩的消息封装

struct CompressedMessage {
    // 消息头
    struct Header {
        uint32_t magic;        // 0x434D504D ("CMPD")
        uint16_t msgId;
        uint32_t originalSize;
        uint32_t compressedSize;
        uint8_t compressionType;  // 0=无, 1=zlib, 2=lz4, 3=snappy
        uint16_t checksum;      // CRC16
    };

    Header header;
    std::vector<uint8_t> body;

    // 序列化
    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> buffer;
        buffer.resize(sizeof(Header) + body.size());

        memcpy(buffer.data(), &header, sizeof(Header));
        memcpy(buffer.data() + sizeof(Header), body.data(), body.size());

        return buffer;
    }

    // 创建压缩消息
    static CompressedMessage create(uint16_t msgId,
                                       const std::string& data) {
        CompressedMessage msg;
        msg.header.msgId = msgId;
        msg.header.originalSize = data.size();
        msg.header.compressionType = 0;  // 默认不压缩

        // 根据大小决定是否压缩
        if (data.size() > 128) {
            MessageCompressor compressor;
            msg.body = compressor.compress(data.data(), data.size());
            msg.header.compressedSize = msg.body.size();
            msg.header.compressionType = 1;  // zlib
        } else {
            msg.body.assign(data.begin(), data.end());
            msg.header.compressedSize = msg.body.size();
        }

        msg.header.checksum = calculateCRC(msg);
        return msg;
    }
};
```

### 3.3 集成到网络层

```cpp
// 集成到网络发送

class NetworkChannel {
public:
    // 发送消息（自动压缩）
    bool send(uint16_t msgId, const std::string& data) {
        auto compressedMsg = CompressedMessage::create(msgId, data);

        // 序列化
        auto buffer = compressedMsg.serialize();

        // 发送
        return socket_->send(buffer.data(), buffer.size());
    }

    // 接收并解压
    bool onReceive(const uint8_t* data, size_t len) {
        CompressedMessage msg;
        if (!msg.deserialize(data, len)) {
            return false;
        }

        // 解压消息体
        std::string payload;
        if (msg.header.compressionType != 0) {
            MessageCompressor compressor;
            auto decompressed = compressor.decompress(
                msg.body.data(), msg.body.size()
            );
            payload.assign(decompressed.begin(), decompressed.end());
        } else {
            payload.assign(msg.body.begin(), msg.body.end());
        }

        // 处理消息
        handleMessage(msg.header.msgId, payload);
        return true;
    }
};
```

---

## 四、优化技巧

### 4.1 字典压缩

```cpp
// 字典压缩器（用于文本类消息）

class DictionaryCompressor {
public:
    // 构建字典
    void buildDictionary(const std::vector<std::string>& messages) {
        // 统计词频
        std::unordered_map<std::string, int> wordFreq;
        for (const auto& msg : messages) {
            std::istringstream iss(msg);
            std::string word;
            while (iss >> word) {
                wordFreq[word]++;
            }
        }

        // 选择高频词加入字典
        for (const auto& [word, freq] : wordFreq) {
            if (freq > 10) {  // 阈值
                dictionary_.push_back(word);
            }
        }
    }

    // 使用字典压缩
    std::string compress(const std::string& text) {
        std::string result;
        std::istringstream iss(text);
        std::string word;

        while (iss >> word) {
            auto it = std::find(dictionary_.begin(), dictionary_.end(), word);
            if (it != dictionary_.end()) {
                // 使用字典索引
                uint16_t index = std::distance(dictionary_.begin(), it);
                result += "$" + std::to_string(index) + " ";
            } else {
                // 保留原词
                result += word + " ";
            }
        }
        return result;
    }

private:
    std::vector<std::string> dictionary_;
};
```

### 4.2 增量压缩

```cpp
// 增量压缩（只压缩变化部分）

class IncrementalCompressor {
public:
    // 设置基准数据
    void setBaseline(const std::string& baseline) {
        baseline_ = baseline;
    }

    // 压缩差异
    std::string compressDelta(const std::string& current) {
        // 计算差异
        std::string delta;
        size_t minLen = std::min(baseline_.size(), current.size());

        for (size_t i = 0; i < minLen; ++i) {
            if (baseline_[i] == current[i]) {
                delta += "0";  // 相同
            } else {
                delta += "1";  // 不同
            }
        }

        // 添加额外部分
        if (current.size() > baseline_.size()) {
            delta += current.substr(baseline_.size());
        }

        return delta;
    }

    // 解压差异
    std::string decompressDelta(const std::string& delta) {
        std::string result = baseline_;

        size_t i = 0;
        for (; i < baseline_.size() && i < delta.size(); ++i) {
            if (delta[i] == '0') {
                // 保持基准
            } else if (delta[i] == '1') {
                // 修改字符（简化处理）
            }
        }

        if (delta.size() > baseline_.size()) {
            result += delta.substr(baseline_.size());
        }

        return result;
    }

private:
    std::string baseline_;
};
```

---

## 五、性能对比

### 5.1 压缩效果测试

```cpp
// 压缩效果测试

struct TestResult {
    std::string type;
    size_t originalSize;
    size_t compressedSize;
    double compressionRatio;
    double compressTime;
    double decompressTime;
};

std::vector<TestResult> benchmarkCompression() {
    std::vector<TestResult> results;

    // 测试数据
    std::vector<std::string> testData = {
        "move:100.5,0.0,200.3",
        generateJSON(100),    // 重复 JSON
        generateRandom(1000),  // 随机数据
        generateText(500),     // 英文文本
    };

    for (const auto& data : testData) {
        TestResult result;
        result.originalSize = data.size();

        // Zlib
        auto start = now();
        auto compressed = zlibCompress(data);
        result.compressTime = msSince(start);
        result.compressedSize = compressed.size();

        start = now();
        auto decompressed = zlibDecompress(compressed);
        result.decompressTime = msSince(start);

        result.compressionRatio = (double)result.compressedSize / result.originalSize;
        results.push_back(result);
    }

    return results;
}
```

### 5.2 性能建议

```
┌─────────────────────────────────────────────────────────────┐
│                  压缩使用建议                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  场景              │ 算法    │ 压缩率 │ 速度 │           │
│  ├───────────────────────────────────────────────────────┤ │
│  │ 实时位置更新      │ 不压缩  │ 100%   │ 100% │           │
│  │ 战斗伤害          │ 不压缩  │ 100%   │ 100% │           │
│  │ 聊天消息(长)      │ Zlib   │ 70%    │ 80%  │           │
│  │ 玩家数据          │ Zstd   │ 60%    │ 85%  │           │
│  │ 配置文件          │ Zlib   │ 80%    │ 90%  │           │
│  │ 日志              │ Snappy │ 50%    │ 95%  │           │
│  │ 客户端之间       │ LZ4    │ 90%    │ 98%  │           │
│  └───────────────────────────────────────────────────────┘ │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 六、KBEngine 支持

### 6.1 KBEngine 压缩配置

```python
# KBEngine 消息压缩配置
# kbengine_defaults.xml

<Network>
    <!-- 是否启用压缩 -->
    <use_encode_auto>1</use_encode_auto>

    <!-- 压缩阈值 -->
    <use_encode_autosend_threshold>128</use_encode_autosend_threshold>

    <!-- 压缩类型 -->
    <use_encode_type>1</use_encode_type>
</Network>
```

### 6.2 KBEngine 压缩实现

```cpp
// KBEngine Bundle 压缩
// src/lib/network/bundle.h

class Bundle : public MemoryStream {
public:
    // 数据包压缩
    void pBundle::pack*(MemoryStream* pStream) {
        // ... 数据打包 ...

        // 自动压缩
        if (pBundle_->pChannel_->isCompressionEnabled() &&
            this->wpos() > pChannel_->compressionThreshold()) {
            this->compress();
        }
    }

    // 压缩数据
    void compress() {
        std::string data(str(), begin(), end());

        // Zlib 压缩
        uLongf destLen = compressBound(data.size());
        std::vector<uint8_t> compressed(destLen);

        compress2((Bytef*)compressed.data(), &destLen,
               (Bytef*)data.data(), data.size(),
               Z_DEFAULT_COMPRESSION);

        // 替换原数据
        data_.assign(compressed.begin(), compressed.end());
        compressed_ = true;
    }
};
```

---

## 七、最佳实践

### 7.1 压缩决策树

```
是否压缩？
│
├─ 消息大小 < 128 字节
│  └─► NO
│
├─ 消息大小 128-512 字节
│  ├─ 需要低延迟
│  │  └─► NO
│  └─ 带宽敏感
│     └─► YES (Zlib 快速)
│
└─ 消息大小 > 512 字节
    ├─ CPU 负载高
    │  └─► LZ4 或 Snappy
    ├─ 压缩率要求高
    │  └─► Zstd
    └─ 通用场景
        └─► Zlib
```

### 7.2 监控指标

```cpp
// 压缩监控

class CompressionMonitor {
public:
    void record(uint16_t msgId, bool compressed,
               size_t original, size_t compressed,
               uint64_t compressTime) {
        CompressionStats& stats = stats_[msgId];

        stats.totalMessages++;
        stats.compressedMessages += compressed;
        stats.totalOriginalSize += original;
        stats.totalCompressedSize += compressed;
        stats.totalCompressTime += compressTime;
    }

    void report() {
        std::cout << "=== Compression Report ===\n";

        for (const auto& [msgId, stats] : stats_) {
            float ratio = (double)stats.totalCompressedSize / stats.totalOriginalSize) * 100;
            float avgTime = (double)stats.totalCompressTime / stats.compressedMessages;

            std::cout << "Msg " << msgId << ": "
                      << "Ratio=" << ratio << "%, "
                      << "AvgTime=" << avgTime << "us\n";
        }
    }
};
```

---

## 八、总结

### 压缩方案选择

| 场景 | 推荐算法 | 理由 |
|------|----------|------|
| **通用消息** | Zlib | 兼容性好，平衡 |
| **高性能** | LZ4/Snappy | 速度快 |
| **高压缩率** | Zstd | 压缩率高 |
| **游戏内客户端** | LZ4 | 速度优先 |

### 最佳实践

```
1. 合理选择阈值
   - 小消息不压缩（开销大）
   - 中等消息按需压缩
   - 大消息总是压缩

2. 根据场景选择
   - 实时消息：不压缩
   - 文本消息：压缩率高
   - 二进制数据：压缩率低

3. 监控压缩效果
   - 统计压缩率
   - 计算CPU开销
   - 调整压缩策略

4. 支持动态调整
   - 根据CPU负载调整
   - 根据带宽压力调整
   - 根据消息类型调整
```

---

## 参考资料

- [Zlib 官方文档](https://www.zlib.net/manual.html)
- [LZ4 网站](https://lz4.github.io/lz4/)
- [Zstd 压缩库](https://github.com/facebook/zstd)
