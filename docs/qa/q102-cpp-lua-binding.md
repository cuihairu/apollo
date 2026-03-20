# Q102: C++ 如何调用 Lua？Lua 如何调用 C++？

## 问题分析

本题考察对 C++/Lua 互调的理解：
- Lua C API
- C++ 调用 Lua 函数
- Lua 调用 C++ 函数
- 绑定库 (LuaBridge/Sol)

---

## 一、Lua C API 基础

### 1.1 栈交互模型

```
┌─────────────────────────────────────────────────────────────┐
│                    Lua 栈交互模型                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Lua 栈:                                                     │
│  ┌─────┬─────┬─────┬─────┬─────┐                            │
│  │  5  │  4  │  3  │  2  │  1  │  栈顶 ->                    │
│  ├─────┼─────┼─────┼─────┼─────┤                            │
│  │ arg3│ arg2│ arg1│  fn │  -  │  函数调用时                 │
│  └─────┴─────┴─────┴─────┴─────┘                            │
│                                                             │
│  API 函数:                                                   │
│  ├── lua_gettop(L)          # 获取栈大小                     │
│  ├── lua_pushXXX(L, val)    # 压入值                        │
│  ├── lua_toXXX(L, idx)      # 获取值                        │
│  ├── lua_pcall(L, args, ret)# 调用函数                     │
│  └── lua_settop(L, idx)     # 设置栈大小                    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、C++ 调用 Lua

### 2.1 基础调用

```cpp
// C++ 调用 Lua

#include <lua.hpp>
#include <iostream>

class LuaCaller {
public:
    LuaCaller() {
        // 创建 Lua 状态
        L = luaL_newstate();

        // 打开标准库
        luaL_openlibs(L);

        // 加载脚本
        if (luaL_dofile(L, "game_logic.lua") != LUA_OK) {
            std::cerr << "Error loading Lua: " << lua_tostring(L, -1) << std::endl;
        }
    }

    ~LuaCaller() {
        lua_close(L);
    }

    // 调用无参数无返回值的函数
    void callSimpleFunction(const char* funcName) {
        // 获取函数
        lua_getglobal(L, funcName);

        // 调用函数 (0 参数, 0 返回值)
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            std::cerr << "Error calling " << funcName << ": "
                     << lua_tostring(L, -1) << std::endl;
            lua_pop(L, 1);  // 清除错误
        }
    }

    // 调用带参数的函数
    void callWithArgs(const char* funcName, int param1, const char* param2) {
        lua_getglobal(L, funcName);      // 压入函数

        lua_pushinteger(L, param1);      // 压入参数1
        lua_pushstring(L, param2);       // 压入参数2

        // 调用 (2 参数, 0 返回值)
        if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
            std::cerr << "Error: " << lua_tostring(L, -1) << std::endl;
            lua_pop(L, 1);
        }
    }

    // 调用带返回值的函数
    int callWithReturn(const char* funcName, int input) {
        lua_getglobal(L, funcName);
        lua_pushinteger(L, input);

        // 调用 (1 参数, 1 返回值)
        if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
            std::cerr << "Error: " << lua_tostring(L, -1) << std::endl;
            lua_pop(L, 1);
            return 0;
        }

        // 获取返回值
        int result = lua_tointeger(L, -1);
        lua_pop(L, 1);  // 弹出返回值

        return result;
    }

    // 调用返回多个值的函数
    struct Position {
        float x, y, z;
    };

    Position getPlayerPosition(int playerId) {
        lua_getglobal(L, "getPlayerPosition");
        lua_pushinteger(L, playerId);

        if (lua_pcall(L, 1, 3, 0) != LUA_OK) {
            std::cerr << "Error: " << lua_tostring(L, -1) << std::endl;
            lua_pop(L, 1);
            return {0, 0, 0};
        }

        // 获取返回值 (注意顺序)
        float z = (float)lua_tonumber(L, -1);
        float y = (float)lua_tonumber(L, -2);
        float x = (float)lua_tonumber(L, -3);
        lua_pop(L, 3);

        return {x, y, z};
    }

