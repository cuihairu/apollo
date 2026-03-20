# Q90: 如何应对 DDoS 攻击？

## 问题分析

本题考察对 DDoS 防护的理解：
- DDoS 攻击类型
- 防护策略
- 流量清洗
- 游戏服务器防护

---

## 一、DDoS 类型

### 1.1 攻击分类

```
┌─────────────────────────────────────────────────────────────┐
│                    DDoS 攻击类型                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 容量攻击                                                │
│  ├── UDP Flood                                             │
│  ├── ICMP Flood                                            │
│  └── 目标: 耗尽带宽                                        │
│                                                             │
│  2. 协议攻击                                                │
│  ├── SYN Flood                                             │
│  ├── ACK Flood                                             │
│  └── 目标: 耗尽连接资源                                    │
│                                                             │
│  3. 应用攻击                                                │
│  ├── HTTP Flood                                            │
│  ├── 慢速 POST                                             │
│  └── 目标: 耗尽应用资源                                    │
│                                                             │
│  4. 游戏攻击                                                │
│  ├── 登录 Flood                                            │
│  ├── 假人攻击                                               │
│  └── 目标: 逻辑漏洞                                        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、防护策略

### 2.1 多层防护

```cpp
// DDoS 防护架构

class DDoSProtection {
public:
    // 1. 接入层防护
    bool checkIP(const std::string& ip) {
        // 检查黑名单
        if (isBlacklisted(ip)) {
            return false;
        }

        // 检查白名单
        if (isWhitelisted(ip)) {
            return true;
        }

        // 检查威胁情报
        if (isThreatIP(ip)) {
            addToGraylist(ip);
            return false;
        }

        return true;
    }

    // 2. 速率限制
    bool checkRateLimit(const std::string& ip) {
        auto& counter = ipCounters_[ip];

        uint64_t now = getCurrentTime();

        // 滑动窗口
        while (!counter.slots.empty() && counter.slots.front() < now - 60000) {
            counter.slots.pop_front();
        }

        // 1秒内最多 100 个请求
        if (counter.slots.size() > 100) {
            // 超限，升级防御
            if (counter.slots.size() > 500) {
                // 严重超限，加入黑名单
                blacklistIP(ip, 3600);
            }
            return false;
        }

        counter.slots.push_back(now);
        return true;
    }

    // 3. 连接限制
    bool checkConnectionLimit(const std::string& ip) {
        auto& info = ipInfo_[ip];

        // 单 IP 连接数限制
        if (info.connections > 100) {
            return false;
        }

        // 新连接速率限制
        uint64_t now = getCurrentTime();
        if (now - info.lastConnect < 100) {  // 100ms 内
            info.rapidCount++;

            if (info.rapidCount > 50) {
                return false;
            }
        } else {
            info.rapidCount = 0;
        }

        info.lastConnect = now;
        return true;
    }

    // 4. 挑战验证
    void challengeClient(const std::string& ip) {
        // 发送 JS 挑战
        sendChallenge(ip, generateChallenge());

        // 等待响应
        // 验证通过才放行
    }

private:
    struct IPCounter {
        std::deque<uint64_t> slots;
    };

    struct IPInfo {
        int connections = 0;
        uint64_t lastConnect = 0;
        int rapidCount = 0;
    };

    std::unordered_map<std::string, IPCounter> ipCounters_;
    std::unordered_map<std::string, IPInfo> ipInfo_;
};
```

---

## 三、游戏服务器防护

### 3.1 登录保护

```cpp
// 游戏登录 DDoS 防护

class LoginProtection {
public:
    // 处理登录请求
    LoginResult handleLogin(const std::string& account,
                            const std::string& ip,
                            const std::string& fingerprint) {
        // 1. 检查 IP 信誉
        if (!checkIPReputation(ip)) {
            // 需要验证码
            return LoginResult::NEED_CAPTCHA;
        }

        // 2. 检查账号状态
        if (isAccountLocked(account)) {
            return LoginResult::ACCOUNT_LOCKED;
        }

        // 3. 检查登录频率
        if (!checkLoginRate(account, ip)) {
            return LoginResult::TOO_FREQUENT;
        }

        // 4. 验证指纹 (防止多开)
        if (!checkFingerprint(account, fingerprint)) {
            return LoginResult::FINGERPRINT_MISMATCH;
        }

        // 5. 执行登录
        return performLogin(account, ip);
    }

private:
    bool checkLoginRate(const std::string& account, const std::string& ip) {
        // 检查账号级别限制
        auto& accountCounter = loginCounters_[account];

        uint64_t now = getCurrentTime();
        uint64_t windowStart = now - 300000;  // 5分钟窗口

        // 清理过期记录
        while (!accountCounter.attempts.empty() &&
               accountCounter.attempts.front().timestamp < windowStart) {
            accountCounter.attempts.pop_front();
        }

        // 5分钟内最多 10 次失败
        int failCount = 0;
        for (const auto& attempt : accountCounter.attempts) {
            if (!attempt.success) {
                failCount++;
            }
        }

        if (failCount > 10) {
            // 账号临时锁定
            lockAccount(account, 300);
            return false;
        }

        return true;
    }

    struct LoginAttempt {
        uint64_t timestamp;
        bool success;
    };

    struct AccountCounter {
        std::deque<LoginAttempt> attempts;
    };

    std::unordered_map<std::string, AccountCounter> loginCounters_;
};
```

---

## 四、最佳实践

### 4.1 防护层次

```
DDoS 防护 = 多层防御 + 流量清洗 + 游戏层防护
- 接入层过滤
- CDN 分流
- 游戏层限流
- 应急预案
```

---

## 五、总结

### DDoS 防护核心

```
防 DDoS = 预防 + 检测 + 响应 + 恢复
- 接入层防护
- 速率限制
- 挑战验证
- 专业服务 (Cloudflare 等)
```

---

## 参考资料

- [DDoS Protection Best Practices](https://www.cloudflare.com/learning/ddos/)
- [Game DDoS Protection](https://www.akamai.com/)
