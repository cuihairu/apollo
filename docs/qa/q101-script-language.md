# Q101: 为什么要嵌入脚本语言？Lua vs Python 如何选择？

## 问题分析

本题考察对脚本语言的理解：
- 脚本语言优势
- Lua vs Python 对比
- KBEngine Python 集成
- 游戏服务器应用

---

## 一、脚本语言优势

### 1.1 为什么使用脚本

```
┌─────────────────────────────────────────────────────────────┐
│                    脚本语言在游戏中的优势                       │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  开发效率:                                                   │
│  ├── 无需编译，快速迭代                                       │
│  ├── 动态类型，代码简洁                                       │
│  ├── 丰富的标准库                                            │
│  └── 示例: 策划可修改游戏参数                                 │
│                                                             │
│  热更新:                                                     │
│  ├── 运行时加载新代码                                        │
│  ├── 无需停服更新                                            │
│  ├── 快速修复 Bug                                           │
│  └── 示例: 紧急 Bug 修复                                     │
│                                                             │
│  安全性:                                                     │
│  ├── 沙箱执行，隔离风险                                       │
│  ├── 限制访问系统资源                                        │
│  ├── 异常捕获                                               │
│  └── 示例: 玩家自定义脚本                                     │
│                                                             │
│  分工协作:                                                   │
│  ├── 程序写 C++ 核心                                         │
│  ├── 策划写游戏逻辑                                          │
│  ├── 降低耦合                                               │
│  └── 示例: 任务系统、技能系统                                 │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、Lua vs Python 对比

### 2.1 特性对比

| 特性 | Lua | Python |
|------|-----|--------|
| **性能** | 极快，轻量级 | 较快，但比 Lua 慢 |
| **内存占用** | 非常小 (几百 KB) | 较大 (几 MB) |
| **学习曲线** | 简单，语法精简 | 简单，语法自然 |
| **标准库** | 最小化 | 非常丰富 |
| **第三方库** | 较少 | 极其丰富 |
| **集成难度** | 容易，C API 设计优秀 | 中等，需要 CPython |
| **多线程** | 协程 (原生) | 线程 + 协程 |
| **面向对象** | 基于 Table (模拟) | 原生支持 |
| **热门游戏** | 魔兽世界、Roblox | EVE Online、KBEngine |

### 2.2 性能对比

```lua
-- Lua 斐波那契示例

function fibonacci(n)
    if n <= 1 then
        return n
    end
    return fibonacci(n - 1) + fibonacci(n - 2)
end

-- LuaJIT 可以达到接近 C 的性能
-- 标准Lua 比 Python 快约 2-3 倍
```

```python
# Python 斐波那契示例

def fibonacci(n):
    if n <= 1:
        return n
    return fibonacci(n - 1) + fibonacci(n - 2)

# CPython 性能约为 Lua 的 1/2 - 1/3
# 但可以使用 PyPy 提升 3-5 倍
```

---

## 三、Lua 在游戏中

### 3.1 Lua 特性

```lua
-- Lua 基础语法

-- 表 (Table) 是核心数据结构
player = {
    name = "Player1",
    level = 10,
    hp = 100,

    -- 方法
    attack = function(self, target)
        print(self.name .. " attacks " .. target.name)
    end
}

-- 元表 (Metatable) 实现 OOP
Player = {}
Player.__index = Player

function Player.new(name, level)
    local self = setmetatable({}, Player)
    self.name = name
    self.level = level
    self.hp = 100
    return self
end

function Player:attack(target)
    damage = self.level * 10
    target.hp = target.hp - damage
    print(string.format("%s attacks %s for %d damage",
        self.name, target.name, damage))
end

-- 使用
local p1 = Player.new("Hero", 10)
local p2 = Player.new("Monster", 5)

p1:attack(p2)

-- 协程 (原生)
co = coroutine.create(function()
    for i = 1, 3 do
        print("Step " .. i)
        coroutine.yield()
    end
end)

