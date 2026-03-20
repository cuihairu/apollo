# Q83: 如何防止加速挂？

## 问题分析

本题考察对加速挂防护的理解：
- 加速挂原理
- 速度检测
- 时间验证
- 异常处理

---

## 一、加速挂原理

### 1.1 工作原理

```
┌─────────────────────────────────────────────────────────────┐
│                    加速挂原理                                  │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  方法 1: 修改系统时钟                                        │
│  ├── 修改系统时间让游戏认为时间变快                          │
│  └── 容易被检测                                            │
│                                                             │
│  方法 2: Hook 游戏函数                                       │
│  ├── Hook Sleep/Wait 函数使其立即返回                        │
│  ├── Hook 读取时间函数返回伪造值                             │
│  └── 需要检测异常                                            │
│                                                             │
│  方法 3: 内存修改                                           │
│  ├── 修改游戏内速度变量                                      │
│  ├── 修改冷却时间                                            │
│  └── 服务端可检测                                           │
│                                                             │
│  方法 4: 网络加速                                           │
│  ├── 修改延迟时间戳                                          │
│  ├── 重放旧包                                              │
│  └── 需要序列号检测                                        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、服务端检测

### 2.1 时间戳验证

```cpp
// 时间戳验证

class TimeValidator {
public:
    // 验证客户端消息时间戳
    bool validateTimestamp(uint64_t playerId, uint64_t clientTimestamp) {
        uint64_t serverTime = getCurrentTimeMillis();

        // 1. 检查时间戳是否在未来
        if (clientTimestamp > serverTime + 1000) {
            logSuspicious(playerId, "Future timestamp detected");
            return false;
        }

        // 2. 检查时间戳是否太久过去
        if (serverTime - clientTimestamp > 30000) {  // 30秒
            return false;  // 超时消息
        }

        // 3. 检查与上次消息的时间间隔
        PlayerTimeInfo& info = playerTimeInfo_[playerId];

        if (info.lastTimestamp > 0) {
            uint64_t clientDelta = clientTimestamp - info.lastTimestamp;
            uint64_t serverDelta = serverTime - info.lastServerTime;

            // 客户端时间差应该接近服务器时间差
            // 允许网络延迟误差
            uint64_t maxDelta = std::max(clientDelta, serverDelta);

            if (serverDelta > maxDelta * 2) {
                // 时间差异常，可能是加速挂
                info.suspicionScore += 10;

                if (info.suspicionScore > 50) {
                    kickPlayer(playerId, "Speed hack detected");
                    return false;
                }
            }
        }

        info.lastTimestamp = clientTimestamp;
        info.lastServerTime = serverTime;

        return true;
    }

    // 验证动作间隔
    bool validateActionInterval(uint64_t playerId, const std::string& action,
                                uint64_t actualInterval) {
        PlayerTimeInfo& info = playerTimeInfo_[playerId];

        uint64_t expectedInterval = getExpectedInterval(action);
        float ratio = (float)actualInterval / expectedInterval;

        // 如果实际间隔远小于预期间隔，可能是加速
        if (ratio < 0.5f) {  // 比预期快一倍
            logSuspicious(playerId, "Action too fast: " + action);

            info.speedHackCount++;

            if (info.speedHackCount > 5) {
                kickPlayer(playerId, "Speed hack detected");
                return false;
            }
        }

        return true;
    }

private:
    struct PlayerTimeInfo {
        uint64_t lastTimestamp = 0;
        uint64_t lastServerTime = 0;
        int suspicionScore = 0;
        int speedHackCount = 0;
    };

    std::unordered_map<uint64_t, PlayerTimeInfo> playerTimeInfo_;
};
```

### 2.2 位置验证

```cpp
// 速度检测 (位置验证)

