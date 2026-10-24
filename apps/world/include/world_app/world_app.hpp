#pragma once

// WorldApp——world 同一二进制的管理角色壳（L98 首切片后）。
//
// 职责分界（todo P2 L98）：模拟循环、WorldHost/Scene/Avatar、AOI、战斗与
// Lua 全部在执行内核 CellApp（cell_app.hpp）；本类型保留管理面——M1 传输
// 端点（m1Link_）、模块投递编排（952）、排空（956）、debug eval（944）受理
// 与推进。执行面经注入缝驱动：出口 = IM1Egress 适配到真传输端点；泵与
// 管理推进 = pre/post tick 钩子（拍首/拍后），全部跑在内核线程内——单写者
// 纪律不破（ADR-015(a)）。
//
// 公共 API 不变：执行面访问全部转发 cell_（外观面，测试与 main 不感知拆分）。

#include "world_app/config.hpp"
#include "world_app/acceptance_queue.hpp"
#include "world_app/m1_link_server.hpp"
#include "world_app/cell_app.hpp"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace apollo::script {
class ModuleRollout;
}

namespace world_app {

namespace protocol = apollo::protocol;

// WorldApp 服务器（manager 角色：传输 + 管理编排；执行内核 CellApp 内嵌）
class WorldApp {
public:
    explicit WorldApp(const WorldAppConfig& config);
    ~WorldApp();

    // 启动服务器（内核闸开 → M1 端点监听；顺序不变式：开闸先于监听）
    void start();

    // 停止服务器（内核停——关闸/尾扫/join；随后 M1 端点停）
    void stop();

    // 是否运行中
    bool isRunning() const { return cell_ && cell_->isRunning(); }

    // 小队副本受理面（P2）：进程内装配完成；测试与后续 M1 入口共用。
    squad_instance::SquadInstanceService* squadInstanceService() noexcept {
        return cell_ ? cell_->squadInstanceService() : nullptr;
    }

    // 受理点判定（M1 面与测试共用）：维护闸闭 / 顶格拒收 = false（M1 面
    // 据此回 2001/2101 即时应答；受理成功不回包，延时应答随 tick 边界消费
    // 后按 request_id 回带，ADR-017）
    bool submitIntent(IntentEnvelope intent) {
        return cell_ && cell_->submitIntent(std::move(intent));
    }

    // 待执行意图数（观测/测试面：tick 边界 drain 断言用）
    std::size_t pendingIntentCount() const {
        return cell_ ? cell_->pendingIntentCount() : 0;
    }

    // M1 链路接收端（M1 上层接线批 2b-ii）：InterServerLink passive 端点，
    // 网关经它投递 cell 意图（924-928 契约族）；listenPort 查实占端口。
    M1LinkServer& m1Link() { return *m1Link_; }

    // 管理侧拨号面拍首钩子（L123 服务注册拨号批）：宿主装配注入需与内核
    // tick 同拍的出站泵面（服务注册拨号客户端等）。在 M1 泵之后、内核线程
    // 每拍执行——线程纪律同 pre_tick 本体（钩子体内不得再回取管理面锁）。
    // start 前后皆可置（钩子体每拍读取成员，无竞态面）；置空 = 无。
    void set_manager_pre_tick(std::function<void(std::uint64_t now_ms)> hook) {
        managerPreTick_ = std::move(hook);
    }

    // scene 反查（内核线程内解析；测试面：eval 宿主接线定位投递目标用）。
    // 注：ScriptRuntime 刻意不外露——scripting-lua §2 一状态一逻辑线程，其
    // 指针只有内核线程可读；管理面对外观察面是 Scene §7.1 debug eval 队列。
    apollo::game::world::Scene* find_scene(std::uint64_t scene_id) {
        return cell_ ? cell_->find_scene(scene_id) : nullptr;
    }

