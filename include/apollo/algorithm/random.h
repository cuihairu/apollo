/**
 * @file random.h
 * @brief 游戏专用随机数生成器
 *
 * 适用场景：
 * - 随机抽奖（概率控制）
 * - 掉落计算（权重随机）
 * - 暴击计算
 * - 随机事件触发
 * - 地图随机生成
 *
 * 特性：
 * - 种子控制（可复现）
 * - 高性能（比 std::rand() 更快）
 * - 线程安全
 * - 多种分布支持
 * - 权重随机（抽取）
 */

#pragma once

#include <cstdint>
#include <vector>
#include <algorithm>
#include <random>
#include <functional>
#include <unordered_map>
#include <limits>

namespace apollo {
namespace algorithm {

//==============================================================================
// 快速随机数生成器（XorShift 128+）
//==============================================================================

/**
 * @brief XorShift 128+ 随机数生成器
 *
 * 特点：
 * - 周期：2^128 - 1
 * - 速度：非常快（简单的位运算）
 * - 状态：128 位
 * - 质量：通过 BigCrush 测试
 *
 * 适合游戏场景：
 * - 不需要加密安全的随机性
 * - 需要高性能
 * - 需要可复现的结果（通过种子）
 */
class XorShift128Plus {
public:
    using result_type = uint64_t;

    /**
     * @brief 构造函数（使用固定种子）
     */
    XorShift128Plus() {
        seed(12345);
    }

    /**
     * @brief 构造函数（指定种子）
     */
    explicit XorShift128Plus(uint64_t seed) {
        this->seed(seed);
    }

    /**
     * @brief 设置种子
     */
    void seed(uint64_t value) {
        state_[0] = value;
        state_[1] = value + 0x9e3779b97f4a7c15;

        // 确保非零
        if (state_[0] == 0) state_[0] = 1;
        if (state_[1] == 0) state_[1] = 1;
    }

    /**
     * @brief 生成随机数
     */
    uint64_t operator()() {
        uint64_t x = state_[0];
        uint64_t const y = state_[1];

        state_[0] = y;
        x ^= x << 23;
        state_[1] = x ^ y ^ (x >> 17) ^ (y >> 26);

        return state_[1] + y;
    }

    /**
     * @brief 生成 0 到 max 之间的随机数（包含）
     */
    uint64_t range(uint64_t max) {
        if (max == 0) return 0;
        return (*this)() % (max + 1);
    }

    /**
     * @brief 生成 min 到 max 之间的随机数
     */
    uint64_t range(uint64_t min, uint64_t max) {
        if (max <= min) return min;
        return min + range(max - min);
    }

    /**
     * @brief 生成 0.0 到 1.0 之间的浮点数
     */
    double nextDouble() {
        return static_cast<double>((*this)()) /
               static_cast<double>(std::numeric_limits<uint64_t>::max());
    }

    /**
     * @brief 生成 0.0f 到 1.0f 之间的浮点数
     */
    float nextFloat() {
        return static_cast<float>((*this)()) /
               static_cast<float>(std::numeric_limits<uint64_t>::max());
    }

    /**
     * @brief 最小值
     */
    static constexpr uint64_t min() {
        return 0;
    }

    /**
     * @brief 最大值
     */
    static constexpr uint64_t max() {
        return std::numeric_limits<uint64_t>::max();
    }

private:
    uint64_t state_[2];
};

//==============================================================================
// 线程本地随机数生成器
//==============================================================================

/**
 * @brief 线程本地 RNG（无锁，高性能）
 */
class ThreadLocalRng {
public:
    using result_type = uint64_t;

    uint64_t operator()() {
        return rng_();  // 线程本地实例
    }

    uint64_t range(uint64_t max) {
        return rng_.range(max);
    }

    uint64_t range(uint64_t min, uint64_t max) {
        return rng_.range(min, max);
    }

    double nextDouble() {
        return rng_.nextDouble();
    }

    float nextFloat() {
        return rng_.nextFloat();
    }

private:
    static thread_local XorShift128Plus rng_;
};

inline thread_local XorShift128Plus ThreadLocalRng::rng_;

//==============================================================================
// 概率分布工具
//==============================================================================

/**
 * @brief 概率分布工具类
 */
class Random {
public:
    //==========================================================================
    // 基础随机数
    //==========================================================================

