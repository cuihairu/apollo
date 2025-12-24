#pragma once

#include <cstdint>
#include <random>
#include <type_traits>
#include <ctime>
#include <cmath>
#include <limits>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace apollo {
namespace utils {

//==============================================================================
// Random - 随机数生成器
//==============================================================================

/**
 * @brief 随机数生成器
 *
 * 使用 PCG-XSH-RR 算法的高质量随机数生成器
 * 比 std::rand() 更快且质量更高
 */
class Random {
public:
    /**
     * @brief 构造函数
     *
     * @param seed 种子，0表示使用当前时间
     */
    explicit Random(uint64_t seed = 0) {
        state_ = (seed == 0) ? std::time(nullptr) : seed;
    }

    // 禁止拷贝
    Random(const Random&) = delete;
    Random& operator=(const Random&) = delete;

    /**
     * @brief 生成随机数 [0, UINT64_MAX]
     */
    uint64_t next() {
        uint64_t x = state_;
        uint64_t count = (x >> 59);

        state_ = x * 6364136223846793005ULL + 1;

        x ^= x >> 5;
        x = (x * 12605985483714917081ULL) >> 5;
        x ^= x >> 43;

        return (x >> count) | (x << (64 - count));
    }

    /**
     * @brief 生成随机数 [0, max)
     */
    uint64_t next(uint64_t max) {
        if (max == 0) {
            return 0;
        }
        return next() % max;
    }

    /**
     * @brief 生成随机数 [min, max]
     */
    uint64_t next(uint64_t min, uint64_t max) {
        if (max <= min) {
            return min;
        }
        return min + (next() % (max - min + 1));
    }

    /**
     * @brief 生成 32 位随机数
     */
    uint32_t nextUint32() {
        return static_cast<uint32_t>(next());
    }

    /**
     * @brief 生成 16 位随机数
     */
    uint16_t nextUint16() {
        return static_cast<uint16_t>(next());
    }

    /**
     * @brief 生成 8 位随机数
     */
    uint8_t nextUint8() {
        return static_cast<uint8_t>(next());
    }

    /**
     * @brief 生成随机浮点数 [0, 1)
     */
    double nextDouble() {
        return (next() >> 11) * (1.0 / 9007199254740992.0);
    }

    /**
     * @brief 生成随机浮点数 [0, 1]
     */
    double nextDoubleClosed() {
        return (next() >> 11) * (1.0 / 9007199254740991.0);
    }

    /**
     * @brief 生成随机浮点数 [min, max]
     */
    double nextDouble(double min, double max) {
        if (max <= min) {
            return min;
        }
        return min + nextDouble() * (max - min);
    }

    /**
     * @brief 生成随机浮点数 [0, 1)
     */
    float nextFloat() {
        return static_cast<float>(nextDouble());
    }

    /**
     * @brief 生成随机布尔值
     */
    bool nextBool() {
        return (next() & 1) == 0;
    }

    /**
     * @brief 以概率 p 返回 true
     *
     * @param p 概率 [0, 1]
     */
    bool chance(double p) {
        return nextDouble() < p;
    }

    /**
     * @brief 填充缓冲区
     */
    void fill(void* buffer, size_t size) {
        uint8_t* ptr = static_cast<uint8_t*>(buffer);
        for (size_t i = 0; i < size; ++i) {
            ptr[i] = nextUint8();
        }
    }

    /**
     * @brief 重新设置种子
     */
    void setSeed(uint64_t seed) {
        state_ = seed;
    }

    //==========================================================================
    // 静态辅助函数（使用默认生成器）
    //==========================================================================

    /**
     * @brief 生成随机整数 [0, max)
     */
    static int32_t randInt(int32_t max) {
        return static_cast<int32_t>(getDefault().next(static_cast<uint64_t>(max)));
    }

    /**
     * @brief 生成随机整数 [min, max]
     */
    static int32_t randInt(int32_t min, int32_t max) {
        if (max <= min) {
            return min;
        }
        return min + static_cast<int32_t>(getDefault().next(static_cast<uint64_t>(max - min + 1)));
    }

    /**
     * @brief 生成随机浮点数 [0, 1)
     */
    static double randDouble() {
        return getDefault().nextDouble();
    }

    /**
     * @brief 生成随机浮点数 [min, max]
     */
    static double randDouble(double min, double max) {
        return getDefault().nextDouble(min, max);
    }

    /**
     * @brief 随机布尔值
     */
    static bool randBool() {
        return getDefault().nextBool();
    }

private:
    static Random& getDefault() {
        static Random instance(std::time(nullptr));
        return instance;
    }

    uint64_t state_;
};

//==============================================================================
// UniformIntDistribution - 均匀整数分布
//==============================================================================

/**
 * @brief 均匀整数分布
 *
 * 类似 std::uniform_int_distribution，但使用我们的 Random
 */
template<typename IntType = int32_t>
class UniformIntDistribution {
public:
    static_assert(std::is_integral<IntType>::value, "IntType must be integral");

