# Q87: 敏感数据如何加密传输？

## 问题分析

本题考察对加密传输的理解：
- 加密算法选择
- 密钥交换
- TLS/SSL
- 游戏协议加密

---

## 一、加密基础

### 1.1 加密层次

```
┌─────────────────────────────────────────────────────────────┐
│                    加密层次                                    │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  传输层: TLS/SSL                                           │
│  ├── 握手协商密钥                                           │
│  ├── 加密通道传输                                           │
│  └── 证书验证身份                                           │
│                                                             │
│  应用层: 自定义加密                                         │
│  ├── 消息加密                                               │
│  ├── 签名验证                                               │
│  └── 密钥轮换                                               │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、TLS 实现

### 2.1 TLS 握手

```cpp
// TLS 握手处理

class TLSServer {
public:
    void onConnection(int fd) {
        // TLS 握手
        SSL* ssl = SSL_new(ctx_);
        SSL_set_fd(ssl, fd);

        if (SSL_accept(ssl) <= 0) {
            SSL_free(ssl);
            close(fd);
            return;
        }

        // 握手成功
        connections_[fd] = ssl;
    }

    void sendMessage(int fd, const std::string& msg) {
        auto it = connections_.find(fd);
        if (it == connections_.end()) return;

        SSL* ssl = it->second;

        int written = SSL_write(ssl, msg.data(), msg.size());
        if (written <= 0) {
            // 错误处理
        }
    }

    std::string receiveMessage(int fd) {
        auto it = connections_.find(fd);
        if (it == connections_.end()) return "";

        SSL* ssl = it->second;

        char buffer[4096];
        int bytesRead = SSL_read(ssl, buffer, sizeof(buffer));

        if (bytesRead > 0) {
            return std::string(buffer, bytesRead);
        }

        return "";
    }

private:
    SSL_CTX* ctx_;
    std::unordered_map<int, SSL*> connections_;
};
```

---

## 三、游戏协议加密

### 3.1 自定义加密

```cpp
// 游戏协议加密

class GameProtocol {
public:
    // 加密消息
    std::string encryptMessage(const std::string& plaintext) {
        // 1. 生成 IV
        std::string iv = generateRandomIV();

        // 2. AES 加密
        std::string ciphertext = aesEncrypt(plaintext, sessionKey_, iv);

        // 3. HMAC 签名
        std::string signature = hmacSign(ciphertext, sessionKey_);

        // 4. 组装: IV(16) + 签名(32) + 密文
        return iv + signature + ciphertext;
    }

    // 解密消息
    bool decryptMessage(const std::string& encrypted, std::string& plaintext) {
        if (encrypted.size() < 48) return false;  // 16+32 最小

        // 1. 提取 IV
        std::string iv = encrypted.substr(0, 16);

        // 2. 提取签名
        std::string signature = encrypted.substr(16, 32);

        // 3. 提取密文
        std::string ciphertext = encrypted.substr(48);

        // 4. 验证签名
        std::string expectedSig = hmacSign(ciphertext, sessionKey_);
        if (!hmacCompare(signature, expectedSig)) {
            return false;
        }

        // 5. 解密
        plaintext = aesDecrypt(ciphertext, sessionKey_, iv);

        return true;
    }

    // 密钥交换
    void performKeyExchange(int fd) {
        // ECDH 密钥交换
        // 1. 生成密钥对
        EVP_PKEY* key = generateECDHKey();

        // 2. 发送公钥
        sendPublicKey(fd, key);

        // 3. 接收对方公钥
        EVP_PKEY* peerKey = receivePeerPublicKey(fd);

        // 4. 派生共享密钥
        deriveSessionKey(key, peerKey);
    }

private:
    std::string sessionKey_;
};
```

---

## 四、最佳实践

### 4.1 加密建议

```
敏感数据传输 = TLS + 应用层加密 + 密钥轮换
- 使用 TLS/SSL
- 定期更换密钥
- 消息签名验证
- 前向保密
```

---

## 五、总结

### 加密传输核心

```
加密传输 = 标准协议 + 密钥管理 + 完整性验证
- 优先使用 TLS
- 强加密算法
- 安全密钥交换
- 定期更新密钥
```

---

## 参考资料

- [OpenSSL Documentation](https://www.openssl.org/)
- [TLS 1.3 RFC](https://datatracker.ietf.org/doc/html/rfc8446)
