/**
 * @file redis_mock.h
 * @brief Redis Mock 实现（用于测试）
 */

#pragma once

#include "apollo/storage/redis/redis.h"
#include <unordered_map>
#include <functional>

namespace apollo {
namespace storage {
namespace redis {

/**
 * @brief 内存 Mock Redis 实现
 *
 * 用于单元测试，不需要真实 Redis 服务器
 */
class MockRedisClient : public IRedisClient {
public:
    MockRedisClient() = default;
    ~MockRedisClient() override = default;

    // 连接操作（Mock 总是成功）
    bool connect(const RedisConfig& config) override {
        config_ = config;
        connected_ = true;
        return true;
    }

    void disconnect() override {
        connected_ = false;
    }

    bool isConnected() const override {
        return connected_;
    }

    // ========== 字符串操作 ==========

    RedisReply set(const std::string& key, const std::string& value) override {
        data_[key] = value;
        return RedisReply::Ok();
    }

    RedisReply set(const std::string& key, const std::string& value, int64_t ttl) override {
        data_[key] = value;
        if (ttl > 0) {
            expirations_[key] = ttl;
        }
        return RedisReply::Ok();
    }

    RedisReply get(const std::string& key) override {
        auto it = data_.find(key);
        if (it != data_.end()) {
            return RedisReply::String(it->second);
        }
        return RedisReply::Nil();
    }

    RedisReply del(const std::string& key) override {
        size_t count = data_.erase(key);
        expirations_.erase(key);
        return RedisReply::Int64(static_cast<int64_t>(count));
    }

    RedisReply del(const std::vector<std::string>& keys) override {
        int64_t count = 0;
        for (const auto& key : keys) {
            count += data_.erase(key);
            expirations_.erase(key);
        }
        return RedisReply::Int64(count);
    }

    RedisReply exists(const std::string& key) override {
        return RedisReply::Int64(data_.count(key));
    }

    RedisReply incr(const std::string& key) override {
        return incrBy(key, 1);
    }

    RedisReply incrBy(const std::string& key, int64_t increment) override {
        auto it = data_.find(key);
        int64_t value = 0;
        if (it != data_.end()) {
            try { value = std::stoll(it->second); } catch (...) {}
        }
        value += increment;
        data_[key] = std::to_string(value);
        return RedisReply::Int64(value);
    }

    RedisReply decr(const std::string& key) override {
        return decrBy(key, 1);
    }

    RedisReply decrBy(const std::string& key, int64_t decrement) override {
        return incrBy(key, -decrement);
    }

    RedisReply expire(const std::string& key, int64_t seconds) override {
        if (data_.find(key) != data_.end()) {
            expirations_[key] = seconds * 1000;
            return RedisReply::Int64(1);
        }
        return RedisReply::Int64(0);
    }

    RedisReply pexpire(const std::string& key, int64_t milliseconds) override {
        if (data_.find(key) != data_.end()) {
            expirations_[key] = milliseconds;
            return RedisReply::Int64(1);
        }
        return RedisReply::Int64(0);
    }

    RedisReply ttl(const std::string& key) override {
        auto it = expirations_.find(key);
        if (it != expirations_.end()) {
            return RedisReply::Int64(it->second / 1000);
        }
        return RedisReply::Int64(-1);
    }

    RedisReply pttl(const std::string& key) override {
        auto it = expirations_.find(key);
        if (it != expirations_.end()) {
            return RedisReply::Int64(it->second);
        }
        return RedisReply::Int64(-1);
    }

    // ========== 哈希操作 ==========

    RedisReply hSet(const std::string& key, const std::string& field, const std::string& value) override {
        hashes_[key][field] = value;
        return RedisReply::Ok();
    }

    RedisReply hGet(const std::string& key, const std::string& field) override {
        auto it = hashes_.find(key);
        if (it != hashes_.end()) {
            auto fit = it->second.find(field);
            if (fit != it->second.end()) {
                return RedisReply::String(fit->second);
            }
        }
        return RedisReply::Nil();
    }

