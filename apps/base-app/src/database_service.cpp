#include "base/database_service.hpp"

#include <chrono>

namespace base {

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
    return true;
}

void DatabaseService::shutdown() {
}

bool DatabaseService::loadPlayer(PlayerID playerId, PlayerData& outData) {
    std::lock_guard<std::mutex> lock(cacheMutex_);

    const auto it = cache_.find(playerId);
    if (it != cache_.end()) {
        outData = it->second;
        return true;
    }

    PlayerData bootstrap{};
    bootstrap.playerId = playerId;
    bootstrap.username = "player_" + std::to_string(playerId);
    cache_[playerId] = bootstrap;
    outData = bootstrap;
    return true;
}

bool DatabaseService::savePlayer(const PlayerData& data) {
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
    std::lock_guard<std::mutex> lock(cacheMutex_);
    return cache_.emplace(data.playerId, data).second;
}

bool DatabaseService::deletePlayer(PlayerID playerId) {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    return cache_.erase(playerId) > 0;
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
    std::lock_guard<std::mutex> lock(cacheMutex_);
    for (const auto& player : players) {
        cache_[player.playerId] = player;
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

        if (task.callback) {
            task.callback(true);
        }
    }
}

} // namespace base
