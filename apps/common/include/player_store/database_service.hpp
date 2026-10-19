#pragma once

#include "player_store/config.hpp"
#include "player_store/storage.hpp"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <deque>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace player_store {

using PlayerID = uint64_t;

// 模拟位置/运行字段（L92 拆分）：模拟域所有，随档案暂存最后位置——非长期
// 进度，不参与写入代次语义；未来玩法快照投影（只读，World 侧）不含本段。
// JSON 仍写平面 x/y/z 键：老档案可读、跨进程 wire 不变。
struct PlayerRuntimeState {
    float x = 0;
    float y = 0;
    float z = 0;
};

// 玩家数据（L92：长期字段与模拟位置/运行字段分型，位置入 runtime 子结构）
struct PlayerData {
    // —— 长期字段：档案持久化的玩法进度（写权在 Base 域）——
    PlayerID playerId;
    std::string username;
    int level = 1;
    int64_t exp = 0;
    int hp = 100;
    int maxHp = 100;
    int mp = 50;
    int maxMp = 50;
    // 写端自持写入代次（随档案持久化，database 验单调拒旧写）；0 = 无版本
    std::uint64_t writeGen = 0;

    // —— 模拟位置/运行字段：见 PlayerRuntimeState（L92 拆分）——
    PlayerRuntimeState runtime;

    // 转换为 JSON
    std::string toJson() const;

    // 从 JSON 解析
    static PlayerData fromJson(const std::string& json);
};

// 数据库服务接口
class DatabaseService : public PlayerStorage {
public:
    explicit DatabaseService(const PlayerStoreConfig& config);
    ~DatabaseService() override;

    // 初始化
    bool initialize() override;

    // 关闭
    void shutdown() override;

    // 加载玩家数据（缓存 → 磁盘档案）；不存在返回 false——
    // 不再凭空 bootstrap「player_+id」（P1-4 清账，建号走 createPlayer，账号域 P2）
    bool loadPlayer(PlayerID playerId, PlayerData& outData) override;

    // 保存玩家数据
    bool savePlayer(const PlayerData& data) override;

    bool listPlayers(std::vector<std::uint64_t>& ids) override;

    // 异步保存
    void savePlayerAsync(const PlayerData& data, std::function<void(bool)> callback);

    // 创建新玩家
    bool createPlayer(const PlayerData& data);

    // 删除玩家
    bool deletePlayer(PlayerID playerId);

    // 查询玩家
    bool queryPlayer(const std::string& username, PlayerID& outPlayerId);

    // 批量保存
    void saveBatch(const std::vector<PlayerData>& players);

private:
    PlayerStoreConfig config_;

    // 连接池
    class ConnectionPool;
    std::unique_ptr<ConnectionPool> pool_;

    // 磁盘玩家档案路径（dataDir/player_<id>.json）
    std::string playerPath(PlayerID playerId) const;

    // 全量落盘（tmp + rename 原子替换）
    bool writePlayerFile(const PlayerData& data);

    // 缓存
    std::unordered_map<PlayerID, PlayerData> cache_;
    std::mutex cacheMutex_;
};

// 保存任务
class SaveTask {
public:
    PlayerID playerId;
    PlayerData data;
    std::function<void(bool)> callback;
    int64_t createdAtMs;
    int retryCount = 0;
    std::uint64_t journalSeq = 0;  // 配对 write-ahead 段全局序（落档水位记账）
};

// 保存队列管理器
class SaveQueue {
public:
    // 落盘执行器（P1-4 真链路）：worker 出队后调用，返回落盘结果；
    // 未设置时视为恒成功（空转兼容）
    using Worker = std::function<bool(const PlayerData&)>;

    // maxPending = 排队+在途总数上限（有界队列；0 = 不设界）
    explicit SaveQueue(int workerThreads = 4, std::size_t maxPending = 4096);
    ~SaveQueue();

    // 添加保存任务。按 playerId 分槽（playerId % 槽数）——同一玩家的
    // 任务永远落在同一单线程执行面，落盘顺序即入队顺序，旧快照不得
    // 覆盖新数据（PlayerStore 存档串行化）；不同玩家仍并行。
    // 返回 false = 拒绝：该玩家已有未落盘在途任务（合并跳过），或队列
    // 满载（排队+在途总数达容量上限，背压拒收）。调用方须保留被跳过的
    // 载荷（脏位或 journal）；队列不会更新在途载荷。
    bool enqueue(const SaveTask& task);

    // 设置落盘执行器（start 之前调用）
    void set_worker(Worker worker);

    // 启动
    void start();

    // 停止
    void stop();

    // 获取队列大小
    size_t size() const;

    // 该玩家是否有未落盘任务（排队或在途）——freeze/adopt 写序收口等待用
    bool has_pending_or_inflight(PlayerID playerId) const;

    // worker 面是否仍在服务（stop 后 drain sink 直写分流判据）
    bool is_running() const { return running_.load(); }

private:
    // 单槽消费者：只取自己槽位（playerId 哈希）——同玩家严格串行
    void workerLoop(std::size_t slot);

    Worker worker_;
    std::vector<std::thread> workers_;
    std::vector<std::deque<SaveTask>> slots_;
    std::vector<std::size_t> inflight_;  // 槽内执行中的任务数（合并判定）
    std::size_t pendingTotal_ = 0;       // 排队+在途总数（有界判定，与 slots_/inflight_ 同锁维护）
    std::size_t maxPending_;             // 容量上限（0 = 不设界）
    std::unordered_set<PlayerID> pendingPlayers_;
    mutable std::mutex queueMutex_;
    std::condition_variable queueCV_;
    std::atomic<bool> running_{false};
};

} // namespace player_store
