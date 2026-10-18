#pragma once

#include "player_store/config.hpp"
#include "player_store/database_service.hpp"
#include "player_store/m1_server.hpp"
#include "player_store/migration_wire.hpp"
#include "apollo/data/orm/persist_journal.hpp"
#include "apollo/game/session/anchor_manager.hpp"
#include "apollo/game/session/player_directory.hpp"
#include "apollo/game/session/recovery.hpp"
#include "apollo/game/session/session_locator.hpp"
#include "apollo/protocol/messages.hpp"
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <atomic>

namespace player_store {

namespace protocol = apollo::protocol;

// 长期玩家对象与基础业务宿主（baseServer）。
class PlayerStore {
public:
    explicit PlayerStore(const PlayerStoreConfig& config,
                         std::unique_ptr<PlayerStorage> storage = nullptr);
    ~PlayerStore();

    // 启动服务器
    void start();

    // 停止服务器
    void stop();

    // 是否运行中
    bool isRunning() const { return running_; }

    // 停机维护位（G-3，attribute-sync §10.2-①）：true = 停机中，受理面
    // 一律维护中应答（不收新 intent）
    bool is_shutting_down() const { return shuttingDown_.load(); }

    // M1 受理面实际监听端口（start() 后有效；未装配 = 0）——迁移 puller
    // 的拨入地址供数（937.source_host/port）
    std::uint16_t m1_listen_port() const {
        return m1Server_ ? m1Server_->listenPort() : 0;
    }

    // 受理面（start() 注册的 handler 即此件）：按消息类型分派。公开为
    // 停机维护闸的直测入口——stub 传输树无真实 REP 面，socket round-trip
    // 测不了受理语义
    std::vector<uint8_t> dispatchRequest(const std::vector<uint8_t>& data);

    std::shared_ptr<apollo::game::session::PlayerAnchor> activatePlayer(PlayerID playerId);
    bool bindSession(PlayerID playerId, const apollo::game::session::SessionBinding& binding);
    bool unbindSession(protocol::SessionID sessionId);
    std::shared_ptr<apollo::game::session::PlayerAnchor> findAnchor(PlayerID playerId) const;
    std::optional<PlayerID> findPlayerBySession(protocol::SessionID sessionId) const;

    // 目录事件上报源（P1-2，session-and-online-directory §2 事件源①）：
    // bind/unbind 产 SessionUp/SessionDown 同形事件，经 sink 上报 manager
    // 域目录（权威表在 manager）；单进程阶段 sink 由宿主注入（默认
    // 无 sink 静默），跨进程总线上报随 P3。
    void set_directory_event_sink(
        apollo::game::session::PlayerDirectory::EventSink sink);

    // 锚点域快照出口（P3-1 增量③：恢复相位全量重报的供数面——进程壳
    // 从 AnchorManager 快照收集现存会话发 FullReport）
    std::shared_ptr<apollo::game::session::AnchorManager> anchor_manager() const {
        return anchorManager_;
    }

    // ---- 玩家迁移受理面（P0-2 路 2 第一步，backup-revive §9.2 五步
    // ①②③ zone 侧；wire = 契约 933-938，编解码见 base/migration_wire.hpp）----

    // ① 停写（933 freeze）：锚点 journal 段同步排平（write-ahead 已入段 →
    // 同步直写追平档案——低频迁移面 ms 级；保存队列路径照旧服务自动保存），
    // 置 Transferring（只读态——后续 flush 跳过迁移中锚点）。未知玩家 false。
    bool freezePlayer(PlayerID playerId);

    // 迁移快照（936 snapshot 载荷；success=false 时仅 error_message 有义。
    // 定义在 migration_wire.hpp——②读出/②'暂存/③接权与 puller 四面共用）
    using PlayerSnapshotData = migration::SnapshotData;

    // ② 快照（935）：前置 = 已冻结（Transferring）——未冻结拒（防半写态
    // 出快照）。读档案 JSON 原样 + 锚点元数据。
    PlayerSnapshotData snapshotPlayer(PlayerID playerId);

