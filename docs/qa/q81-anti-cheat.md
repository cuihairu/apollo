# Q81: 如何防止外挂？

## 问题分析

本题考察对外挂防护的理解：
- 外挂类型分析
- 服务端验证
- 客户端检测
- 行为分析

---

## 一、外挂类型

### 1.1 常见外挂分类

```
┌─────────────────────────────────────────────────────────────┐
│                    外挂类型                                    │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 修改器类                                                 │
│  ├── 内存修改器 (修改金币、血量)                             │
│  ├── 速度修改器 (加速游戏)                                  │
│  └── 封包修改器 (修改网络数据)                               │
│                                                             │
│  2. 自动化类                                                 │
│  ├── 脚本挂 (自动打怪、任务)                                │
│  ├── 按键模拟器 (连点器)                                    │
│  └── AI 自动操作                                            │
│                                                             │
│  3. 透视类                                                   │
│  ├── 显示隐藏敌人 (透视)                                    │
│  ├── 显示物品掉落                                           │
│  └── 自动瞄准 (自瞄)                                        │
│                                                             │
│  4. 破解类                                                   │
│  ├── 破解协议 (伪造消息)                                    │
│  ├── 绕过检测                                               │
│  └── 私服外挂                                               │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、服务端验证

### 2.1 权威服务端

```cpp
// 服务端权威架构

class ServerAuthoritative {
public:
    // 客户端请求移动
    void onMoveRequest(uint64_t playerId, const Vector3& newPos) {
        Player* player = getPlayer(playerId);
        if (!player) return;

        // 1. 验证移动合理性
        if (!validateMove(player, newPos)) {
            kickPlayer(playerId, "Invalid move");
            return;
        }

        // 2. 服务端计算实际位置
        Vector3 actualPos = calculateActualPosition(player, newPos);

        // 3. 更新服务端状态
        player->setPosition(actualPos);

        // 4. 广播给周围玩家
        broadcastMove(player, actualPos);
    }

    // 客户端请求攻击
    void onAttackRequest(uint64_t playerId, uint64_t targetId) {
        Player* attacker = getPlayer(playerId);
        Entity* target = getEntity(targetId);

        if (!attacker || !target) return;

        // 1. 验证攻击条件
        if (!validateAttack(attacker, target)) {
            return;
        }

        // 2. 服务端计算伤害
        int damage = calculateDamage(attacker, target);

        // 3. 应用伤害
        target->takeDamage(damage);

        // 4. 通知结果
        sendAttackResult(playerId, targetId, damage);
    }

private:
    bool validateMove(Player* player, const Vector3& newPos) {
        // 检查距离
        float distance = player->getPosition().distanceTo(newPos);
        float maxDistance = player->getSpeed() * getDeltaTime();

        if (distance > maxDistance * 1.5f) {  // 允许 50% 误差
            logSuspiciousActivity(player->getId(), "Too fast movement");
            return false;
        }

        // 检查地图边界
        if (!isInMapBounds(newPos)) {
            return false;
        }

        // 检查障碍物
        if (isColliding(newPos)) {
            return false;
        }

        return true;
    }

    bool validateAttack(Player* attacker, Entity* target) {
        // 检查距离
        float distance = attacker->getPosition().distanceTo(target->getPosition());
        if (distance > attacker->getAttackRange()) {
            return false;
        }

        // 检查冷却
        if (attacker->isOnCooldown()) {
            return false;
        }

        // 检查目标是否可见
        if (!hasLineOfSight(attacker, target)) {
            return false;
        }

        return true;
    }
};
```

### 2.2 状态校验

```cpp
// 玩家状态校验

class PlayerValidator {
public:
    // 综合校验
    ValidationResult validate(Player* player, const ClientState& state) {
        ValidationResult result;
        result.valid = true;

        // 检查位置
        if (!validatePosition(player, state.position)) {
            result.valid = false;
            result.reasons.push_back("Invalid position");
        }

        // 检查血量
        if (!validateHP(player, state.hp)) {
            result.valid = false;
            result.reasons.push_back("HP mismatch");
        }

        // 检查属性
        if (!validateAttributes(player, state.attributes)) {
            result.valid = false;
            result.reasons.push_back("Invalid attributes");
        }

        // 检查背包
        if (!validateInventory(player, state.inventory)) {
            result.valid = false;
            result.reasons.push_back("Inventory mismatch");
        }

        return result;
    }

