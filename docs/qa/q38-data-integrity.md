# Q38: 如何防止数据被篡改？

## 问题分析

本题考察对数据安全的理解：
- 数据加密存储
- 数字签名验证
- 防篡改机制
- 审计日志

---

## 一、防篡改策略

### 1.1 多层防护

```
┌─────────────────────────────────────────────────────────────┐
│                    数据防篡改策略                               │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  L1: 数据库层                                               │
│  ├── 字段加密存储                                         │
│  ├── 触发器约束                                           │
│  └── 事务日志                                             │
│                                                             │
│  L2: 应用层                                                 │
│  ├── 参数校验                                             │
│  ├── 业务规则验证                                         │
│  └── 操作日志                                             │
│                                                             │
│  L3: 传输层                                                 │
│  ├── TLS 加密通信                                         │
│  ├── 消息签名                                             │
│  └── 防重放机制                                           │
│                                                             │
│  L4: 审计层                                                 │
│  ├── 操作日志记录                                         │
│  ├── 关键操作审计                                         │
│  └── 异常检测                                             │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、加密与签名

### 2.1 数据加密

```cpp
// 敏感数据加密

class DataEncryption {
public:
    // 加密敏感字段
    std::string encrypt(const std::string& plain) {
        // AES 加密
        AES_KEY aes_key = getEncryptionKey();

        std::string encrypted = aes_encrypt(plain, aes_key);

        // Base64 编码
        return base64_encode(encrypted);
    }

    // 解密
    std::string decrypt(const std::string& cipher) {
        std::string decoded = base64_decode(cipher);

        AES_KEY aes_key = getEncryptionKey();

        return aes_decrypt(decoded, aes_key);
    }

    // 计算哈希
    std::string hash(const std::string& data) {
        return sha256(data + getSalt());
    }

private:
    std::string getSalt() {
        return "your-salt-here";
    }
};
```

### 2.2 数字签名

```cpp
// 数字签名

class DigitalSignature {
public:
    // 签名
    std::string sign(const std::string& data) {
        std::string hash = sha256(data);
        return rsaSign(hash, privateKey_);
    }

    // 验证
    bool verify(const std::string& data, const std::string& signature) {
        std::string hash = sha256(data);
        return rsaVerify(hash, signature, publicKey_);
    }

private:
    std::string privateKey_;
    std::string publicKey_;
};
```

---

## 三、审计日志

### 3.1 操作审计

```sql
-- 审计日志表

CREATE TABLE `audit_log` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `player_id` BIGINT UNSIGNED NOT NULL COMMENT '玩家ID',
    `action` VARCHAR(64) NOT NULL COMMENT '操作类型',
    `target_type` VARCHAR(32) NOT NULL COMMENT '目标类型',
    `target_id` BIGINT UNSIGNED NOT NULL COMMENT '目标ID',
    `old_value` JSON DEFAULT NULL COMMENT '变更前值',
    `new_value` JSON DEFAULT NULL COMMENT '变更后值',
    `ip` VARCHAR(64) DEFAULT NULL COMMENT '操作IP',
    `action_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '操作时间',
    PRIMARY KEY (`id`),
    KEY `idx_player_id` (`player_id`),
    KEY `idx_action_time` (`action_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='审计日志表';
```

---

## 四、最佳实践

### 防篡改措施

| 措施 | 说明 | 复杂度 |
|------|------|--------|
| **字段加密** | 敏感字段加密存储 | 中 |
| **数字签名** | 关键操作签名验证 | 高 |
| **参数校验** | 前端严格校验 | 低 |
| **操作审计** | 完整操作日志 | 中 |
| **异常检测** | 行为分析预警 | 高 |

---

## 参考资料

- [OWASP 数据完整性](https://owasp.org/www-community/attacks/Data_Tampering/)