    RedisReply hGetAll(const std::string& key) override {
        auto it = hashes_.find(key);
        if (it != hashes_.end()) {
            return RedisReply::Map(it->second);
        }
        return RedisReply::Map({});
    }

    RedisReply hDel(const std::string& key, const std::string& field) override {
        auto it = hashes_.find(key);
        if (it != hashes_.end()) {
            return RedisReply::Int64(static_cast<int64_t>(it->second.erase(field)));
        }
        return RedisReply::Int64(0);
    }

    RedisReply hExists(const std::string& key, const std::string& field) override {
        auto it = hashes_.find(key);
        if (it != hashes_.end()) {
            return RedisReply::Int64(it->second.count(field));
        }
        return RedisReply::Int64(0);
    }

    RedisReply hKeys(const std::string& key) override {
        std::vector<std::string> result;
        auto it = hashes_.find(key);
        if (it != hashes_.end()) {
            for (const auto& pair : it->second) {
                result.push_back(pair.first);
            }
        }
        return RedisReply::Array(result);
    }

    RedisReply hVals(const std::string& key) override {
        std::vector<std::string> result;
        auto it = hashes_.find(key);
        if (it != hashes_.end()) {
            for (const auto& pair : it->second) {
                result.push_back(pair.second);
            }
        }
        return RedisReply::Array(result);
    }

    RedisReply hLen(const std::string& key) override {
        auto it = hashes_.find(key);
        if (it != hashes_.end()) {
            return RedisReply::Int64(static_cast<int64_t>(it->second.size()));
        }
        return RedisReply::Int64(0);
    }

    RedisReply hMSet(const std::string& key, const std::map<std::string, std::string>& values) override {
        for (const auto& pair : values) {
            hashes_[key][pair.first] = pair.second;
        }
        return RedisReply::Ok();
    }

    RedisReply hMGet(const std::string& key, const std::vector<std::string>& fields) override {
        std::vector<std::string> result;
        auto it = hashes_.find(key);
        if (it != hashes_.end()) {
            for (const auto& field : fields) {
                auto fit = it->second.find(field);
                result.push_back(fit != it->second.end() ? fit->second : "");
            }
        } else {
            result.resize(fields.size());
        }
        return RedisReply::Array(result);
    }

    RedisReply hIncrBy(const std::string& key, const std::string& field, int64_t increment) override {
        auto& hash = hashes_[key];
        auto it = hash.find(field);
        int64_t value = 0;
        if (it != hash.end()) {
            try { value = std::stoll(it->second); } catch (...) {}
        }
        value += increment;
        hash[field] = std::to_string(value);
        return RedisReply::Int64(value);
    }

    // ========== 列表操作 ==========

    RedisReply lPush(const std::string& key, const std::string& value) override {
        lists_[key].insert(lists_[key].begin(), value);
        return RedisReply::Int64(static_cast<int64_t>(lists_[key].size()));
    }

    RedisReply rPush(const std::string& key, const std::string& value) override {
        lists_[key].push_back(value);
        return RedisReply::Int64(static_cast<int64_t>(lists_[key].size()));
    }

    RedisReply lPop(const std::string& key) override {
        auto it = lists_.find(key);
        if (it != lists_.end() && !it->second.empty()) {
            std::string value = it->second.front();
            it->second.erase(it->second.begin());
            return RedisReply::String(value);
        }
        return RedisReply::Nil();
    }

    RedisReply rPop(const std::string& key) override {
        auto it = lists_.find(key);
        if (it != lists_.end() && !it->second.empty()) {
            std::string value = it->second.back();
            it->second.pop_back();
            return RedisReply::String(value);
        }
        return RedisReply::Nil();
    }