    // 位置校验
    bool validatePosition(Player* player, const Vector3& clientPos) {
        Vector3 serverPos = player->getPosition();

        // 允许一定误差 (网络延迟)
        float tolerance = 0.5f;
        if (serverPos.distanceTo(clientPos) > tolerance) {
            // 记录可疑
            player->addSuspicionScore(10);
            return false;
        }

        return true;
    }

    // 属性校验
    bool validateAttributes(Player* player, const Attributes& clientAttrs) {
        Attributes serverAttrs = player->getAttributes();

        // 检查基础属性是否被修改
        if (clientAttrs.baseStrength != serverAttrs.baseStrength) {
            return false;
        }

        // 计算客户端应该的属性
        Attributes expected = calculateAttributes(player);
        if (std::abs(clientAttrs.attack - expected.attack) > 1) {
            return false;
        }

        return true;
    }

private:
    struct ValidationResult {
        bool valid;
        std::vector<std::string> reasons;
    };
};
```

---

## 三、客户端检测

### 3.1 反调试检测

```cpp
// 客户端反调试检测

class AntiDebug {
public:
    static bool isDebuggerPresent() {
        #ifdef _WIN32
        return IsDebuggerPresent();
        #else
        // Linux: 检查 TracerPid
        std::ifstream file("/proc/self/status");
        std::string line;
        while (std::getline(file, line)) {
            if (line.find("TracerPid:") == 0) {
                int pid = std::stoi(line.substr(10));
                return pid > 0;
            }
        }
        return false;
        #endif
    }

    static bool isAttached() {
        // 检查是否有调试器附加
        #ifdef _WIN32
        return CheckRemoteDebuggerPresent(GetCurrentProcess(), nullptr);
        #else
        return false;
        #endif
    }

    static void antiCheatCheck() {
        if (isDebuggerPresent()) {
            // 检测到调试器，退出或上报
            exit(0);
        }

        if (isAttached()) {
            exit(0);
        }
    }
};
```

### 3.2 进程检测

```cpp
// 检测已知外挂进程

class ProcessDetector {
public:
    static bool isSuspiciousProcessRunning() {
        const std::vector<std::string> suspiciousProcesses = {
            "cheatengine",
            "x64dbg",
            "ollydbg",
            "ida",
            "Wireshark",
            "Cheat Engine"
        };

        #ifdef _WIN32
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(snapshot, &pe32)) {
            do {
                std::string processName = pe32.szExeFile;
                std::transform(processName.begin(), processName.end(),
                             processName.begin(), ::tolower);

                for (const auto& susp : suspiciousProcesses) {
                    if (processName.find(susp) != std::string::npos) {
                        CloseHandle(snapshot);
                        return true;
                    }
                }
            } while (Process32Next(snapshot, &pe32));
        }

        CloseHandle(snapshot);
        #endif

        return false;
    }
};
```

---

## 四、行为分析

### 4.1 异常行为检测

```cpp
// 玩家行为分析

class BehaviorAnalyzer {
public:
    struct PlayerStats {
        uint64_t playerId;
        std::vector<float> moveSpeeds;
        std::vector<float> reactionTimes;
        std::vector<float> clickIntervals;
        uint64_t attacksPerSecond;
        uint64_t hitsPerSecond;
    };

    void analyzeBehavior(Player* player) {
        PlayerStats stats = collectStats(player);

        // 1. 分析移动速度
        if (analyzeMoveSpeed(stats)) {
            flagPlayer(player->getId(), "Suspicious move speed");
        }

        // 2. 分析反应时间
        if (analyzeReactionTime(stats)) {
            flagPlayer(player->getId(), "Inhuman reaction time");
        }

        // 3. 分析命中率
        if (analyzeAccuracy(stats)) {
            flagPlayer(player->getId(), "Too high accuracy");
        }

        // 4. 分析点击模式
        if (analyzeClickPattern(stats)) {
            flagPlayer(player->getId(), "Bot-like clicking");
        }
    }

private:
    bool analyzeMoveSpeed(const PlayerStats& stats) {
        // 计算平均移动速度
        float avgSpeed = 0;
        for (float speed : stats.moveSpeeds) {
            avgSpeed += speed;
        }
        avgSpeed /= stats.moveSpeeds.size();

        // 检查是否异常
        return avgSpeed > getMaxNormalSpeed() * 1.3f;
    }

    bool analyzeReactionTime(const PlayerStats& stats) {
        // 计算平均反应时间
        float avgReaction = 0;
        for (float time : stats.reactionTimes) {
            avgReaction += time;
        }
        avgReaction /= stats.reactionTimes.size();

        // 反应时间过短 (<100ms) 可疑
        return avgReaction < 0.1f;
    }