    /**
     * @brief 生成随机整数 [min, max]
     */
    static int32_t range(int32_t min, int32_t max) {
        static thread_local XorShift128Plus rng;
        return static_cast<int32_t>(rng.range(min, max));
    }

    /**
     * @brief 生成随机浮点数 [min, max]
     */
    static double range(double min, double max) {
        static thread_local XorShift128Plus rng;
        return min + (max - min) * rng.nextDouble();
    }

    /**
     * @brief 生成随机布尔值（真概率）
     */
    static bool boolean(double probability = 0.5) {
        static thread_local XorShift128Plus rng;
        return rng.nextDouble() < probability;
    }

    //==========================================================================
    // 常用分布
    //==========================================================================

    /**
     * @brief 均匀分布整数
     */
    static int32_t uniformInt(int32_t min, int32_t max) {
        return range(min, max);
    }

    /**
     * @brief 均匀分布浮点数
     */
    static double uniform(double min, double max) {
        return range(min, max);
    }

    /**
     * @brief 正态分布（高斯分布）
     * @param mean 均值
     * @param stddev 标准差
     */
    static double normal(double mean = 0.0, double stddev = 1.0) {
        // Box-Muller 变换
        static thread_local XorShift128Plus rng;
        static thread_local bool hasSpare = false;
        static thread_local double spare;

        if (hasSpare) {
            hasSpare = false;
            return spare * stddev + mean;
        }

        double u, v, s;
        do {
            u = rng.nextDouble() * 2.0 - 1.0;
            v = rng.nextDouble() * 2.0 - 1.0;
            s = u * u + v * v;
        } while (s >= 1.0 || s == 0.0);

        s = std::sqrt(-2.0 * std::log(s) / s);
        spare = v * s;
        hasSpare = true;

        return mean + stddev * u * s;
    }

    /**
     * @brief 指数分布
     * @param lambda 速率参数
     */
    static double exponential(double lambda = 1.0) {
        static thread_local XorShift128Plus rng;
        return -std::log(1.0 - rng.nextDouble()) / lambda;
    }

    /**
     * @brief 泊松分布
     * @param lambda 平均发生率
     */
    static int32_t poisson(double lambda = 1.0) {
        if (lambda <= 0.0) {
            return 0;
        }

        // 使用 Knuth 算法
        static thread_local XorShift128Plus rng;

        int32_t k = 0;
        double p = std::exp(-lambda);
        double sum = p;

        double u = rng.nextDouble();
        while (u > sum) {
            ++k;
            p *= lambda / k;
            sum += p;
        }

        return k;
    }

    //==========================================================================
    // 游戏专用分布
    //==========================================================================

    /**
     * @brief 暴击检测
     * @param critRate 暴击率（0.0 - 1.0）
     * @return 是否暴击
     */
    static bool crit(double critRate) {
        return boolean(critRate);
    }

    /**
     * @brief 暴击倍数（随机）
     * @param minMult 最小倍数
     * @param maxMult 最大倍数
     */
    static double critMultiplier(double minMult = 1.5, double maxMult = 2.0) {
        return uniform(minMult, maxMult);
    }

    /**
     * @brief 伤害浮动（±百分比）
     * @param baseDamage 基础伤害
     * @param variance 浮动百分比（0.1 = ±10%）
     */
    static int32_t damageVariance(int32_t baseDamage, double variance = 0.1) {
        double factor = 1.0 + uniform(-variance, variance);
        return static_cast<int32_t>(baseDamage * factor);
    }

    /**
     * @brief 技能命中检测
     * @param hitRate 命中率（0.0 - 1.0）
     * @return 是否命中
     */
    static bool hitCheck(double hitRate) {
        return boolean(hitRate);
    }

    /**
     * @brief 闪避检测
     * @param evasionRate 闪避率（0.0 - 1.0）
     * @return 是否闪避
     */
    static bool evasionCheck(double evasionRate) {
        return boolean(evasionRate);
    }

    //==========================================================================
    // 集合操作
    //==========================================================================

