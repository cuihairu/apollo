#include "world_app/world_app.hpp"
#include "world_app/m1_projection_client.hpp"
#include "world_app_detail.hpp"

namespace world_app {

namespace {

// 库面日志出口（detail 单定义，本单元转发——定义在 world_app.cpp）
apollo::core::log::Logger& worldLog() {
    return detail::world_log();
}

// 能力位覆盖判定（容量申报裁决）：cell 声明的 capabilities 须覆盖 Space
// 要求的 needs 全部位才有认领资格。needs=0 = 无要求——任何 cell 均
// 合格，含未声明（capabilities=0）的现状语义。
bool capability_fit(std::int32_t capabilities, std::int32_t needs) {
    return (capabilities & needs) == needs;
}
} // namespace

//==============================================================================
// Space 分配权威与对账（962 全池视图 / 认领裁决 / 967 自报对账）
//==============================================================================

void WorldApp::pushSpaceAssignments(const std::string& peer,
                                    const CellRegistration& reg) {
    // 分配面（L101 首切片 + L108 回收切片 + L104 就绪/排空/负载裁决切片）：
    // 受理即推全池权威归属视图。未登记 Space 首认领（版本 1）；owner=0
    // （过期释放）重认领且版本 +1（认领转移面：离线旧主本地视图停在旧版本，
    // 须版本前进才收敛，防 ABA 重放；同主回拨也前进）；本 cell 已归属的
    // 刷新（版本不变幂等）；归属其它 cell 的推 ready=0 撤销标记（收帧方据
    // 此撤本地残留路由——离线期缺口经重注册收敛，见 L108 切片）。认领独占：
    // 不重分他人 Space（重平衡随 L108 拓扑批）。
    //
    // 认领资格（L104 负载裁决 + 容量申报裁决切片）：未就绪 / 自报排空中 /
    // World 单方缩容（retire）的 cell 不认领新 Space——进程存活不等于可分配，
    // 既有归属照常刷新（撤空不撤权）；声明能力未覆盖该 Space needs 的
    // cell 同样不认领——没有容量明确拒绝（world-cell-management §5），留池
    // 等合格候选，不落到不合格 cell 上凑数。已释放的 Space 走最小负载优先，
    // 存在负载严格更低且同样合格的候选则本 cell 退让（等负载面收敛后随下次
    // 受理/刷新认领），同负载让位给先受理者（不抖动重分）。
    if (config_.assignSpaces.empty()) {
        return;
    }
    if (journalMissingAtBoot_) {
        if (!missingJournal_) {
            missingJournal_ = true;
            worldLog().warning(
                "world journal missing evidence: cell=" + std::to_string(reg.cell_id)
                + " has no tombstone; all assignments frozen");
        }
        for (const auto& slot : config_.assignSpaces) {
            worldLog().warning(
                "space assignment blocked by missing journal evidence: space="
                + std::to_string(slot.space_id));
        }
        return;
    }
    const bool may_claim = reg.ready && reg.drain_state == 0 && !reg.retire;
    for (const auto& slot : config_.assignSpaces) {
        const std::uint64_t space_id = slot.space_id;
        if (missingJournal_) {
            worldLog().warning(
                "space assignment blocked by missing journal evidence: space="
                + std::to_string(space_id));
            continue;
        }
        // 冲突冻结（批B 967 裁决面）：冻结期不推新分配、不推刷新、不推撤销
        //（设计稿 §4——自报与权威表分歧未决前，任何一侧声明都可能固化错误
        // 归属；解冻随人工修复批挂账）。
        if (conflictedSpaces_.count(space_id) > 0) {
            continue;
        }
        auto it = spaceAssignments_.find(space_id);
        if (it == spaceAssignments_.end() || it->second.cell_id == 0) {
            // 未分配 / 已释放：无资格、无容量（能力未覆盖）或让位者不发 962
            //（无归属视图可推）
            if (!may_claim || !capability_fit(reg.capabilities, slot.needs) ||
                lower_load_peer(reg.cell_id, reg.load, slot.needs)) {
                continue;
            }
            const bool reclaim = it != spaceAssignments_.end();  // owner=0
            if (reclaim) {
                ++it->second.version;  // 重认领转移：版本前进（防 ABA）
            } else {
                SpaceAssignment a;
                a.space_id = space_id;
                a.cell_id = reg.cell_id;
                a.version = 1;
                it = spaceAssignments_.emplace(space_id, a).first;
            }
            it->second.cell_id = reg.cell_id;
            // write-ahead（批A journal v2）：A 行 append+flush 成功才推 962
            // ——持久决定先于对外声明。落盘失败回滚内存认领并跳过声明
            //（Space 留池待下次受理重试，告警 loudly；刷新/撤销标记不经此闸
            //——无转移发生，最后一条 A 行已覆盖该归属+版本）。
            if (!appendJournalRow("A " + std::to_string(space_id) + "|"
                                  + std::to_string(reg.cell_id) + "|"
                                  + std::to_string(it->second.version), true)) {
                worldLog().warning(
                    "world journal append failed: " + config_.worldJournalPath
                    + " (space claim withheld: space=" + std::to_string(space_id)
                    + " cell=" + std::to_string(reg.cell_id) + ")");
                if (reclaim) {
                    --it->second.version;  // 回滚版本前进（保持释放态）
                } else {
                    spaceAssignments_.erase(it);
                }
                continue;
            }
            worldLog().info(reclaim
                                ? "space re-claimed: space=" + std::to_string(space_id)
                                      + " cell=" + std::to_string(reg.cell_id)
                                      + " version=" + std::to_string(it->second.version)
                                : "space assigned: space=" + std::to_string(space_id)
                                      + " cell=" + std::to_string(reg.cell_id)
                                      + " version=1");
            // 位置投影发布（L115，D05 §1.1 时序：journal A 行 → 962 → 968）：
            // 已提交归属事实推目录镜像——claim/reclaim 均 ready=1（版本水位
            // 由目录侧幂等/拒乱序裁决）。
            publishProjection(space_id, reg.cell_id, it->second.version, true);
        } else if (it->second.cell_id != reg.cell_id) {
            if (m1Link_) {
                m1Link_->sendCellSpaceAssign(peer, it->second.version,
                                             config_.m1AuthorityEpoch, space_id,
                                             it->second.cell_id, false);
            }
            continue;  // 归属它方：撤销标记（owner/版本 = 当前归属视图）
        }
        if (m1Link_) {
            m1Link_->sendCellSpaceAssign(peer, it->second.version,
                                         config_.m1AuthorityEpoch, space_id,
                                         it->second.cell_id, true);
        }
    }
}

bool WorldApp::lower_load_peer(std::uint64_t cell_id, std::int32_t load,
                               std::int32_t needs) const {
    // 最小负载优先（L104 负载裁决 + 容量申报裁决切片）：登记表内任一合格
    // 候选负载严格低于本 cell 即让位——合格 = 就绪、不排空、未缩容且声明
    // 能力覆盖该 Space 的 needs（不合格的更低负载 peer 不参裁，否则合格
    // 方会把归属让给无资格方，Space 永久搁置）。同负载不退让（认领归先受理
    // 者，裁决对注册顺序稳定）；存疑（suspect）条目仍参裁——链路未决时归属
    // 不动比抢分安全。缩容意图（retire）条目排除在外：它永不认领，让位给
    // 它会永久搁置该 Space。
    for (const auto& entry : cellRegistrations_) {
        const auto& other = entry.second;
        if (other.cell_id != cell_id && other.ready && other.drain_state == 0 &&
            !other.retire && capability_fit(other.capabilities, needs) &&
            other.load < load) {
            return true;
        }
    }
    return false;
}

void WorldApp::releaseCellSpaces(std::uint64_t cell_id) {
    // 过期释放（L108 回收切片，driveCellRegistry 同上下文）：owner 置 0、
    // 版本不变——释放不是认领，无转移发生；重认领方经 pushSpaceAssignments
    // 版本 +1 收敛。条目保留：池成员身份不随心跳消失，Space 回池待认领。
    for (auto& entry : spaceAssignments_) {
        // 冲突冻结（批B 967 裁决面）：冻结期不释放（设计稿 §4「不推新分配、
        // 不释放」）——条目停尸在当前 owner，等待人工修复裁决解冻。
        if (conflictedSpaces_.count(entry.first) > 0) {
            continue;
        }
        if (entry.second.cell_id == cell_id) {
            entry.second.cell_id = 0;
            // R 行（批A journal v2）：释放是终态转移（owner 归零、版本不变），
            // 重放以每 space 最后一条 A/R 为准——缺席则重启误认旧主仍持有权。
            // 失败告警降级（批㉚ 可用性优先；装载面对账裁决随批B）。
            if (!appendJournalRow("R " + std::to_string(entry.first))) {
                worldLog().warning("world journal append failed: "
                                   + config_.worldJournalPath
                                   + " (release replays as owned: space="
                                   + std::to_string(entry.first) + ")");
            }
            worldLog().info("space released: space="
                            + std::to_string(entry.first)
                            + " cell=" + std::to_string(cell_id)
                            + " version=" + std::to_string(entry.second.version));
            // 释放墓碑投影（L115）：R 行已落盘（或降级告警）——内存归属事实
            // （owner=0）推目录抹除面；版本不变由目录侧墓碑命中水位裁决。
            publishProjection(entry.first, 0, entry.second.version, false);
        }
    }
}

void WorldApp::onCellSpaceReport(const std::string& peer, const CellSpaceReport& report) {
    // 对账自报受理（967，断连对账批B；M1 泵上下文 = 注册表单写者上下文，
    // 与 onCellRegister/onCellRetire 同口）。逐条目核对权威归属表（设计稿
    // §4）——裁决就地生效，无回执（962 全池权威视图即裁决载体，冻结不经
    // 协议下发）。request_id = 0 保留（静默弃，同 952/960 关联键纪律）。
    if (report.request_id == 0 || report.cell_id == 0 || report.space_id == 0) {
        return;
    }
    const auto reg = cellRegistrations_.find(report.cell_id);
    if (reg == cellRegistrations_.end()) {
        // 未登记来源：无裁决地位（对账表以登记为准），丢弃。
        worldLog().info("space report from unregistered cell dropped: cell="
                        + std::to_string(report.cell_id)
                        + " space=" + std::to_string(report.space_id));
        return;
    }
    if (report.start_epoch != reg->second.start_epoch) {
        // 旧化身上报：登记已是新代次（新化身），旧端视图无对账效力，丢弃。
        worldLog().info("space report from stale incarnation dropped: cell="
                        + std::to_string(report.cell_id)
                        + " reported_epoch=" + std::to_string(report.start_epoch));
        return;
    }
    if (reg->second.suspect) {
        // 控制面失联期隔离：961 复核归来前，同登记代次但走新/旧链路的
        // 旧化身或旧端自报不得参与对账，也不能借 967 证明旧归属。
        worldLog().warning("space report from suspect cell dropped: cell="
                           + std::to_string(report.cell_id)
                           + " space=" + std::to_string(report.space_id));
        return;
    }
    ++spaceReportCount_;
    const auto entry = spaceAssignments_.find(report.space_id);
    if (!config_.worldJournalPath.empty() && entry != spaceAssignments_.end() &&
        entry->second.cell_id == report.cell_id &&
        entry->second.version == report.applied_version) {
        appendJournalRow("C " + std::to_string(report.space_id) + '|'
                         + std::to_string(report.cell_id) + '|'
                         + std::to_string(report.applied_version));
        ++reconciledSpaceCount_;
    }
    if (entry == spaceAssignments_.end()) {
        // 未登记权威（§4「无 S 记录 | C 报 S」行）：权威归属表无此 Space
        //（池外 id，或池内从未认领/记忆已失）——源隔离不明，进冲突集
        //（观测面；池外 id 无分配语义可冻结，冻结面天然只作用于池内）。
        const bool fresh = conflictedSpaces_.insert(report.space_id).second;
        worldLog().warning("space report conflict (no authoritative record): space="
                           + std::to_string(report.space_id)
                           + " cell=" + std::to_string(report.cell_id)
                           + (fresh ? " (frozen)" : " (already frozen)"));
        return;
    }
    if (entry->second.cell_id == report.cell_id) {
        if (entry->second.version == report.applied_version) {
            // 匹配（§4 行1）：保持，无操作。独立 cellServer 进程 967 真链路
            // 对账观测行（不改变裁决，只让冒烟可断言）。
            worldLog().info("space report reconciled: space="
                            + std::to_string(report.space_id)
                            + " cell=" + std::to_string(report.cell_id)
                            + " version=" + std::to_string(report.applied_version));
            return;
        }
        // 版本冲突（§4 行2）：权威仍归属本 cell 但应用版本对不上——journal
        // 落后或应用面偏离，该 Space 冻结（不认领不释放不推送），待对账修复。
        const bool fresh = conflictedSpaces_.insert(report.space_id).second;
        worldLog().warning("space report version conflict: space="
                           + std::to_string(report.space_id)
                           + " cell=" + std::to_string(report.cell_id)
                           + " authoritative=" + std::to_string(entry->second.version)
                           + " applied=" + std::to_string(report.applied_version)
                           + (fresh ? " (frozen)" : " (already frozen)"));
        return;
    }
    // 其余（owner=0 已释放 / 归属它方）：权威已前进——962 全池视图即裁决，
    // 上报端随撤销标记/重认领推送收敛（§4 行3 既有路径），收敛滞后不入冲突集。
    worldLog().info("space report lags authority: space="
                    + std::to_string(report.space_id)
                    + " cell=" + std::to_string(report.cell_id)
                    + " authoritative_owner=" + std::to_string(entry->second.cell_id)
                    + " applied=" + std::to_string(report.applied_version));
}

const WorldApp::SpaceAssignment* WorldApp::find_space_assignment(
    std::uint64_t space_id) const {
    const auto it = spaceAssignments_.find(space_id);
    return it != spaceAssignments_.end() ? &it->second : nullptr;
}

void WorldApp::publishProjection(std::uint64_t space_id, std::uint64_t cell_id,
                                 std::uint64_t version, bool ready) {
    // 发布面缺席 = 无操作（面关形态：目录侧经 kUp 重放与下次归属变更收敛）；
    // 在场即直发（同管理泵线程——与 publish/pump 同线程纪律，见客户端头）。
    if (projection_ != nullptr) {
        projection_->publish(space_id, cell_id, version,
                             config_.m1AuthorityEpoch, ready);
    }
}

} // namespace world_app
