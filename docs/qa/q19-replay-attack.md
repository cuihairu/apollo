# Q19: 如何防止消息重放攻击？

## 问题分析

本题考察对网络安全和防作弊的理解：
- 重放攻击的原理和危害
- 消息认证和防重放机制
- 序列号、时间戳、Nonce 等技术
- KBEngine 的安全措施

---

## 一、重放攻击原理

### 1.1 什么是重放攻击

```
┌─────────────────────────────────────────────────────────────┐
│                    重放攻击原理                               │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  正常流程:                                                  │
│  ┌─────────────────────────────────────────────────┐       │
│  │  客户端 ──请求──► 服务器 ──处理──► 返回结果       │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  重放攻击流程:                                              │
│  ┌─────────────────────────────────────────────────┐       │
│  │  攻击者捕获请求                                   │       │
│  │       │                                          │       │
│  │       ▼                                          │       │
│  │  ┌─────────────┐                                │       │
│  │  │  捕获的请求  │ "login:user=abc,pass=123"      │       │
│  │  └─────────────┘                                │       │
│  │       │                                          │       │
│  │       │ 重复发送                                  │       │
│  │       ▼                                          │       │
│  │  服务器 ──再次处理──► 可能导致危害                 │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  危害:                                                     │
│  ├── 重复消费 (刷金币、刷道具)                              │
│  ├── 绕过验证 (重放登录包)                                  │
│  ├── 恶意刷数据 (刷排行榜、刷评论)                          │
│  └── 破坏业务逻辑 (重复操作)                                │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 常见重放攻击场景

```
┌─────────────────────────────────────────────────────────────┐
│                  重放攻击常见场景                             │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  场景 1: 重复消费                                            │
│  ┌─────────────────────────────────────────────────┐       │
│  │  1. 玩家购买道具，发送购买请求                     │       │
│  │  2. 服务器扣款，发放道具                           │       │
│  │  3. 攻击者捕获购买请求                             │       │
│  │  4. 重复发送购买请求                               │       │
│  │  5. 服务器再次扣款，再次发放道具 → 刷道具！         │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  场景 2: 登录重放                                            │
│  ┌─────────────────────────────────────────────────┐       │
│  │  1. 玩家 A 登录成功                               │       │
│  │  2. 攻击者捕获登录包                              │       │
│  │  3. 玩家 A 下线                                   │       │
│  │  4. 攻击者重放登录包                               │       │
│  │  5. 伪装成玩家 A 登录 → 盗号！                      │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  场景 3: 战斗操作重放                                        │
│  ┌─────────────────────────────────────────────────┐       │
│  │  1. 玩家发送攻击请求                               │       │
│  │  2. 服务器处理，造成伤害                           │       │
│  │  3. 攻击者重放攻击请求                             │       │
│  │  4. 服务器重复处理，重复伤害 → 秒怪！              │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  场景 4: 交易重放                                            │
│  ┌─────────────────────────────────────────────────┐       │
│  │  1. 玩家 A 向 B 转账                               │       │
│  │  2. 攻击者捕获转账请求                             │       │
│  │  3. 重复发送转账请求                               │       │
│  │  4. 重复转账 → 刷金币！                            │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、防重放机制

### 2.1 消息序列号

```
┌─────────────────────────────────────────────────────────────┐
│                    消息序列号机制                             │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  原理:                                                     │
│  ┌─────────────────────────────────────────────────┐       │
│  │  每个会话维护一个递增的序列号                       │       │
│  │  每条消息携带序列号                                 │       │
│  │  服务器只接受更大序列号的消息                       │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  流程:                                                     │
│  ┌─────────────────────────────────────────────────┐       │
│  │  客户端                         服务器           │       │
│  │  ┌─────┐                     ┌─────┐            │       │
│  │  │Seq=1│ ─────────────────────→│接受 │            │       │
│  │  └─────┘                     └─────┘            │       │
│  │  ┌─────┐                     ┌─────┐            │       │
│  │  │Seq=2│ ─────────────────────→│接受 │            │       │
│  │  └─────┘                     └─────┘            │       │
│  │  ┌─────┐                     ┌─────┐            │       │
│  │  │Seq=2│ ─(重放)─────────────→│拒绝！│            │       │
│  │  └─────┘   (序列号不递增)       └─────┘            │       │
│  │  ┌─────┐                     ┌─────┐            │       │
│  │  │Seq=3│ ─────────────────────→│接受 │            │       │
│  │  └─────┘                     └─────┘            │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  优势: 简单、高效                                           │
│  劣势: 需要维护状态、掉包导致空洞                            │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 序列号实现

```cpp
// 消息序列号防重放

