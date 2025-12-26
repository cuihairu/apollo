/**
 * @file skill_system_demo.cpp
 * @brief 技能系统示例 - 使用时间轮和最小堆
 *
 * 演示内容：
 * 1. 时间轮管理技能冷却
 * 2. 最小堆管理技能释放优先级
 * 3. 对象池复用技能事件对象
 * 4. Buff/Debuff 效果管理
 */

#include "apollo/algorithm/timing_wheel.h"
#include "apollo/algorithm/min_heap.h"
#include "apollo/algorithm/object_pool.h"

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <unordered_map>

using namespace apollo::utils;

//==============================================================================
// 技能定义
//==============================================================================

/**
 * @brief 技能类型
 */
enum class SkillType {
    Active,      // 主动技能
    Passive,     // 被动技能
    Channeling,  // 引导技能
    Toggle       // 开关技能
};

/**
 * @brief 技能优先级（用于最小堆排序）
 */
enum class SkillPriority {
    Low = 0,
    Normal = 1,
    High = 2,
    Ultimate = 3
};

/**
 * @brief 技能定义
 */
struct Skill {
    uint32_t id;              // 技能 ID
    std::string name;         // 技能名称
    SkillType type;           // 技能类型
    int32_t cooldownMs;       // 冷却时间（毫秒）
    SkillPriority priority;   // 释放优先级
    uint32_t manaCost;        // 法力消耗

    // 运行时状态
    int64_t lastCastTime = 0; // 上次释放时间
    bool isReady = true;      // 是否就绪
};

//==============================================================================
// 技能事件（用于对象池复用）
//==============================================================================

/**
 * @brief 技能事件（定时任务）
 */
struct SkillEvent {
    uint64_t playerId;
    uint32_t skillId;
    int64_t triggerTime;
    bool isCancelled = false;

    void reset() {
        playerId = 0;
        skillId = 0;
        triggerTime = 0;
        isCancelled = false;
    }
};

//==============================================================================
// Buff/Debuff 系统
//==============================================================================

/**
 * @brief 效果类型
 */
enum class BuffType {
    Buff,      // 增益
    Debuff,    // 减益
    Control    // 控制
};

/**
 * @brief Buff 定义
 */
struct Buff {
    uint32_t id;
    std::string name;
    BuffType type;
    int32_t durationMs;    // 持续时间
    int32_t tickIntervalMs; // 跳动间隔（DoT 效果）

    // 效果参数
    int32_t value;         // 数值（如攻击力加成）
    bool stackable;        // 是否可叠加
    int32_t maxStacks;     // 最大叠加层数

    // 运行时状态
    int32_t currentStacks = 1;
    int64_t applyTime;
    int64_t lastTickTime;
};

/**
 * @brief Buff 定时任务
 */
class BuffTimer : public ITimerTask {
public:
    BuffTimer(uint64_t playerId, const Buff& buff, std::function<void(const Buff&)> onTick)
        : playerId_(playerId)
        , buff_(buff)
        , onTick_(std::move(onTick))
        , cancelled_(false) {

        expiryMs_ = getCurrentTimeMs() + buff_.durationMs;
    }

    void run() override {
        if (!cancelled_) {
            if (onTick_) {
                onTick_(buff_);
            }
        }
    }

    int64_t getExpiryMs() const override {
        return expiryMs_;
    }

    bool isCancelled() const override {
        return cancelled_.load(std::memory_order_acquire);
    }

    void cancel() override {
        cancelled_.store(true, std::memory_order_release);
    }

    const Buff& buff() const { return buff_; }
    uint64_t playerId() const { return playerId_; }

private:
    static int64_t getCurrentTimeMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    uint64_t playerId_;
    Buff buff_;
    std::function<void(const Buff&)> onTick_;
    std::atomic<bool> cancelled_;
    int64_t expiryMs_;
};

//==============================================================================
// 技能系统（使用时间轮管理冷却）
//==============================================================================

/**
 * @brief 技能系统
 *
 * 使用时间轮管理：
 * - 技能冷却
 * - 引导技能持续时间
 * - Buff 效果持续时间
 */
class SkillSystem {
public:
    explicit SkillSystem(uint64_t playerId)
        : playerId_(playerId)
        , cooldownWheel_({10, 512, 4})  // 10ms tick, 512 slots, 4 wheels
        , buffWheel_({100, 256, 5})      // 100ms tick for Buffs
        , nextCastId_(1) {

        // 初始化技能池
        initSkills();

        // 启动处理线程
        running_ = true;
        tickThread_ = std::thread([this]() { tickLoop(); });
    }

    ~SkillSystem() {
        running_ = false;
        if (tickThread_.joinable()) {
            tickThread_.join();
        }
    }

