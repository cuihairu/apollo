# Q84: 如何防止内存修改？

## 问题分析

本题考察对内存修改防护的理解：
- 内存修改原理
- 关键数据保护
- 完整性检查
- 混淆与加密

---

## 一、内存修改原理

### 1.1 常见手法

```
┌─────────────────────────────────────────────────────────────┐
│                    内存修改手法                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 直接修改内存                                             │
│  ├── 使用 Cheat Engine 搜索数值                             │
│  ├── 修改血量、金币、物品数量等                              │
│  └── 实时修改游戏状态                                       │
│                                                             │
│  2. 指针扫描                                                 │
│  ├── 找到动态地址                                           │
│  ├── 通过指针链修改                                         │
│  └── 绕过简单的地址随机化                                   │
│                                                             │
│  3. 代码注入                                                 │
│  ├── DLL 注入                                               │
│  ├── Hook 游戏函数                                          │
│  └── 修改游戏逻辑                                           │
│                                                             │
│  4. 内存读写                                                 │
│  ├── ReadProcessMemory / WriteProcessMemory                 │
│  ├── 外挂读写游戏内存                                        │
│  └── 需要进程权限                                           │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、数据保护

### 2.1 服务端权威

```cpp
// 服务端权威 - 根本防护

class ServerAuthoritative {
public:
    // 客户端请求攻击
    void onAttackRequest(uint64_t playerId, uint64_t targetId) {
        // 服务端计算伤害
        Player* attacker = getPlayer(playerId);
        Entity* target = getEntity(targetId);

        if (!attacker || !target) return;

        // 服务端计算伤害
        DamageResult result = calculateDamage(attacker, target);

        // 服务端扣血
        target->setHP(result.finalDamage);

        // 通知客户端结果
        sendDamageResult(playerId, {
            {"targetId", targetId},
            {"damage", result.finalDamage},
            {"isCritical", result.isCritical}
        });

        // 客户端只是显示，无法修改结果
    }

    // 定期同步状态
    void syncState(uint64_t playerId) {
        Player* player = getPlayer(playerId);
        if (!player) return;

        // 定期发送服务端权威状态
        sendStateUpdate(playerId, {
            {"hp", player->getHP()},
            {"mp", player->getMP()},
            {"position", player->getPosition()},
            {"level", player->getLevel()},
            {"exp", player->getExp()}
        });
    }
};
```

### 2.2 关键数据加密

```cpp
// 加密关键数据

class EncryptedGameData {
public:
    // 加密存储
    void setGold(uint64_t gold) {
        // 加密
        uint64_t encrypted = encryptValue(gold, playerKey_);

        // 添加校验和
        encrypted = appendChecksum(encrypted);

        // 存储
        gold_ = encrypted;

        // 服务端也需要存储真实值
        serverGold_ = gold;
    }

    // 读取
    uint64_t getGold() const {
        // 从加密值解密
        uint64_t encrypted = gold_;

        // 验证校验和
        if (!verifyChecksum(encrypted)) {
            // 被修改过
            return serverGold_;  // 返回服务端真实值
        }

        return decryptValue(encrypted, playerKey_);
    }

    // 与服务端同步验证
    bool validateWithServer() {
        uint64_t clientGold = getGold();
        uint64_t serverGold = queryServerGold();

        return clientGold == serverGold;
    }

private:
    uint64_t encryptValue(uint64_t value, uint64_t key) {
        // 简单加密 (实际应使用更强的加密)
        return value ^ key;
    }

    uint64_t decryptValue(uint64_t encrypted, uint64_t key) {
        return encrypted ^ key;
    }

    uint64_t appendChecksum(uint64_t value) {
        uint32_t checksum = calculateChecksum(value);
        return (value << 32) | checksum;
    }

    bool verifyChecksum(uint64_t value) const {
        uint32_t checksum = value & 0xFFFFFFFF;
        uint64_t data = value >> 32;
        return calculateChecksum(data) == checksum;
    }

    uint32_t calculateChecksum(uint64_t value) const {
        // CRC32 或其他校验算法
        return crc32(&value, sizeof(value));
    }

    uint64_t gold_;        // 加密存储
    uint64_t serverGold_;  // 服务端真实值
    uint64_t playerKey_;   // 玩家密钥
};
```

---

## 三、完整性检查

### 3.1 内存校验

```cpp
// 内存完整性检查

class MemoryIntegrityChecker {
public:
    // 初始化时保存关键数据哈希
    void initialize() {
        // 获取关键数据区
        keyDataStart_ = getKeyDataAddress();
        keyDataSize_ = getKeyDataSize();

        // 计算初始哈希
        originalHash_ = calculateHash(keyDataStart_, keyDataSize_);
    }

