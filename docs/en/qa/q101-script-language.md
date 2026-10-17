
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q101: Why Embed a Scripting Language? How to Choose Between Lua and Python?

## Problem Analysis

This question tests your understanding of scripting languages:
- Advantages of scripting languages
- Lua vs Python comparison
- KBEngine Python integration
- Game server applications

---

## 1. Advantages of Scripting Languages

### 1.1 Why Use Scripts

```
┌─────────────────────────────────────────────────────────────┐
│          Advantages of Scripting Languages in Games          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Development Efficiency:                                    │
│  ├── No compilation, fast iteration                         │
│  ├── Dynamic typing, concise code                           │
│  ├── Rich standard library                                  │
│  └── Example: designers can modify game parameters          │
│                                                             │
│  Hot Updates:                                               │
│  ├── Load new code at runtime                               │
│  ├── No server shutdown needed for updates                  │
│  ├── Fast bug fixes                                         │
│  └── Example: emergency bug fixes                           │
│                                                             │
│  Security:                                                  │
│  ├── Sandboxed execution, isolated risk                     │
│  ├── Restrict access to system resources                    │
│  ├── Exception catching                                     │
│  └── Example: player-defined scripts                        │
│                                                             │
│  Division of Labor:                                         │
│  ├── Programmers write the C++ core                         │
│  ├── Designers write game logic                             │
│  ├── Reduced coupling                                       │
│  └── Example: quest system, skill system                    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. Lua vs Python Comparison

### 2.1 Feature Comparison

| Feature | Lua | Python |
|---------|-----|--------|
| **Performance** | Extremely fast, lightweight | Fairly fast, but slower than Lua |
| **Memory footprint** | Very small (a few hundred KB) | Larger (a few MB) |
| **Learning curve** | Simple, minimal syntax | Simple, natural syntax |
| **Standard library** | Minimal | Very rich |
| **Third-party libraries** | Fewer | Extremely rich |
| **Integration difficulty** | Easy, excellent C API design | Moderate, requires CPython |
| **Multithreading** | Coroutines (native) | Threads + coroutines |
| **OOP** | Table-based (simulated) | Native support |
| **Popular games** | World of Warcraft, Roblox | EVE Online, KBEngine |

### 2.2 Performance Comparison

```lua
-- Lua Fibonacci example

function fibonacci(n)
    if n <= 1 then
        return n
    end
    return fibonacci(n - 1) + fibonacci(n - 2)
end

-- LuaJIT can achieve near-C performance
-- Standard Lua is about 2-3x faster than Python
```

```python
# Python Fibonacci example

def fibonacci(n):
    if n <= 1:
        return n
    return fibonacci(n - 1) + fibonacci(n - 2)

# CPython performance is about 1/2 - 1/3 of Lua
# But PyPy can be used for a 3-5x improvement
```

---

## 3. Lua in Games

### 3.1 Lua Features

```lua
-- Lua basic syntax

-- Table is the core data structure
player = {
    name = "Player1",
    level = 10,
    hp = 100,

    -- method
    attack = function(self, target)
        print(self.name .. " attacks " .. target.name)
    end
}

-- Metatable implements OOP
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

-- usage
local p1 = Player.new("Hero", 10)
local p2 = Player.new("Monster", 5)

p1:attack(p2)

-- coroutines (native)
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

### 3.2 Lua C Binding

```c
// Lua C API example

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

// C function callable from Lua
static int lua_get_player_level(lua_State *L) {
    // Get argument (player ID)
    int player_id = luaL_checkinteger(L, 1);

    // Get player level (from C++ data structure)
    int level = get_player_level_cpp(player_id);

    // Return result
    lua_pushinteger(L, level);
    return 1;  // Number of return values
}

// Register function to Lua
void register_lua_functions(lua_State *L) {
    lua_register(L, "getPlayerLevel", lua_get_player_level);
}

// Call Lua from C
void call_lua_function(lua_State *L) {
    // Load Lua file
    if (luaL_dofile(L, "script/game_logic.lua") != LUA_OK) {
        printf("Error: %s\n", lua_tostring(L, -1));
        return;
    }

    // Call Lua function
    lua_getglobal(L, "onPlayerLogin");
    lua_pushinteger(L, 12345);  // Player ID

    // Execute call (1 argument, 0 return values)
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        printf("Error: %s\n", lua_tostring(L, -1));
    }
}
```

---

## 4. Python in Games

### 4.1 KBEngine Python Integration