    /**
     * @brief 释放技能
     * @return 是否成功释放
     */
    bool castSkill(uint32_t skillId) {
        auto it = skills_.find(skillId);
        if (it == skills_.end()) {
            std::cout << "[SkillSystem] Skill " << skillId << " not found\n";
            return false;
        }

        Skill& skill = it->second;

        // 检查冷却
        if (!skill.isReady) {
            int64_t remainingMs = skill.cooldownMs -
                (getCurrentTimeMs() - skill.lastCastTime);
            std::cout << "[SkillSystem] Skill " << skill.name
                      << " is on cooldown (" << remainingMs << "ms remaining)\n";
            return false;
        }

        // 执行技能效果
        executeSkill(skill);

        // 更新冷却
        skill.lastCastTime = getCurrentTimeMs();
        skill.isReady = false;

        // 添加冷却定时器
        cooldownWheel_.add(skill.cooldownMs, [this, skillId]() {
            onCooldownReady(skillId);
        });

        return true;
    }

    /**
     * @brief 添加 Buff
     */
    void addBuff(const Buff& buff) {
        uint64_t buffId = nextBuffId_++;

        // 创建 Buff 定时器
        auto timer = std::make_shared<BuffTimer>(
            playerId_,
            buff,
            [this, buffId](const Buff& b) {
                onBuffExpire(buffId, b);
            }
        );

        buffWheel_.add(buff.durationMs, timer);

        activeBuffs_[buffId] = {
            .buff = buff,
            .timer = timer,
            .applyTime = getCurrentTimeMs()
        };

        std::cout << "[SkillSystem] Buff " << buff.name << " added to player "
                  << playerId_ << " (" << buff.durationMs << "ms)\n";
    }

    /**
     * @brief 移除 Buff
     */
    void removeBuff(uint32_t buffId) {
        auto it = activeBuffs_.find(buffId);
        if (it != activeBuffs_.end()) {
            it->second.timer->cancel();
            activeBuffs_.erase(it);
            std::cout << "[SkillSystem] Buff " << buffId << " removed\n";
        }
    }

    /**
     * @brief 获取技能状态
     */
    void printSkillStatus() const {
        std::cout << "\n=== Skill Status ===\n";

        for (const auto& [id, skill] : skills_) {
            std::cout << "  " << skill.name << " (ID:" << id << "): ";

            if (skill.isReady) {
                std::cout << "READY";
            } else {
                int64_t remainingMs = skill.cooldownMs -
                    (getCurrentTimeMs() - skill.lastCastTime);
                std::cout << "CD (" << remainingMs << "ms)";
            }

            std::cout << "\n";
        }

        std::cout << "\n=== Active Buffs ===\n";
        for (const auto& [id, data] : activeBuffs_) {
            int64_t remainingMs = data.buff.durationMs -
                (getCurrentTimeMs() - data.applyTime);
            std::cout << "  " << data.buff.name << " (" << remainingMs << "ms remaining)\n";
        }

        std::cout << "\n";
    }

    /**
     * @brief 获取统计信息
     */
    void printStats() const {
        std::cout << "\n=== Statistics ===\n";
        std::cout << "Cooldown Wheel: " << cooldownWheel_.size() << " tasks\n";
        std::cout << "Buff Wheel: " << buffWheel_.size() << " buffs\n";
        std::cout << "===================\n\n";
    }

private:
    /**
     * @brief 初始化技能列表
     */
    void initSkills() {
        skills_ = {
            {1, {1, "Fireball", SkillType::Active, 5000, SkillPriority::Normal, 50}},
            {2, {2, "Ice Lance", SkillType::Active, 3000, SkillPriority::High, 30}},
            {3, {3, "Heal", SkillType::Active, 10000, SkillPriority::High, 80}},
            {4, {4, "Lightning Bolt", SkillType::Active, 2000, SkillPriority::Normal, 25}},
            {5, {5, "Meteor", SkillType::Active, 60000, SkillPriority::Ultimate, 200}},
            {6, {6, "Auto Attack", SkillType::Active, 1000, SkillPriority::Low, 0}},
        };
    }

    /**
     * @brief 执行技能效果
     */
    void executeSkill(Skill& skill) {
        uint64_t castId = nextCastId_++;

        std::cout << "[SkillSystem] Player " << playerId_
                  << " casts " << skill.name
                  << " (Cast ID: " << castId << ")\n";

        // 引导技能特殊处理
        if (skill.type == SkillType::Channeling) {
            // 添加引导完成定时器
            // cooldownWheel_.add(2000, [this, skillId]() {
            //     onChannelingComplete(skillId);
            // });
        }

        // 技能命中效果可以立即应用
        applySkillEffect(skill);
    }