private:
    lua_State* L;
};


// Lua 脚本示例 (game_logic.lua)
/*
-- 简单函数
function onGameStart()
    print("Game started!")
end

-- 带参数
function onPlayerLogin(playerId, playerName)
    print("Player " .. playerId .. " (" .. playerName .. ") logged in")
end

-- 带返回值
function calculateLevel(exp)
    return math.floor(exp / 100) + 1
end

-- 多返回值
function getPlayerPosition(playerId)
    return 100.0, 200.0, 300.0
end
*/
```

---

## 三、Lua 调用 C++

### 3.1 C 函数注册

```cpp
// Lua 调用 C++

#include <lua.hpp>
#include <string>
#include <unordered_map>

// 游戏数据 (C++ 侧)
class GameData {
public:
    static std::unordered_map<int, std::string> playerNames;
    static std::unordered_map<int, int> playerLevels;
};

std::unordered_map<int, std::string> GameData::playerNames;
std::unordered_map<int, int> GameData::playerLevels;

// C 函数: 获取玩家名称
static int lua_get_player_name(lua_State *L) {
    // 获取参数
    int playerId = luaL_checkinteger(L, 1);

    // 查找玩家
    auto it = GameData::playerNames.find(playerId);
    if (it != GameData::playerNames.end()) {
        lua_pushstring(L, it->second.c_str());
    } else {
        lua_pushnil(L);
    }

    return 1;  // 返回值个数
}

// C 函数: 设置玩家名称
static int lua_set_player_name(lua_State *L) {
    int playerId = luaL_checkinteger(L, 1);
    const char* name = luaL_checkstring(L, 2);

    GameData::playerNames[playerId] = name;

    return 0;  // 无返回值
}

// C 函数: 多返回值
static int lua_get_player_info(lua_State *L) {
    int playerId = luaL_checkinteger(L, 1);

    auto nameIt = GameData::playerNames.find(playerId);
    auto levelIt = GameData::playerLevels.find(playerId);

    if (nameIt != GameData::playerNames.end() &&
        levelIt != GameData::playerLevels.end()) {
        lua_pushstring(L, nameIt->second.c_str());  // 返回1
        lua_pushinteger(L, levelIt->second);        // 返回2
        lua_pushboolean(L, true);                   // 返回3
    } else {
        lua_pushnil(L);
        lua_pushinteger(L, 0);
        lua_pushboolean(L, false);
    }

    return 3;  // 3个返回值
}

// C 函数: 可变参数
static int lua_sum(lua_State *L) {
    int n = lua_gettop(L);  // 参数个数
    int sum = 0;

    for (int i = 1; i <= n; ++i) {
        sum += luaL_checkinteger(L, i);
    }

    lua_pushinteger(L, sum);
    return 1;
}

// 注册所有函数
void registerFunctions(lua_State *L) {
    // 方法1: 使用 lua_register
    lua_register(L, "getPlayerName", lua_get_player_name);
    lua_register(L, "setPlayerName", lua_set_player_name);
    lua_register(L, "getPlayerInfo", lua_get_player_info);

    // 方法2: 使用 lua_pushcfunction + lua_setglobal
    lua_pushcfunction(L, lua_sum);
    lua_setglobal(L, "sum");

    // 方法3: 注册为表中的方法
    lua_newtable(L);  // 创建表

    lua_pushcfunction(L, lua_get_player_name);
    lua_setfield(L, -2, "getName");

    lua_pushcfunction(L, lua_set_player_name);
    lua_setfield(L, -2, "setName");

    lua_setglobal(L, "Player");  // 设置为全局变量 Player
}


// Lua 使用示例
/*
-- 直接调用
name = getPlayerName(100)
setPlayerName(100, "NewName")

-- 多返回值
name, level, success = getPlayerInfo(100)

-- 表方法调用
Player:getName(100)
Player:setName(100, "AnotherName")

-- 可变参数
total = sum(1, 2, 3, 4, 5)  -- 15
*/
```

---

## 四、高级绑定

### 4.1 LuaBridge 示例

```cpp
// 使用 LuaBridge 简化绑定