    RedisReply lRange(const std::string& key, int64_t start, int64_t stop) override {
        auto it = lists_.find(key);
        if (it != lists_.end()) {
            auto& vec = it->second;
            int64_t size = static_cast<int64_t>(vec.size());

            // 处理负索引
            if (start < 0) start = size + start;
            if (stop < 0) stop = size + stop;

            // 限制范围
            start = std::max<int64_t>(0, start);
            stop = std::min<int64_t>(size - 1, stop);

            std::vector<std::string> result;
            for (int64_t i = start; i <= stop && i < size; ++i) {
                result.push_back(vec[i]);
            }
            return RedisReply::Array(result);
        }
        return RedisReply::Array({});
    }

    RedisReply lLen(const std::string& key) override {
        auto it = lists_.find(key);
        if (it != lists_.end()) {
            return RedisReply::Int64(static_cast<int64_t>(it->second.size()));
        }
        return RedisReply::Int64(0);
    }

    RedisReply lSet(const std::string& key, int64_t index, const std::string& value) override {
        auto it = lists_.find(key);
        if (it != lists_.end()) {
            if (index >= 0 && index < static_cast<int64_t>(it->second.size())) {
                it->second[index] = value;
                return RedisReply::Ok();
            }
        }
        return RedisReply::Error("Index out of range");
    }

    RedisReply lIndex(const std::string& key, int64_t index) override {
        auto it = lists_.find(key);
        if (it != lists_.end()) {
            int64_t size = static_cast<int64_t>(it->second.size());
            if (index < 0) index = size + index;
            if (index >= 0 && index < size) {
                return RedisReply::String(it->second[index]);
            }
        }
        return RedisReply::Nil();
    }

    RedisReply lRem(const std::string& key, int64_t count, const std::string& value) override {
        auto it = lists_.find(key);
        if (it != lists_.end()) {
            int64_t removed = 0;
            auto& vec = it->second;
            auto pred = [&](const std::string& s) { return s == value; };

            if (count == 0) {
                // 删除所有
                auto oldSize = vec.size();
                vec.erase(std::remove_if(vec.begin(), vec.end(), pred), vec.end());
                removed = static_cast<int64_t>(oldSize - vec.size());
            } else if (count > 0) {
                // 从头删除 count 个
                for (auto i = vec.begin(); i != vec.end() && removed < count; ) {
                    if (pred(*i)) {
                        i = vec.erase(i);
                        removed++;
                    } else {
                        ++i;
                    }
                }
            } else {
                // 从尾删除 |count| 个
                for (auto i = vec.rbegin(); i != vec.rend() && removed < -count; ) {
                    if (pred(*i)) {
                        i = std::reverse_iterator<decltype(i)>(vec.erase(std::next(i).base()));
                        removed++;
                    } else {
                        ++i;
                    }
                }
            }
            return RedisReply::Int64(removed);
        }
        return RedisReply::Int64(0);
    }

    // ========== 集合操作 ==========

    RedisReply sAdd(const std::string& key, const std::string& member) override {
        sets_[key].insert(member);
        return RedisReply::Int64(1);
    }

    RedisReply sRem(const std::string& key, const std::string& member) override {
        auto it = sets_.find(key);
        if (it != sets_.end()) {
            return RedisReply::Int64(static_cast<int64_t>(it->second.erase(member)));
        }
        return RedisReply::Int64(0);
    }

    RedisReply sMembers(const std::string& key) override {
        std::vector<std::string> result;
        auto it = sets_.find(key);
        if (it != sets_.end()) {
            result.assign(it->second.begin(), it->second.end());
        }
        return RedisReply::Array(result);
    }

    RedisReply sIsMember(const std::string& key, const std::string& member) override {
        auto it = sets_.find(key);
        if (it != sets_.end()) {
            return RedisReply::Int64(it->second.count(member));
        }
        return RedisReply::Int64(0);
    }

    RedisReply sCard(const std::string& key) override {
        auto it = sets_.find(key);
        if (it != sets_.end()) {
            return RedisReply::Int64(static_cast<int64_t>(it->second.size()));
        }
        return RedisReply::Int64(0);
    }

    RedisReply sPop(const std::string& key) override {
        auto it = sets_.find(key);
        if (it != sets_.end() && !it->second.empty()) {
            auto value = *it->second.begin();
            it->second.erase(it->second.begin());
            return RedisReply::String(value);
        }
        return RedisReply::Nil();
    }

