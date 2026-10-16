[English](q102-cpp-lua-binding.en.md) | [中文](q102-cpp-lua-binding.md)

# Q102: How Does C++ Call Lua? How Does Lua Call C++?

## Problem Analysis

This question examines understanding of C++/Lua interop:
- Lua C API
- C++ calling Lua functions
- Lua calling C++ functions
- Binding libraries (LuaBridge/Sol)

---

## I. Lua C API Basics

### 1.1 Stack Interaction Model

```
┌─────────────────────────────────────────────────────────────┐
│                  Lua Stack Interaction Model                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Lua stack:                                                 │
│  ┌─────┬─────┬─────┬─────┬─────┐                            │
│  │  5  │  4  │  3  │  2  │  1  │  Stack top ->              │
│  ├─────┼─────┼─────┼─────┼─────┤                            │
│  │ arg3│ arg2│ arg1│  fn │  -  │  When a function is called │
│  └─────┴─────┴─────┴─────┴─────┘                            │
│                                                             │
│  API functions:                                             │
│  ├── lua_gettop(L)          # Get stack size                │
│  ├── lua_pushXXX(L, val)    # Push a value                  │
│  ├── lua_toXXX(L, idx)      # Get a value                   │
│  ├── lua_pcall(L, args, ret)# Call a function               │
│  └── lua_settop(L, idx)     # Set stack size                │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## II. C++ Calling Lua

### 2.1 Basic Calls

```cpp
// C++ calling Lua (C++ 调用 Lua)

#include <lua.hpp>
#include <iostream>

class LuaCaller {
public:
    LuaCaller() {
        // Create a Lua state (创建 Lua 状态)
        L = luaL_newstate();

        // Open the standard libraries (打开标准库)
        luaL_openlibs(L);

        // Load the script (加载脚本)
        if (luaL_dofile(L, "game_logic.lua") != LUA_OK) {
            std::cerr << "Error loading Lua: " << lua_tostring(L, -1) << std::endl;
        }
    }

    ~LuaCaller() {
        lua_close(L);
    }

    // Call a function with no arguments and no return value (调用无参数无返回值的函数)
    void callSimpleFunction(const char* funcName) {
        // Get the function (获取函数)
        lua_getglobal(L, funcName);

        // Call the function (0 arguments, 0 return values) (调用函数 0 参数 0 返回值)
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            std::cerr << "Error calling " << funcName << ": "
                     << lua_tostring(L, -1) << std::endl;
            lua_pop(L, 1);  // Clear the error (清除错误)
        }
    }

    // Call a function with arguments (调用带参数的函数)
    void callWithArgs(const char* funcName, int param1, const char* param2) {
        lua_getglobal(L, funcName);      // Push the function (压入函数)

        lua_pushinteger(L, param1);      // Push argument 1 (压入参数1)
        lua_pushstring(L, param2);       // Push argument 2 (压入参数2)

        // Call (2 arguments, 0 return values) (调用 2 参数 0 返回值)
        if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
            std::cerr << "Error: " << lua_tostring(L, -1) << std::endl;
            lua_pop(L, 1);
        }
    }

    // Call a function with a return value (调用带返回值的函数)
    int callWithReturn(const char* funcName, int input) {
        lua_getglobal(L, funcName);
        lua_pushinteger(L, input);

        // Call (1 argument, 1 return value) (调用 1 参数 1 返回值)
        if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
            std::cerr << "Error: " << lua_tostring(L, -1) << std::endl;
            lua_pop(L, 1);
            return 0;
        }

        // Get the return value (获取返回值)
        int result = lua_tointeger(L, -1);
        lua_pop(L, 1);  // Pop the return value (弹出返回值)

        return result;
    }

    // Call a function returning multiple values (调用返回多个值的函数)
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

        // Get the return values, note the order (获取返回值 注意顺序)
        float z = (float)lua_tonumber(L, -1);
        float y = (float)lua_tonumber(L, -2);
        float x = (float)lua_tonumber(L, -3);
        lua_pop(L, 3);

        return {x, y, z};
    }

private:
    lua_State* L;
};


// Lua script example (game_logic.lua) (Lua 脚本示例 game_logic.lua)
/*
-- Simple function (简单函数)
function onGameStart()
    print("Game started!")
end

-- With arguments (带参数)
function onPlayerLogin(playerId, playerName)
    print("Player " .. playerId .. " (" .. playerName .. ") logged in")
end

-- With a return value (带返回值)
function calculateLevel(exp)
    return math.floor(exp / 100) + 1
end

-- Multiple return values (多返回值)
function getPlayerPosition(playerId)
    return 100.0, 200.0, 300.0
end
*/
```

---

## III. Lua Calling C++

### 3.1 C Function Registration

```cpp
// Lua calling C++ (Lua 调用 C++)

#include <lua.hpp>
#include <string>
#include <unordered_map>