    /**
     * @brief 随机打乱数组
     */
    template<typename T>
    static void shuffle(std::vector<T>& vec) {
        static thread_local XorShift128Plus rng;

        for (size_t i = vec.size() - 1; i > 0; --i) {
            size_t j = static_cast<size_t>(rng.range(0, static_cast<uint64_t>(i)));
            std::swap(vec[i], vec[j]);
        }
    }

    /**
     * @brief 随机选择一个元素
     */
    template<typename T>
    static const T& choose(const std::vector<T>& vec) {
        if (vec.empty()) {
            throw std::out_of_range("Random::choose() - empty vector");
        }
        static thread_local XorShift128Plus rng;
        return vec[static_cast<size_t>(rng.range(0, static_cast<uint64_t>(vec.size() - 1)))];
    }

    /**
     * @brief 随机选择 N 个不重复元素
     */
    template<typename T>
    static std::vector<T> sample(const std::vector<T>& vec, size_t n) {
        if (n > vec.size()) {
            n = vec.size();
        }

        std::vector<T> result(vec);
        shuffle(result);
        result.resize(n);
        return result;
    }

    //==========================================================================
    // 权重随机
    //==========================================================================

    /**
     * @brief 权重项
     */
    template<typename T>
    struct WeightedItem {
        T item;
        uint64_t weight;

        WeightedItem() : weight(0) {}
        WeightedItem(const T& i, uint64_t w) : item(i), weight(w) {}
        WeightedItem(T&& i, uint64_t w) : item(std::move(i)), weight(w) {}
    };

    /**
     * @brief 按权重随机选择
     * @param items 带权重的项列表
     * @return 选中的项
     */
    template<typename T>
    static const T& weightedChoice(const std::vector<WeightedItem<T>>& items) {
        if (items.empty()) {
            throw std::out_of_range("Random::weightedChoice() - empty items");
        }

        uint64_t totalWeight = 0;
        for (const auto& item : items) {
            totalWeight += item.weight;
        }

        if (totalWeight == 0) {
            return items[0].item;
        }

        static thread_local XorShift128Plus rng;
        uint64_t random = rng.range(totalWeight - 1);

        uint64_t sum = 0;
        for (const auto& item : items) {
            sum += item.weight;
            if (random < sum) {
                return item.item;
            }
        }

        return items.back().item;
    }

    /**
     * @brief 简化的权重选择（使用两个分离的 vector）
     */
    template<typename T>
    static const T& weightedChoice(const std::vector<T>& items, const std::vector<uint64_t>& weights) {
        if (items.empty() || items.size() != weights.size()) {
            throw std::out_of_range("Random::weightedChoice() - invalid input");
        }

        uint64_t totalWeight = 0;
        for (uint64_t w : weights) {
            totalWeight += w;
        }

        if (totalWeight == 0) {
            return items[0];
        }

        static thread_local XorShift128Plus rng;
        uint64_t random = rng.range(totalWeight - 1);

        uint64_t sum = 0;
        for (size_t i = 0; i < items.size(); ++i) {
            sum += weights[i];
            if (random < sum) {
                return items[i];
            }
        }

        return items.back();
    }

    /**
     * @brief 按权重随机选择 N 个不重复项
     */
    template<typename T>
    static std::vector<T> weightedSample(
            const std::vector<WeightedItem<T>>& items,
            size_t n) {

        if (items.empty() || n == 0) {
            return {};
        }

        std::vector<T> result;
        std::vector<bool> used(items.size(), false);

        for (size_t count = 0; count < n && count < items.size(); ++count) {
            // 构建可用项列表
            std::vector<WeightedItem<T>> available;
            for (size_t i = 0; i < items.size(); ++i) {
                if (!used[i]) {
                    available.emplace_back(items[i].item, items[i].weight);
                }
            }

            if (available.empty()) {
                break;
            }

            const T& selected = weightedChoice(available);

            // 标记为已使用
            for (size_t i = 0; i < items.size(); ++i) {
                if (!used[i] && items[i].item == selected) {
                    used[i] = true;
                    result.push_back(selected);
                    break;
                }
            }
        }

        return result;
    }

    //==========================================================================
    // 掉落表
    //==========================================================================