coroutine.resume(co)  -- Step 1
coroutine.resume(co)  -- Step 2
coroutine.resume(co)  -- Step 3
```

### 3.2 Lua C 绑定

```c
// Lua C API 示例

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

// C 函数供 Lua 调用
static int lua_get_player_level(lua_State *L) {
    // 获取参数 (玩家 ID)
    int player_id = luaL_checkinteger(L, 1);

    // 获取玩家等级 (从 C++ 数据结构)
    int level = get_player_level_cpp(player_id);

    // 返回结果
    lua_pushinteger(L, level);
    return 1;  // 返回值个数
}

// 注册函数到 Lua
void register_lua_functions(lua_State *L) {
    lua_register(L, "getPlayerLevel", lua_get_player_level);
}

// 从 C 调用 Lua
void call_lua_function(lua_State *L) {
    // 加载 Lua 文件
    if (luaL_dofile(L, "script/game_logic.lua") != LUA_OK) {
        printf("Error: %s\n", lua_tostring(L, -1));
        return;
    }

    // 调用 Lua 函数
    lua_getglobal(L, "onPlayerLogin");
    lua_pushinteger(L, 12345);  // 玩家 ID

    // 执行调用 (1 个参数，0 个返回值)
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        printf("Error: %s\n", lua_tostring(L, -1));
    }
}
```

---

## 四、Python 在游戏中

### 4.1 KBEngine Python 集成

```python
# KBEngine 使用 Python 作为脚本语言

"""
KBEngine Python 架构:

1. C++ 引擎核心
2. Python 脚本层
3. 实体定义使用 Python
4. 回调函数在 Python 中实现
"""

import KBEngine

class Account(KBEngine.Account):
    """账号类 - 继承自 KBEngine.Account"""

    def __init__(self):
        KBEngine.Account.__init__(self)

        # 自定义属性
        self.playerName = ""
        self.level = 1
        self.gold = 0

    def onLogin(self, entityType):
        """登录回调 (C++ 调用 Python)"""
        INFO_MSG(f"Account {self.id} login as {entityType}")

        # 创建角色
        if entityType == "Avatar":
            self.createAvatar()

    def onLogout(self):
        """登出回调"""
        INFO_MSG(f"Account {self.id} logout")

    def createAvatar(self):
        """创建角色"""
        avatar = KBEngine.createEntityLocally(
            "Avatar",
            {"position": (0, 0, 0)}
        )


class Avatar(KBEngine.Entity):
    """角色类"""

    def __init__(self):
        KBEngine.Entity.__init__(self)

        # 属性
        self.hp = 100
        self.maxHp = 100
        self.mp = 50
        self.maxMp = 50
        self.level = 1
        self.exp = 0

    def receiveDamage(self, attackerID, damage):
        """受到伤害 (C++ 调用)"""
        self.hp -= damage

        INFO_MSG(f"Avatar {self.id} takes {damage} damage, HP: {self.hp}")

        if self.hp <= 0:
            self.onDeath()

    def onDeath(self):
        """死亡处理"""
        INFO_MSG(f"Avatar {self.id} died")

        # 复活逻辑
        KBEngine.addTimer(5.0, 0, self.resurrect)

    def resurrect(self):
        """复活"""
        self.hp = self.maxHp
        INFO_MSG(f"Avatar {self.id} resurrected")
```

### 4.2 Python C 扩展

```cpp
// Python C 扩展示例

#include <Python.h>

// C 函数供 Python 调用
static PyObject* py_get_player_level(PyObject *self, PyObject *args) {
    int player_id;

    // 解析参数
    if (!PyArg_ParseTuple(args, "i", &player_id)) {
        return NULL;
    }

    // 获取玩家等级 (从 C++ 数据结构)
    int level = get_player_level_cpp(player_id);

    // 返回结果
    return PyLong_FromLong(level);
}

// 方法定义
static PyMethodDef game_methods[] = {
    {"getPlayerLevel", py_get_player_level, METH_VARARGS,
     "Get player level by ID"},
    {NULL, NULL, 0, NULL}  // 结束标记
};