// Game data, C++ side (游戏数据 C++ 侧)
class GameData {
public:
    static std::unordered_map<int, std::string> playerNames;
    static std::unordered_map<int, int> playerLevels;
};

std::unordered_map<int, std::string> GameData::playerNames;
std::unordered_map<int, int> GameData::playerLevels;

// C function: get player name (C 函数: 获取玩家名称)
static int lua_get_player_name(lua_State *L) {
    // Get arguments (获取参数)
    int playerId = luaL_checkinteger(L, 1);

    // Find the player (查找玩家)
    auto it = GameData::playerNames.find(playerId);
    if (it != GameData::playerNames.end()) {
        lua_pushstring(L, it->second.c_str());
    } else {
        lua_pushnil(L);
    }

    return 1;  // Number of return values (返回值个数)
}

// C function: set player name (C 函数: 设置玩家名称)
static int lua_set_player_name(lua_State *L) {
    int playerId = luaL_checkinteger(L, 1);
    const char* name = luaL_checkstring(L, 2);

    GameData::playerNames[playerId] = name;

    return 0;  // No return values (无返回值)
}

// C function: multiple return values (C 函数: 多返回值)
static int lua_get_player_info(lua_State *L) {
    int playerId = luaL_checkinteger(L, 1);

    auto nameIt = GameData::playerNames.find(playerId);
    auto levelIt = GameData::playerLevels.find(playerId);

    if (nameIt != GameData::playerNames.end() &&
        levelIt != GameData::playerLevels.end()) {
        lua_pushstring(L, nameIt->second.c_str());  // Return 1 (返回1)
        lua_pushinteger(L, levelIt->second);        // Return 2 (返回2)
        lua_pushboolean(L, true);                   // Return 3 (返回3)
    } else {
        lua_pushnil(L);
        lua_pushinteger(L, 0);
        lua_pushboolean(L, false);
    }

    return 3;  // 3 return values (3个返回值)
}

// C function: variadic arguments (C 函数: 可变参数)
static int lua_sum(lua_State *L) {
    int n = lua_gettop(L);  // Number of arguments (参数个数)
    int sum = 0;

    for (int i = 1; i <= n; ++i) {
        sum += luaL_checkinteger(L, i);
    }

    lua_pushinteger(L, sum);
    return 1;
}

// Register all functions (注册所有函数)
void registerFunctions(lua_State *L) {
    // Method 1: use lua_register (方法1: 使用 lua_register)
    lua_register(L, "getPlayerName", lua_get_player_name);
    lua_register(L, "setPlayerName", lua_set_player_name);
    lua_register(L, "getPlayerInfo", lua_get_player_info);

    // Method 2: use lua_pushcfunction + lua_setglobal (方法2: 使用 lua_pushcfunction + lua_setglobal)
    lua_pushcfunction(L, lua_sum);
    lua_setglobal(L, "sum");

    // Method 3: register as methods in a table (方法3: 注册为表中的方法)
    lua_newtable(L);  // Create a table (创建表)

    lua_pushcfunction(L, lua_get_player_name);
    lua_setfield(L, -2, "getName");

    lua_pushcfunction(L, lua_set_player_name);
    lua_setfield(L, -2, "setName");

    lua_setglobal(L, "Player");  // Set as the global variable Player (设置为全局变量 Player)
}


// Lua usage example (Lua 使用示例)
/*
-- Direct calls (直接调用)
name = getPlayerName(100)
setPlayerName(100, "NewName")

-- Multiple return values (多返回值)
name, level, success = getPlayerInfo(100)

-- Table method calls (表方法调用)
Player:getName(100)
Player:setName(100, "AnotherName")

-- Variadic arguments (可变参数)
total = sum(1, 2, 3, 4, 5)  -- 15
*/
```

---

## IV. Advanced Binding

### 4.1 LuaBridge Example

```cpp
// Use LuaBridge to simplify binding (使用 LuaBridge 简化绑定)

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
            // Register the class (注册类)
            .beginClass<Player>("Player")
                // Constructor (构造函数)
                .addConstructor<void(*)(const std::string&)>()

                // Properties (属性)
                .addProperty("name", &Player::name)
                .addProperty("level", &Player::level)
                .addProperty("hp", &Player::hp)

                // Methods (方法)
                .addFunction("attack", &Player::attack)
                .addFunction("takeDamage", &Player::takeDamage)
                .addFunction("getInfo", &Player::getInfo)
            .endClass()
        .endNamespace();
}