class SequenceNumberManager {
public:
    // 生成下一个序列号
    uint32_t nextSequence() {
        return ++currentSequence_;
    }

    // 验证序列号
    bool validateSequence(uint32_t seq) {
        // 首次连接
        if (lastSequence_ == 0) {
            lastSequence_ = seq;
            return true;
        }

        // 检查是否递增
        if (seq > lastSequence_) {
            // 检查跳跃是否过大（可能的重放攻击）
            if (seq - lastSequence_ > MAX_SEQUENCE_GAP) {
                return false;
            }
            lastSequence_ = seq;
            return true;
        }

        // 序列号回退，可能是重放
        return false;
    }

    // 处理乱序（允许一定窗口内的乱序）
    bool validateSequenceWithWindow(uint32_t seq) {
        // 在窗口内的直接接受
        if (seq > lastSequence_ &&
            seq - lastSequence__ <= SEQUENCE_WINDOW) {
            lastSequence_ = seq;
            return true;
        }

        // 检查是否在乱序窗口内
        if (outOfOrderBuffer_.contains(seq)) {
            return false; // 已经处理过
        }

        if (seq < lastSequence_ &&
            lastSequence_ - seq <= SEQUENCE_WINDOW) {
            // 在乱序窗口内，缓存
            outOfOrderBuffer_.insert(seq);
            return true;
        }

        return false;
    }

private:
    uint32_t currentSequence_ = 0;
    uint32_t lastSequence_ = 0;
    std::unordered_set<uint32_t> outOfOrderBuffer_;

    static constexpr uint32_t MAX_SEQUENCE_GAP = 1000;
    static constexpr uint32_t SEQUENCE_WINDOW = 100;
};

// 消息封装
struct SequencedMessage {
    uint32_t sequence;
    uint16_t messageId;
    std::vector<uint8_t> data;
    uint32_t timestamp;     // 额外：时间戳
    uint8_t signature[32];  // 额外：签名

    // 序列化
    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> buffer;
        buffer.reserve(sizeof(SequencedMessage) + data.size());

        appendUint32(buffer, sequence);
        appendUint16(buffer, messageId);
        appendUint32(buffer, timestamp);
        buffer.insert(buffer.end(), signature, signature + 32);
        appendUint16(buffer, data.size());
        buffer.insert(buffer.end(), data.begin(), data.end());

        return buffer;
    }

    // 反序列化
    static SequencedMessage deserialize(const uint8_t* data, size_t len) {
        SequencedMessage msg;
        size_t offset = 0;

        msg.sequence = readUint32(data + offset); offset += 4;
        msg.messageId = readUint16(data + offset); offset += 2;
        msg.timestamp = readUint32(data + offset); offset += 4;
        memcpy(msg.signature, data + offset, 32); offset += 32;

        uint16_t dataLen = readUint16(data + offset); offset += 2;
        msg.data.assign(data + offset, data + offset + dataLen);

        return msg;
    }
};
```

### 2.3 时间戳机制

```
┌─────────────────────────────────────────────────────────────┐
│                    时间戳防重放                               │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  原理:                                                     │
│  ┌─────────────────────────────────────────────────┐       │
│  │  每条消息携带发送时间戳                             │       │
│  │  服务器检查时间戳是否在合理窗口内                   │       │
│  │  过期消息被拒绝                                     │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  窗口计算:                                                 │
│  ┌─────────────────────────────────────────────────┐       │
│  │  服务器时间 = 2024-01-01 10:00:00                 │       │
│  │  时间窗口 = ±5 秒                                   │       │
│  │                                                   │       │
│  │  接受范围: 09:59:55 ~ 10:00:05                    │       │
│  │                                                   │       │
│  │  消息时间戳 = 09:55:00 → 拒绝！(太旧)              │       │
│  │  消息时间戳 = 09:59:58 → 接受                      │       │
│  │  消息时间戳 = 10:00:00 → 接受                      │       │
│  │  消息时间戳 = 10:00:03 → 接受                      │       │
│  │  消息时间戳 = 10:00:10 → 拒绝！(太新，时钟偏差大)    │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  优势: 无状态、实现简单                                     │
│  劣势: 依赖时钟同步、窗口大小难以平衡                        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

