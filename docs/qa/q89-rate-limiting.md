# Q89: 如何设计限流和防刷机制？

## 问题分析

本题考察对限流防刷的理解：
- 限流算法
- 防刷策略
- 令牌桶
- 漏桶算法

---

## 一、限流算法

### 1.1 令牌桶

```cpp
// 令牌桶算法

class TokenBucketRateLimiter {
public:
    TokenBucketRateLimiter(size_t capacity, size_t refillRate)
        : capacity_(capacity), refillRate_(refillRate),
          tokens_(capacity), lastRefill_(getCurrentTime()) {}

    bool allowRequest(int tokens = 1) {
        uint64_t now = getCurrentTime();

        // 补充令牌
        refill(now);

        // 检查是否有足够令牌
        if (tokens_ >= tokens) {
            tokens_ -= tokens;
            return true;
        }

        return false;
    }

private:
    void refill(uint64_t now) {
        uint64_t elapsed = now - lastRefill_;
        uint64_t tokensToAdd = (elapsed * refillRate_) / 1000;

        tokens_ = std::min(capacity_, tokens_ + tokensToAdd);
        lastRefill_ = now;
    }

    size_t capacity_;    // 桶容量
    size_t refillRate_;  // 补充速率 (tokens/second)
    size_t tokens_;      // 当前令牌数
    uint64_t lastRefill_;
};
```

---

## 二、应用场景

### 2.1 多级限流

```cpp
// 多级限流系统

class RateLimiter {
public:
    enum class LimitResult {
        ALLOW,
        WARN,
        DENY
    };

    LimitResult check(uint64_t playerId, const std::string& action) {
        // 检查个人限制
        if (!checkPlayerLimit(playerId, action)) {
            return LimitResult::DENY;
        }

        // 检查 IP 限制
        std::string ip = getPlayerIP(playerId);
        if (!checkIPLimit(ip, action)) {
            return LimitResult::DENY;
        }

        // 检查全局限制
        if (!checkGlobalLimit(action)) {
            return LimitResult::WARN;
        }

        return LimitResult::ALLOW;
    }

private:
    bool checkPlayerLimit(uint64_t playerId, const std::string& action) {
        PlayerLimits& limits = playerLimits_[playerId];

        uint64_t now = getCurrentTime();
        uint64_t window = 60000;  // 1分钟窗口

        // 滑动窗口计数
        limits.counter++;
        limits.windowStart = now;

        // 1分钟内最多 100 次
        if (limits.counter > 100) {
            return false;
        }

        return true;
    }

    struct PlayerLimits {
        uint64_t counter = 0;
        uint64_t windowStart = 0;
    };

    std::unordered_map<uint64_t, PlayerLimits> playerLimits_;
};
```

---

## 三、总结

### 限流防刷核心

```
限流防刷 = 令牌桶 + 滑动窗口 + 多级防护
- 令牌桶算法
- 滑动窗口计数
- 个人/IP/全局三级
- 超限返回错误
```

---

## 参考资料

- [Rate Limiting Algorithms](https://konghq.com/blog/how-to-design-a-scalable-rate-limiting-service/)
- [Token Bucket Wikipedia](https://en.wikipedia.org/wiki/Token_bucket)