    // 定期检查完整性
    bool checkIntegrity() {
        // 计算当前哈希
        uint32_t currentHash = calculateHash(keyDataStart_, keyDataSize_);

        // 对比
        if (currentHash != originalHash_) {
            // 内存被修改
            onMemoryTampered();
            return false;
        }

        return true;
    }

    // 客户端随机检查
    void randomCheck() {
        if (!checkIntegrity()) {
            // 上报服务器
            reportToServer("Memory tampering detected");
            // 可以选择退出游戏
            exit(0);
        }
    }

private:
    uint32_t calculateHash(void* data, size_t size) {
        // 使用 SHA256 或 CRC32
        uint32_t crc = 0;
        uint8_t* bytes = (uint8_t*)data;

        for (size_t i = 0; i < size; ++i) {
            crc = crc32Table[(crc ^ bytes[i]) & 0xFF] ^ (crc >> 8);
        }

        return crc;
    }

    void* keyDataStart_;
    size_t keyDataSize_;
    uint32_t originalHash_;

    static const uint32_t crc32Table[256];
};
```

### 3.2 代码校验

```cpp
// 代码完整性检查

class CodeIntegrityChecker {
public:
    // 初始化时保存代码段哈希
    static void initialize() {
        HMODULE module = GetModuleHandleA(nullptr);

        MODULEINFO modInfo;
        GetModuleInformation(GetCurrentProcess(), module, &modInfo, sizeof(modInfo));

        // 计算代码段哈希
        codeHash_ = calculateHash((BYTE*)module, modInfo.SizeOfImage);

        // 保存到安全位置
        saveHashToSecureLocation(codeHash_);
    }

    // 验证代码完整性
    static bool verifyIntegrity() {
        HMODULE module = GetModuleHandleA(nullptr);

        MODULEINFO modInfo;
        GetModuleInformation(GetCurrentProcess(), module, &modInfo, sizeof(modInfo));

        uint32_t currentHash = calculateHash((BYTE*)module, modInfo.SizeOfImage);

        return currentHash == codeHash_;
    }

private:
    static uint32_t calculateHash(const BYTE* data, size_t size) {
        uint32_t hash = 0;
        for (size_t i = 0; i < size; ++i) {
            hash = hash * 31 + data[i];
        }
        return hash;
    }

    static uint32_t codeHash_;
};
```

---

## 四、混淆与保护

### 4.1 代码混淆

```cpp
// 代码混淆技术

class ObfuscatedValue {
public:
    // 混淆后的血量获取
    int getHP() const {
        // 不直接返回 hp_
        // 而是通过一系列运算
        return (hp_ ^ XOR_KEY) * MULTIPLIER + ADDEND;
    }

    void setHP(int hp) {
        // 同样混淆写入
        hp_ = (hp - ADDEND) / MULTIPLIER ^ XOR_KEY;
    }

private:
    int hp_;
    static const int XOR_KEY = 0x12345678;
    static const int MULTIPLIER = 3;
    static const int ADDEND = 100;
};

// 多层指针
class MultiPointerValue {
public:
    int getValue() const {
        // 通过多级指针访问
        return **ptr3_;
    }

    void setValue(int value) {
        **ptr3_ = value;
    }

private:
    int* value_;
    int** ptr1_ = &value_;
    int*** ptr2_ = &ptr1_;
    int**** ptr3_ = &ptr2_;
};
```

### 4.2 字符串加密

```cpp
// 字符串加密 (防止内存搜索)

class EncryptedString {
public:
    EncryptedString(const char* str) {
        size_t len = strlen(str);
        data_.resize(len);

        // 加密存储
        for (size_t i = 0; i < len; ++i) {
            data_[i] = str[i] ^ (XOR_KEY + i);
        }
    }

    std::string decrypt() const {
        std::string result;
        result.reserve(data_.size());

        for (size_t i = 0; i < data_.size(); ++i) {
            result += data_[i] ^ (XOR_KEY + i);
        }

        return result;
    }

    // 运行时解密使用
    operator std::string() const {
        return decrypt();
    }

private:
    std::vector<char> data_;
    static const char XOR_KEY = 0x7A;
};

// 使用示例
void checkPassword() {
    EncryptedString expected{"CorrectPassword"};
    std::string input = getUserInput();

    if (input == expected) {  // 自动解密比较
        // 密码正确
    }
}
```

---

## 五、反调试

### 5.1 调试检测

```cpp
// 反调试保护