    // ②' 快照落位（新 zone 客户端侧收 936 后暂存——③ handoff 受理时取用；
    // 覆盖语义 = 最后写入者胜；home_zone 随快照走，937 契约不带）
    void storeMigrationSnapshot(PlayerID playerId, PlayerSnapshotData snapshot);

    // ③ 接权（937 handoff → 938 reply）：档案落盘（原子写）+ 建锚（bind
    // session + home zone）+ 置 Online。accepted = 新 epoch 已生效——此后
    // 不可撤销（epoch 单调，旧 epoch 已被判陈旧）。
    bool adoptPlayer(PlayerID playerId, const std::string& profileJson,
                     std::uint64_t sessionId, std::uint32_t gatewayId,
                     std::uint32_t homeZoneId);

private:
    // 处理数据库加载请求
    std::vector<uint8_t> handleDbLoadRequest(const std::vector<uint8_t>& request);

    // 处理数据库保存请求
    std::vector<uint8_t> handleDbSaveRequest(const std::vector<uint8_t>& request);

    // 处理查询请求
    std::vector<uint8_t> handleDbQueryRequest(const std::vector<uint8_t>& request);

    std::vector<uint8_t> handlePlayerActivateRequest(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handlePlayerBindSessionRequest(const std::vector<uint8_t>& request);

    // 玩家迁移族受理（P0-2 路 2；分流见 dispatchRequest 933/935/937 case）
    std::vector<uint8_t> handlePlayerFreezeRequest(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handlePlayerSnapshotRequest(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handlePlayerHandoffRequest(const std::vector<uint8_t>& request);

    // 结算分值入账受理（P1 基础业务与结算迁入；958——World 结算返回业务
    // 请求，锚点写权在本 store；幂等归调用方账本）
    std::vector<uint8_t> handlePlayerScoreCreditRequest(const std::vector<uint8_t>& request);

    // 取下一落库代次（L64 Base 半；storeMutex_ 持有内调用）：账内自增；
    // 缺账播种 = max(调用方已持真源 seed_hint， 现档代次)——每玩家每进程
    // 至多补一次 loadPlayer。返回值即本次落库载荷的 writeGen。
    std::uint64_t nextWriteGen(PlayerID playerId, const std::uint64_t* seed_hint);

    // 落点裁决与路由解析（PLAYER_ASSIGN_WORLD / PLAYER_RESOLVE_ROUTE）已拆出，
    // 归 apps/baseappmgr（目录 + 调度面，BW OnlineDirectory 直系）。

    // 处理心跳
    std::vector<uint8_t> handlePing(const std::vector<uint8_t>& request);

    // 自动保存循环
    void autoSaveLoop();

    // 脏锚点 flush（P0-4）：把 needs_save() 的锚点置 Saving 并入保存队列；
    // 关闭协议与自动保存共用。返回入队数量（占位实现，落库接 P1）。
    std::size_t flushDirtyAnchors(const char* reason);

    // journal write-ahead 与消费侧（P1-5）：appendJournal 返回段全局序
    // （0 = 写盘失败）——随 SaveTask 落档记水位，drain 跳过已被更新落档
    // 覆盖的旧段（旧快照不倒灌）
    std::uint64_t appendJournal(const PlayerData& data);
    std::size_t drainJournal();

    int64_t getCurrentTimeMs() const;
    std::shared_ptr<apollo::game::session::PlayerAnchor> activatePlayerAnchor(PlayerID playerId);
    void finalizeSave(PlayerID playerId, bool success, std::uint64_t journalSeq);

    PlayerStoreConfig config_;
    std::unique_ptr<PlayerStorage> database_;
    std::unique_ptr<SaveQueue> saveQueue_;
    std::unique_ptr<apollo::data::journal::PersistJournal> journal_;
    std::unique_ptr<apollo::game::session::RecoveryCoordinator> recovery_;
    std::shared_ptr<apollo::game::session::AnchorManager> anchorManager_;
    std::shared_ptr<apollo::game::session::SessionLocator> sessionLocator_;
    apollo::game::session::PlayerDirectory::EventSink directoryEventSink_;

    // 存档串行化（P2 存档竞争清账）：锚点态/脏位改动、journal 入段与保存
    // 入队的同一串行点——自动保存、RPC 受理、保存回调、迁移冻结四线在此
    // 原子交错；落盘 I/O 全部在锁外（SaveQueue worker 面按玩家分槽串行，
    // 见 database_service.hpp enqueue）。锁序：storeMutex_ → SaveQueue/
    // PersistJournal 内部锁，反向不入。
    mutable std::mutex storeMutex_;

    std::atomic<bool> running_{false};
    std::atomic<bool> shuttingDown_{false};  // 停机五阶段幂等闸 + 维护位（G-3）

    // M1 base 受理面（P4-4 批③；m1Port = 0 = 不装配）
    std::unique_ptr<PlayerStoreLinkServer> m1Server_;

    // 迁移快照暂存（P0-2 路 2 ②'：收 936 落、③ handoff 受理取用取走）
    std::unordered_map<PlayerID, PlayerSnapshotData> migrationSnapshots_;

    // 已接权威代次高水位（P3-6 重复接管拒绝）：player → 已 adopt 的最大
    // new_epoch。handoff new_epoch ≤ 高水位 = 重复/旧代次接管，拒——迟到的
    // 旧 937 会按暂存快照（或现场重拉旧档）二次 adopt，覆盖目标侧迁移后
    // 新写。进程内存账（同 migrationSnapshots_ 生命周期）；目标进程重启于
    // 迁移窗内 = §9.2 人工介入形态，不自动裁决。
    std::unordered_map<PlayerID, std::uint64_t> adoptedEpochs_;
    // 写端自持写入代次账（L64 Base 半）：player → 本进程已落库代次。
    // 种子 = 激活/接权首载档案代次（重启续代），每次落库前自增——database
    // 半只验单调（高受理/同代幂等/低拒），接管后旧进程无有效账，标记值
    // 必 ≤ database 地板，迟到写被拒。
    std::unordered_map<PlayerID, std::uint64_t> writeGens_;

    // 接权落档水位（存档串行化批）：player → adopt 时的 journal 全局序。
    // drain 回放序 < 水位的条目 = 接权前旧域快照——消费但不落档（接权
    // 直写已是终值，旧条目回放会覆盖它）；水位后的条目为新域写入照常
    // 落档。同 adoptedEpochs_ 生命周期（进程内存账）。
    std::unordered_map<PlayerID, std::uint64_t> adoptWatermarks_;

    // 已落档水位（存档串行化批）：player → 最后落盘 SaveTask 的 journal
    // 段全局序。drain 回放序 ≤ 水位的条目已被更新或同版的落盘覆盖——
    // 消费不落档（防旧快照经 journal 回放倒灌覆盖新数据）；> 水位的条目
    // 是被合并跳过的唯一载荷载体（如 coalesce 被拒的 RPC 保存），照常
    // 经队列落档。同上：进程内存账，重启归零（重启 replay 全量应用 =
    // 恢复语义）。
    std::unordered_map<PlayerID, std::uint64_t> landedSeqs_;

    std::thread autoSaveThread_;
};

// 报告条目收集（P3-1 增量③；资料归属切换后由 baseServer 宿主调用）：
// store 锚点快照 → 目录镜像条目。报告条目 zone = 承载域（调用方宿主域，
// 同 FleetReporter zone 口径）而非锚点 home_zone_id（原区服）：目录条目
// zone 是死亡反查与迁移改指的路由键——迁移后玩家由目标域承载，周期重报
// 若按原区服进 intake 会回卷 ④ 已改指条目（P3-5 迁移链实测修）。冻结
// 锚点（Transferring）离在册集（§9.2 ①）：停写交权中，不再是本域活会话
// ——周期重报不得再以本域 zone 上报（否则 intake 把 ④ 已改指条目回卷
// 源域，目录在两报告方之间来回翻转）。⑤ 未接线时锚点停留 Transferring，
// 即持续缺席重报——目标域独占报告权。
std::vector<apollo::game::session::MirrorEntry> collect_mirror_entries(
    const PlayerStore& store, std::uint32_t hosting_zone);

} // namespace player_store