    /**
     * @brief 掉落表
     *
     * 支持多个稀有度等级，每个等级有不同的概率
     */
    template<typename ItemType>
    class LootTable {
    public:
        /**
         * @brief 添加掉落项
         * @param item 物品
         * @param weight 权重（相对概率）
         */
        void add(const ItemType& item, uint64_t weight) {
            items_.emplace_back(item, weight);
        }

        /**
         * @brief 随机抽取一个物品
         * @return 抽中的物品指针，未抽中返回 nullptr
         */
        const ItemType* roll() const {
            if (items_.empty()) {
                return nullptr;
            }

            // 计算是否掉落（可以设置整体掉落率）
            // 这里简化为一定掉落

            return &weightedChoice(items_);
        }

        /**
         * @brief 抽取 N 个物品（可能重复）
         */
        std::vector<const ItemType*> rollMultiple(size_t n) const {
            std::vector<const ItemType*> result;
            result.reserve(n);

            for (size_t i = 0; i < n; ++i) {
                if (auto* item = roll()) {
                    result.push_back(item);
                }
            }

            return result;
        }

        /**
         * @brief 抽取 N 个不重复物品
         */
        std::vector<const ItemType*> rollUnique(size_t n) const {
            std::vector<const ItemType*> result;
            if (n > items_.size()) {
                n = items_.size();
            }

            std::vector<bool> used(items_.size(), false);

            for (size_t i = 0; i < n; ++i) {
                // 找到可用项
                std::vector<WeightedItem<const ItemType*>> available;
                for (size_t j = 0; j < items_.size(); ++j) {
                    if (!used[j]) {
                        available.emplace_back(&items_[j].item, items_[j].weight);
                    }
                }

                if (available.empty()) {
                    break;
                }

                const ItemType* selected = weightedChoice(available);

                // 标记为已使用
                for (size_t j = 0; j < items_.size(); ++j) {
                    if (!used[j] && &items_[j].item == selected) {
                        used[j] = true;
                        result.push_back(selected);
                        break;
                    }
                }
            }

            return result;
        }

        size_t size() const { return items_.size(); }
        bool empty() const { return items_.empty(); }

    private:
        std::vector<WeightedItem<ItemType>> items_;
    };

    //==========================================================================
    // 抽奖系统
    //==========================================================================

    /**
     * @brief 抽奖结果
     */
    template<typename T>
    struct GachaResult {
        T item;
        bool isNew;         // 是否是新获得
        bool isRare;        // 是否稀有
        uint64_t rarity;    // 稀有度等级
    };

    /**
     * @brief 抽奖/扭蛋系统
     *
     * 支持：
     * - 保底机制
     * - 概率提升
     * - 多种稀有度
     */
    template<typename ItemType>
    class GachaSystem {
    public:
        /**
         * @brief 稀有度配置
         */
        struct RarityConfig {
            uint64_t rarity;         // 稀有度等级（越高越稀有）
            double baseProbability;  // 基础概率
            uint64_t pityCounter;     // 保底次数
            double probabilityBoost;  // 每次未抽中的概率提升

            RarityConfig(uint64_t r, double p, uint64_t pity = 0, double boost = 0.0)
                : rarity(r), baseProbability(p), pityCounter(pity), probabilityBoost(boost) {}
        };

        /**
         * @brief 玩家状态
         */
        struct PlayerState {
            uint64_t totalPulls = 0;       // 总抽取次数
            std::unordered_map<uint64_t, uint64_t> pityCounters;  // 各稀有度保底计数
            std::unordered_map<ItemType, uint64_t> ownedItems;    // 已拥有的物品
        };

        /**
         * @brief 添加稀有度配置
         */
        void addRarity(const RarityConfig& config) {
            rarities_.push_back(config);
            std::sort(rarities_.begin(), rarities_.end(),
                [](const RarityConfig& a, const RarityConfig& b) {
                    return a.rarity < b.rarity;
                });
        }

        /**
         * @brief 添加物品
         */
        void addItem(const ItemType& item, uint64_t rarity) {
            items_[rarity].push_back(item);
        }