    // ========== 有序集合操作 ==========

    RedisReply zAdd(const std::string& key, double score, const std::string& member) override {
        zsets_[key][member] = score;
        return RedisReply::Int64(1);
    }

    RedisReply zRem(const std::string& key, const std::string& member) override {
        auto it = zsets_.find(key);
        if (it != zsets_.end()) {
            return RedisReply::Int64(static_cast<int64_t>(it->second.erase(member)));
        }
        return RedisReply::Int64(0);
    }

    RedisReply zRange(const std::string& key, int64_t start, int64_t stop) override {
        std::vector<std::string> result;
        auto it = zsets_.find(key);
        if (it != zsets_.end()) {
            // 按分数排序
            std::vector<std::pair<double, std::string>> sorted;
            for (const auto& pair : it->second) {
                sorted.push_back({pair.second, pair.first});
            }
            std::sort(sorted.begin(), sorted.end());

            int64_t size = static_cast<int64_t>(sorted.size());
            if (start < 0) start = size + start;
            if (stop < 0) stop = size + stop;
            start = std::max<int64_t>(0, start);
            stop = std::min<int64_t>(size - 1, stop);

            for (int64_t i = start; i <= stop && i < size; ++i) {
                result.push_back(sorted[i].second);
            }
        }
        return RedisReply::Array(result);
    }

    RedisReply zRevRange(const std::string& key, int64_t start, int64_t stop) override {
        std::vector<std::string> result;
        auto it = zsets_.find(key);
        if (it != zsets_.end()) {
            std::vector<std::pair<double, std::string>> sorted;
            for (const auto& pair : it->second) {
                sorted.push_back({pair.second, pair.first});
            }
            std::sort(sorted.begin(), sorted.end(), std::greater<>());

            int64_t size = static_cast<int64_t>(sorted.size());
            if (start < 0) start = size + start;
            if (stop < 0) stop = size + stop;
            start = std::max<int64_t>(0, start);
            stop = std::min<int64_t>(size - 1, stop);

            for (int64_t i = start; i <= stop && i < size; ++i) {
                result.push_back(sorted[i].second);
            }
        }
        return RedisReply::Array(result);
    }

    RedisReply zScore(const std::string& key, const std::string& member) override {
        auto it = zsets_.find(key);
        if (it != zsets_.end()) {
            auto fit = it->second.find(member);
            if (fit != it->second.end()) {
                return RedisReply::Double(fit->second);
            }
        }
        return RedisReply::Nil();
    }

    RedisReply zRangeByScore(const std::string& key, double min, double max) override {
        std::vector<std::string> result;
        auto it = zsets_.find(key);
        if (it != zsets_.end()) {
            for (const auto& pair : it->second) {
                if (pair.second >= min && pair.second <= max) {
                    result.push_back(pair.first);
                }
            }
            std::sort(result.begin(), result.end(), [&](const std::string& a, const std::string& b) {
                return it->second.at(a) < it->second.at(b);
            });
        }
        return RedisReply::Array(result);
    }

    RedisReply zRank(const std::string& key, const std::string& member) override {
        auto it = zsets_.find(key);
        if (it != zsets_.end()) {
            auto fit = it->second.find(member);
            if (fit != it->second.end()) {
                std::vector<std::pair<double, std::string>> sorted;
                for (const auto& pair : it->second) {
                    sorted.push_back({pair.second, pair.first});
                }
                std::sort(sorted.begin(), sorted.end());
                for (size_t i = 0; i < sorted.size(); ++i) {
                    if (sorted[i].second == member) {
                        return RedisReply::Int64(static_cast<int64_t>(i));
                    }
                }
            }
        }
        return RedisReply::Nil();
    }

