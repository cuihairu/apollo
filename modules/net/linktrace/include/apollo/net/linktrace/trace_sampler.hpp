#pragma once
//
// 头部采样器（logging.md §5.1：默认 1/10⁴、配置常量、入口进程决定；
// 下游按旗标传播不再重采）。确定性计数取模（every-Nth）——测试可断言
// 精确命中数；对「慢链路定位」用途，非随机化采样足够（地位 = 观测采样
// 面，非安全面）。
//
// dev 覆写：APOLLO_TRACE_SAMPLE_RATE（正整数 = 1/N 的 N；1 = 全采；
// 未设/坏值/0 = 缺省 10000）。编队演练可设 1 实证 span 行。

#include <atomic>
#include <cstdint>
#include <cstdlib>

namespace apollo::net::trace {

inline constexpr std::uint32_t kDefaultSampleRateDenom = 10000;

class HeadSampler {
public:
    explicit HeadSampler(std::uint32_t rate_denom = kDefaultSampleRateDenom)
        : rate_denom_(rate_denom == 0 ? 1 : rate_denom) {}

    // 头部采样裁决（入口进程对每条新链首消息调用一次；线程安全——
    // 原子推进，命中位 = counter ≡ 0 (mod N)，首条恒命中便于连通自证）
    bool should_sample() {
        return counter_.fetch_add(1, std::memory_order_relaxed) % rate_denom_ == 0;
    }

    std::uint32_t rate_denom() const { return rate_denom_; }

private:
    std::uint32_t rate_denom_;
    std::atomic<std::uint64_t> counter_{0};
};

// APOLLO_TRACE_SAMPLE_RATE 读取（dev 面；生产以配置常量装配 HeadSampler
// 为准——装配面 = gateway configure 期 -DAPOLLO_TRACE_SAMPLE_RATE=<N>，
// 定死编译期常量 APOLLO_TRACE_SAMPLE_RATE_CONST 并压过本 env 面）。
// 未设/空/非数字/0 = 缺省；溢出截断为缺省。
inline std::uint32_t sample_rate_from_env() {
    const char* v = std::getenv("APOLLO_TRACE_SAMPLE_RATE");
    if (v == nullptr || *v == '\0') return kDefaultSampleRateDenom;
    std::uint64_t n = 0;
    for (const char* p = v; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9') return kDefaultSampleRateDenom;
        n = n * 10 + static_cast<std::uint64_t>(*p - '0');
        if (n > 0xFFFFFFFFull) return kDefaultSampleRateDenom;
    }
    return n == 0 ? kDefaultSampleRateDenom : static_cast<std::uint32_t>(n);
}

}  // namespace apollo::net::trace