        /**
         * @brief 抽取
         */
        GachaResult<ItemType> pull(PlayerState& state) {
            state.totalPulls++;

            // 检查保底
            for (const auto& rarity : rarities_) {
                uint64_t& counter = state.pityCounters[rarity.rarity];
                counter++;

                if (rarity.pityCounter > 0 && counter >= rarity.pityCounter) {
                    counter = 0;
                    return createResult(state, rarity.rarity, true);
                }
            }

            // 正常抽取
            return doPull(state);
        }

        /**
         * @brief 十连抽
         */
        std::vector<GachaResult<ItemType>> pullTen(PlayerState& state) {
            std::vector<GachaResult<ItemType>> results;
            results.reserve(10);

            for (int i = 0; i < 10; ++i) {
                results.push_back(pull(state));
            }

            return results;
        }

    private:
        GachaResult<ItemType> doPull(PlayerState& state) {
            double rand = nextDouble();

            // 根据概率选择稀有度
            double cumulative = 0.0;
            uint64_t selectedRarity = 0;

            // 计算当前概率（考虑概率提升）
            for (const auto& rarity : rarities_) {
                uint64_t counter = state.pityCounters[rarity.rarity];
                double boostedProb = rarity.baseProbability +
                    counter * rarity.probabilityBoost;

                cumulative += boostedProb;

                if (rand <= cumulative) {
                    selectedRarity = rarity.rarity;
                    state.pityCounters[rarity.rarity] = 0;  // 重置该稀有度计数
                    break;
                }
            }

            return createResult(state, selectedRarity, false);
        }

        GachaResult<ItemType> createResult(PlayerState& state, uint64_t rarity, bool isPity) {
            auto it = items_.find(rarity);
            if (it == items_.end() || it->second.empty()) {
                // 没有该稀有度的物品，返回默认
                return {ItemType(), false, isPity, rarity};
            }

            const ItemType& item = Random::choose(it->second);
            bool isNew = state.ownedItems[item]++ == 0;

            return {item, isNew, isPity, rarity};
        }

        static double nextDouble() {
            static thread_local XorShift128Plus rng;
            return rng.nextDouble();
        }

        std::vector<RarityConfig> rarities_;
        std::unordered_map<uint64_t, std::vector<ItemType>> items_;
    };
};

//==============================================================================
// 种子随机数生成器（用于可复现的随机）
//==============================================================================

/**
 * @brief 可播种的随机数生成器
 *
 * 使用场景：
 * - 地图生成（同一种子生成相同地图）
 * - 战斗回放（随机数序列可复现）
 * - 程序化内容生成
 */
class SeededRandom {
public:
    explicit SeededRandom(uint64_t seed = 0)
        : rng_(seed) {}

    /**
     * @brief 重置种子
     */
    void setSeed(uint64_t seed) {
        rng_.seed(seed);
    }

    /**
     * @brief 保存当前状态
     */
    uint64_t getState() const {
        // 简化实现
        return 0;
    }

    /**
     * @brief 恢复状态
     */
    void setState(uint64_t state) {
        // 简化实现
    }

    uint64_t operator()() { return rng_(); }
    uint64_t range(uint64_t max) { return rng_.range(max); }
    uint64_t range(uint64_t min, uint64_t max) { return rng_.range(min, max); }
    double nextDouble() { return rng_.nextDouble(); }

private:
    XorShift128Plus rng_;
};

//==============================================================================
// 工具函数
//==============================================================================

/**
 * @brief 生成随机 UUID
 */
inline uint64_t generateUuid() {
    static thread_local XorShift128Plus rng(std::chrono::steady_clock::now().time_since_epoch().count());
    return rng();
}

/**
 * @brief 生成随机字符串
 */
inline std::string randomString(size_t length, const char* charset = "abcdefghijklmnopqrstuvwxyz0123456789") {
    static thread_local XorShift128Plus rng;

    std::string result;
    result.reserve(length);

    size_t charsetLen = std::strlen(charset);
    for (size_t i = 0; i < length; ++i) {
        result.push_back(charset[rng.range(charsetLen - 1)]);
    }

    return result;
}

/**
 * @brief 生成随机颜色
 */
inline uint32_t randomColor() {
    static thread_local XorShift128Plus rng;
    return rng.range(0x000000, 0xFFFFFF);
}

} // namespace algorithm
} // namespace apollo
