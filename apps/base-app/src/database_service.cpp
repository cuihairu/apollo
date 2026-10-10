#include "base/database_service.hpp"
#include "apollo/core/log/log_manager.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace base {

namespace fs = std::filesystem;

//==============================================================================
// PlayerData 序列化（P1-4：fromJson/toJson 全字段对称；定义自 base_server.cpp
// 归位——PlayerData 声明在本文件对应的 database_service.hpp）
//==============================================================================

std::string PlayerData::toJson() const {
    std::stringstream ss;
    ss << "{"
       << "\"playerId\":" << playerId << ","
       << "\"username\":\"" << username << "\","
       << "\"level\":" << level << ","
       << "\"exp\":" << exp << ","
       << "\"hp\":" << hp << ","
       << "\"maxHp\":" << maxHp << ","
       << "\"mp\":" << mp << ","
       << "\"maxMp\":" << maxMp << ","
       << "\"x\":" << x << ","
       << "\"y\":" << y << ","
       << "\"z\":" << z
       << "}";
    return ss.str();
}

PlayerData PlayerData::fromJson(const std::string& json) {
    PlayerData data;
    // 简化解析，实际应该用 nlohmann/json
    // 这里只做基础实现
    size_t pos = 0;

    auto extractString = [&json, &pos](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\":\"";
        size_t p = json.find(search, pos);
        if (p == std::string::npos) return "";
        p += search.length();
        size_t end = json.find("\"", p);
        if (end == std::string::npos) return "";
        std::string result = json.substr(p, end - p);
        pos = end + 1;
        return result;
    };

    auto extractInt = [&json, &pos](const std::string& key) -> int {
        std::string search = "\"" + key + "\":";
        size_t p = json.find(search, pos);
        if (p == std::string::npos) return 0;
        p += search.length();
        size_t end = json.find_first_of(",}", p);
        if (end == std::string::npos) return 0;
        pos = end + 1;
        return std::stoi(json.substr(p, end - p));
    };

    auto extractInt64 = [&json](const std::string& key) -> int64_t {
        std::string search = "\"" + key + "\":";
        size_t p = json.find(search);
        if (p == std::string::npos) return 0;
        p += search.length();
        size_t end = json.find_first_of(",}", p);
        if (end == std::string::npos) return 0;
        return std::stoll(json.substr(p, end - p));
    };

    // 浮点抽取（P1-4 对称修复：fromJson 此前缺 x/y/z，round-trip 丢位置）
    auto extractFloat = [&json](const std::string& key) -> float {
        std::string search = "\"" + key + "\":";
        size_t p = json.find(search);
        if (p == std::string::npos) return 0.0f;
        p += search.length();
        size_t end = json.find_first_of(",}", p);
        if (end == std::string::npos) return 0.0f;
        return std::stof(json.substr(p, end - p));
    };

    data.playerId = static_cast<PlayerID>(extractInt64("playerId"));
    data.username = extractString("username");
    data.level = extractInt("level");
    data.exp = extractInt64("exp");
    data.hp = extractInt("hp");
    data.maxHp = extractInt("maxHp");
    data.mp = extractInt("mp");
    data.maxMp = extractInt("maxMp");
    data.x = extractFloat("x");
    data.y = extractFloat("y");
    data.z = extractFloat("z");

    return data;
}


class DatabaseService::ConnectionPool {
public:
    explicit ConnectionPool(const BaseConfig& config)
        : config_(config) {
    }

private:
    BaseConfig config_;
};

DatabaseService::DatabaseService(const BaseConfig& config)
    : config_(config)
    , pool_(std::make_unique<ConnectionPool>(config)) {
}

DatabaseService::~DatabaseService() = default;

bool DatabaseService::initialize() {
    std::error_code ec;
    fs::create_directories(config_.dataDir, ec);
    if (ec) {
        apollo::core::log::global_log_manager()
            .createLogger("database_service")
            ->error(std::string("create data dir failed: ") + config_.dataDir
                    + " (" + ec.message() + ")");
        return false;
    }
    return true;
}

void DatabaseService::shutdown() {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    cache_.clear();
}

