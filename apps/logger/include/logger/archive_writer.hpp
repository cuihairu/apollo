#pragma once

#include <functional>
#include <fstream>
#include <string>

namespace logger {

// collector 归档写入器（logging.md §5 collector 职责「磁盘滚动」落地）：
// 按日滚动 + 保留份数清理，滚动语义沿用 FileAppender 既有（P3-3 批 C）——
// 关旧→开新当日本名，不落 rename（rename 原子语义归 §7 外采 tail 面），
// 同日重启 = 续写同一文件（采集面 durability 优先，硬杀不吞缓冲）。
//
// 文件名 <base>_<日期>.log（同 FileAppender ByDate scheme；定宽日期 + 无
// 序号 = 字典序恒等时间序，检索面按 *.log 排序即时间序，follow 位移键
// 独立于排序）。
//
// 单写者纪律：受理/归档全在宿主 serve 循环拍内（logger_serve.cpp 同
// 口径），本类不加锁——与 discovery 库「调用方单线程驱动」纪律同源。
class ArchiveWriter {
public:
    struct Config {
        std::string directory;                ///< 归档目录（缺省由调用侧填）
        std::string base_name = "collector";  ///< 文件名主名
        int max_files = 30;                   ///< 保留份数（<=0 = 不清理）
    };

    // 日期缝：缺省 wall clock，测试注假日期强制跨日。
    using DateProvider = std::function<std::string()>;

    explicit ArchiveWriter(Config config);

    // 建目录 + 开当日文件（app 模式续写）；false = 装配失败（起机报错）。
    bool open();

    void set_date_provider(DateProvider provider) { date_provider_ = std::move(provider); }

    // 一行落盘：跨日先滚，写后冲刷（逐行 durability，归档前形态同口径）。
    void write_line(const std::string& line);

    void flush();
    const std::string& current_path() const { return current_path_; }

private:
    void rotate_to(const std::string& date);
    void cleanup_old_files();
    std::string path_for(const std::string& date) const;

    Config config_;
    DateProvider date_provider_;
    std::ofstream stream_;
    std::string current_date_;
    std::string current_path_;
};

// wall clock 日期（YYYYMMDD，本地时区）——DateProvider 缺省实现。
std::string archive_today();

// 保留份数解析（APOLLO_LOGGER_ARCHIVE_MAX_FILES）：未设/空/非法 = 缺省 30，
// <=0 = 不清理。
int max_files_from_env(const char* value);

}  // namespace logger