    /**
     * @brief 应用技能效果
     */
    void applySkillEffect(Skill& skill) {
        std::cout << "  -> Effect applied: " << skill.name << "\n";

        // 某些技能会附加 Buff
        if (skill.id == 1) {  // Fireball
            Buff burn;
            burn.id = 101;
            burn.name = "Burn";
            burn.type = BuffType::Debuff;
            burn.durationMs = 5000;
            burn.tickIntervalMs = 1000;
            burn.value = 100;
            burn.stackable = true;
            burn.maxStacks = 3;

            addBuff(burn);
        } else if (skill.id == 2) {  // Ice Lance
            Buff slow;
            slow.id = 102;
            slow.name = "Slowed";
            slow.type = BuffType::Debuff;
            slow.durationMs = 3000;
            slow.value = 30;  // 30% slow

            addBuff(slow);
        }
    }

    /**
     * @brief 冷却就绪回调
     */
    void onCooldownReady(uint32_t skillId) {
        auto it = skills_.find(skillId);
        if (it != skills_.end()) {
            it->second.isReady = true;
            std::cout << "[SkillSystem] Skill " << it->second.name << " is now READY\n";
        }
    }

    /**
     * @brief Buff 过期回调
     */
    void onBuffExpire(uint64_t buffId, const Buff& buff) {
        auto it = activeBuffs_.find(buffId);
        if (it != activeBuffs_.end()) {
            std::cout << "[SkillSystem] Buff " << buff.name << " expired\n";
            activeBuffs_.erase(it);
        }
    }