```python
# KBEngine uses Python as its scripting language

"""
KBEngine Python architecture:

1. C++ engine core
2. Python script layer
3. Entity definitions use Python
4. Callback functions implemented in Python
"""

import KBEngine

class Account(KBEngine.Account):
    """Account class - inherits from KBEngine.Account"""

    def __init__(self):
        KBEngine.Account.__init__(self)

        # Custom attributes
        self.playerName = ""
        self.level = 1
        self.gold = 0

    def onLogin(self, entityType):
        """Login callback (C++ calls Python)"""
        INFO_MSG(f"Account {self.id} login as {entityType}")

        # Create avatar
        if entityType == "Avatar":
            self.createAvatar()

    def onLogout(self):
        """Logout callback"""
        INFO_MSG(f"Account {self.id} logout")

    def createAvatar(self):
        """Create avatar"""
        avatar = KBEngine.createEntityLocally(
            "Avatar",
            {"position": (0, 0, 0)}
        )

class Avatar(KBEngine.Entity):
    """Avatar class"""

    def __init__(self):
        KBEngine.Entity.__init__(self)

        # Attributes
        self.hp = 100
        self.maxHp = 100
        self.mp = 50
        self.maxMp = 50
        self.level = 1
        self.exp = 0

    def receiveDamage(self, attackerID, damage):
        """Take damage (called by C++)"""
        self.hp -= damage

        INFO_MSG(f"Avatar {self.id} takes {damage} damage, HP: {self.hp}")

        if self.hp <= 0:
            self.onDeath()

    def onDeath(self):
        """Death handling"""
        INFO_MSG(f"Avatar {self.id} died")

        # Resurrection logic
        KBEngine.addTimer(5.0, 0, self.resurrect)

    def resurrect(self):
        """Resurrect"""
        self.hp = self.maxHp
        INFO_MSG(f"Avatar {self.id} resurrected")
```

### 4.2 Python C Extensions

```cpp
// Python C extension example

#include <Python.h>

// C function callable from Python
static PyObject* py_get_player_level(PyObject *self, PyObject *args) {
    int player_id;

    // Parse arguments
    if (!PyArg_ParseTuple(args, "i", &player_id)) {
        return NULL;
    }

    // Get player level (from C++ data structure)
    int level = get_player_level_cpp(player_id);

    // Return result
    return PyLong_FromLong(level);
}

// Method definitions
static PyMethodDef game_methods[] = {
    {"getPlayerLevel", py_get_player_level, METH_VARARGS,
     "Get player level by ID"},
    {NULL, NULL, 0, NULL}  // End marker
};

// Module definition
static struct PyModuleDef game_module = {
    PyModuleDef_HEAD_INIT,
    "game",        // Module name
    "Game module", // Module documentation
    -1,
    game_methods
};

// Module initialization
PyMODINIT_FUNC PyInit_game(void) {
    return PyModule_Create(&game_module);
}
```

---

## 5. Selection Advice

### 5.1 When to Choose Lua

```
Applicable scenarios:
├── Embedded environments (memory constrained)
├── Extremely high performance requirements
├── Client UI scripts
├── Lightweight solutions needed
└── Example: mobile games, indie games
```

**Reasons to recommend:**
- LuaJIT performance close to C
- Extremely small memory footprint
- Easy to integrate into any C/C++ project
- Rich game frameworks (Love2D, Corona SDK)

### 5.2 When to Choose Python

```
Applicable scenarios:
├── Complex game logic
├── Need rich library support
├── Server-side development
├── Fast development iteration
└── Example: KBEngine, MMO servers
```

**Reasons to recommend:**
- Extremely rich standard library
- Native OOP support
- Easy to learn and maintain
- Many ready-made frameworks

---

## 6. KBEngine Script Architecture

### 6.1 Entity Definitions

```python
# KBEngine entity definitions

"""
KBEngine entity file structure:

scripts/
├── __init__.py           # Entry point
├── account.py            # Account entity
├── avatar.py             # Avatar entity
├── monster.py            # Monster entity
├── npc.py                # NPC entity
└── data/
    ├── skills.xml        # Skill configuration
    └── items.xml         # Item configuration
"""

# data/entities/Avatar.def
# Entity definition file (KBEngine specific)

<root>
    <Avatar>
        <!-- Inheritance -->
        <Base>Entity</Base>

        <!-- Properties -->
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

        <!-- Methods -->
        <Methods>
            <receiveDamage>
                <Arg>UINT32</Arg>  <!-- attackerID -->
                <Arg>UINT16</Arg>  <!-- damage -->
            </receiveDamage>
        </Methods>

        <!-- Client methods -->
        <ClientMethods>
            <onHpChanged>
                <Arg>UINT32</Arg>  <!-- newHp -->
            </onHpChanged>
        </ClientMethods>
    </Avatar>
</root>
```

---

## 7. Best Practices

### 7.1 Script Language Selection Advice

| Scenario | Recommendation | Reason |
|----------|----------------|--------|
| Client UI | Lua | Lightweight, fast |
| Server-side logic | Python | Rich libraries, easy to maintain |
| Performance critical | Lua/C++ | Good LuaJIT performance |
| Rapid prototyping | Python | High development efficiency |
| Embedded systems | Lua | Simple integration |

---

## 8. Summary

### Core of Script Language Selection

```
Script selection = performance needs + ecosystem + team skills + project scale
- Lua: lightweight and high-performance, suitable for embedding
- Python: rich ecosystem, suitable for server-side
- KBEngine chose Python for development efficiency
```

---

## References

- [Lua Reference Manual](https://www.lua.org/manual/5.4/)
- [Python C API](https://docs.python.org/3/c-api/index.html)
- [KBEngine Python Programming](https://kbengine.github.io/docs/en/programming-guide/script-entity.html)
- [Lua vs Python in Game Development](https://www.gamedeveloper.com/business/lua-vs.-python-in-game-development)