    bool analyzeAccuracy(const PlayerStats& stats) {
        if (stats.attacksPerSecond == 0) return false;

        float hitRate = (float)stats.hitsPerSecond / stats.attacksPerSecond;

        // 命中率过高 (>95%) 可疑
        return hitRate > 0.95f;
    }

    bool analyzeClickPattern(const PlayerStats& stats) {
        if (stats.clickIntervals.size() < 10) return false;

        // 检查点击间隔是否过于一致 (机器人特征)
        float variance = calculateVariance(stats.clickIntervals);

        return variance < 0.01f;  // 方差太小
    }
};
```

### 4.2 统计异常检测

```cpp
// 基于统计的异常检测

class StatisticalDetector {
public:
    // 使用 3-sigma 规则检测异常
    template<typename T>
    static bool isAnomaly(const T& value, const std::vector<T>& samples) {
        if (samples.size() < 10) return false;

        // 计算均值
        T mean = std::accumulate(samples.begin(), samples.end(), T(0));
        mean /= samples.size();

        // 计算标准差
        T variance = 0;
        for (const auto& s : samples) {
            T diff = s - mean;
            variance += diff * diff;
        }
        variance /= samples.size();
        T stddev = std::sqrt(variance);

        // 3-sigma 规则
        return std::abs(value - mean) > 3 * stddev;
    }

    // 移动平均检测
    static float getMovingAverage(const std::deque<float>& values) {
        if (values.empty()) return 0.0f;

        float sum = std::accumulate(values.begin(), values.end(), 0.0f);
        return sum / values.size();
    }
};
```

---

## 五、KBEngine 安全实践

### 5.1 KBEngine 安全机制

```python
# KBEngine 安全机制

class Security:
    """
    KBEngine 安全机制:

    1. 服务端权威计算
    2. 消息验证
    3. 速率限制
    4. 异常检测
    """

    @staticmethod
    def validate_client_message(player, message, method):
        """验证客户端消息"""

        # 1. 检查消息序列号
        if not Security.check_sequence(player, message):
            return False, "Invalid sequence"

        # 2. 检查时间戳
        if not Security.check_timestamp(message):
            return False, "Invalid timestamp"

        # 3. 检查签名
        if not Security.check_signature(player, message):
            return False, "Invalid signature"

        # 4. 检查速率
        if not Security.check_rate_limit(player, method):
            return False, "Rate limit exceeded"

        return True, "OK"

    @staticmethod
    def check_sequence(player, message):
        """检查消息序列号"""
        expected = player.next_sequence
        actual = message.sequence

        if actual < expected:
            # 旧消息，可能是重放攻击
            KBEngine.warning(f"Old message from {player.id}")
            return False

        if actual > expected + 10:
            # 序列号跳跃过大
            KBEngine.warning(f"Sequence jump from {player.id}")
            return False

        player.next_sequence = actual + 1
        return True

    @staticmethod
    def check_timestamp(message):
        """检查时间戳"""
        now = int(time.time() * 1000)
        msg_time = message.timestamp

        # 允许 5 秒的时间差
        return abs(now - msg_time) < 5000

    @staticmethod
    def check_signature(player, message):
        """检查消息签名"""
        # 使用共享密钥验证签名
        key = get_shared_key(player.id)
        expected = hmac_sha256(key, message.data)
        return hmac.compare_digest(expected, message.signature)
```

---

## 六、最佳实践

### 6.1 安全设计原则

| 原则 | 说明 |
|------|------|
| **服务端权威** | 所有重要计算在服务端 |
| **不信任客户端** | 客户端消息必须验证 |
| **最小权限** | 客户端只获得必要信息 |
| **纵深防御** | 多层防护 |
| **持续监控** | 实时检测异常 |

### 6.2 防护层次

```
防护层次:

1. 网络层: 加密传输、防重放
2. 协议层: 消息签名、序列号
3. 逻辑层: 服务端验证、状态校验
4. 数据层: 完整性检查、日志审计
5. 行为层: 异常检测、机器学习
```

---

## 七、总结

### 外挂防护核心

```
防外挂 = 服务端权威 + 客户端检测 + 行为分析 + 持续更新
- 永远不信任客户端
- 所有关键逻辑服务端计算
- 多维度综合判断
- 及时更新对抗策略
```

---

## 参考资料

- [Game Anti-Cheat Best Practices](https://www.gamedeveloper.com/)
- [Valve Anti-Cheat](https://www.valvesoftware.com/)
- [Easy Anti-Cheat](https://www.eac.com/)
