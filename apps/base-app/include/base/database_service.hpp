#pragma once

#include "base/config.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace base {

using PlayerID = uint64_t;

// 玩家数据
struct PlayerData {
    PlayerID playerId;
    std::string username;
    int level = 1;
    int64_t exp = 0;
    int hp = 100;
    int maxHp = 100;
    int mp = 50;
    int maxMp = 50;
    float x = 0;
    float y = 0;
    float z = 0;

    // 转换为 JSON
    std::string toJson() const;

    // 从 JSON 解析
    static PlayerData fromJson(const std::string& json);
};

// 数据库服务接口
class DatabaseService {
public:
    explicit DatabaseService(const BaseConfig& config);
    ~DatabaseService();

    // 初始化
    bool initialize();

    // 关闭
    void shutdown();

    // 加载玩家数据
    bool loadPlayer(PlayerID playerId, PlayerData& outData);

    // 保存玩家数据
    bool savePlayer(const PlayerData& data);

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
    BaseConfig config_;

    // 连接池
    class ConnectionPool;
    std::unique_ptr<ConnectionPool> pool_;

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
};

// 保存队列管理器
class SaveQueue {
public:
    explicit SaveQueue(int workerThreads = 4);
    ~SaveQueue();

    // 添加保存任务
    void enqueue(const SaveTask& task);

    // 启动
    void start();

    // 停止
    void stop();

    // 获取队列大小
    size_t size() const;

private:
    void workerLoop();

    std::vector<std::thread> workers_;
    std::queue<SaveTask> queue_;
    mutable std::mutex queueMutex_;
    std::condition_variable queueCV_;
    std::atomic<bool> running_{false};
};

} // namespace base