class SpeedDetector {
public:
    // 验证移动速度
    bool validateMoveSpeed(uint64_t playerId, const Vector3& from,
                           const Vector3& to, uint64_t deltaTime) {
        float distance = from.distanceTo(to);

        // 计算允许的最大速度
        Player* player = getPlayer(playerId);
        float maxSpeed = getMaxSpeed(player);

        // 实际速度
        float actualSpeed = distance / (deltaTime / 1000.0f);

        // 检查是否超速
        if (actualSpeed > maxSpeed * 1.5f) {  // 允许 50% 误差
            PlayerStats& stats = playerStats_[playerId];

            stats.violationCount++;
            stats.lastViolationTime = getCurrentTime();

            if (stats.violationCount > 5) {
                kickPlayer(playerId, "Speed hack detected");
                return false;
            }

            // 服务端修正位置
            Vector3 correctedPos = calculateCorrectedPosition(from, to,
                                                               deltaTime, maxSpeed);
            sendPositionCorrection(playerId, correctedPos);

            return false;
        }

        return true;
    }

    // 验证冷却时间
    bool validateCooldown(uint64_t playerId, int skillId, uint64_t clientTime) {
        Player* player = getPlayer(playerId);
        if (!player) return false;

        // 获取技能冷却
        uint64_t lastUseTime = player->getSkillLastUse(skillId);
        uint64_t cooldown = getSkillCooldown(skillId);

        uint64_t serverTime = getCurrentTimeMillis();
        uint64_t actualElapsed = serverTime - lastUseTime;

        // 检查冷却是否结束
        if (actualElapsed < cooldown) {
            // 客户端声称可以使用，但实际冷却未结束
            logSuspicious(playerId, "Cooldown hack detected");
            return false;
        }

        return true;
    }

private:
    struct PlayerStats {
        int violationCount = 0;
        uint64_t lastViolationTime = 0;
    };

    std::unordered_map<uint64_t, PlayerStats> playerStats_;
};
```

---

## 三、客户端检测

### 3.1 时间函数检测

```cpp
// 客户端时间函数完整性检测

class ClientTimeChecker {
public:
    // 初始化原始时间函数
    static void initialize() {
        // 保存原始函数指针
        originalGetTickCount_ = GetTickCount;
        originalQueryPerformanceCounter_ = QueryPerformanceCounter;

        // 计算真实时间基准
        QueryPerformanceFrequency(&frequency_);
        baseline_ = getRealTime();
    }

    // 检查时间函数是否被 Hook
    static bool checkIntegrity() {
        // 1. 检查函数地址是否被修改
        if (isFunctionHooked(GetTickCount)) {
            return false;
        }

        // 2. 对比不同时间源
        DWORD tickCount = GetTickCount();
        LARGE_INTEGER perfCount;
        QueryPerformanceCounter(&perfCount);

        uint64_t tickTime = tickCount;
        uint64_t perfTime = perfCount.QuadPart * 1000 / frequency_.QuadPart;

        // 两者应该接近 (误差 < 100ms)
        if (tickTime > perfTime && tickTime - perfTime > 100) {
            return false;  // 可能被修改
        }

        // 3. 检查系统时间
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        uint64_t systemTime = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTimeTime;

        // 系统时间应该与 tick count 接近
        uint64_t tickTimeMs = tickCount;
        uint64_t systemTimeMs = systemTime / 10000;

        if (abs((int64_t)(tickTimeMs - systemTimeMs)) > 1000) {
            return false;
        }

        return true;
    }

    // 获取真实时间 (不被 Hook)
    static uint64_t getRealTime() {
        // 使用 RDTSC 读取 CPU 周期数
        return __rdtsc();
    }

private:
    static decltype(&GetTickCount) originalGetTickCount_;
    static decltype(&QueryPerformanceCounter) originalQueryPerformanceCounter_;
    static LARGE_INTEGER frequency_;
    static uint64_t baseline_;
};
```

### 3.2 进程/模块检测

```cpp
// 检测已知加速挂进程

class SpeedHackDetector {
public:
    static bool isSpeedHackActive() {
        // 检查已知加速挂
        const std::vector<std::wstring> hackProcesses = {
            L"CheatEngine",
            L"speedhack",
            L"CE",
            L"MemoryModifier"
        };

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        PROCESSENTRY32W pe32;
        pe32.dwSize = sizeof(pe32);

        if (Process32FirstW(snapshot, &pe32)) {
            do {
                std::wstring processName = pe32.szExeFile;

                for (const auto& hack : hackProcesses) {
                    if (processName.find(hack) != std::wstring::npos) {
                        CloseHandle(snapshot);
                        return true;
                    }
                }
            } while (Process32NextW(snapshot, &pe32));
        }

        CloseHandle(snapshot);
        return false;
    }

