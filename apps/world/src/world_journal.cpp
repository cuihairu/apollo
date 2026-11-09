#include "world_app/world_app.hpp"
#include "world_app_detail.hpp"
// POSIX journal fsync（批A 决定收口）
#include <fcntl.h>
#include <fstream>
#include <unistd.h>
#include <algorithm>
#include <limits>

namespace world_app {

namespace {

// 库面日志出口（detail 单定义，本单元转发——定义在 world_app.cpp）
apollo::core::log::Logger& worldLog() {
    return detail::world_log();
}

// 分配池成员判定（SpaceSlot 表口径，pool 复用两处：装载终态重建 + 观测）。
bool pool_has(const WorldAppConfig& config, std::uint64_t space_id) {
    for (const auto& slot : config.assignSpaces) {
        if (slot.space_id == space_id) {
            return true;
        }
    }
    return false;
}
} // namespace

//==============================================================================
// 管理日志（journal v2 装载回放 / 追加统一入口 / 代次墓碑水位）
//==============================================================================

void WorldApp::loadWorldJournal() {
    // 管理日志装载（启动窗，M1 链路开闸前——受理面未开，无并发写者）。
    // v2 行型（world-reconciliation 设计稿 §2，前缀区型）：`[T] cell|epoch`
    // 代次墓碑（T 前缀可省——v1 裸行直接升级不迁移）、`A space|cell|version`
    // 认领转移、`R space` 释放（owner 归零版本不变）、`B epoch` 启动代次。
    // 装载终态 = 每 space 最后一条 A/R + 每 cell 最大 T + 最大 B。0 号/非
    // 数字/缺字段/多字段 = 损坏显式拒启（不静默部分回放——orders journal
    // 同口径）；行尾 \r 剥除（Windows 落盘坑账防御）。文件缺省 = 首启，
    // append 建档（空日志顶替禁令的判定随批C）。
    if (config_.worldJournalPath.empty()) {
        return;
    }
    std::ifstream in(config_.worldJournalPath, std::ios::binary);
    if (!in) {
        // 空日志判定（world-reconciliation §2）分两步：启动时文件缺失先按
        // 「无 cell 拨入的真空候选」放行；首个 961 拨入即证明曾存在旧管理
        // 关系，转记录缺失并全池冻结。装载面 T 水位也可直接证明证据缺失。
        journalMissingAtBoot_ = true;
        if (!cellEpochWatermarks_.empty()) {
            missingJournal_ = true;
            worldLog().warning(
                "world journal file missing: " + config_.worldJournalPath
                + " known_cells=" + std::to_string(cellEpochWatermarks_.size())
                + " (missing journal evidence frozen all assignments)");
            return;
        }
        return;  // 无记录 = 首启候选，受理后按拨入证据裁决
    }
    const auto bad_line = [&](std::uint64_t line_no, const char* why) {
        throw std::runtime_error(std::string("world journal corrupt (") + why
                                 + ") at " + config_.worldJournalPath + ":"
                                 + std::to_string(line_no));
    };
    const auto parse_u64 = [&](const std::string& field,
                               std::uint64_t line_no) -> std::uint64_t {
        std::size_t pos = 0;
        std::uint64_t value = 0;
        try {
            value = std::stoull(field, &pos);
        } catch (const std::exception&) {
            bad_line(line_no, "not a number");
        }
        if (pos != field.size() || field.empty()) {
            bad_line(line_no, "trailing garbage");
        }
        return value;
    };
    struct ReplaySpace {
        std::uint64_t cell_id = 0;
        std::uint64_t version = 0;
    };
    std::unordered_map<std::uint64_t, ReplaySpace> replay;
    std::uint64_t max_boot_epoch = 0;
    bool saw_boot_row = false;
    std::string line;
    std::uint64_t line_no = 0;
    while (std::getline(in, line)) {
        ++line_no;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        if (line.compare(0, 2, "A ") == 0) {
            const std::string body = line.substr(2);
            const auto b1 = body.find('|');
            if (b1 == std::string::npos) {
                bad_line(line_no, "missing '|'");
            }
            const auto b2 = body.find('|', b1 + 1);
            if (b2 == std::string::npos) {
                bad_line(line_no, "missing '|'");
            }
            if (body.find('|', b2 + 1) != std::string::npos) {
                bad_line(line_no, "extra field");
            }
            const auto space_id = parse_u64(body.substr(0, b1), line_no);
            const auto cell_id = parse_u64(body.substr(b1 + 1, b2 - b1 - 1), line_no);
            const auto version = parse_u64(body.substr(b2 + 1), line_no);
            if (space_id == 0 || cell_id == 0 || version == 0) {
                bad_line(line_no, "zero id or version");
            }
            replay[space_id] = ReplaySpace{cell_id, version};  // 最后一条 A 为准
            continue;
        }
        if (line.compare(0, 2, "R ") == 0) {
            const std::string body = line.substr(2);
            if (body.find('|') != std::string::npos) {
                bad_line(line_no, "extra field");
            }
            const auto space_id = parse_u64(body, line_no);
            if (space_id == 0) {
                bad_line(line_no, "zero space id");
            }
            const auto it = replay.find(space_id);
            if (it != replay.end()) {
                it->second.cell_id = 0;  // 终态 owner 归零（版本保留）
            }
            // 无 A 的 R = 无操作（释放等价于从未认领）
            continue;
        }
        if (line.compare(0, 2, "B ") == 0) {
            const std::string body = line.substr(2);
            if (body.find('|') != std::string::npos) {
                bad_line(line_no, "extra field");
            }
            const auto epoch = parse_u64(body, line_no);
            if (epoch == 0) {
                bad_line(line_no, "zero epoch");
            }
            max_boot_epoch = std::max(max_boot_epoch, epoch);
            saw_boot_row = true;
            continue;
        }
        if (line.compare(0, 2, "C ") == 0 || line.compare(0, 2, "S ") == 0) {
            continue;
        }
        // T 行（可省前缀）：v2 带 `T ` 前缀或 v1 裸行，同解析；其余无管道
        // 行型显式损坏（防新行型被 T 兼容面误吞）。
        if (line.compare(0, 2, "T ") != 0 && line.find('|') == std::string::npos) {
            bad_line(line_no, "unknown row type");
        }
        std::string body;
        if (line.compare(0, 2, "T ") == 0) {
            body = line.substr(2);
        } else {
            body = line;
        }
        const auto bar = body.find('|');
        if (bar == std::string::npos) {
            bad_line(line_no, "missing '|'");
        }
        if (body.find('|', bar + 1) != std::string::npos) {
            bad_line(line_no, "extra field");
        }
        const auto cell_id = parse_u64(body.substr(0, bar), line_no);
        const auto epoch = parse_u64(body.substr(bar + 1), line_no);
        if (cell_id == 0 || epoch == 0) {
            bad_line(line_no, "zero id or epoch");
        }
        auto& wm = cellEpochWatermarks_[cell_id];
        if (epoch > wm) {
            wm = epoch;
        }
    }
    // B 行铸代次（设计稿 §5 步骤3）：重启 = 最大 B + 1；首启无 B 行 = 配置
    // 代次即初始代次（写首个 B 行——先记后开受理面，本函数先于链路装配）。
    // 自铸代次（配置 0）且无 B 行 = 无从记，沿用进程自铸（每启动自然不同）。
    // 追加失败告警降级：内存代次已铸，两次启动可能同代次（loudly 可见；
    // flush/fsync 持久级别随设计稿待拍板项）。
    if (config_.m1AuthorityEpoch != 0 || saw_boot_row) {
        if (saw_boot_row && max_boot_epoch == std::numeric_limits<std::uint64_t>::max()) {
            throw std::runtime_error("world journal epoch exhausted at "
                                     + config_.worldJournalPath);
        }
        const std::uint64_t cast = saw_boot_row ? max_boot_epoch + 1
                                                : config_.m1AuthorityEpoch;
        if (!appendJournalRow("B " + std::to_string(cast))) {
            worldLog().warning("world journal B row append failed: "
                               + config_.worldJournalPath
                               + " (restart may reuse management epoch)");
        }
        config_.m1AuthorityEpoch = cast;  // 受理面统一用铸后代次（961/962 透传）
    }
    // 装载终态重建进池（批A）：仅池成员生效（非池行计数告警——池配置变更后
    // 旧记录无权威落点，逐项对账随批B）；已释放（owner=0）条目照旧重建——
    // 重认领版本 +1 依赖历史版本（新启 version 1 会倒退，ABA 回放面）。
    // 有主的条目进 pending 过期窗（driveCellRegistry 驱动释放）。
    std::size_t seeded = 0;
    std::size_t skipped = 0;
    for (const auto& entry : replay) {
        if (!pool_has(config_, entry.first)) {
            ++skipped;
            continue;
        }
        SpaceAssignment a;
        a.space_id = entry.first;
        a.cell_id = entry.second.cell_id;
        a.version = entry.second.version;
        spaceAssignments_[entry.first] = a;
        if (a.cell_id != 0) {
            pendingJournalOwners_[a.cell_id] = config_.bootAtMs;
        }
        ++seeded;
    }
    worldLog().info("world journal loaded: " + config_.worldJournalPath
                    + " watermarks=" + std::to_string(cellEpochWatermarks_.size())
                    + " spaces=" + std::to_string(seeded)
                    + (skipped != 0
                           ? " skipped_nonpool=" + std::to_string(skipped)
                           : "")
                    + " epoch=" + std::to_string(config_.m1AuthorityEpoch));
}

bool WorldApp::appendJournalRow(const std::string& row, bool durable) {
    // journal v2 追加统一入口（批A；批C 巡检代决：A 行 fsync，write-ahead
    // 决定必须先落介质；R/B/T 行 flush + 失败告警降级，沿可用性优先）。
    // journal 关 = 纯内存语义恒成功（批㉚ 已代决沿用）。调用点全在内核线程
    //（受理点/驱动钩子 = 注册表单写者上下文），无并发写者。
    if (config_.worldJournalPath.empty()) {
        return true;
    }
    std::ofstream out(config_.worldJournalPath, std::ios::app | std::ios::binary);
    if (!out) {
        return false;
    }
    out << row << '\n';
    out.flush();
    if (!out) return false;
    if (!durable) {
        return true;
    }
    const int fd = ::open(config_.worldJournalPath.c_str(), O_RDONLY);
    if (fd < 0) return false;
    const bool synced = ::fsync(fd) == 0;
    ::close(fd);
    return synced;
}

void WorldApp::recordEpochWatermark(std::uint64_t cell_id, std::uint64_t epoch) {
    // 水位记录 + 墓碑追加（L101 管理日志切片）：仅水位真实前进才落盘——
    // 同代次刷新/回拨不追加（行冗余无害但省 I/O；prev 已在日志或本进程
    // 追加过）。v1 裸行格式保留（装载面直接升级，不迁移）。追加失败只告
    // 警不拒受理：墓碑缺席时隔离退化回批㉗ 内存水位（可用性优先，loudly
    // 可见）。调用点 = onCellRegister 受理点（内核线程泵上下文，单写者）。
    const auto it = cellEpochWatermarks_.find(cell_id);
    const std::uint64_t prev = it != cellEpochWatermarks_.end() ? it->second : 0;
    cellEpochWatermarks_[cell_id] = std::max(prev, epoch);
    if (epoch <= prev) {
        return;
    }
    if (!appendJournalRow(std::to_string(cell_id) + '|' + std::to_string(epoch))) {
        worldLog().warning("world journal append failed: " + config_.worldJournalPath
                           + " (isolation degrades to in-memory watermark)");
    }
}

} // namespace world_app