#include <LuaBridge/LuaBridge.h>

class Player {
public:
    std::string name;
    int level;
    int hp;

    Player(const std::string& n) : name(n), level(1), hp(100) {}

    void attack(const std::string& target) {
        printf("%s attacks %s!\n", name.c_str(), target.c_str());
    }

    int takeDamage(int damage) {
        hp -= damage;
        return hp;
    }

    std::string getInfo() const {
        return name + " Lv" + std::to_string(level) + " HP:" + std::to_string(hp);
    }
};

void registerWithLuaBridge(lua_State *L) {
    luabridge::getGlobalNamespace(L)
        .beginNamespace("Game")
            // 注册类
            .beginClass<Player>("Player")
                // 构造函数
                .addConstructor<void(*)(const std::string&)>()

                // 属性
                .addProperty("name", &Player::name)
                .addProperty("level", &Player::level)
                .addProperty("hp", &Player::hp)

                // 方法
                .addFunction("attack", &Player::attack)
                .addFunction("takeDamage", &Player::takeDamage)
                .addFunction("getInfo", &Player::getInfo)
            .endClass()
        .endNamespace();
}


// Lua 使用
/*
local player = Game.Player("Hero")
player.level = 10

print(player:getInfo())  -- Hero Lv10 HP:100

player:attack("Monster")

local remainingHp = player:takeDamage(30)
print("Remaining HP:", remainingHp)
*/
```

### 4.2 Sol 示例

```cpp
// 使用 Sol3 (现代 C++ 绑定)

#include <sol/sol.hpp>

class Entity {
public:
    float x, y, z;

    Entity(float x = 0, float y = 0, float z = 0)
        : x(x), y(y), z(z) {}

    void moveTo(float nx, float ny, float nz) {
        x = nx; y = ny; z = nz;
    }

    float distanceTo(const Entity& other) const {
        float dx = x - other.x;
        float dy = y - other.y;
        float dz = z - other.z;
        return std::sqrt(dx*dx + dy*dy + dz*dz);
    }
};

void registerWithSol(sol::state& lua) {
    // 打开标准库
    lua.open_libraries(sol::lib::base, sol::lib::math);

    // 注册类型
    lua.new_usertype<Entity>("Entity",
        // 构造函数
        sol::constructors<Entity(float, float, float)>(),

        // 属性
        "x", &Entity::x,
        "y", &Entity::y,
        "z", &Entity::z,

        // 方法
        "moveTo", &Entity::moveTo,
        "distanceTo", &Entity::distanceTo
    );

    // 注册函数
    lua.set_function("createEntity", []() {
        return Entity(0, 0, 0);
    });

    // 注册 Lambda
    lua.set_function("log", [](const std::string& msg) {
        std::cout << "[LUA] " << msg << std::endl;
    });
}


// Lua 使用
/*
local e1 = Entity(10, 20, 30)
local e2 = Entity(40, 50, 60)

log("Distance: " .. e1:distanceTo(e2))

e1:moveTo(100, 200, 300)

local e3 = createEntity()
e3.x = 50
*/
```

---

## 五、Userdata 封装

### 5.1 Full userdata

```cpp
// Userdata 封装 (轻量对象)

#include <lua.hpp>
#include <iostream>

struct Vector3 {
    float x, y, z;

    Vector3(float x = 0, float y = 0, float z = 0)
        : x(x), y(y), z(z) {}

    float length() const {
        return std::sqrt(x*x + y*y + z*z);
    }

    Vector3 operator+(const Vector3& other) const {
        return Vector3(x + other.x, y + other.y, z + other.z);
    }
};

