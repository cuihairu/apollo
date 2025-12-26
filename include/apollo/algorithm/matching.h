/**
 * @file matching.h
 * @brief 玩家匹配算法
 *
 * 适用场景：
 * - PVP 匹配（竞技场、排位赛）
 * - 组队匹配（副本、活动）
 * - 跨服匹配
 *
 * 特性：
 * - 基于 ELO 值的匹配
 * - 基于等待时间的动态放宽
 * - 支持多种匹配模式（1v1, 3v3, 5v5, 组队）
 * - 支持优先级队列（VIP 玩家优先匹配）
 */

#pragma once

#include <cstdint>
#include <vector>
#include <queue>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <cmath>

namespace apollo {
namespace algorithm {

//==============================================================================
// 匹配配置
//==============================================================================

/**
 * @brief 匹配模式
 */
enum class MatchMode {
    OneVOne,       // 1v1
    ThreeVThree,   // 3v3
    FiveVFive,     // 5v5
    Custom         // 自定义
};

/**
 * @brief 匹配请求
 */
struct MatchRequest {
    uint64_t playerId;       // 玩家 ID
    uint32_t teamId;         // 队伍 ID（0 表示单人）
    int32_t rating;          // ELO/积分
    int32_t minRating;       // 最低接受对手积分
    int32_t maxRating;       // 最高接受对手积分
    int64_t waitTimeMs;      // 已等待时间
    int64_t requestTime;     // 请求时间戳

    // 优先级（VIP 等级等）
    int32_t priority = 0;

    // 扩展数据（用于自定义匹配逻辑）
    void* userData = nullptr;

    MatchRequest() : playerId(0), teamId(0), rating(0),
                     minRating(0), maxRating(0), waitTimeMs(0),
                     requestTime(0), priority(0), userData(nullptr) {}

    MatchRequest(uint64_t pid, int32_t r)
        : playerId(pid), teamId(0), rating(r),
          minRating(r - 100), maxRating(r + 100),
          waitTimeMs(0), requestTime(getCurrentTimeMs()) {}
};

/**
 * @brief 匹配结果
 */
struct MatchResult {
    std::vector<uint64_t> team1;  // 队伍 1 玩家 IDs
    std::vector<uint64_t> team2;  // 队伍 2 玩家 IDs
    int32_t avgRatingDiff;        // 平均积分差

    // 扩展数据
    void* userData = nullptr;
};

//==============================================================================
// 匹配器配置
//==============================================================================

/**
 * @brief 匹配器配置
 */
struct MatcherConfig {
    MatchMode mode = MatchMode::FiveVFive;

    int32_t initialRatingTolerance = 100;  // 初始积分容忍度
    int32_t maxRatingTolerance = 500;      // 最大积分容忍度
    int64_t toleranceExpandTimeMs = 30000; // 容忍度扩展时间（30秒）

    int32_t teamSize = 5;                 // 每队人数
    size_t maxWaitTimeMs = 300000;         // 最大等待时间（5分钟）

    bool enablePriority = true;            // 启用优先级匹配
    bool enableFastMatch = false;          // 启用快速匹配（降低要求）
};

//==============================================================================
// ELO 评分系统
//==============================================================================

/**
 * @brief ELO 评分计算
 *
 * 用于：
 * - 计算匹配后的积分变化
 * - 预测胜率
 */
class EloRating {
public:
    /**
     * @brief 计算预期胜率
     * @param playerRating 玩家积分
     * @param opponentRating 对手积分
     * @return 预期胜率（0.0 - 1.0）
     */
    static double expectedScore(double playerRating, double opponentRating) {
        return 1.0 / (1.0 + std::pow(10.0, (opponentRating - playerRating) / 400.0));
    }

    /**
     * @brief 计算新积分
     * @param playerRating 玩家积分
     * @param expectedScore 预期得分
     * @param actualScore 实际得分（1=胜, 0.5=平, 0=负）
     * @param kFactor K因子（默认 32）
     * @return 新积分
     */
    static double calculateNewRating(double playerRating,
                                      double expectedScore,
                                      double actualScore,
                                      double kFactor = 32.0) {
        return playerRating + kFactor * (actualScore - expectedScore);
    }

