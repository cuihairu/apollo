# Q37: 如何实现数据的缓存淘汰策略？

## 问题分析

本题考察对缓存淘汰的理解：
- 缓存淘汰算法
- LRU/LFU 实现
- Redis 淘汰策略
| 淘汰策略 | 说明 |
|--------|------|
| **noeviction** | 不淘汰，内存满时报错 |
| **allkeys-lru** | 所有键的 LRU |
| **volatile-lru** | 设置了 TTL 的键的 LRU |
| **allkeys-lfu** | 所有键的 LFU |
| **volatile-lfu** | 设置了 TTL 的键的 LFU |

---

## 一、淘汰算法

### 1.1 LRU 实现

```cpp
// LRU 缓存实现

template<typename K, typename V>
class LRUCache {
public:
    LRUCache(size_t capacity) : capacity_(capacity) {}

    void put(const K& key, const V& value) {
        auto it = cache_.find(key);

        if (it != cache_.end()) {
            // 更新现有值
            it->second.second = value;
            // 移到最前面
            lruList_.splice(lruList_.begin(), lruList_, it->second.first);
        } else {
            // 检查容量
            if (cache_.size() >= capacity_) {
                // 淘汰最久未使用
                auto last = lruList_.back();
                cache_.erase(last->first);
                lruList_.pop_back();
            }

            // 添加新项
            lruList_.push_front(key);
            cache_[key] = {lruList_.begin(), value};
        }
    }

    V* get(const K& key) {
        auto it = cache_.find(key);
        if (it == cache_.end()) {
            return nullptr;
        }

        // 移到最前面（最近使用）
        lruList_.splice(lruList_.begin(), lruList_, it->second.first);

        return &it->second.second;
    }

private:
    size_t capacity_;
    std::list<K> lruList_;
    std::unordered_map<K, std::pair<typename std::list<K>::iterator, V>> cache_;
};
```

### 1.2 LFU 实现

```cpp
// LFU 缓存实现

template<typename K, typename V>
class LFUCache {
public:
    LFUCache(size_t capacity) : capacity_(capacity) {}

    void put(const K& key, const V& value) {
        // 增加访问频率
        accessCount_[key]++;

        if (cache_.size() >= capacity_) && cache_.find(key) == cache_.end()) {
            // 淘汰访问频率最低的
            evictLFU();
        }

        cache_[key] = value;
    }

    V* get(const K& key) {
        accessCount_[key]++;
        auto it = cache_.find(key);
        return it != cache_.end() ? &it->second : nullptr;
    }

private:
    void evictLFU() {
        K minKey;
        size_t minCount = SIZE_MAX;

        for (const auto& [key, count] : accessCount_) {
            if (count < minCount) {
                minCount = count;
                minKey = key;
            }
        }

        cache_.erase(minKey);
        accessCount_.erase(minKey);
    }

    size_t capacity_;
    std::unordered_map<K, V> cache_;
    std::unordered_map<K, size_t> accessCount_;
};
```

---

## 二、Redis 淘汰策略

```
maxmemory-policy 配置:

# 内存使用达到上限时淘汰
volatile-lru → 淘汰设置了 TTL 的键中很少使用的
allkeys-lru → 淘汰所有键中很少使用的
volatile-lfu → 淘汰设置了 TTL 的键中使用频率最低的
allkeys-lfu → 淘汰所有键中使用频率最低的
```

---

## 三、最佳实践

### 缓存淘汰建议

| 场景 | 策略 | TTL 设置 |
|------|------|---------|
| 热点数据 | volatile-lru | 短 TTL |
| 会话数据 | volatile-lru | 会话过期 |
| 静态资源 | allkeys-lru | 长 TTL |
| 计算结果 | volatile-lru | 短 TTL |

---

## 参考资料

- [Redis 内存淘汰策略](https://redis.io/docs/manual/eviction/)