    /**
     * @brief 时间轮处理循环
     */
    void tickLoop() {
        int64_t lastTick = getCurrentTimeMs();

        while (running_) {
            int64_t now = getCurrentTimeMs();

            // 处理冷却时间轮
            auto cooldownTasks = cooldownWheel_.tick(now);
            for (auto& task : cooldownTasks) {
                task->run();
            }

            // 处理 Buff 时间轮
            auto buffTasks = buffWheel_.tick(now);
            for (auto& task : buffTasks) {
                task->run();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    static int64_t getCurrentTimeMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    uint64_t playerId_;
    TimingWheel cooldownWheel_;
    TimingWheel buffWheel_;

    std::unordered_map<uint32_t, Skill> skills_;
    std::unordered_map<uint32_t, Buff> buffs_;
    std::unordered_map<uint64_t, BuffTimer> buffTimers_;

    struct ActiveBuff {
        Buff buff;
        std::shared_ptr<BuffTimer> timer;
        int64_t applyTime;
    };
    std::unordered_map<uint64_t, ActiveBuff> activeBuffs_;

    std::atomic<uint64_t> nextBuffId_{1};
    std::atomic<uint64_t> nextCastId_{1};

    std::thread tickThread_;
    std::atomic<bool> running_{false};
};

//==============================================================================
// 技能优先级队列（使用最小堆）
//==============================================================================

/**
 * @brief 技能释放请求（用于优先级队列）
 */
struct CastRequest {
    uint64_t playerId;
    uint32_t skillId;
    SkillPriority priority;
    int64_t requestTime;

    // 用于比较（最小堆，优先级高的在顶）
    bool operator<(const CastRequest& other) const {
        if (priority != other.priority) {
            return static_cast<int>(priority) > static_cast<int>(other.priority);
        }
        return requestTime > other.requestTime;  // 同优先级，时间短的优先
    }
};

/**
 * @brief 技能调度器（使用最小堆）
 */
class SkillScheduler {
public:
    SkillScheduler() = default;

    /**
     * @brief 添加释放请求
     */
    void addCastRequest(const CastRequest& request) {
        requestHeap_.push(request);
    }

    /**
     * @brief 处理下一个请求
     */
    bool processNext() {
        if (requestHeap_.empty()) {
            return false;
        }

        CastRequest request = requestHeap_.pop();

        std::cout << "[Scheduler] Processing cast request: "
                  << "Player " << request.playerId
                  << ", Skill " << request.skillId
                  << ", Priority " << static_cast<int>(request.priority)
                  << "\n";

        // 这里会调用 SkillSystem::castSkill()
        return true;
    }

    /**
     * @brief 处理所有请求
     */
    void processAll() {
        while (!requestHeap_.empty()) {
            processNext();
        }
    }

    /**
     * @brief 取消玩家的所有请求
     */
    void cancelPlayerRequests(uint64_t playerId) {
        // 需要遍历堆并移除（实际实现可能需要更高效的方式）
        std::vector<CastRequest> remaining;

        while (!requestHeap_.empty()) {
            CastRequest req = requestHeap_.pop();
            if (req.playerId != playerId) {
                remaining.push_back(req);
            }
        }

        for (const auto& req : remaining) {
            requestHeap_.push(req);
        }
    }

    size_t pendingCount() const {
        return requestHeap_.size();
    }

private:
    MinHeap<CastRequest> requestHeap_;
};

//==============================================================================
// 对象池示例（复用技能事件）
//==============================================================================

/**
 * @brief 技能事件管理器（使用对象池）
 */
class SkillEventManager {
public:
    SkillEventManager() : eventPool_(16, 1000, 2, false, true) {}

    /**
     * @brief 创建事件
     */
    std::shared_ptr<SkillEvent> createEvent(uint64_t playerId, uint32_t skillId) {
        // 从对象池获取
        SkillEvent* raw = eventPool_.allocate();

        if (!raw) {
            // 池已满，创建新对象
            return std::make_shared<SkillEvent>(SkillEvent{playerId, skillId, getCurrentTimeMs()});
        }

        raw->playerId = playerId;
        raw->skillId = skillId;
        raw->triggerTime = getCurrentTimeMs();
        raw->isCancelled = false;

        // 使用自定义删除器归还到池
        return std::shared_ptr<SkillEvent>(raw, [this](SkillEvent* ptr) {
            eventPool_.deallocate(ptr);
        });
    }

    void printStats() const {
        auto stats = eventPool_.getStats();
        std::cout << "[EventPool] Total: " << stats.totalAllocated
                  << ", Free: " << stats.currentlyFree
                  << ", In Use: " << stats.currentlyInUse
                  << ", Hit Rate: " << (stats.hitRate * 100) << "%\n";
    }

private:
    static int64_t getCurrentTimeMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    ObjectPool<SkillEvent, false> eventPool_;
};

//==============================================================================
// 主函数
//==============================================================================

int main() {
    std::cout << "========================================\n";
    std::cout << "    Skill System Demo\n";
    std::cout << "========================================\n\n";

    // 创建技能系统
    SkillSystem skillSystem(1001);  // 玩家 ID = 1001

    // 创建调度器
    SkillScheduler scheduler;

    // 创建事件管理器
    SkillEventManager eventManager;

    //==========================================================================
    // 示例 1: 技能释放和冷却
    //==========================================================================

    std::cout << "\n--- Test 1: Skill Casting ---\n";

    skillSystem.castSkill(1);  // Fireball (5s CD)
    skillSystem.castSkill(2);  // Ice Lance (3s CD)
    skillSystem.castSkill(3);  // Heal (10s CD)

    skillSystem.printSkillStatus();

    // 尝试在冷却中释放
    std::cout << "\nTrying to cast Fireball again...\n";
    skillSystem.castSkill(1);

    // 等待冷却
    std::this_thread::sleep_for(std::chrono::seconds(4));
    skillSystem.printSkillStatus();

    std::this_thread::sleep_for(std::chrono::seconds(2));
    skillSystem.printSkillStatus();

    //==========================================================================
    // 示例 2: 优先级队列
    //==========================================================================

    std::cout << "\n--- Test 2: Priority Queue ---\n";

    scheduler.addCastRequest({1001, 6, SkillPriority::Low, getCurrentTimeMs()});      // Auto Attack
    scheduler.addCastRequest({1001, 5, SkillPriority::Ultimate, getCurrentTimeMs()}); // Meteor
    scheduler.addCastRequest({1001, 1, SkillPriority::Normal, getCurrentTimeMs()});   // Fireball

    std::cout << "Processing " << scheduler.pendingCount() << " requests...\n";
    scheduler.processAll();

    //==========================================================================
    // 示例 3: Buff 系统
    //==========================================================================

    std::cout << "\n--- Test 3: Buff System ---\n";

    Buff poison;
    poison.id = 201;
    poison.name = "Poison";
    poison.type = BuffType::Debuff;
    poison.durationMs = 8000;
    poison.value = 50;

    skillSystem.addBuff(poison);
    skillSystem.printSkillStatus();

    std::this_thread::sleep_for(std::chrono::seconds(5));
    skillSystem.printSkillStatus();

    std::this_thread::sleep_for(std::chrono::seconds(4));
    skillSystem.printSkillStatus();

    //==========================================================================
    // 示例 4: 对象池
    //==========================================================================

    std::cout << "\n--- Test 4: Object Pool ---\n";

    {
        auto event1 = eventManager.createEvent(1001, 1);
        auto event2 = eventManager.createEvent(1001, 2);
        auto event3 = eventManager.createEvent(1001, 3);

        eventManager.printStats();
    }

    // 事件离开作用域，自动归还
    eventManager.printStats();

    //==========================================================================
    // 统计信息
    //==========================================================================

    skillSystem.printStats();

    std::cout << "\n========================================\n";
    std::cout << "    Demo Complete\n";
    std::cout << "========================================\n";

    return 0;
}

int64_t getCurrentTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