```cpp
// 时间戳防重放实现

class TimestampValidator {
public:
    // 验证时间戳
    bool validateTimestamp(uint32_t messageTime) {
        uint32_t currentTime = getCurrentTime();
        int32_t timeDiff = (int32_t)(currentTime - messageTime);

        // 检查时间差是否在允许窗口内
        if (abs(timeDiff) > TIME_WINDOW_SECONDS) {
            return false;
        }

        return true;
    }

    // 带时钟偏差的时间戳验证
    bool validateTimestampWithSkew(uint32_t messageTime, uint32_t clientClockSkew) {
        uint32_t currentTime = getCurrentTime();
        uint32_t adjustedTime = messageTime + clientClockSkew;
        int32_t timeDiff = (int32_t)(currentTime - adjustedTime);

        return abs(timeDiff) <= TIME_WINDOW_SECONDS;
    }

    // 计算时钟偏差（基于 NTP 或多次消息）
    uint32_t calculateClockSkew(uint32_t clientTime, uint32_t serverTime) {
        return serverTime - clientTime;
    }

private:
    uint32_t getCurrentTime() {
        return std::time(nullptr);
    }

    static constexpr int32_t TIME_WINDOW_SECONDS = 5;
};
```

### 2.4 Nonce 机制

```
┌─────────────────────────────────────────────────────────────┐
│                    Nonce 防重放                               │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  原理:                                                     │
│  ┌─────────────────────────────────────────────────┐       │
│  │  Nonce = Number Used Once                        │       │
│  │  每条消息使用一个唯一的随机数                       │       │
│  │  服务器记录已使用的 Nonce                          │       │
│  │  重复的 Nonce 被拒绝                               │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  流程:                                                     │
│  ┌─────────────────────────────────────────────────┐       │
│  │  1. 客户端生成随机 Nonce                           │       │
│  │  2. 发送消息 {data, nonce, signature}             │       │
│  │  3. 服务器检查 Nonce 是否已使用                    │       │
│  │  4. 如果未使用，记录并处理                          │       │
│  │  5. 如果已使用，拒绝                                │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  Nonce 管理:                                               │
│  ┌─────────────────────────────────────────────────┐       │
│  │  - 使用 Bloom Filter 快速检测                     │       │
│  │  - 定期清理过期 Nonce                              │       │
│  │  - 分布式环境使用 Redis                            │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  优势: 完全防重放                                           │
│  劣势: 需要存储、有一定开销                                  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

```cpp
// Nonce 防重放实现

class NonceManager {
public:
    // 生成 Nonce
    std::string generateNonce() {
        uint8_t nonceBytes[16];
        RAND_bytes(nonceBytes, sizeof(nonceBytes));

        char nonceStr[33];
        for (int i = 0; i < 16; i++) {
            sprintf(nonceStr + i * 2, "%02x", nonceBytes[i]);
        }
        nonceStr[32] = '\0';

        return std::string(nonceStr);
    }

    // 验证并使用 Nonce
    bool useNonce(const std::string& nonce) {
        // 检查 Bloom Filter
        if (nonceBloomFilter_.contains(nonce)) {
            // 可能已使用，精确检查
            if (usedNonces_.find(nonce) != usedNonces_.end()) {
                return false; // 确认已使用
            }
        }

        // 记录 Nonce
        usedNonces_[nonce] = getCurrentTime();
        nonceBloomFilter_.add(nonce);
        nonceCount_++;

        // 定期清理
        if (nonceCount_ > CLEANUP_THRESHOLD) {
            cleanupExpiredNonces();
        }

        return true;
    }

    // 清理过期 Nonce
    void cleanupExpiredNonces() {
        uint32_t expireTime = getCurrentTime() - NONCE_EXPIRE_SECONDS;

        auto it = usedNonces_.begin();
        while (it != usedNonces_.end()) {
            if (it->second < expireTime) {
                it = usedNonces_.erase(it);
                nonceCount_--;
            } else {
                ++it;
            }
        }

        // 重建 Bloom Filter
        nonceBloomFilter_.clear();
        for (const auto& [nonce, time] : usedNonces_) {
            nonceBloomFilter_.add(nonce);
        }
    }

private:
    std::unordered_map<std::string, uint32_t> usedNonces_;
    BloomFilter nonceBloomFilter_;
    size_t nonceCount_ = 0;

