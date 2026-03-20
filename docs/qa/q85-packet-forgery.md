# Q85: 如何防止封包伪造？

## 问题分析

本题考察对网络包伪造防护的理解：
- 封包伪造原理
- 消息认证
- 序列号机制
- 重放攻击防护

---

## 一、防护策略

### 1.1 消息签名

```cpp
// 消息签名认证

class MessageAuthenticator {
public:
    // 添加签名到消息
    void signMessage(Message& msg, const std::string& key) {
        // 计算消息内容的 HMAC
        std::string signature = calculateHMAC(msg.data, msg.size, key);
        msg.signature = signature;
    }

    // 验证签名
    bool verifyMessage(const Message& msg, const std::string& key) {
        std::string expected = calculateHMAC(msg.data, msg.size, key);
        return hmacCompare(msg.signature, expected);
    }

    // 处理客户端消息
    bool handleClientMessage(Player* player, const Message& msg) {
        // 1. 验证签名
        std::string key = getPlayerKey(player->getId());
        if (!verifyMessage(msg, key)) {
            logSuspicious(player->getId(), "Invalid message signature");
            return false;
        }

        // 2. 检查时间戳
        if (!validateTimestamp(msg.timestamp)) {
            return false;
        }

        // 3. 处理消息
        return processMessage(player, msg);
    }

private:
    std::string calculateHMAC(const void* data, size_t size,
                               const std::string& key) {
        unsigned char* digest;
        unsigned int digest_len;

        digest = HMAC(
            EVP_sha256(),
            (unsigned char*)key.data(), key.size(),
            (unsigned char*)data, size,
            nullptr, &digest_len
        );

        return std::string((char*)digest, digest_len);
    }

    bool hmacCompare(const std::string& a, const std::string& b) {
        // 恒定时间比较，防止时序攻击
        if (a.size() != b.size()) return false;

        volatile int result = 0;
        for (size_t i = 0; i < a.size(); ++i) {
            result |= a[i] ^ b[i];
        }

        return result == 0;
    }
};
```

### 1.2 序列号机制

```cpp
// 序列号防重放

class SequenceManager {
public:
    // 检查序列号
    bool checkSequence(uint64_t playerId, uint64_t sequence) {
        PlayerSeqInfo& info = playerSeqInfo_[playerId];

        // 拒绝旧序列号
        if (sequence <= info.lastSequence) {
            if (sequence > info.lastSequence - 100) {
                // 在合理范围内，可能是乱序
                info.pendingSequences.insert(sequence);
            } else {
                // 太旧的序列号，可能是重放攻击
                logSuspicious(playerId, "Old sequence number");
                return false;
            }
        }

        // 检查是否在待处理列表中
        if (info.pendingSequences.count(sequence)) {
            // 已处理过
            return false;
        }

        // 更新序列号
        if (sequence == info.lastSequence + 1) {
            info.lastSequence = sequence;

            // 处理待处理的序列号
            while (info.pendingSequences.count(info.lastSequence + 1)) {
                info.lastSequence++;
                info.pendingSequences.erase(info.lastSequence);
            }
        }

        return true;
    }

private:
    struct PlayerSeqInfo {
        uint64_t lastSequence = 0;
        std::set<uint64_t> pendingSequences;  // 乱序消息
    };

    std::unordered_map<uint64_t, PlayerSeqInfo> playerSeqInfo_;
};
```

---

## 二、加密传输

### 2.1 消息加密

```cpp
// 加密消息传输

class EncryptedProtocol {
public:
    // 加密消息
    std::string encryptMessage(const Message& msg) {
        // 序列化
        std::string data = serialize(msg);

        // AES 加密
        std::string encrypted = aesEncrypt(data, sessionKey_);

        // 添加 IV
        std::string iv = generateIV();
        std::string result = iv + encrypted;

        return result;
    }

    // 解密消息
    bool decryptMessage(const std::string& data, Message& msg) {
        if (data.size() < AES_BLOCK_SIZE) {
            return false;
        }

        // 提取 IV
        std::string iv = data.substr(0, AES_BLOCK_SIZE);
        std::string encrypted = data.substr(AES_BLOCK_SIZE);

        // AES 解密
        std::string decrypted = aesDecrypt(encrypted, iv, sessionKey_);

        // 反序列化
        return deserialize(decrypted, msg);
    }

private:
    std::string sessionKey_;  // 会话密钥
};
```

---

## 三、最佳实践

### 3.1 防护层次

```
封包伪造防护 = 消息签名 + 序列号 + 加密 + 限流
- 每个消息带签名
- 序列号防重放
- 关键数据加密
- 速率限制
```

---

## 四、参考资料

- [Network Security Best Practices](https://owasp.org/)
- [HMAC Wikipedia](https://en.wikipedia.org/wiki/HMAC)