    /**
     * @brief 批量计算新积分（用于多人比赛）
     */
    static std::vector<double> calculateNewRatings(
            const std::vector<double>& ratings,
            const std::vector<double>& actualScores,
            double kFactor = 32.0) {

        std::vector<double> newRatings;
        newRatings.reserve(ratings.size());

        for (size_t i = 0; i < ratings.size(); ++i) {
            double expected = 0.0;

            // 计算预期得分
            for (size_t j = 0; j < ratings.size(); ++j) {
                if (i != j) {
                    expected += expectedScore(ratings[i], ratings[j]) /
                              static_cast<double>(ratings.size() - 1);
                }
            }

            newRatings.push_back(calculateNewRating(
                ratings[i], expected, actualScores[i], kFactor));
        }

        return newRatings;
    }

private:
    static double expectedScore(double playerRating, double opponentRating) {
        return 1.0 / (1.0 + std::pow(10.0, (opponentRating - playerRating) / 400.0));
    }
};

//==============================================================================
// 匹配队列（优先级队列）
//==============================================================================

/**
 * @brief 匹配队列（按优先级排序）
 */
class MatchQueue {
public:
    /**
     * @brief 比较器（优先级高、等待时间长的优先）
     */
    struct Compare {
        bool operator()(const MatchRequest& a, const MatchRequest& b) const {
            if (a.priority != b.priority) {
                return a.priority < b.priority;  // 优先级大的在前
            }
            return a.waitTimeMs < b.waitTimeMs;  // 等待时间长的在前
        }
    };

    /**
     * @brief 添加请求
     */
    void push(const MatchRequest& request) {
        std::lock_guard lock(mutex_);
        queue_.push(request);
        cond_.notify_one();
    }

    /**
     * @brief 弹出请求
     */
    bool pop(MatchRequest& request, int64_t timeoutMs = 100) {
        std::unique_lock lock(mutex_);

        if (queue_.empty()) {
            cond_.wait_for(lock, std::chrono::milliseconds(timeoutMs));
        }

        if (queue_.empty()) {
            return false;
        }

        request = queue_.top();
        queue_.pop();
        return true;
    }

    /**
     * @brief 移除请求
     */
    bool remove(uint64_t playerId) {
        std::lock_guard lock(mutex_);

        // 由于优先队列不支持随机删除，需要重建
        std::vector<MatchRequest> temp;
        bool found = false;

        while (!queue_.empty()) {
            if (queue_.top().playerId == playerId) {
                found = true;
                queue_.pop();
            } else {
                temp.push_back(queue_.top());
                queue_.pop();
            }
        }

        for (const auto& req : temp) {
            queue_.push(req);
        }

        return found;
    }

    /**
     * @brief 更新等待时间
     */
    void updateWaitTimes(int64_t deltaMs) {
        std::lock_guard lock(mutex_);

        std::vector<MatchRequest> temp;
        while (!queue_.empty()) {
            MatchRequest req = queue_.top();
            queue_.pop();
            req.waitTimeMs += deltaMs;
            temp.push_back(req);
        }

        for (const auto& req : temp) {
            queue_.push(req);
        }
    }

    size_t size() const {
        std::lock_guard lock(mutex_);
        return queue_.size();
    }

    bool empty() const {
        std::lock_guard lock(mutex_);
        return queue_.empty();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cond_;
    std::priority_queue<MatchRequest, std::vector<MatchRequest>, Compare> queue_;
};

//==============================================================================
// 匹配器
//==============================================================================

/**
 * @brief 玩家匹配器
 */
class PlayerMatcher {
public:
    using MatchCallback = std::function<void(const MatchResult&)>;
    using TimeoutCallback = std::function<void(uint64_t)>;

    /**
     * @brief 构造函数
     */
    explicit PlayerMatcher(const MatcherConfig& config = MatcherConfig{})
        : config_(config)
        , running_(false)
        , nextMatchId_(1) {

        // 根据模式设置队伍大小
        switch (config_.mode) {
            case MatchMode::OneVOne:
                config_.teamSize = 1;
                break;
            case MatchMode::ThreeVThree:
                config_.teamSize = 3;
                break;
            case MatchMode::FiveVFive:
                config_.teamSize = 5;
                break;
            default:
                break;
        }
    }

    ~PlayerMatcher() {
        stop();
    }

    //==========================================================================
    // 匹配控制
    //==========================================================================

    /**
     * @brief 启动匹配器
     */
    bool start() {
        if (running_.exchange(true)) {
            return true;  // 已启动
        }

        workerThread_ = std::thread([this]() { matchLoop(); });
        return true;
    }

    /**
     * @brief 停止匹配器
     */
    void stop() {
        running_.store(false);
        if (workerThread_.joinable()) {
            workerThread_.join();
        }
    }

    //==========================================================================
    // 匹配请求
    //==========================================================================