    static constexpr uint32_t NONCE_EXPIRE_SECONDS = 300; // 5分钟
    static constexpr size_t CLEANUP_THRESHOLD = 10000;

    uint32_t getCurrentTime() {
        return std::time(nullptr);
    }

    // 简化的 Bloom Filter
    class BloomFilter {
    public:
        BloomFilter(size_t size = 10000, size_t hashes = 3)
            : bits_(size), hashCount_(hashes) {}

        void add(const std::string& key) {
            for (size_t i = 0; i < hashCount_; i++) {
                size_t index = hash(key, i) % bits_.size();
                bits_[index] = true;
            }
        }

        bool contains(const std::string& key) const {
            for (size_t i = 0; i < hashCount_; i++) {
                size_t index = hash(key, i) % bits_.size();
                if (!bits_[index]) {
                    return false;
                }
            }
            return true;
        }

        void clear() {
            std::fill(bits_.begin(), bits_.end(), false);
        }

    private:
        std::vector<bool> bits_;
        size_t hashCount_;

        size_t hash(const std::string& key, size_t seed) const {
            size_t h = seed;
            for (char c : key) {
                h = h * 31 + c;
            }
            return h;
        }
    };
};
```

---

## 三、综合防重放方案

### 3.1 组合机制

```
┌─────────────────────────────────────────────────────────────┐
│              组合防重放方案 (推荐)                             │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  消息格式:                                                  │
│  ┌─────────────────────────────────────────────────┐       │
│  │  struct SecureMessage {                          │       │
│  │      uint32_t sequence;      // 序列号           │       │
│  │      uint32_t timestamp;     // 时间戳           │       │
│  │      uint8_t  nonce[16];     // Nonce            │       │
│  │      uint16_t messageId;     // 消息 ID          │       │
│  │      uint8_t  signature[32];// 签名              │       │
│  │      uint8_t  data[];        // 消息数据         │       │
│  │  };                                                │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  验证流程:                                                  │
│  ┌─────────────────────────────────────────────────┐       │
│  │  1. 验证签名 (防止篡改)                            │       │
│  │  2. 检查时间戳 (拒绝过期消息)                       │       │
│  │  3. 检查序列号 (拒绝重复/乱序)                      │       │
│  │  4. 检查 Nonce (双重保险)                          │       │
│  │  5. 全部通过则处理消息                             │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  各层作用:                                                  │
│  ┌─────────────────────────────────────────────────┐       │
│  │  签名     → 防止消息被篡改                          │       │
│  │  时间戳   → 拒绝明显过期的消息                      │       │
│  │  序列号   → 防止短期重放、检测乱序                  │       │
│  │  Nonce    → 防止长期重放                           │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 完整实现