// Lua usage (Lua 使用)
/*
local player = Game.Player("Hero")
player.level = 10

print(player:getInfo())  -- Hero Lv10 HP:100

player:attack("Monster")

local remainingHp = player:takeDamage(30)
print("Remaining HP:", remainingHp)
*/
```

### 4.2 Sol Example

```cpp
// Use Sol3, a modern C++ binding (使用 Sol3 现代 C++ 绑定)

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
    // Open the standard libraries (打开标准库)
    lua.open_libraries(sol::lib::base, sol::lib::math);

    // Register the type (注册类型)
    lua.new_usertype<Entity>("Entity",
        // Constructor (构造函数)
        sol::constructors<Entity(float, float, float)>(),

        // Properties (属性)
        "x", &Entity::x,
        "y", &Entity::y,
        "z", &Entity::z,

        // Methods (方法)
        "moveTo", &Entity::moveTo,
        "distanceTo", &Entity::distanceTo
    );

    // Register a function (注册函数)
    lua.set_function("createEntity", []() {
        return Entity(0, 0, 0);
    });

    // Register a lambda (注册 Lambda)
    lua.set_function("log", [](const std::string& msg) {
        std::cout << "[LUA] " << msg << std::endl;
    });
}


// Lua usage (Lua 使用)
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

## V. Userdata Wrapping

### 5.1 Full userdata

```cpp
// Userdata wrapping, lightweight objects (Userdata 封装 轻量对象)

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

// Create Vector3 (创建 Vector3)
static int lua_vector3_new(lua_State *L) {
    float x = (float)luaL_optnumber(L, 1, 0);
    float y = (float)luaL_optnumber(L, 2, 0);
    float z = (float)luaL_optnumber(L, 3, 0);

    // Allocate userdata (分配 userdata)
    Vector3* v = (Vector3*)lua_newuserdata(L, sizeof(Vector3));
    *v = Vector3(x, y, z);

    // Set the metatable (设置元表)
    luaL_getmetatable(L, "Vector3");
    lua_setmetatable(L, -2);

    return 1;
}

// Get the Vector3 pointer (获取 Vector3 指针)
static Vector3* check_vector3(lua_State *L, int idx) {
    void* ud = luaL_checkudata(L, idx, "Vector3");
    luaL_argcheck(L, ud != NULL, idx, "'Vector3' expected");
    return (Vector3*)ud;
}

// Method: length (方法: length)
static int lua_vector3_length(lua_State *L) {
    Vector3* v = check_vector3(L, 1);
    lua_pushnumber(L, v->length());
    return 1;
}

// Method: add (方法: add)
static int lua_vector3_add(lua_State *L) {
    Vector3* v1 = check_vector3(L, 1);
    Vector3* v2 = check_vector3(L, 2);

    Vector3* result = (Vector3*)lua_newuserdata(L, sizeof(Vector3));
    *result = *v1 + *v2;

    luaL_getmetatable(L, "Vector3");
    lua_setmetatable(L, -2);

    return 1;
}

// Metamethod: __tostring (元方法: __tostring)
static int lua_vector3_tostring(lua_State *L) {
    Vector3* v = check_vector3(L, 1);
    lua_pushfstring(L, "Vector3(%f, %f, %f)", v->x, v->y, v->z);
    return 1;
}

// Metamethod: __add (元方法: __add)
static int lua_vector3_meta_add(lua_State *L) {
    return lua_vector3_add(L);
}

// Register the Vector3 type (注册 Vector3 类型)
void register_vector3(lua_State *L) {
    // Create the metatable (创建元表)
    luaL_newmetatable(L, "Vector3");

    // Metamethods (元方法)
    lua_pushcfunction(L, lua_vector3_tostring);
    lua_setfield(L, -2, "__tostring");

    lua_pushcfunction(L, lua_vector3_meta_add);
    lua_setfield(L, -2, "__add");

    // Index metamethod, method lookup (索引元方法 方法查找)
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    // Methods (方法)
    lua_pushcfunction(L, lua_vector3_length);
    lua_setfield(L, -2, "length");

    lua_pushcfunction(L, lua_vector3_add);
    lua_setfield(L, -2, "add");

    // Clean up the stack (清理栈)
    lua_pop(L, 1);

    // Register the constructor (注册构造函数)
    lua_register(L, "Vector3", lua_vector3_new);
}


// Lua usage (Lua 使用)
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

## VI. Best Practices

### 6.1 Binding Recommendations

| Practice | Description |
|------|------|
| **Error checking** | Validate arguments with luaL_checkXXX |
| **Stack balance** | Clean up the stack after calls |
| **Use binding libraries** | LuaBridge/Sol simplify development |
| **Avoid frequent calls** | The C++/Lua boundary has overhead |
| **Batch operations** | Pass multiple values at once |
| **Metatable reuse** | Shared metatables reduce memory |

---

## VII. Summary

### C++/Lua Interop Core

```
C++/Lua interop = stack operations + function registration + Userdata + binding libraries
- C++ calling Lua: lua_getglobal + lua_pcall
- Lua calling C++: lua_register + lua_CFunction
- LuaBridge/Sol are recommended to simplify development
```

---

## References

- [Lua 5.4 Reference Manual](https://www.lua.org/manual/5.4/)
- [LuaBridge GitHub](https://github.com/vinniefalco/LuaBridge)
- [Sol3 GitHub](https://github.com/ThePhD/sol2)
- [Lua C API Wiki](https://en.wikibooks.org/wiki/Lua_Programming/in_C)