    explicit UniformIntDistribution(IntType a = 0, IntType b = std::numeric_limits<IntType>::max())
        : min_(a), max_(b) {}

    void reset() {}

    template<typename URNG>
    IntType operator()(URNG& g) {
        return static_cast<IntType>(min_ + g() % (max_ - min_ + 1));
    }

    IntType min() const { return min_; }
    IntType max() const { return max_; }

private:
    IntType min_;
    IntType max_;
};

//==============================================================================
// UniformRealDistribution - 均匀实数分布
//==============================================================================

/**
 * @brief 均匀实数分布
 */
template<typename RealType = double>
class UniformRealDistribution {
public:
    static_assert(std::is_floating_point<RealType>::value, "RealType must be floating point");

    explicit UniformRealDistribution(RealType a = 0.0, RealType b = 1.0)
        : min_(a), max_(b) {}

    void reset() {}

    template<typename URNG>
    RealType operator()(URNG& g) {
        double ratio = static_cast<double>(g()) / (std::numeric_limits<uint64_t>::max() + 1.0);
        return static_cast<RealType>(min_ + ratio * (max_ - min_));
    }

    RealType min() const { return min_; }
    RealType max() const { return max_; }

private:
    RealType min_;
    RealType max_;
};

//==============================================================================
// NormalDistribution - 正态分布（Box-Muller 变换）
//==============================================================================

/**
 * @brief 正态分布（高斯分布）
 *
 * 使用 Box-Muller 变换实现
 */
class NormalDistribution {
public:
    /**
     * @brief 构造函数
     *
     * @param mean 均值
     * @param stddev 标准差
     */
    explicit NormalDistribution(double mean = 0.0, double stddev = 1.0)
        : mean_(mean)
        , stddev_(stddev)
        , hasSpare_(false)
        , spare_(0.0) {}

    /**
     * @brief 生成随机数
     */
    template<typename URNG>
    double operator()(URNG& g) {
        if (hasSpare_) {
            hasSpare_ = false;
            return spare_ * stddev_ + mean_;
        }

        hasSpare_ = true;

        // Box-Muller 变换
        double u1, u2;
        do {
            u1 = static_cast<double>(g()) / (std::numeric_limits<uint64_t>::max() + 1.0);
            u2 = static_cast<double>(g()) / (std::numeric_limits<uint64_t>::max() + 1.0);
        } while (u1 <= std::numeric_limits<double>::epsilon());

        double mag = stddev_ * std::sqrt(-2.0 * std::log(u1));
        double z0 = mag * std::cos(2.0 * M_PI * u2);
        spare_ = mag * std::sin(2.0 * M_PI * u2);

        return z0 + mean_;
    }

    double mean() const { return mean_; }
    double stddev() const { return stddev_; }

    void reset() {
        hasSpare_ = false;
    }

private:
    double mean_;
    double stddev_;
    bool hasSpare_;
    double spare_;
};

//==============================================================================
// Shuffle - 随机打乱
//==============================================================================

/**
 * @brief Fisher-Yates 洗牌算法
 *
 * @param begin 起始迭代器
 * @param end 结束迭代器
 * @param rng 随机数生成器
 */
template<typename RandomIt, typename URNG>
void shuffle(RandomIt begin, RandomIt end, URNG& rng) {
    using std::iter_swap;
    using difference_type = typename std::iterator_traits<RandomIt>::difference_type;

    difference_type n = end - begin;
    if (n <= 1) {
        return;
    }

    for (difference_type i = n - 1; i > 0; --i) {
        difference_type j = static_cast<difference_type>(rng() % (i + 1));
        iter_swap(begin + i, begin + j);
    }
}

/**
 * @brief 使用默认随机数生成器洗牌
 */
template<typename RandomIt>
inline void shuffle(RandomIt begin, RandomIt end) {
    Random rng;
    shuffle(begin, end, [&](size_t) { return rng.next(); });
}

//==============================================================================
// RandomSample - 随机采样
//==============================================================================

/**
 * @brief 随机采样
 *
 * @param population 总体范围
 * @param sample 输出迭代器
 * @param n 采样数量
 * @param rng 随机数生成器
 */
template<typename PopulationIterator, typename SampleIterator, typename URNG>
void randomSample(PopulationIterator begin, PopulationIterator end,
                  SampleIterator out, size_t n, URNG& rng) {
    using difference_type = typename std::iterator_traits<PopulationIterator>::difference_type;
    difference_type populationSize = end - begin;

    if (n > static_cast<size_t>(populationSize)) {
        n = static_cast<size_t>(populationSize);
    }

    // Reservoir sampling
    size_t i = 0;
    for (auto it = begin; it != end && n > 0; ++it, ++i) {
        if (i < n) {
            *out++ = *it;
        } else {
            size_t j = static_cast<size_t>(rng() % (i + 1));
            if (j < n) {
                *(out + j) = *it;
            }
        }
    }
}

} // namespace utils
} // namespace apollo