```cpp
// 完整的防重放消息处理器

class SecureMessageHandler {
public:
    // 发送安全消息
    void sendSecureMessage(uint16_t messageId, const std::vector<uint8_t>& data) {
        SecureMessage msg;
        msg.sequence = sequenceManager_.nextSequence();
        msg.timestamp = getCurrentTime();
        msg.messageId = messageId;
        msg.data = data;

        // 生成 Nonce
        msg.nonce = nonceManager_.generateNonce();

        // 序列化并签名
        auto serialized = msg.serializeWithoutSignature();
        sign(serialized, msg.signature);

        // 发送
        network_.send(msg.serialize());
    }

    // 接收并验证消息
    bool onReceiveMessage(const uint8_t* data, size_t len) {
        SecureMessage msg;
        if (!msg.deserialize(data, len)) {
            return false;
        }

        // 1. 验证签名
        if (!verifySignature(msg)) {
            LOG_WARNING("Invalid signature");
            return false;
        }

        // 2. 验证时间戳
        if (!timestampValidator_.validateTimestamp(msg.timestamp)) {
            LOG_WARNING("Invalid timestamp");
            return false;
        }

        // 3. 验证序列号
        if (!sequenceManager_.validateSequence(msg.sequence)) {
            LOG_WARNING("Invalid sequence number");
            return false;
        }

        // 4. 验证 Nonce
        if (!nonceManager_.useNonce(msg.nonce)) {
            LOG_WARNING("Duplicate nonce");
            return false;
        }

        // 5. 处理消息
        handleMessage(msg.messageId, msg.data);
        return true;
    }

private:
    struct SecureMessage {
        uint32_t sequence;
        uint32_t timestamp;
        std::string nonce;      // 16 字节 hex
        uint16_t messageId;
        uint8_t signature[32];
        std::vector<uint8_t> data;

        std::vector<uint8_t> serializeWithoutSignature() const {
            std::vector<uint8_t> buffer;
            appendUint32(buffer, sequence);
            appendUint32(buffer, timestamp);
            appendString(buffer, nonce);
            appendUint16(buffer, messageId);
            buffer.insert(buffer.end(), data.begin(), data.end());
            return buffer;
        }

        std::vector<uint8_t> serialize() const {
            auto buffer = serializeWithoutSignature();
            buffer.insert(buffer.end(), signature, signature + 32);
            return buffer;
        }

        bool deserialize(const uint8_t* data, size_t len) {
            size_t offset = 0;

            sequence = readUint32(data + offset); offset += 4;
            timestamp = readUint32(data + offset); offset += 4;

            size_t nonceLen = 32; // 16 字节 = 32 hex
            nonce.assign((char*)data + offset, nonceLen);
            offset += nonceLen;

            messageId = readUint16(data + offset); offset += 2;

            memcpy(signature, data + offset, 32); offset += 32;

            size_t dataLen = len - offset;
            data.assign(data + offset, data + offset + dataLen);

            return true;
        }
    };

    SequenceNumberManager sequenceManager_;
    TimestampValidator timestampValidator_;
    NonceManager nonceManager_;
    Network& network_;

    void sign(const std::vector<uint8_t>& data, uint8_t* signature) {
        // 使用 HMAC-SHA256 或 Ed25519
        HMAC(EVP_sha256(), secretKey_.data(), secretKey_.size(),
             data.data(), data.size(), signature, nullptr);
    }

    bool verifySignature(SecureMessage& msg) {
        auto data = msg.serializeWithoutSignature();
        uint8_t computedSig[32];

        HMAC(EVP_sha256(), secretKey_.data(), secretKey_.size(),
             data.data(), data.size(), computedSig, nullptr);

        return memcmp(computedSig, msg.signature, 32) == 0;
    }

    std::vector<uint8_t> secretKey_;
};
```

---

## 四、KBEngine 安全措施

### 4.1 KBEngine 消息加密

```cpp
// KBEngine 消息加密机制
// src/lib/network/bundle.h

namespace KBEngine {

class Bundle : public MemoryStream {
public:
    // 消息封装时添加加密
    void newMessage(MessageID msgID) {
        (*this) << msgID;

        // 如果启用了加密，添加加密标记
        if (pChannel_->isEncrypted()) {
            (*this) << (uint8)1;
            encrypt();
        } else {
            (*this) << (uint8)0;
        }
    }

    // 加密数据
    void encrypt() {
        if (pChannel_->encryptionType() == ENCRYPTION_TYPE_AES) {
            // AES 加密
            aesEncrypt();
        } else if (pChannel_->encryptionType() == ENCRYPTION_TYPE_BLOWFISH) {
            // Blowfish 加密 (KBEngine 默认)
            blowfishEncrypt();
        }
    }

private:
    Channel* pChannel_;

    void blowfishEncrypt() {
        // 使用 Blowfish 加密
        const uint8_t* key = pChannel_->encryptionKey();
        uint32_t keyLen = pChannel_->encryptionKeyLength();

        // KBEngine 使用 Blowfish 进行消息加密
        // src/lib/network/blowfish_box.h
        blowfish_box_encrypt(
            data() + wpos(),
            size() - wpos(),
            key,
            keyLen
        );
    }
};

} // namespace KBEngine
```

### 4.2 KBEngine 登录安全