// 模块定义
static struct PyModuleDef game_module = {
    PyModuleDef_HEAD_INIT,
    "game",        // 模块名
    "Game module", // 模块文档
    -1,
    game_methods
};

// 模块初始化
PyMODINIT_FUNC PyInit_game(void) {
    return PyModule_Create(&game_module);
}
```

---

## 五、选择建议

### 5.1 选择 Lua 的情况

```
适用场景:
├── 嵌入式环境 (内存受限)
├── 性能要求极高
├── 客户端 UI 脚本
├── 需要轻量级解决方案
└── 示例: 手游、独立游戏
```

**推荐理由:**
- LuaJIT 性能接近 C
- 内存占用极小
- 容易集成到任何 C/C++ 项目
- 丰富的游戏框架 (Love2D, Corona SDK)

### 5.2 选择 Python 的情况

```
适用场景:
├── 复杂游戏逻辑
├── 需要丰富的库支持
├── 服务端开发
├── 快速开发迭代
└── 示例: KBEngine, MMO 服务器
```

**推荐理由:**
- 标准库极其丰富
- 面向对象原生支持
- 易于学习和维护
- 大量现成框架

---

## 六、KBEngine 脚本架构

### 6.1 实体定义

```python
# KBEngine 实体定义

"""
KBEngine 实体文件结构:

scripts/
├── __init__.py           # 入口
├── account.py            # Account 实体
├── avatar.py             # Avatar 实体
├── monster.py            # Monster 实体
├── npc.py                # NPC 实体
└── data/
    ├── skills.xml        # 技能配置
    └── items.xml         # 物品配置
"""

# data/entities/Avatar.def
# 实体定义文件 (KBEngine 特有)

<root>
    <Avatar>
        <!-- 继承 -->
        <Base>Entity</Base>

        <!-- 属性 -->
        <Properties>
            <hp>
                <Type>UINT32</Type>
                <Flags>CELL_PUBLIC</Flags>
                <Default>100</Default>
            </hp>

            <level>
                <Type>UINT8</Type>
                <Flags>BASE_AND_CLIENT</Flags>
                <Default>1</Default>
            </level>

            <position>
                <Type>VECTOR3</Type>
                <Flags>CELL_PUBLIC</Flags>
                <Default>0,0,0</Default>
            </position>
        </Properties>

        <!-- 方法 -->
        <Methods>
            <receiveDamage>
                <Arg>UINT32</Arg>  <!-- attackerID -->
                <Arg>UINT16</Arg>  <!-- damage -->
            </receiveDamage>
        </Methods>

        <!-- 客户端方法 -->
        <ClientMethods>
            <onHpChanged>
                <Arg>UINT32</Arg>  <!-- newHp -->
            </onHpChanged>
        </ClientMethods>
    </Avatar>
</root>
```

---

## 七、最佳实践

### 7.1 脚本语言选择建议

| 场景 | 推荐 | 理由 |
|------|------|------|
| 客户端 UI | Lua | 轻量、快速 |
| 服务端逻辑 | Python | 库丰富、易维护 |
| 性能关键 | Lua/C++ | LuaJIT 性能好 |
| 快速原型 | Python | 开发效率高 |
| 嵌入系统 | Lua | 集成简单 |

---

## 八、总结

### 脚本语言选择核心

```
脚本选择 = 性能需求 + 生态 + 团队技能 + 项目规模
- Lua: 轻量高性能，适合嵌入
- Python: 生态丰富，适合服务端
- KBEngine 选择 Python 是为了开发效率
```

---

## 参考资料

- [Lua Reference Manual](https://www.lua.org/manual/5.4/)
- [Python C API](https://docs.python.org/3/c-api/index.html)
- [KBEngine Python Programming](https://kbengine.github.io/docs/en/programming-guide/script-entity.html)
- [Lua vs Python in Game Development](https://www.gamedeveloper.com/business/lua-vs.-python-in-game-development)