    RedisReply zRevRank(const std::string& key, const std::string& member) override {
        auto it = zsets_.find(key);
        if (it != zsets_.end()) {
            auto fit = it->second.find(member);
            if (fit != it->second.end()) {
                std::vector<std::pair<double, std::string>> sorted;
                for (const auto& pair : it->second) {
                    sorted.push_back({pair.second, pair.first});
                }
                std::sort(sorted.begin(), sorted.end(), std::greater<>());
                for (size_t i = 0; i < sorted.size(); ++i) {
                    if (sorted[i].second == member) {
                        return RedisReply::Int64(static_cast<int64_t>(i));
                    }
                }
            }
        }
        return RedisReply::Nil();
    }

    RedisReply zIncrBy(const std::string& key, double increment, const std::string& member) override {
        auto& zset = zsets_[key];
        auto it = zset.find(member);
        double score = (it != zset.end()) ? it->second : 0.0;
        score += increment;
        zset[member] = score;
        return RedisReply::Double(score);
    }

    // ========== 发布订阅 ==========

    RedisReply publish(const std::string& channel, const std::string& message) override {
        published_[channel].push_back(message);
        return RedisReply::Int64(1);  // Mock: 总是返回1个订阅者
    }

    // ========== 事务 ==========

    bool multi() override {
        inTransaction_ = true;
        return true;
    }

    bool discard() override {
        inTransaction_ = false;
        transactionQueue_.clear();
        return true;
    }

    std::vector<RedisReply> exec() override {
        inTransaction_ = false;
        // Mock: 返回空结果
        transactionQueue_.clear();
        return {};
    }

    // ========== 通用操作 ==========

    RedisReply select(int database) override {
        currentDb_ = database;
        return RedisReply::Ok();
    }

    RedisReply keys(const std::string& pattern) override {
        std::vector<std::string> result;
        // 简单实现：只支持精确匹配和 "*"
        if (pattern == "*") {
            for (const auto& pair : data_) {
                result.push_back(pair.first);
            }
        } else {
            auto it = data_.find(pattern);
            if (it != data_.end()) {
                result.push_back(pattern);
            }
        }
        return RedisReply::Array(result);
    }

    std::vector<std::string> scan(const std::string& pattern, uint64_t cursor, uint64_t* newCursor) override {
        std::vector<std::string> result;
        auto reply = keys(pattern);
        if (reply.isOk()) {
            result = reply.asArray();
        }
        if (newCursor) *newCursor = 0;  // Mock: 一次返回所有
        return result;
    }

    RedisReply flushDb() override {
        data_.clear();
        hashes_.clear();
        lists_.clear();
        sets_.clear();
        zsets_.clear();
        expirations_.clear();
        return RedisReply::Ok();
    }

    RedisReply dbSize() override {
        return RedisReply::Int64(static_cast<int64_t>(data_.size()));
    }

    std::string info(const std::string& section) override {
        return "# Mock Redis\n"
               "redis_version:999.999.999\n"
               "connected_clients:1\n"
               "used_memory_human:1M\n";
    }

    // ========== Mock 特有方法 ==========

    const std::unordered_map<std::string, std::string>& getData() const { return data_; }
    const std::unordered_map<std::string, std::map<std::string, std::string>>& getHashes() const { return hashes_; }
    const std::vector<std::string>& getPublished(const std::string& channel) const {
        static std::vector<std::string> empty;
        auto it = published_.find(channel);
        return (it != published_.end()) ? it->second : empty;
    }

private:
    bool connected_ = true;
    int currentDb_ = 0;
    RedisConfig config_;

    // 数据存储
    std::unordered_map<std::string, std::string> data_;
    std::unordered_map<std::string, std::map<std::string, std::string>> hashes_;
    std::unordered_map<std::string, std::vector<std::string>> lists_;
    std::unordered_map<std::string, std::unordered_set<std::string>> sets_;
    std::unordered_map<std::string, std::map<std::string, double>> zsets_;
    std::unordered_map<std::string, int64_t> expirations_;
    std::unordered_map<std::string, std::vector<std::string>> published_;

    // 事务
    bool inTransaction_ = false;
    std::vector<RedisReply> transactionQueue_;
};

} // namespace redis
} // namespace storage
} // namespace apollo