```cpp
// KBEngine 登录防重放
// src/server/loginapp/loginapp_interface.cpp

namespace KBEngine {

class LoginApp {
public:
    // 登录请求处理
    void login(const std::string& accountName,
               const std::string& password,
               const std::string& datas) {

        // 1. 检查是否已有活跃会话
        if (hasActiveSession(accountName)) {
            // 防止重复登录
            sendLoginError(accountName, "Already logged in");
            return;
        }

        // 2. 验证密码
        if (!verifyPassword(accountName, password)) {
            sendLoginError(accountName, "Invalid password");
            return;
        }

        // 3. 创建登录令牌 (带时间戳和随机数)
        std::string token = generateLoginToken(accountName);

        // 4. 记录登录信息
        recordLogin(accountName, token);

        // 5. 返回令牌
        sendLoginSuccess(accountName, token);
    }

private:
    std::string generateLoginToken(const std::string& accountName) {
        // 令牌格式: account:timestamp:random:signature
        uint32_t timestamp = time(nullptr);
        uint8_t random[16];
        RAND_bytes(random, sizeof(random));

        std::string tokenData = accountName + ":" +
                               std::to_string(timestamp) + ":" +
                               hexEncode(random, 16);

        // 签名
        std::string signature = hmacSha256(tokenData, secretKey_);

        return tokenData + ":" + signature;
    }

    bool verifyLoginToken(const std::string& token) {
        // 1. 解析 token
        auto parts = split(token, ':');
        if (parts.size() != 4) return false;

        std::string account = parts[0];
        uint32_t timestamp = std::stoul(parts[1]);
        std::string random = parts[2];
        std::string signature = parts[3];

        // 2. 检查时间戳 (5分钟窗口)
        uint32_t currentTime = time(nullptr);
        if (abs((int32_t)(currentTime - timestamp)) > 300) {
            return false;
        }

        // 3. 验证签名
        std::string tokenData = account + ":" + parts[1] + ":" + random;
        std::string computedSig = hmacSha256(tokenData, secretKey_);

        return computedSig == signature;
    }

    std::string secretKey_;
};

} // namespace KBEngine
```

---

## 五、特殊场景处理

### 5.1 关键操作幂等性

```
┌─────────────────────────────────────────────────────────────┐
│                 关键操作幂等性设计                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  原则：即使消息被重放，也不应产生负面影响                      │
│                                                             │
│  幂等操作设计:                                              │
│  ┌─────────────────────────────────────────────────┐       │
│  │  1. 为每个关键操作分配唯一 ID                      │       │
│  │  2. 服务器记录已处理的操作 ID                       │       │
│  │  3. 重复 ID 的操作只执行一次                        │       │
│  │  4. 返回之前的结果                                  │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  示例：购买道具                                             │
│  ┌─────────────────────────────────────────────────┐       │
│  │  客户端请求:                                       │       │
│  │  {                                                │       │
│  │    "messageId": "buyItem",                       │       │
│  │    "operationId": "uuid-12345",                  │       │
│  │    "itemId": 1001,                               │       │
│  │    "count": 1                                    │       │
│  │  }                                                │       │
│  │                                                   │       │
│  │  服务器处理:                                       │       │
│  │  1. 检查 operationId 是否已处理                     │       │
│  │  2. 如果已处理，返回之前的结果                       │       │
│  │  3. 如果未处理，执行购买                             │       │
│  │  4. 记录 operationId                               │       │
│  │  5. 返回结果                                        │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

```cpp
// 幂等操作处理器

template<typename Result>
class IdempotentOperation {
public:
    struct Operation {
        std::string operationId;
        std::string request;
        Result result;
        uint32_t timestamp;
    };

    // 执行或获取操作结果
    Result execute(const std::string& operationId,
                   const std::string& request,
                   std::function<Result()> handler) {

        // 检查是否已处理
        auto it = operations_.find(operationId);
        if (it != operations_.end()) {
            LOG_INFO("Returning cached result for operation: " + operationId);
            return it->second.result;
        }

        // 执行操作
        Result result = handler();

        // 记录结果
        Operation op;
        op.operationId = operationId;
        op.request = request;
        op.result = result;
        op.timestamp = getCurrentTime();

        operations_[operationId] = op;

        // 清理过期记录
        if (operations_.size() > CLEANUP_THRESHOLD) {
            cleanupExpiredOperations();
        }

        return result;
    }

private:
    std::unordered_map<std::string, Operation> operations_;

