// BI 分流 tap 测试（BiTapTests，P3-3 余项批）。
//
// 覆盖（apps/logger/include/logger/bi_tap.hpp 头注；logging.md §5
// collector 职责 + attribute-sync §8.2 BI 旁路）：
//   1. cat 白名单过滤 + 原文透传逐位（非白名单零状态直返）；
//   2. 确定性采样：每 N 条命中保 1（首条恒保），计数面 exported/
//      sampled_out 对账；
//   3. 缺省关：空 dir = enabled false，observe 无状态不落盘；
//   4. open 失败降级：dir 为常规文件 = open false，observe 无害；
//   5. 解析面：cat 白名单逗号解析（空白段剥离/空段跳过）+ 采样率
//      （未设/空/坏值/0 归 1）。

#include "logger/bi_tap.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#define TEST_ASSERT(cond, msg)                                                           \
    do {                                                                                 \
        if (!(cond)) {                                                                   \
            std::cerr << "FAIL: " << (msg) << " (" << __FILE__ << ":" << __LINE__ << ")" \
                      << std::endl;                                                      \
            return false;                                                                \
        }                                                                                \
    } while (0)

namespace {

namespace fs = std::filesystem;

// 逐跑唯一临时目录（跑毕清理由调用方负责）
std::string make_tmp_dir() {
    static int seq = 0;
    const std::string dir = (fs::temp_directory_path() /
                             ("apollo-bi-tap-test-" + std::to_string(++seq)))
                                .string();
    fs::remove_all(dir);
    return dir;
}

std::string slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

bool test_filter_and_passthrough() {
    const std::string dir = make_tmp_dir();
    apollo::logger::BiTapConfig cfg;
    cfg.dir = dir;
    cfg.cats = {"trace", "script_fault"};
    apollo::logger::BiTap tap(cfg);
    TEST_ASSERT(tap.open() && tap.enabled(), "开启成功");

    const std::string l1 =
        "ts=100 level=INFO proc=cell tick=1 cat=trace msg=span one kv=1";
    const std::string l2 =
        "ts=101 level=ERROR proc=cell tick=1 cat=script_fault msg=boom";
    const std::string l3 =
        "ts=102 level=INFO proc=cell tick=1 cat=other msg=noise";
    tap.observe(l1, "trace");
    tap.observe(l2, "script_fault");
    tap.observe(l3, "other");
    TEST_ASSERT(tap.exported() == 2, "白名单内两行导出");
    TEST_ASSERT(slurp(dir + "/bi.log") == l1 + "\n" + l2 + "\n",
                "原文透传逐位（含 kv 段）");
    fs::remove_all(dir);
    return true;
}

bool test_sample_denom() {
    const std::string dir = make_tmp_dir();
    apollo::logger::BiTapConfig cfg;
    cfg.dir = dir;
    cfg.cats = {"trace"};
    cfg.sample_rate_denom = 3;  // 每 3 条命中保 1
    apollo::logger::BiTap tap(cfg);
    TEST_ASSERT(tap.open(), "开启成功");
    for (int i = 0; i < 5; ++i) {
        tap.observe("ts=" + std::to_string(i) + " cat=trace msg=m" +
                        std::to_string(i),
                    "trace");
    }
    TEST_ASSERT(tap.exported() == 2, "5 中保 2（第 0/3 条）");
    TEST_ASSERT(tap.sampled_out() == 3, "采样弃 3 计数");
    const std::string body = slurp(dir + "/bi.log");
    TEST_ASSERT(body.find("msg=m0") != std::string::npos &&
                    body.find("msg=m3") != std::string::npos,
                "确定性取模首条恒保");
    TEST_ASSERT(body.find("msg=m1") == std::string::npos,
                "弃样行不落盘");
    fs::remove_all(dir);
    return true;
}

bool test_disabled_by_default() {
    apollo::logger::BiTap tap(apollo::logger::BiTapConfig{});  // dir 空 = 关
    TEST_ASSERT(!tap.open() && !tap.enabled(), "缺省关");
    tap.observe("ts=1 cat=trace msg=x", "trace");  // 无状态直返不抛
    TEST_ASSERT(tap.exported() == 0, "关面零导出");
    return true;
}

bool test_open_failure_dir_is_file() {
    const std::string dir = make_tmp_dir();
    const std::string file = dir + "_as_file";
    { std::ofstream f(file); }
    apollo::logger::BiTapConfig cfg;
    cfg.dir = file;  // dir 指向常规文件 = create_directories 失败
    apollo::logger::BiTap tap(cfg);
    TEST_ASSERT(!tap.open(), "open 失败降级");
    tap.observe("ts=1 cat=trace msg=x", "trace");
    TEST_ASSERT(tap.exported() == 0, "失败面零导出不抛");
    fs::remove_all(dir);
    fs::remove(file);
    return true;
}

bool test_parsing_faces() {
    const auto cats = apollo::logger::parse_bi_cats("trace, script_fault ,,x");
    TEST_ASSERT(cats.size() == 3, "空段跳过");
    TEST_ASSERT(cats[0] == "trace" && cats[1] == "script_fault" && cats[2] == "x",
                "段内空白剥离");
    TEST_ASSERT(apollo::logger::parse_bi_cats("").empty(), "空 spec 零白名单");
    TEST_ASSERT(apollo::logger::parse_bi_cats("  ,  ").empty(), "纯空白段全跳");

    TEST_ASSERT(apollo::logger::parse_bi_sample_rate("") == 1, "未设归全采");
    TEST_ASSERT(apollo::logger::parse_bi_sample_rate("10") == 10, "正整数取值");
    TEST_ASSERT(apollo::logger::parse_bi_sample_rate(" 7 ") == 7, "环绕空白剥离");
    TEST_ASSERT(apollo::logger::parse_bi_sample_rate("0") == 1, "0 归全采");
    TEST_ASSERT(apollo::logger::parse_bi_sample_rate("2x") == 1, "坏值归全采");
    return true;
}

}  // namespace

int main() {
    int passed = 0;
    int failed = 0;

    struct Case {
        const char* name;
        bool (*fn)();
    } cases[] = {
        {"filter_and_passthrough", test_filter_and_passthrough},
        {"sample_denom", test_sample_denom},
        {"disabled_by_default", test_disabled_by_default},
        {"open_failure_dir_is_file", test_open_failure_dir_is_file},
        {"parsing_faces", test_parsing_faces},
    };

    for (const auto& c : cases) {
        std::cout << "[ RUN  ] " << c.name << std::endl;
        if (c.fn()) {
            std::cout << "[  OK  ] " << c.name << std::endl;
            ++passed;
        } else {
            std::cout << "[ FAIL ] " << c.name << std::endl;
            ++failed;
        }
    }

    std::cout << "BiTapTests: " << passed << " passed, " << failed
              << " failed" << std::endl;
    return failed == 0 ? 0 : 1;
}
