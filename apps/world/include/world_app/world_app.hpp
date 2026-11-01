#pragma once

// WorldApp——world 同一二进制的管理角色壳（L98 首切片后；切片3 删除内嵌
// 执行内核收口）。
//
// 职责分界（todo P2 L98）：模拟循环、WorldHost/Scene/Avatar、AOI、战斗与
// Lua 全部在执行内核 CellApp（cell_app.hpp）与独立 cellServer 宿主；本类型
// 只保留管理面——M1 传输端点（m1Link_）、cell 注册、管理命令受理与执行端
// 命令转发（切片2 起 932/944/952/956 经注册 cell 连接反向转发）。

#include "world_app/config.hpp"
#include "world_app/acceptance_queue.hpp"
#include "world_app/m1_link_server.hpp"
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace world_app {

namespace protocol = apollo::protocol;

// WorldApp 服务器（manager 角色：传输 + 管理编排；不承载模拟域）
class WorldApp {
public:
    explicit WorldApp(const WorldAppConfig& config);
    ~WorldApp();

    // 启动服务器（M1 端点监听 + 管理泵线程）
    void start();

    // 停止服务器（管理泵线程停 → M1 端点停）
    void stop();

    // 是否运行中（管理生命周期驱动）
    bool isRunning() const { return running_; }

    // M1 链路接收端（M1 上层接线批 2b-ii）：InterServerLink passive 端点，
    // 网关经它投递 cell 意图（924-928 契约族）；listenPort 查实占端口。
    M1LinkServer& m1Link() { return *m1Link_; }

    // 管理侧拨号面拍首钩子（L123 服务注册拨号批）：管理泵每拍调用
    // （钩子体每拍读取成员，无竞态面）；置空 = 无。
    void set_manager_pre_tick(std::function<void(std::uint64_t now_ms)> hook) {
        managerPreTick_ = std::move(hook);
    }

    // ---- cell 注册登记（960/961，L101 注册首切片）----
    // 注册表条目：逻辑 ID + 启动代次 + 自报面（地址/能力/负载/排空/就绪）
    // + 链路 peer 自报名（回执与后续命令按名查链路）。registered_at_ms =
    // last-heard（steady 毫秒，每次受理刷新；心跳失效窗 cellExpiryMs 的
    // 摘除基准）。
    struct CellRegistration {
        std::uint64_t cell_id = 0;
        std::uint64_t start_epoch = 0;
        std::string host;
        std::int32_t port = 0;
        std::int32_t capabilities = 0;
        // 契约兼容投影（960 版本/兼容裁决切片）：受理即裁决同值，登记保持
        // 声明值供观测（registry 只存已受理者——投影必等 World 自身，观测
        // 面证明裁决已过）。0 不落入（未声明拒 code 5）。
        std::int64_t compat = 0;
        std::int32_t load = 0;
        std::int32_t drain_state = 0;
        bool ready = false;
        std::uint64_t registered_at_ms = 0;
        std::string peer;
        // 断连存疑（L101 断连存疑切片）：控制链路断（M1 on_down，泵上下文
        // 即本表单写者线程）即置位——gap 表 §line71 断开后先标记对账；清除
        // 只随 961 复核受理（同 ID 重注册裁决接受 = 活体归来），过期摘除
        // 整条带走。
        bool suspect = false;
        // World 单方缩容意图（L104 缩容切片；与自报 drain_state 分面）：
        // 自报 drain_state 是 cell 自己的活动状态，本位是 World 的调度决定
        // ——begin_cell_retire 置位即停止向其认领新 Space 并释放其名下归属
        // （自然排空，不强断在途活动），cancel_cell_retire 清除（未退出可
        // 取消）。受理刷新不改本位（World 状态不由 cell 自报覆盖）；新代次
        // 替换亦保留（退出中不可重新分配，须显式取消），过期摘除整条带走。
        bool retire = false;
    };
    // 注册表只读观测面（泵上下文口径，同世界状态单写者——受理点在 M1 泵
    // 上下文改表，读面供测试/管理消费；跨线程持有返回指针违反单写者纪律）。
    std::size_t registered_cell_count() const { return cellRegistrations_.size(); }
    const CellRegistration* find_cell_registration(std::uint64_t cell_id) const;
    // 存疑条目数（L101 断连存疑切片观测面；轮询口径同 registered_cell_count）
    std::size_t suspect_cell_count() const;

    // ---- Space 分配表（962，L101 分配面首切片 + L104 就绪/排空/负载裁决）----
    // 分配条目：space_id → （归属 cell + 分配版本）。池 = config.assignSpaces；
    // 策略 = 未就绪/排空中的 cell 不认领（进程存活不等于可分配，既有归属
    // 保留），可认领者中最小负载优先独占认领全池未分配 Space（code 1/2
    // 受理即推当前分配），他人已归属的不迁移不重分——重平衡/拓扑版本随
    // L108 拓扑批。观测面同注册表口径（泵上下文单写者）。
    struct SpaceAssignment {
        std::uint64_t space_id = 0;
        std::uint64_t cell_id = 0;
        std::uint64_t version = 0;  // 每 Space 独立单调（本切片首分配 = 1）
    };
    std::size_t assigned_space_count() const { return spaceAssignments_.size(); }
    const SpaceAssignment* find_space_assignment(std::uint64_t space_id) const;
    // 对账冲突集（断连对账批B 观测面；轮询口径同 registered_cell_count）：
    // 967 自报与权威归属核对不上的 Space（版本冲突 / 未登记权威——池外
    // Space）。冻结面：不认领不释放不推送（§4），解冻随对账修复批。
    std::size_t conflicted_space_count() const { return conflictedSpaces_.size(); }
    // 已裁决自报条目数（967 受理并核对的条数；丢弃面——未登记/旧化身/
    // 0 保留——不计）。
    std::uint64_t space_report_count() const { return spaceReportCount_; }

    // ---- cell 缩容编排（L104 缩容切片；管理面入口）----
    // 单写者纪律：与 driveCellRegistry 同上下文（受理点/M1 泵线程 = 注册表单
    // 写者线程）调用；内核线程外调用会与受理点并发改表。管理命令入口（契约
    // 面）随后续批接线，本批交付的是编排语义本身。
    // begin：置缩容意图 + 释放该 cell 名下归属（owner 归零、版本不变）+ 向
    // 其推当前权威视图（在途活动在本地自然排空，World 不再路由新业务）；
    // 返回是否命中已登记 cell。已释放归属的重认领由合格 peer 在下一次
    // 受理/刷新时按最小负载优先承接（版本 +1）。
    bool beginCellRetire(std::uint64_t cell_id);
    // cancel：清除缩容意图（设计稿「尚未退出可取消缩容」）。归属维持现状
    // ——已被他人重认领的不取回（不迁移不重分），仍释放（owner=0）的随下次
    // 受理由合格 peer 承接。返回是否命中已登记 cell。
    bool cancelCellRetire(std::uint64_t cell_id);
    // 缩容意图条目数（观测面，口径同 suspect_cell_count）
    std::size_t retired_cell_count() const;

private:
    struct PendingCellReply {
        std::string fleetctl_peer;
        std::string cell_peer;
        std::uint64_t sent_at_ms = 0;
    };
    // ---- 管理面（全部在管理泵线程内执行：M1 泵每拍驱动）----

    // 模块投递编排受理点（952）：切片2 转发——经注册 cell 连接反向发送；
    // cell 宿主处理后回 953（onCellRolloutReply 按原 fleetctl peer 回传）。
    void onM1ModuleRollout(const std::string& peer, const ModuleRolloutRequest& request);

    // World 排空受理点（956）：切片2 转发——经注册 cell 连接反向发送；
    // cell 宿主处理后回 957（onCellDrainReply 按原 fleetctl peer 回传）。
    void onM1WorldDrain(const std::string& peer, const WorldDrainRequest& request);

    // 脚本调试 eval 受理（944，§7.1 attach transport；泵上下文）：切片2 转
    // 发——经注册 cell 连接反向发送 944；cell 宿主处理后回 945（onCellEvalResult
    // 按原 fleetctl peer 回传）。无合格 cell = 即时回执。
    void onM1DebugEval(const std::string& peer, const DebugEvalRequest& request);

    // cell 注册受理点（960，L101 注册首切片；泵上下文，注册表单线程独占
    // 无锁）：代次规则（world-cell-management §4.4）——同 ID 同代次重注册
    // 幂等刷新，新代次替换（重启换代），旧代次拒绝不可覆盖；非法（cell_id/
    // start_epoch = 0）显式驳回。回执 961 同步回带（管理代次 = §5.7 透传）。
    void onCellRegister(const std::string& peer, const CellRegisterRequest& request);
    // 缩容管理命令受理点（963→966，L104 缩容管理命令面；泵上下文 = 登记表
    // 单写者上下文）：begin 置意图并回池、cancel 只清意图；op 未知/cell_id
    // 非法驳回，目标未登记显式 miss。回执 966 同步回带（管理代次 = §5.7）。
    void onCellRetire(const std::string& peer, const CellRetireRequest& request);
    // 对账自报受理点（967，断连对账批B；泵上下文 = 注册表单写者上下文）：
    // 逐条目核对权威归属表（world-reconciliation 设计稿 §4）——匹配保持；
    // 版本冲突/未登记权威进冲突集（该 Space 冻结：不认领不释放不推送）；
    // 释放态上报 = 收敛滞后不冲突；未登记来源/旧化身上报丢弃。
    void onCellSpaceReport(const std::string& peer, const CellSpaceReport& report);
    // cell 执行端回执受理（切片2 World→cell 命令转发）：cell 宿主经注册
    // 连接反向发送 945/953/957，World 在此按 eval_id/request_id 关联原
    // fleetctl/admin peer 后经 m1Link_ 回传对应回执帧。
    void onCellEvalResult(const std::string& peer, const CellEvalResult& result);
    void onCellRolloutReply(const std::string& peer, const CellRolloutReply& reply);
    void onCellDrainReply(const std::string& peer, const CellDrainReply& reply);
    void onCellIntentAck(const std::string& peer, const CellIntentAck& ack);
    // 注册表过期清扫（960 心跳批；管理泵驱动，同注册表单写者上下文）：
    // last-heard 超 cellExpiryMs 未刷新即整条摘除（心跳失效的 World 侧裁决；
    // 摘除后重注册走受理面首注册路径——代次墓碑随旧执行端隔离批）。0 = 关。
    void driveCellRegistry();
    // 管理日志装载（L101 管理日志切片 → 批A journal v2）：装载在 start()
    // 闸开前（无并发写者）；T/A/R/B 行终态重建 + B 行铸启动代次（见
    // world_app.cpp）。recordEpochWatermark = 内存水位对齐 + 前进才落盘。
    void loadWorldJournal();
    void recordEpochWatermark(std::uint64_t cell_id, std::uint64_t epoch);
    // journal v2 行追加（批A）统一入口：append + flush 即返回。「成功才推
    // 962」的声明闸由调用面裁决——A 行失败回滚认领不声明（write-ahead），
    // R/B 行失败告警降级（批㉚ 可用性优先）。journal 关 = 纯内存语义恒成功。
    bool appendJournalRow(const std::string& row, bool durable = false);
    // 断连存疑标记（L101 断连存疑切片）：M1 on_peer_down 回调（泵上下文 =
    // 管理泵线程，注册表单写者直接改表）。peer 对上登记条目即置 suspect；
    // 未知 peer（gateway/admin 链路）无登记 = 无操作。
    void onPeerLinkDown(const std::string& peer);
    // 在途关联账清扫（断连/过期共用；管理泵线程独占）：目标 cell 断连即
    // 撤账并显式回「cell link lost」；cellExpiryMs 超窗未回执同样清扫。
    void dropCellReplyAccountsForPeer(const std::string& peer);
    void sweepExpiredCellReplyAccounts(std::uint64_t now_ms);
    // 分配推送（962，受理点内同步发）：按本条自报面（ready/drain/load）
    // 裁决认领资格——首分配版本 1、已释放（owner=0）重认领版本 +1、本 cell
    // 已归属的刷新版本不变、归属其它 cell 的推撤销标记（不迁移不重分），
    // 逐条推给该 cell 的 peer 链路；code 1/2 受理都推（断链重拨后分配不丢
    // ——cell 侧按版本去重幂等应用）。
    void pushSpaceAssignments(const std::string& peer, const CellRegistration& reg);
    // 最小负载优先裁决（注册表单只读）：登记表内是否存在就绪、不排空、
    // 未缩容、能力覆盖 needs 且负载严格低于 load 的候选（排除自身）；
    // true = 本 cell 退让。needs 参裁是为了避免合格方把归属让给不合格
    // 的更低负载 peer（让位目标也须有资格）。
    bool lower_load_peer(std::uint64_t cell_id, std::int32_t load,
                         std::int32_t needs) const;
    // 过期释放（driveCellRegistry 同上下文，注册表单写者线程）：该 cell
    // 名下的归属置 0，版本不变、条目保留（见 spaceAssignments_ 注）。
    void releaseCellSpaces(std::uint64_t cell_id);
    // 目标 cell peer 选择（切片2 转发）：首个就绪、未排空、未缩容的登记
    // cell 的链路 peer 自报名；无合格目标返回空串。管理命令（932/944/952/
    // 956）经此寻址转发。
    std::string firstReadyCellPeer() const;
    // 管理命令 cell peer 选择：首个已登记、未缩容、未存疑的 cell（不要求
    // ready/未排空——排空中 cell 仍应接收 952/956 status/begin 命令）。
    std::string firstRegisteredCellPeer() const;
    // eval 回执出口（945 debug_eval_result；peer 按名查链路，未知 = 静默弃）。
    void sendM1EvalReply(const std::string& peer, std::uint64_t eval_id, bool ok,
                         const std::string& result, const std::string& error);

    WorldAppConfig config_;
    std::unique_ptr<M1LinkServer> m1Link_;
    // 管理侧拨号面拍首泵（L123）：set_manager_pre_tick 注入，管理泵每拍调用
    std::function<void(std::uint64_t)> managerPreTick_;

    // M1 eval 关联账（944 受理面；管理泵线程独占）：eval_id → 原请求方
    // 与目标 cell/时戳。回带失败或目标 cell 断连即撤销；剩余在途项随
    // cellExpiryMs 过期清扫。0 = 永不过期（同注册表面口径）。
    std::unordered_map<std::uint64_t, PendingCellReply> evalReplyPeers_;
    // 切片2 转发关联账（World→cell 命令）：944 eval_id → 原 fleetctl peer；
    // 952/956 request_id → 原 fleetctl peer。cell 回执按 id 查表回传。
    std::unordered_map<std::uint64_t, PendingCellReply> rolloutReplyPeers_;
    std::unordered_map<std::uint64_t, PendingCellReply> drainReplyPeers_;
    // cell 注册登记（960，L101 注册首切片）：cell_id → 登记；代次裁决在
    // 受理点（同线程独占无锁）。
    std::unordered_map<std::uint64_t, CellRegistration> cellRegistrations_;
    // 代次水位（L101 旧执行端隔离切片）：cell_id → 本进程曾受理的最高启动
    // 代次（旧化身重注册拒收的裁决事实源；B 行落盘水位）。
    std::unordered_map<std::uint64_t, std::uint64_t> cellEpochWatermarks_;
    // journal 已知归属主（批A 装载终态重建）：cell_id → 装载时刻。重启后
    // 同 cell 同代次首注册不重推该 Space（已归属态重建完成）；新代次替换
    // 才重推（见 loadWorldJournal 终态重建语义）。
    std::unordered_map<std::uint64_t, std::uint64_t> pendingJournalOwners_;
    // 分配表（962）：space_id → 归属 + 版本（单写者上下文）。
    std::unordered_map<std::uint64_t, SpaceAssignment> spaceAssignments_;
    // 对账冲突集（967，断连对账批B）：space_id 集（该 Space 冻结）。
    std::unordered_set<std::uint64_t> conflictedSpaces_;
    std::uint64_t spaceReportCount_{0};
    // 空日志缺失证据（§2）：journal 文件缺失，首个 cell 拨入即形成旧管理
    // 关系证据；true = 全池冻结，不开新分配。
    bool missingJournal_ = false;
    // journal 配置开且启动装载时文件缺失（§2 真空首启候选状态）。
    bool journalMissingAtBoot_ = false;
    // journal 卷锁（L101）：跨重启单写者排他（start 前装载时 acquire）。
    bool volumeLocked_{false};
    // 管理日志批最小对账面：已通过 967 复核且与权威终态一致的 Space 计数。
    std::uint64_t reconciledSpaceCount_ = 0;

    // 管理壳运行态（不内嵌执行内核）
    std::atomic<bool> running_{false};
    // 管理壳泵线程：M1 传输泵 + 注册表推进。
    std::thread managerThread_;
};

} // namespace world_app
