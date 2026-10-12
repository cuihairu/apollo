#include "logger/bi_tap.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <system_error>

namespace apollo::logger {

std::vector<std::string> parse_bi_cats(const std::string& spec) {
    std::vector<std::string> cats;
    std::size_t begin = 0;
    while (begin <= spec.size()) {
        const std::size_t end = spec.find(',', begin);
        const std::size_t stop =
            end == std::string::npos ? spec.size() : end;
        std::string item = spec.substr(begin, stop - begin);
        // 剥段内空白（"trace, script_fault" 同型两段）
        const auto is_space = [](char c) {
            return c == ' ' || c == '\t';
        };
        item.erase(item.begin(), std::find_if(item.begin(), item.end(),
                                              [&](char c) { return !is_space(c); }));
        item.erase(std::find_if(item.rbegin(), item.rend(),
                                [&](char c) { return !is_space(c); })
                       .base(),
                   item.end());
        if (!item.empty()) {
            cats.push_back(std::move(item));
        }
        if (end == std::string::npos) {
            break;
        }
        begin = end + 1;
    }
    return cats;
}

std::uint32_t parse_bi_sample_rate(const std::string& spec) {
    if (spec.empty()) {
        return 1;
    }
    std::size_t begin = 0;
    std::size_t end = spec.size();
    const auto is_space = [](char c) { return c == ' ' || c == '\t'; };
    while (begin < end && is_space(spec[begin])) ++begin;
    while (end > begin && is_space(spec[end - 1])) --end;
    if (begin == end) {
        return 1;
    }
    for (std::size_t i = begin; i < end; ++i) {
        if (spec[i] < '0' || spec[i] > '9') {
            return 1;  // 坏值归全采
        }
    }
    const unsigned long long n = std::strtoull(spec.substr(begin, end - begin).c_str(),
                                               nullptr, 10);
    return n >= 1 ? static_cast<std::uint32_t>(n) : 1;
}

BiTapConfig bi_tap_config_from_env() {
    BiTapConfig cfg;
    const char* dir = std::getenv("APOLLO_LOGGER_BI_DIR");
    if (dir == nullptr || *dir == '\0') {
        return cfg;  // 未设/空 = 分流关
    }
    cfg.dir = dir;
    const char* cats = std::getenv("APOLLO_LOGGER_BI_CATS");
    if (cats != nullptr) {
        cfg.cats = parse_bi_cats(cats);
    }
    const char* rate = std::getenv("APOLLO_LOGGER_BI_SAMPLE_RATE");
    if (rate != nullptr) {
        cfg.sample_rate_denom = parse_bi_sample_rate(rate);
    }
    return cfg;
}

BiTap::BiTap(BiTapConfig config) : config_(std::move(config)) {}

bool BiTap::open() {
    if (config_.dir.empty()) {
        return false;  // 装配缺省 = 分流关
    }
    std::error_code ec;
    std::filesystem::create_directories(config_.dir, ec);
    if (ec) {
        return false;
    }
    out_.open(config_.dir + "/bi.log", std::ios::app);
    enabled_ = static_cast<bool>(out_);
    return enabled_;
}

void BiTap::observe(const std::string& line, const std::string& cat) {
    if (!enabled_ || broken_) {
        return;
    }
    if (std::find(config_.cats.begin(), config_.cats.end(), cat) ==
        config_.cats.end()) {
        return;
    }
    // 确定性采样：每 N 条命中保 1（N=1 全采；取模基准 = 白名单命中序）
    if (config_.sample_rate_denom > 1 &&
        counter_++ % config_.sample_rate_denom != 0) {
        ++sampled_out_;
        return;
    }
    out_ << line << '\n';
    out_.flush();  // 逐行冲刷——与归档面同口径（外挂 tail 即时可见）
    if (!out_) {
        ++failed_;
        broken_ = true;  // 本行丢 + 永久关（无重试风暴）
        out_.close();
        return;
    }
    ++exported_;
}

}  // namespace apollo::logger