class AntiDebug {
public:
    static bool isDebuggerPresent() {
        // 方法 1: IsDebuggerPresent API
        if (IsDebuggerPresent()) {
            return true;
        }

        // 方法 2: CheckRemoteDebuggerPresent
        BOOL isRemotePresent = FALSE;
        CheckRemoteDebuggerPresent(GetCurrentProcess(), &isRemotePresent);
        if (isRemotePresent) {
            return true;
        }

        // 方法 3: 检查调试标志
        if (isDebugFlagSet()) {
            return true;
        }

        // 方法 4: 检查调试器进程
        if (isDebuggerProcessRunning()) {
            return true;
        }

        return false;
    }

    static void triggerAntiDebug() {
        if (isDebuggerPresent()) {
            // 可以采取多种措施
            // 1. 静默退出
            exit(0);

            // 2. 崩溃 (误导调试者)
            // *((int*)nullptr) = 0;

            // 3. 上报服务器
            // reportToServer("Debugger detected");
        }
    }

private:
    static bool isDebugFlagSet() {
        #ifdef _WIN32
        typedef NTSTATUS (NTAPI* NtQueryInformationProcess)(
            HANDLE, DWORD, PVOID, ULONG, PULONG);

        HMODULE ntdll = GetModuleHandleA("ntdll.dll");
        if (ntdll) {
            auto NtQueryInformationProcess = (NtQueryInformationProcess)
                GetProcAddress(ntdll, "NtQueryInformationProcess");

            if (NtQueryInformationProcess) {
                DWORD dwDebugPort = 0;
                NTSTATUS status = NtQueryInformationProcess(
                    GetCurrentProcess(),
                    0x7,  // ProcessDebugPort
                    &dwDebugPort,
                    sizeof(dwDebugPort),
                    nullptr
                );

                return dwDebugPort != 0;
            }
        }
        #endif
        return false;
    }

    static bool isDebuggerProcessRunning() {
        const wchar_t* debuggerProcesses[] = {
            L"ollydbg.exe",
            L"Wireshark.exe",
            L"IDA.exe",
            L"ida64.exe",
            L"idaq.exe",
            L"x64dbg.exe",
            L"x32dbg.exe",
            L"cheatengine.exe"
        };

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        PROCESSENTRY32W pe32;
        pe32.dwSize = sizeof(pe32);

        if (Process32FirstW(snapshot, &pe32)) {
            do {
                for (const auto& proc : debuggerProcesses) {
                    if (_wcsicmp(pe32.szExeFile, proc) == 0) {
                        CloseHandle(snapshot);
                        return true;
                    }
                }
            } while (Process32NextW(snapshot, &pe32));
        }

        CloseHandle(snapshot);
        return false;
    }
};
```

---

## 六、KBEngine 防护

### 6.1 KBEngine 数据保护

```python
# KBEngine 数据保护

class Entity(KBEngine.Entity):
    """
    KBEngine 实体数据保护:
    1. 服务端存储真实值
    2. 客户端只是显示
    3. 定期同步验证
    """

    def __init__(self):
        KBEngine.Entity.__init__(self)

        # 服务端权威属性
        self.__hp = 100
        self.__max_hp = 100
        self.__mp = 50
        self.__max_mp = 50

        # 客户端可读属性 (显示用)
        self.client_hp = 100
        self.client_mp = 50

    def modifyHP(self, delta):
        """服务端修改血量"""
        # 服务端计算
        old_hp = self.__hp
        self.__hp = max(0, min(self.__max_hp, self.__hp + delta))

        # 如果客户端显示与服务端不一致，强制同步
        if self.client_hp != self.__hp:
            self.client_hp = self.__hp
            self.syncToClient("hp", self.__hp)

        # 触发事件
        if self.__hp != old_hp:
            self.onHPChanged(old_hp, self.__hp)

    def getHP(self):
        """获取血量"""
        # 返回服务端真实值
        return self.__hp

    def onClientModifyHP(self, new_hp):
        """客户端请求修改血量 (拒绝)"""
        # 记录可疑行为
        KBEngine.warning(f"Player {self.id} attempted to modify HP")

        # 强制同步正确值
        self.syncToClient("hp", self.__hp)
```

---

## 七、总结

### 防内存修改核心

```
防内存修改 = 服务端权威 + 数据加密 + 完整性检查 + 混淆
- 关键数据服务端存储
- 客户端只负责显示
- 定期验证完整性
- 混淆增加破解难度
```

---

## 参考资料

- [Memory Protection Techniques](https://www.sans.org/)
- [Anti-Cheat Development](https://www.valvesoftware.com/)