    /**
     * @brief 添加匹配请求
     * @param playerId 玩家 ID
     * @param rating 积分
     * @return 匹配 ID
     */
    uint64_t addPlayer(uint64_t playerId, int32_t rating) {
        MatchRequest request(playerId, rating);

        std::lock_guard lock(pendingMutex_);
        pending_[playerId] = request;
        matchQueue_.push(request);

        return nextMatchId_++;
    }

    /**
     * @brief 取消匹配
     */
    bool cancel(uint64_t playerId) {
        std::lock_guard lock(pendingMutex_);

        if (pending_.erase(playerId) > 0) {
            matchQueue_.remove(playerId);
            return true;
        }

        return false;
    }

    //==========================================================================
    // 回调设置
    //==========================================================================

    /**
     * @brief 设置匹配成功回调
     */
    void setMatchCallback(MatchCallback callback) {
        matchCallback_ = std::move(callback);
    }

    /**
     * @brief 设置超时回调
     */
    void void setTimeoutCallback(TimeoutCallback callback) {
        timeoutCallback_ = std::move(callback);
    }

private:
    //==========================================================================
    // 匹配循环
    //==========================================================================

    void matchLoop() {
        auto lastUpdateTime = std::chrono::steady_clock::now();

        while (running_.load()) {
            // 尝试匹配
            tryMatch();

            // 更新等待时间
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - lastUpdateTime).count();

            if (elapsed >= 1000) {  // 每秒更新一次
                matchQueue_.updateWaitTimes(elapsed);
                lastUpdateTime = now;

                // 检查超时
                checkTimeouts();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    /**
     * @brief 尝试匹配
     */
    void tryMatch() {
        // 收集待匹配玩家
        std::vector<MatchRequest> candidates;

        {
            std::lock_guard lock(pendingMutex_);
            if (pending_.size() < config_.teamSize * 2) {
                return;  // 人数不够
            }

            candidates.reserve(pending_.size());
            for (const auto& [id, req] : pending_) {
                candidates.push_back(req);
            }
        }

        // 计算当前容忍度
        int64_t avgWaitTime = calculateAverageWaitTime(candidates);
        int32_t tolerance = calculateTolerance(avgWaitTime);

        // 尝试分组
        auto match = tryFindMatch(candidates, tolerance);

        if (match) {
            // 从待匹配列表中移除已匹配的玩家
            {
                std::lock_guard lock(pendingMutex_);
                for (uint64_t pid : match->team1) {
                    pending_.erase(pid);
                    matchQueue_.remove(pid);
                }
                for (uint64_t pid : match->team2) {
                    pending_.erase(pid);
                    matchQueue_.remove(pid);
                }
            }

            // 触发回调
            if (matchCallback_) {
                matchCallback_(*match);
            }
        }
    }

    /**
     * @brief 查找匹配
     */
    std::optional<MatchResult> tryFindMatch(
            const std::vector<MatchRequest>& candidates,
            int32_t tolerance) {

        if (candidates.size() < static_cast<size_t>(config_.teamSize * 2)) {
            return std::nullopt;
        }

        // 简化实现：按积分排序后分组
        std::vector<MatchRequest> sorted = candidates;
        std::sort(sorted.begin(), sorted.end(),
            [](const MatchRequest& a, const MatchRequest& b) {
                return a.rating > b.rating;
            });

        // 找到积分最接近的两个队伍
        for (size_t i = 0; i + config_.teamSize * 2 <= sorted.size(); ++i) {
            int32_t team1Avg = 0;
            int32_t team2Avg = 0;

            for (size_t j = 0; j < config_.teamSize; ++j) {
                team1Avg += sorted[i + j].rating;
                team2Avg += sorted[i + config_.teamSize + j].rating;
            }

            team1Avg /= config_.teamSize;
            team2Avg /= config_.teamSize;

            int32_t diff = std::abs(team1Avg - team2Avg);

            if (diff <= tolerance) {
                // 找到匹配
                MatchResult result;
                result.avgRatingDiff = diff;

                for (size_t j = 0; j < config_.teamSize; ++j) {
                    result.team1.push_back(sorted[i + j].playerId);
                    result.team2.push_back(sorted[i + config_.teamSize + j].playerId);
                }

                return result;
            }
        }

        return std::nullopt;
    }

    /**
     * @brief 计算当前容忍度
     */
    int32_t calculateTolerance(int64_t avgWaitTime) const {
        int32_t tolerance = config_.initialRatingTolerance;

        // 随等待时间线性增加
        tolerance += static_cast<int32_t>(
            (config_.maxRatingTolerance - config_.initialRatingTolerance) *
            static_cast<double>(avgWaitTime) / config_.toleranceExpandTimeMs
        );

        return std::min(tolerance, config_.maxRatingTolerance);
    }

    /**
     * @brief 计算平均等待时间
     */
    int64_t calculateAverageWaitTime(const std::vector<MatchRequest>& candidates) const {
        if (candidates.empty()) {
            return 0;
        }

        int64_t sum = 0;
        for (const auto& req : candidates) {
            sum += req.waitTimeMs;
        }

        return sum / static_cast<int64_t>(candidates.size());
    }

    /**
     * @brief 检查超时
     */
    void checkTimeouts() {
        std::lock_guard lock(pendingMutex_);

        int64_t now = getCurrentTimeMs();
        std::vector<uint64_t> toRemove;

        for (const auto& [id, req] : pending_) {
            if (now - req.requestTime > config_.maxWaitTimeMs) {
                toRemove.push_back(id);
            }
        }

        for (uint64_t id : toRemove) {
            pending_.erase(id);
            matchQueue_.remove(id);

            if (timeoutCallback_) {
                timeoutCallback_(id);
            }
        }
    }

    static int64_t getCurrentTimeMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    MatcherConfig config_;
    MatchQueue matchQueue_;

    std::unordered_map<uint64_t, MatchRequest> pending_;
    mutable std::mutex pendingMutex_;

    std::atomic<bool> running_;
    std::thread workerThread_;

    std::atomic<uint64_t> nextMatchId_;

    MatchCallback matchCallback_;
    TimeoutCallback timeoutCallback_;
};

//==============================================================================
// 快速匹配（简化版）
//==============================================================================

/**
 * @brief 快速匹配器（单次匹配，无队列）
 *
 * 适用于需要立即匹配的场景。
 */
class QuickMatcher {
public:
    /**
     * @brief 从候选列表中创建对战
     *
     * @param candidates 候选玩家列表
     * @param teamSize 每队人数
     * @param tolerance 积分容忍度
     * @return 匹配结果
     */
    static std::optional<MatchResult> match(
            const std::vector<MatchRequest>& candidates,
            size_t teamSize = 5,
            int32_t tolerance = 100) {

        if (candidates.size() < teamSize * 2) {
            return std::nullopt;
        }

        // 按积分排序
        std::vector<size_t> indices(candidates.size());
        for (size_t i = 0; i < candidates.size(); ++i) {
            indices[i] = i;
        }

        std::sort(indices.begin(), indices.end(),
            [&candidates](size_t a, size_t b) {
                return candidates[a].rating > candidates[b].rating;
            });

        // 尝试创建最接近的对战
        for (size_t i = 0; i + teamSize * 2 <= indices.size(); ++i) {
            int32_t team1Rating = 0;
            int32_t team2Rating = 0;

            for (size_t j = 0; j < teamSize; ++j) {
                team1Rating += candidates[indices[i + j]].rating;
                team2Rating += candidates[indices[i + teamSize + j]].rating;
            }

            int32_t diff = std::abs(team1Rating - team2Rating) / static_cast<int32_t>(teamSize);

            if (diff <= tolerance) {
                MatchResult result;
                result.avgRatingDiff = diff;

                for (size_t j = 0; j < teamSize; ++j) {
                    result.team1.push_back(candidates[indices[i + j]].playerId);
                    result.team2.push_back(candidates[indices[i + teamSize + j]].playerId);
                }

                return result;
            }
        }

        return std::nullopt;
    }

    /**
     * @brief 多场对战生成（锦标赛用）
     */
    static std::vector<MatchResult> createMatches(
            const std::vector<MatchRequest>& candidates,
            size_t teamSize = 5) {

        std::vector<MatchResult> results;
        std::vector<bool> used(candidates.size(), false);

        for (size_t i = 0; i + teamSize * 2 <= candidates.size(); ++i) {
            if (used[i]) continue;

            MatchResult result;

            for (size_t j = 0; j < teamSize * 2; ++j) {
                if (!used[i + j]) {
                    if (j < teamSize) {
                        result.team1.push_back(candidates[i + j].playerId);
                    } else {
                        result.team2.push_back(candidates[i + j].playerId);
                    }
                    used[i + j] = true;
                }
            }

            if (result.team1.size() == teamSize && result.team2.size() == teamSize) {
                results.push_back(std::move(result));
            }
        }

        return results;
    }
};

} // namespace algorithm
} // namespace apollo
