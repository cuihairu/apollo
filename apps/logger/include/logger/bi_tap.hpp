#pragma once
//
// BI 分流 tap（logging.md §5 collector 职责——attribute-sync §8.2 的 BI
// 旁路：业务进程不必同时喂两套出口，BI/TLog 走独立采集通道在收集阶段
// 旁路采样导出，§5 预算外）。形态 = 独立追加文件（外挂 tail 上送
// Kafka/ES——Kafka 在采集器之后，游戏进程三禁不破）；行原文透传（BI
// 消费面自解析六键 + kv 段，采集器不做 TLog 字段翻译——翻译面随 TLog
// 事件族落地批）。
//
// 失败语义（预算外旁路）：open 失败 = 分流关（serve 降级警告，不拦起
// 机）；运行期写失败 = 本行丢弃 + failed 计数后永久关（无重试风暴）——
// BI 永不构成收集的运行依赖（§5「收集永不构成运行依赖」的旁路推论）。
//
// 文件布局：单文件 `<dir>/bi.log` 追加（滚动同 collector 归档滚动边界，
// 挂后续批）。线程口径：observe 单写者（serve 930 受理拍内直调），不加锁。
// 平台无关（纯 std），Windows 同跑。

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace apollo::logger {

struct BiTapConfig {
    std::string dir;                      // 空 = 分流关
    std::vector<std::string> cats;        // cat 白名单（精确匹配）
    std::uint32_t sample_rate_denom = 1;  // 每 N 条命中保 1（确定性计数取模）
};

/// cat 白名单解析（逗号分隔；空段与纯空白段跳过；段内空白剥离）
std::vector<std::string> parse_bi_cats(const std::string& spec);

/// 采样率解析：正整数 N（每 N 条命中保 1）；未设/空/坏值/0 归 1（全采）
std::uint32_t parse_bi_sample_rate(const std::string& spec);

/// env 装配面：APOLLO_LOGGER_BI_DIR（未设/空 = 分流关）+
/// APOLLO_LOGGER_BI_CATS + APOLLO_LOGGER_BI_SAMPLE_RATE
BiTapConfig bi_tap_config_from_env();

class BiTap {
public:
    explicit BiTap(BiTapConfig config);

    // 建目录 + 打开追加文件；false = dir 建不出/文件开不出（分流关）
    bool open();

    // 逐行喂（仅结构化行；cat 白名单 + 采样命中 → 原文追加一行）。
    // 未开/白名单外 = 无状态直返（零开销面）。
    void observe(const std::string& line, const std::string& cat);

    bool enabled() const { return enabled_; }
    std::uint64_t exported() const { return exported_; }
    std::uint64_t sampled_out() const { return sampled_out_; }
    std::uint64_t failed() const { return failed_; }

private:
    BiTapConfig config_;
    bool enabled_ = false;
    bool broken_ = false;  // 写失败后永久关（预算外旁路不重试）
    std::uint64_t counter_ = 0;  // 白名单命中计数（采样取模基准）
    std::uint64_t exported_ = 0;
    std::uint64_t sampled_out_ = 0;
    std::uint64_t failed_ = 0;
    std::ofstream out_;
};

}  // namespace apollo::logger