    void cleanupExpiredOperations() {
        uint32_t expireTime = getCurrentTime() - OPERATION_EXPIRE_SECONDS;

        auto it = operations_.begin();
        while (it != operations_.end()) {
            if (it->second.timestamp < expireTime) {
                it = operations_.erase(it);
            } else {
                ++it;
            }
        }
    }

    static constexpr uint32_t OPERATION_EXPIRE_SECONDS = 3600; // 1小时
    static constexpr size_t CLEANUP_THRESHOLD = 10000;
};

// 使用示例
class ShopService {
public:
    BuyResult buyItem(const std::string& playerId,
                      uint32_t itemId,
                      uint32_t count,
                      const std::string& operationId) {

        return idempotentBuy_.execute(operationId,
            "buyItem:" + std::to_string(itemId),
            [&]() {
                // 实际购买逻辑
                Player* player = getPlayer(playerId);
                if (!player) return BuyResult::PlayerNotFound;

                if (player->getGold() < getItemPrice(itemId)) {
                    return BuyResult::InsufficientGold;
                }

                player->deductGold(getItemPrice(itemId));
                player->addItem(itemId, count);

                return BuyResult::Success;
            });
    }

private:
    IdempotentOperation<BuyResult> idempotentBuy_;
};
```

---

## 六、最佳实践

### 6.1 防重放设计原则

```
┌─────────────────────────────────────────────────────────────┐
│                  防重放设计原则                               │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 深度防御                                                │
│     ├── 多层验证 (签名 + 时间戳 + 序列号 + Nonce)            │
│     ├── 不同场景使用不同策略                                  │
│     └── 任何一层失败都拒绝消息                               │
│                                                             │
│  2. 权威服务器                                              │
│     ├── 所有关键操作在服务器验证                              │
│     ├── 客户端只发送请求                                     │
│     └── 服务器计算所有结果                                   │
│                                                             │
│  3. 幂等设计                                                │
│     ├── 关键操作设计为幂等                                   │
│     ├── 使用唯一操作 ID                                      │
│     └── 重复请求返回相同结果                                  │
│                                                             │
│  4. 合理窗口                                                │
│     ├── 时间窗口: 根据网络延迟调整                            │
│     ├── 序列号窗口: 允许一定乱序                              │
│     └── Nonce 过期: 平衡安全和存储                            │
│                                                             │
│  5. 监控告警                                                │
│     ├── 记录重放攻击尝试                                      │
│     ├── 异常模式检测                                          │
│     └── 及时响应处理                                         │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 6.2 不同场景的策略

| 场景 | 推荐策略 | 理由 |
|------|----------|------|
| **登录** | 时间戳 + Nonce + 签名 | 防止盗号、会话劫持 |
| **战斗操作** | 序列号 + 时间戳 | 高频、低延迟要求 |
| **交易** | 幂等 ID + Nonce + 签名 | 涉及资产、高安全 |
| **聊天** | 时间戳 + 频率限制 | 防刷屏、低安全要求 |
| **移动** | 序列号 | 纯粹的防重放 |
| **道具使用** | 幂等 ID + 签名 | 防刷道具 |

---

## 七、总结

### 防重放技术对比

| 技术 | 优势 | 劣势 | 适用场景 |
|------|------|------|----------|
| **序列号** | 简单、高效 | 需要状态、掉包敏感 | 实时操作 |
| **时间戳** | 无状态 | 时钟依赖 | 通用场景 |
| **Nonce** | 完全防重放 | 需要存储 | 关键操作 |
| **签名** | 防篡改 | 计算开销 | 所有场景 |
| **幂等 ID** | 业务安全 | 需要业务配合 | 交易、消费 |

### KBEngine 安全机制

```
KBEngine 的安全措施:
1. Blowfish 消息加密
2. 登录令牌验证
3. 会话管理
4. 账号绑定连接

开发者需要:
1. 实现业务层幂等性
2. 添加防外挂验证
3. 监控异常行为
4. 定期更新密钥
```

---

## 参考资料

- [KBEngine GitHub - Bundle 加密](https://github.com/kbengine/kbengine/blob/master/kbe/src/lib/network/bundle.h)
- [OWASP 防重放攻击指南](https://owasp.org/www-community/attacks/Replay_Attack)
- [网络安全最佳实践](https://www.cisecurity.org/controls/)
