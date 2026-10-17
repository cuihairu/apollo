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
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <atomic>

namespace player_store {

namespace protocol = apollo::protocol;

// BaseApp 服务器
class PlayerStore {
public:
    explicit PlayerStore(const PlayerStoreConfig& config);
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

    // 落点裁决与路由解析（PLAYER_ASSIGN_WORLD / PLAYER_RESOLVE_ROUTE）已拆出，
    // 归 apps/baseappmgr（目录 + 调度面，BW OnlineDirectory 直系）。

    // 处理心跳
    std::vector<uint8_t> handlePing(const std::vector<uint8_t>& request);

    // 自动保存循环
    void autoSaveLoop();

    // 脏锚点 flush（P0-4）：把 needs_save() 的锚点置 Saving 并入保存队列；
    // 关闭协议与自动保存共用。返回入队数量（占位实现，落库接 P1）。
    std::size_t flushDirtyAnchors(const char* reason);

    // journal write-ahead 与消费侧（P1-5）
    std::size_t appendJournal(const PlayerData& data);
    std::size_t drainJournal();

    int64_t getCurrentTimeMs() const;
    std::shared_ptr<apollo::game::session::PlayerAnchor> activatePlayerAnchor(PlayerID playerId);
    void finalizeSave(PlayerID playerId, bool success);

    PlayerStoreConfig config_;
    std::unique_ptr<DatabaseService> database_;
    std::unique_ptr<SaveQueue> saveQueue_;
    std::unique_ptr<apollo::data::journal::PersistJournal> journal_;
    std::unique_ptr<apollo::game::session::RecoveryCoordinator> recovery_;
    std::shared_ptr<apollo::game::session::AnchorManager> anchorManager_;
    std::shared_ptr<apollo::game::session::SessionLocator> sessionLocator_;
    apollo::game::session::PlayerDirectory::EventSink directoryEventSink_;

    std::atomic<bool> running_{false};
    std::atomic<bool> shuttingDown_{false};  // 停机五阶段幂等闸 + 维护位（G-3）

    // M1 base 受理面（P4-4 批③；m1Port = 0 = 不装配）
    std::unique_ptr<PlayerStoreLinkServer> m1Server_;

    // 迁移快照暂存（P0-2 路 2 ②'：收 936 落、③ handoff 受理取用取走）
    std::unordered_map<PlayerID, PlayerSnapshotData> migrationSnapshots_;

    std::thread autoSaveThread_;
};

} // namespace player_store