std::string DatabaseService::playerPath(PlayerID playerId) const {
    std::ostringstream os;
    os << config_.dataDir << "/player_" << playerId << ".json";
    return os.str();
}

bool DatabaseService::writePlayerFile(const PlayerData& data) {
    const std::string path = playerPath(data.playerId);
    const std::string tmp = path + ".tmp";

    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) {
            return false;
        }
        out << data.toJson();
        out.flush();
        if (!out) {
            return false;
        }
    }

    // 原子替换：崩溃不留半文件
    std::error_code ec;
    fs::rename(tmp, path, ec);
    if (ec) {
        std::error_code ignore;
        fs::remove(tmp, ignore);
        return false;
    }
    return true;
}

bool DatabaseService::loadPlayer(PlayerID playerId, PlayerData& outData) {
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        const auto it = cache_.find(playerId);
        if (it != cache_.end()) {
            outData = it->second;
            return true;
        }
    }

    // 磁盘档案（P1-4 真链路 load 侧）；不存在即失败——不再凭空 bootstrap
    std::ifstream in(playerPath(playerId));
    if (!in) {
        return false;
    }
    std::ostringstream os;
    os << in.rdbuf();
    if (os.str().empty()) {
        return false;
    }

    PlayerData data = PlayerData::fromJson(os.str());
    data.playerId = playerId;  // 路径即权威，防载荷缺失

    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        cache_[playerId] = data;
    }
    outData = data;
    return true;
}

bool DatabaseService::savePlayer(const PlayerData& data) {
    if (!writePlayerFile(data)) {
        return false;
    }
    std::lock_guard<std::mutex> lock(cacheMutex_);
    cache_[data.playerId] = data;
    return true;
}

void DatabaseService::savePlayerAsync(const PlayerData& data, std::function<void(bool)> callback) {
    const bool success = savePlayer(data);
    if (callback) {
        callback(success);
    }
}

bool DatabaseService::createPlayer(const PlayerData& data) {
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        if (!cache_.emplace(data.playerId, data).second) {
            return false;
        }
    }
    if (!writePlayerFile(data)) {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        cache_.erase(data.playerId);
        return false;
    }
    return true;
}

bool DatabaseService::deletePlayer(PlayerID playerId) {
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        if (cache_.erase(playerId) == 0) {
            return false;
        }
    }
    std::error_code ec;
    fs::remove(playerPath(playerId), ec);
    return true;
}

bool DatabaseService::queryPlayer(const std::string& username, PlayerID& outPlayerId) {
    std::lock_guard<std::mutex> lock(cacheMutex_);

    for (const auto& [player_id, player] : cache_) {
        if (player.username == username) {
            outPlayerId = player_id;
            return true;
        }
    }

    return false;
}

void DatabaseService::saveBatch(const std::vector<PlayerData>& players) {
    for (const auto& player : players) {
        savePlayer(player);
    }
}

SaveQueue::SaveQueue(int workerThreads)
    : workers_(static_cast<std::size_t>(workerThreads)) {
}

SaveQueue::~SaveQueue() {
    stop();
}

void SaveQueue::enqueue(const SaveTask& task) {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        queue_.push(task);
    }
    queueCV_.notify_one();
}

void SaveQueue::set_worker(Worker worker) {
    worker_ = std::move(worker);
}

void SaveQueue::start() {
    if (running_.exchange(true)) {
        return;
    }

    for (auto& worker : workers_) {
        worker = std::thread(&SaveQueue::workerLoop, this);
    }
}

void SaveQueue::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    queueCV_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

size_t SaveQueue::size() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return queue_.size();
}

void SaveQueue::workerLoop() {
    while (running_) {
        SaveTask task;

        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCV_.wait_for(lock, std::chrono::milliseconds(100), [this] {
                return !running_ || !queue_.empty();
            });

            if (!running_ && queue_.empty()) {
                return;
            }

            if (queue_.empty()) {
                continue;
            }

            task = queue_.front();
            queue_.pop();
        }

        // P1-4 真链路落盘分支：worker 未设置时视为成功（空转兼容）
        const bool ok = worker_ ? worker_(task.data) : true;
        if (task.callback) {
            task.callback(ok);
        }
    }
}

} // namespace base