    // 意图源会话 → 玩家号（0 = 未知会话）。互斥会话表的只读反查，跨线程
    // 安全；观测/测试面：受理单写者纪律断言用（submitIntent 不触碰模拟
    // 状态，会话绑定仅在 tick 边界 drain 后显形）。
    std::uint64_t resolveIntentPlayer(std::uint64_t session_id) const {
        return cell_ ? cell_->resolveIntentPlayer(session_id) : 0;
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
    // M1 出口适配（内核缝的宿主侧实现）：IM1Egress → 真传输端点。链路未
    // 装配 = 尽力面落地（同内核「egress 缺席」语义）。
    class EgressAdapter final : public IM1Egress {
    public:
        M1LinkServer* link = nullptr;
        void sendClientDeliver(std::uint64_t session_id, std::uint32_t gateway_id,
                               const std::vector<std::uint8_t>& payload) override {
            if (link) link->sendClientDeliver(session_id, gateway_id, payload);
        }
        void sendIntentAck(std::uint64_t request_id, std::int32_t code,
                           const std::string& message) override {
            if (link) link->sendIntentAck(request_id, code, message);
        }
        std::uint64_t receivedIntents() const override {
            return link ? link->receivedIntents() : 0;
        }
        std::uint64_t rejectedFrames() const override {
            return link ? link->rejectedFrames() : 0;
        }
    };

    // ---- 管理面（全在内核线程内执行：M1 泵挂 pre-tick 钩子，受理点即泵
    // 上下文；三驱动挂 post-tick 钩子，玩法拍后）----

    // 模块投递编排受理点（952，P4 仅代码更新批）：World 本地单目标
    // ModuleRollout（target = 内核 runtime，canary = 自身）prepare/commit/
    // rollback/status，回执 953 即时回带（观察窗随拍自驱）。
    void onM1ModuleRollout(const std::string& peer, const ModuleRolloutRequest& request);
    // 编排推进（post-tick）：观察窗推拍 + promote/rollback 队列驱动 + 在装
    // 模块 onTick 驱动（fault 归因源——canary 窗的 fault 计账靠模块函数真
    // 被调用）。
    void driveModuleRollout();

    // World 排空受理点（956，P4 更新排空批；与 launch/玩法推进同线程，状态
    // 机单线程独占无锁）：op 1=begin（排空闸闭合 + 期限登记）2=status（快照
    // 回带），回执 957 即时回。
    void onM1WorldDrain(const std::string& peer, const WorldDrainRequest& request);
    // 排空推进（post-tick）：draining 期 active_count 归零落 drained；期限到
    // 未清强停剩余副本（abort 取消线 / dismiss 结算线收尾）。drained 终态不
    // 反闸——恢复 = 进程替换。
    void driveWorldDrain();

    // 脚本调试 eval 受理（944，§7.1 attach transport；泵上下文）：定位
    // scene（0 = 默认）→ 未挂宿主/排队失败即时回 945 拒收；受理成功挂关联
    // 账（eval_id → 回传 peer），回执随 tick 窗口 drain 异步回 945。
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
    // 注册表过期清扫（960 心跳批；post-tick 驱动，同注册表单写者上下文）：
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
    // 内核线程，注册表单写者直接改表）。peer 对上登记条目即置 suspect；
    // 未知 peer（gateway/admin 链路）无登记 = 无操作。
    void onPeerLinkDown(const std::string& peer);
    // 分配推送（962，受理点内同步发）：按本条自报面（ready/drain/load）
    // 裁决认领资格——首分配版本 1、已释放（owner=0）重认领版本 +1、本 cell
    // 已归属的刷新版本不变、归属其它 cell 的推撤销标记（不迁移不重分），
    // 逐条推给该 cell 的 peer 链路；code 1/2 受理都推（断链重拨后分配不丢
    // ——cell 侧按版本去重幂等应用）。
    void pushSpaceAssignments(const std::string& peer, const CellRegistration& reg);
    // 最小负载优先裁决（注册表单只读）：登记表内是否存在就绪且不排空、
    // 负载严格低于 load 的候选（排除自身）；true = 本 cell 退让。
    bool lower_load_peer(std::uint64_t cell_id, std::int32_t load) const;
    // 过期释放（driveCellRegistry 同上下文，注册表单写者线程）：该 cell
    // 名下的归属置 0，版本不变、条目保留（见 spaceAssignments_ 注）。
    void releaseCellSpaces(std::uint64_t cell_id);
    // eval 回执出口（945 debug_eval_result；peer 按名查链路，未知 = 静默弃）。
    void sendM1EvalReply(const std::string& peer, std::uint64_t eval_id, bool ok,
                         const std::string& result, const std::string& error);
    // tick 窗口收账（post-tick）：仅在有 M1 eval 在途时 drain 对应 scene——
    // 直接 enqueue 面（测试/程序内消费者）不受挤占。
    void drainDebugEvalResults();

    WorldAppConfig config_;
    // 出口适配先于内核声明（析构序：内核先灭，适配缝指针恒有效）
    EgressAdapter egressAdapter_;
    // 执行内核（L98）：模拟域全量所有权；构造即装配（squadInstanceService
    // 等构造期可用），start/stop 走生命周期
    std::unique_ptr<CellApp> cell_;
    std::unique_ptr<M1LinkServer> m1Link_;
    // 管理侧拨号面拍首泵（L123）：set_manager_pre_tick 注入，pre_tick 内调用
    std::function<void(std::uint64_t)> managerPreTick_;

    // ---- 管理面状态（内核线程独占——受理点与驱动同在钩子上下文）----
    // 模块投递编排（P4 仅代码更新批）：单目标编排器懒建（终态可串行多轮）；
    // 受理过的模块名随轮累积（onTick 驱动面 = fault 归因源）。
    std::unique_ptr<apollo::script::ModuleRollout> moduleRollout_;
    std::vector<std::string> rolloutModuleNames_;
    // 在装模块名（自驱相位跃迁日志面：promoted/rolled back 行带模块名）
    std::string lastRolloutName_;
    // World 排空状态机（P4 更新排空批）。期限 0 = 无期限自然排空；drained
    // 终态不反闸——恢复 = 进程替换（副本临时态随进程消亡）。
    std::int32_t worldDrainState_{kWorldDrainServing};
    std::uint64_t worldDrainDeadlineMs_{0};  // 0 = 无期限（steady 毫秒域）
    std::string worldDrainDetail_;
    // M1 eval 关联账（944 受理面；内核线程独占——受理与 drain 同泵线程）：
    // eval_id → 回传 peer 自报名；scene_id → 在途 eval id 集（drain 圈定面）。
    std::unordered_map<std::uint64_t, std::string> evalReplyPeers_;
    std::unordered_map<std::uint64_t, std::unordered_set<std::uint64_t>> scenePendingEvals_;
    // cell 注册登记（960，L101 注册首切片）：cell_id → 登记；代次裁决在
    // 受理点（旧代次拒、新代次替换、同代次幂等刷新），心跳失效摘除在
    // driveCellRegistry（过期窗 cellExpiryMs）。旧执行端隔离已落地（代次
    // 水位，见 cellEpochWatermarks_）；World 侧断连存疑标记与对账、持久墓
    // 碑（跨 World 重启）随管理日志批，分配完整面随 L108 拓扑批。
    std::unordered_map<std::uint64_t, CellRegistration> cellRegistrations_;
    // 代次水位（L101 旧执行端隔离切片）：cell_id → 本进程曾受理的最高启动
    // 代次。过期摘除忘却登记但水位保留——摘除后旧代次（< 水位）复活拒
    // code 3，同代次（= 水位，同一化身断链回拨）仍首注册受理。内存面：
    // 跨 World 重启的持久墓碑随管理日志批（不假装记得重启前的事）。
    std::unordered_map<std::uint64_t, std::uint64_t> cellEpochWatermarks_;
    // Space 分配表（962 分配面，L101 首切片 + L108 回收切片）：space_id →
    // 分配；认领独占（不重分他人），过期释放 owner→0（版本不变、条目保留
    // 回池待认领，重认领版本 +1 防 ABA——离线旧主经重注册收 ready=0 撤销
    // 标记收敛残留路由）。重平衡策略/拓扑版本随 L108 后续批。
    std::unordered_map<std::uint64_t, SpaceAssignment> spaceAssignments_;
    // journal 已知归属主（批A 装载终态重建）：cell_id → 装载时刻。重启后
    // 未归队前 Space 保持归属（设计稿 §4「过期窗不来」行——不立即另端
    // 重建），过期窗（cellExpiryMs）内不归队即释放回池；真身登记即除名
    //（此后随登记面心跳/过期语义）。时刻与 CellRegistration::
    // registered_at_ms 同为 steady 毫秒，并共用 cellExpiryMs；journal 缺失
    // 时间基准时取 config.bootAtMs，不引入第二套过期判定。
    std::unordered_map<std::uint64_t, std::uint64_t> pendingJournalOwners_;
    // 空日志缺失证据（§2）：journal 文件缺失，首个 cell 拨入即形成旧管理
    // 关系证据；true = 全池冻结，不开新分配。
    bool missingJournal_ = false;
    // journal 配置开且启动装载时文件缺失（§2 真空首启候选状态）。
    bool journalMissingAtBoot_ = false;
    // 持久卷锁已持有（§5/§6）：只锁管理 journal 同名 `.lock` 文件；启动
    // 成功保持到 stop/析构，失败回滚即清。
    bool volumeLocked_ = false;
    // 对账冲突集（批B）：967 自报核对不上的 Space（版本冲突 / 未登记权威
    //——含池外 id）。冻结 = pushSpaceAssignments/releaseCellSpaces 跳过该
    // Space（§4「不推新分配、不释放」）；解冻随对账修复批（journal 补记 +
    // 重推 962，人工命令与否待拍板）。
    std::unordered_set<std::uint64_t> conflictedSpaces_;
    // 管理日志批最小对账面：已通过 967 复核且与权威终态一致的 Space 计数。
    // journal 开启时同步记 C 行；分歧/失联来源不记（分歧仍进批B 冲突集）。
    std::uint64_t reconciledSpaceCount_ = 0;
    // 已裁决自报条目计数（批B 观测面）
    std::uint64_t spaceReportCount_ = 0;
    // 与权威表匹配且已记入管理日志的 967 条数（journal 关 = 0）
    std::uint64_t reconciled_space_count() const { return reconciledSpaceCount_; }
};

} // namespace world_app