// 创建 Vector3
static int lua_vector3_new(lua_State *L) {
    float x = (float)luaL_optnumber(L, 1, 0);
    float y = (float)luaL_optnumber(L, 2, 0);
    float z = (float)luaL_optnumber(L, 3, 0);

    // 分配 userdata
    Vector3* v = (Vector3*)lua_newuserdata(L, sizeof(Vector3));
    *v = Vector3(x, y, z);

    // 设置元表
    luaL_getmetatable(L, "Vector3");
    lua_setmetatable(L, -2);

    return 1;
}

// 获取 Vector3 指针
static Vector3* check_vector3(lua_State *L, int idx) {
    void* ud = luaL_checkudata(L, idx, "Vector3");
    luaL_argcheck(L, ud != NULL, idx, "'Vector3' expected");
    return (Vector3*)ud;
}

// 方法: length
static int lua_vector3_length(lua_State *L) {
    Vector3* v = check_vector3(L, 1);
    lua_pushnumber(L, v->length());
    return 1;
}

// 方法: add
static int lua_vector3_add(lua_State *L) {
    Vector3* v1 = check_vector3(L, 1);
    Vector3* v2 = check_vector3(L, 2);

    Vector3* result = (Vector3*)lua_newuserdata(L, sizeof(Vector3));
    *result = *v1 + *v2;

    luaL_getmetatable(L, "Vector3");
    lua_setmetatable(L, -2);

    return 1;
}

// 元方法: __tostring
static int lua_vector3_tostring(lua_State *L) {
    Vector3* v = check_vector3(L, 1);
    lua_pushfstring(L, "Vector3(%f, %f, %f)", v->x, v->y, v->z);
    return 1;
}

// 元方法: __add
static int lua_vector3_meta_add(lua_State *L) {
    return lua_vector3_add(L);
}

// 注册 Vector3 类型
void register_vector3(lua_State *L) {
    // 创建元表
    luaL_newmetatable(L, "Vector3");

    // 元方法
    lua_pushcfunction(L, lua_vector3_tostring);
    lua_setfield(L, -2, "__tostring");

    lua_pushcfunction(L, lua_vector3_meta_add);
    lua_setfield(L, -2, "__add");

    // 索引元方法 (方法查找)
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    // 方法
    lua_pushcfunction(L, lua_vector3_length);
    lua_setfield(L, -2, "length");

    lua_pushcfunction(L, lua_vector3_add);
    lua_setfield(L, -2, "add");

    // 清理栈
    lua_pop(L, 1);

    // 注册构造函数
    lua_register(L, "Vector3", lua_vector3_new);
}


// Lua 使用
/*
local v1 = Vector3(1, 2, 3)
local v2 = Vector3(4, 5, 6)

print(v1)           -- Vector3(1.000000, 2.000000, 3.000000)
print(v1:length())  -- 3.741657

local v3 = v1 + v2
print(v3)           -- Vector3(5.000000, 7.000000, 9.000000)
*/
```

---

## 六、最佳实践

### 6.1 绑定建议

| 实践 | 说明 |
|------|------|
| **错误检查** | 使用 luaL_checkXXX 验证参数 |
| **栈平衡** | 调用后清理栈 |
| **使用绑定库** | LuaBridge/Sol 简化开发 |
| **避免频繁调用** | C++/Lua 边界有开销 |
| **批量操作** | 一次传递多个值 |
| **元表复用** | 共享元表减少内存 |

---

## 七、总结

### C++/Lua 互调核心

```
C++/Lua 互调 = 栈操作 + 函数注册 + Userdata + 绑定库
- C++ 调用 Lua: lua_getglobal + lua_pcall
- Lua 调用 C++: lua_register + lua_CFunction
- 推荐使用 LuaBridge/Sol 简化开发
```

---

## 参考资料

- [Lua 5.4 Reference Manual](https://www.lua.org/manual/5.4/)
- [LuaBridge GitHub](https://github.com/vinniefalco/LuaBridge)
- [Sol3 GitHub](https://github.com/ThePhD/sol2)
- [Lua C API Wiki](https://en.wikibooks.org/wiki/Lua_Programming/in_C)
