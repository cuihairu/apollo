#include "apollo/data/orm/persist_journal.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>

namespace apollo::data::journal {

namespace {

// 行格式：seq|timestamp_ms|key|payload（key/payload 不含换行；'|" 转义原文保留）
bool parse_line(const std::string& line, JournalEntry& out) {
    const auto p1 = line.find('|');
    if (p1 == std::string::npos) {
        return false;
    }
    const auto p2 = line.find('|', p1 + 1);
    if (p2 == std::string::npos) {
        return false;
    }
    const auto p3 = line.find('|', p2 + 1);
    if (p3 == std::string::npos) {
        return false;
    }
    try {
        out.sequence = std::stoull(line.substr(0, p1));
        out.timestamp_ms = std::stoll(line.substr(p1 + 1, p2 - p1 - 1));
    } catch (const std::exception&) {
        return false;
    }
    out.key = line.substr(p2 + 1, p3 - p2 - 1);
    out.payload = line.substr(p3 + 1);
    return true;
}

std::string encode_line(const JournalEntry& e) {
    std::ostringstream os;
    os << e.sequence << '|' << e.timestamp_ms << '|' << e.key << '|' << e.payload << '\n';
    return os.str();
}

// 原子重写：tmp + rename，压薄/截断崩溃时不留半文件
bool rewrite_file(const std::string& path, const std::deque<JournalEntry>& entries) {
    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) {
            return false;
        }
        for (const auto& e : entries) {
            out << encode_line(e);
        }
    }
    std::remove(path.c_str());
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}

} // namespace

PersistJournal::PersistJournal(std::string journal_path)
    : journal_path_(std::move(journal_path)) {
}

bool PersistJournal::ensure_handle() {
    if (out_.is_open()) {
        return true;
    }
    // binary 打开：行原文逐位，Windows 文本模式不做换行翻译
    out_.open(journal_path_, std::ios::app | std::ios::binary);
    return static_cast<bool>(out_);
}

bool PersistJournal::open() {
    if (opened_) {
        return true;
    }

    std::ifstream in(journal_path_);
    if (in) {
        JournalEntry e;
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || !parse_line(line, e)) {
                continue;
            }
            pending_.push_back(e);
            if (e.sequence >= next_sequence_) {
                next_sequence_ = e.sequence + 1;
            }
        }
    }

    opened_ = true;
    // 句柄开不出不拦 open（replay-only 恢复面照常）；append 逐拍重试
    ensure_handle();
    return true;
}

bool PersistJournal::append(std::string key, std::string payload, std::int64_t timestamp_ms) {
    if (!opened_) {
        return false;
    }

    JournalEntry e;
    e.sequence = next_sequence_;
    e.timestamp_ms = timestamp_ms;
    e.key = std::move(key);
    e.payload = std::move(payload);

    // write-ahead：先落盘，再入内存队列（常驻句柄 + 逐行 flush 耐久口径不变）
    if (!ensure_handle()) {
        return false;
    }
    out_ << encode_line(e);
    out_.flush();
    if (!out_) {
        out_.close();  // 下拍 append 重开（自愈同旧逐行开语义）
        return false;
    }

    ++next_sequence_;
    pending_.push_back(std::move(e));
    return true;
}

std::size_t PersistJournal::drain(const ApplySink& sink, std::size_t max_entries) {
    if (!sink) {
        return 0;
    }

    std::size_t applied = 0;
    while (applied < max_entries && !pending_.empty()) {
        const JournalEntry& front = pending_.front();
        if (!sink(front)) {
            break;  // 首条失败即停（保序；失败条目下轮重试）
        }
        pending_.pop_front();
        ++applied;
    }

    if (applied > 0) {
        compact();  // 已应用部分压出文件
    }
    return applied;
}

std::size_t PersistJournal::replay(const ApplySink& sink) {
    if (!opened_) {
        return 0;
    }

    std::size_t count = 0;
    std::ifstream in(journal_path_);
    if (!in) {
        return 0;
    }

    JournalEntry e;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || !parse_line(line, e)) {
            continue;
        }
        if (sink && sink(e)) {
            ++count;
        }
    }
    return count;
}

std::size_t PersistJournal::compact() {
    // 先关句柄再重写：tmp+rename 换 inode（旧句柄续写=幽灵文件丢数据），
    // Windows 更不许删打开中文件；重写后重开，开不出则下拍 append 重试
    out_.close();
    if (!rewrite_file(journal_path_, pending_)) {
        return pending_.size();
    }
    ensure_handle();
    return pending_.size();
}

} // namespace apollo::data::journal