    // 检测内存注入
    static bool detectMemoryInjection() {
        HMODULE hModule = GetModuleHandleA(nullptr);

        MODULEINFO modInfo;
        GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(modInfo));

        // 检查代码段完整性
        BYTE hash[16];
        calculateHash((BYTE*)hModule, modInfo.SizeOfImage, hash);

        // 与已知正常哈希对比
        if (!compareWithKnownHash(hash)) {
            return true;
        }

        return false;
    }
};
```

---

## 四、KBEngine 防加速

### 4.1 KBEngine 时间管理

```python
# KBEngine 风格的加速检测

class TimeManager:
    """
    KBEngine 时间管理:
    1. 服务端权威时间
    2. 客户端时间同步
    3. 动作时间验证
    """

    def __init__(self):
        self.client_time_offsets = {}  # player_id -> time_offset

    def on_client_message(self, player, message):
        """处理客户端消息"""
        client_time = message.get("timestamp", 0)
        server_time = int(time.time() * 1000)

        # 计算时间偏移
        offset = server_time - client_time

        # 初始化或更新偏移
        if player.id not in self.client_time_offsets:
            self.client_time_offsets[player.id] = offset
        else:
            # 偏移应该稳定
            expected_offset = self.client_time_offsets[player.id]

            # 如果偏移变化太大，可能是时间修改
            if abs(offset - expected_offset) > 5000:  # 5秒
                KBEngine.warning(f"Player {player.id} time anomaly")
                player.addSuspicionScore(10)

        # 验证消息时间戳是否合理
        if not self.is_timestamp_valid(player, message):
            return False

        return True

    def is_timestamp_valid(self, player, message):
        """验证时间戳"""
        client_time = message.get("timestamp", 0)
        server_time = int(time.time() * 1000)
        offset = self.client_time_offsets.get(player.id, 0)

        # 计算客户端预期时间
        expected_client_time = server_time - offset

        # 允许网络延迟误差
        tolerance = 1000  # 1秒

        if abs(client_time - expected_client_time) > tolerance:
            KBEngine.warning(
                f"Player {player.id} timestamp mismatch: "
                f"{client_time} vs {expected_client_time}"
            )
            return False

        return True

    def validate_action_time(self, player, action, interval):
        """验证动作间隔"""
        server_interval = self.get_server_interval(action)

        # 客户端声称的间隔不应该小于服务端间隔
        if interval < server_interval * 0.8:  # 允许 20% 误差
            player.speedHackViolations += 1

            if player.speedHackViolations > 5:
                KBEngine.warning(
                    f"Player {player.id} speed hack detected: "
                    f"{action} interval {interval} < {server_interval}"
                )
                return False

        return True
```

---

## 五、最佳实践

### 5.1 防护策略

| 策略 | 说明 |
|------|------|
| **服务端权威** | 所有关键逻辑服务端计算 |
| **时间戳验证** | 验证客户端时间戳合理性 |
| **速度检测** | 检测移动/动作速度 |
| **完整性检查** | 客户端代码完整性 |
| **异常行为** | 统计分析检测模式 |

### 5.2 综合检测

```
加速挂检测 = 时间验证 + 速度检测 + 客户端检测
- 服务端时间戳验证
- 移动速度合理性检查
- 冷却时间严格验证
- 客户端进程检测
```

---

## 六、总结

### 防加速挂核心

```
防加速 = 服务端验证 + 时间同步 + 异常检测
- 永远不信任客户端时间
- 所有时间相关逻辑服务端计算
- 检测异常时间差
- 综合判断避免误判
```

---

## 参考资料

- [Game Anti-Cheat Speed Detection](https://www.gamedeveloper.com/)
- [Kernel Mode Anti-Cheat](https://github.com/OpenSecurityResearch)
