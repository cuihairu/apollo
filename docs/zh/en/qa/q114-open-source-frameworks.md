
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q114: Which open-source game server frameworks are you familiar with? What are the characteristics of each?

## Problem Analysis

This question tests the breadth of knowledge:
- Mainstream frameworks
- Architectural characteristics
- Applicable scenarios
- Comparative analysis

---

## 1. Overview of Mainstream Frameworks

### 1.1 Framework Classification

```
┌─────────────────────────────────────────────────────────────┐
│              Open-Source Game Server Frameworks               │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  C/C++ frameworks:                                          │
│  ├── KBEngine - Python scripting                           │
│  ├── Skynet - Lua/C                                        │
│  ├── Pomelo - Node.js                                      │
│  └── Colyseus - Node.js                                    │
│                                                             │
│  Go frameworks:                                             │
│  ├── Leaf - lightweight                                    │
│  ├── Nano - minimalist                                     │
│  └── Go-World - MMO                                        │
│                                                             │
│  Java frameworks:                                           │
│  ├── Netty + self-developed                                │
│  ├── Wild World Open                                       │
│  └── Zoe - multiplayer game framework                      │
│                                                             │
│  Other frameworks:                                          │
│  ├── Photon (C#)                                           │
│  ├── SmartFox (Java)                                       │
│  └── RedDwarf (Java)                                       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. KBEngine

### 2.1 Architectural Characteristics

```python
# KBEngine characteristics

class KBEngineOverview:
    """KBEngine overview"""

    info = {
        '语言': 'C++ + Python',
        '架构': 'Distributed microservices',
        '特点': [
            'Component-based architecture (LoginApp/BaseApp/CellApp/DBMgr)',
            'Python script hot reload',
            'Built-in AOI system',
            'Entity component system',
            'Watcher monitoring system',
            'Rich documentation'
        ],
        '优势': [
            'Mature and stable',
            'Well-documented',
            'Active community',
            'Fast development',
            'Low learning cost'
        ],
        '劣势': [
            'Average performance',
            'Python GIL limitation',
            'Limited extensibility',
            'Fixed architecture'
        ],
        '适用场景': [
            'Small-to-medium MMO',
            '2D/3D online games',
            'Card games',
            'Strategy games'
        ],
        '承载能力': '5000-8000 CCU per server',
        '开源协议': 'GPLv2'
    }

    architecture = """
    KBEngine architecture:

    LoginApp: login authentication, load balancing
       ↓
    BaseAppMgr: manages BaseApp
       ↓
    BaseApp: player data management, non-real-time logic
       ↓
    CellAppMgr: manages CellApp
       ↓
    CellApp: game logic, scene management, AOI
       ↕
    DBMgr: database operations, cache management
    """
```

---

## 3. Skynet

### 3.1 Architectural Characteristics

```lua
-- Skynet characteristics

local SkynetOverview = {
    语言 = "C + Lua",
    架构 = "Actor model",
    特点 = {
        "Actor message passing",
        "Lightweight services",
        "High concurrency support",
        "Hot reload support",
        "Cluster support"
    },
    优势 = {
        "Excellent performance",
        "Small memory footprint",
        "Strong concurrency capability",
        "Flexible extension"
    },
    劣势 = {
        "Steep learning curve",
        "Fewer documents",
        "Difficult debugging",
        "Lua performance limitation"
    },
    适用场景 = {
        "High-concurrency games",
        "Real-time combat",
        "SLG games",
        "Card games"
    },
    承载能力 = "10000+ services on a single machine",
    开源协议 = "MIT"
}

-- Skynet service example
skynet.start(function()
    -- Start a service
    local service = skynet.newservice("simpledb")

    -- Send a message
    skynet.call(service, "lua", "SET", "hello", "world")

    -- Get the response
    local r = skynet.call(service, "lua", "GET", "hello")
    print(r)  -- world
end)
```

---

## 4. Pomelo

### 4.1 Architectural Characteristics

```javascript
// Pomelo characteristics

const PomeloOverview = {
    语言: "Node.js",
    架构: "Distributed",
    特点: [
        "Node.js async based",
        "Component-based design",
        "HTTP/WS protocol",
        "Hot reload support",
        "Fast development"
    ],
    优势: [
        "JavaScript full stack",
        "Rich ecosystem",
        "High development efficiency",
        "Active community"
    ],
    劣势: [
        "Single-thread limitation",
        "Average performance",
        "V8 memory overhead"
    ],
    适用场景: [
        "Web games",
        "H5 games",
        "Card games",
        "Casual games"
    ],
    承载能力: "3000-5000 CCU per server",
    开源协议: "MIT"
};

// Pomelo routing example
app.route('area.playerHandler.enterScene', function(msg, session, next) {
    var playerId = session.uid;
    var areaId = msg.areaId;

    // Route to a specific connector
    next(null, {
        playerId: playerId,
        areaId: areaId,
        host: utils.getHostByAreaId(areaId),
        port: utils.getPortByAreaId(areaId)
    });
});
```

---

## 5. Go Frameworks

### 5.1 Leaf

```go
// Leaf characteristics

type LeafOverview struct {
    语言     string
    架构     string
    特点     []string
    优势     []string
    劣势     []string
    适用场景 []string
}

var leaf = LeafOverview{
    语言: "Go",
    架构: "Simple and practical",
    特点: []string{
        "Simple and easy to use",
        "High performance",
        "Concurrency support",
        "Modularization",
    },
    优势: []string{
        "Excellent performance",
        "Small memory footprint",
        "Low learning cost",
        "Simple deployment",
    },
    劣势: []string{
        "Fewer features",
        "Needs your own extension",
        "Limited documentation",
    },
    适用场景: []string{
        "Board and card games",
        "Casual games",
        "Card games",
    },
}

// Leaf example
func main() {
    leaf.Run("leaf", "conf/server.json")

    // Register a message handler
    skeleton.RegisterChan(time.Second*10,
        // Handler
        new(HelloHandler),
        // Receiver
        &HelloReceiver{},
        // Sender
        &HelloSender{},
    )
}
```

### 5.2 Go-World

```go
// Go-World characteristics

var goWorld = struct {
    语言     string
    架构     string
    特点     []string
}{
    语言: "Go",
    架构: "ECS + spatial partitioning",
    特点: []string{
        "ECS architecture",
        "Spatial partitioning",
        "AOI support",
        "State synchronization",
        "High concurrency",
    },
    适用场景: []string{
        "MMO",
        "MOBA",
        "Open-world games",
    },
}

// Go-World example
type PlayerEntity struct {
    BaseEntity
    Position Vector3
    HP       int
}

func (e *PlayerEntity) On interest(hooks []EntityHook) {
    // Called when entering AOI
}

func (e *PlayerEntity) Update(dt float64) {
    // Update every frame
}
```

---

## 6. Framework Comparison

### 6.1 Comparison Table

| Framework | Language | Performance | Learning Cost | Capacity | Applicable Scenarios |
|------|------|------|----------|------|----------|
| KBEngine | C++/Python | Medium | Low | 5K | Small-to-medium MMO |
| Skynet | C/Lua | High | High | 10K+ | High-concurrency games |
| Pomelo | Node.js | Medium | Low | 3-5K | Web games |
| Leaf | Go | High | Medium | 5K+ | Board/casual |
| Go-World | Go | High | Medium | 10K+ | MMO/MOBA |
| Photon | C# | High | Medium | 10K+ | Commercial projects |

### 6.2 Selection Advice

```python
# Framework selection advice

class FrameworkSelector:
    """Framework selector"""

    @staticmethod
    def select(requirements):
        """Select based on requirements"""

        # Fast development, small-to-medium scale
        if requirements['team_size'] < 5 and \
           requirements['target_ccu'] < 5000:
            return 'KBEngine' if requirements['language'] == 'Python' else 'Pomelo'

        # High performance, large scale
        if requirements['target_ccu'] > 10000:
            return 'Skynet' if requirements['language'] == 'Lua' else 'Go-World'

        # Simple games
        if requirements['complexity'] == 'simple':
            return 'Leaf'

        # Commercial projects
        if requirements['budget'] == 'high':
            return 'Photon (commercial license)'

        # Default recommendation
        return 'KBEngine (easy to start)'

# Usage example
selection = FrameworkSelector.select({
    'team_size': 10,
    'target_ccu': 5000,
    'language': 'Python',
    'complexity': 'medium',
    'budget': 'medium'
})
# Result: KBEngine
```

---

## 7. Other Frameworks

### 7.1 List

| Framework | Language | Characteristics |
|------|------|------|
| **Colyseus** | Node.js | Modern API, Aris framework |
| **Zoe** | Java | Focused on multiplayer games |
| **RedDwarf** | Java | Veteran framework, stable |
| **SmartFox** | Java | Commercial solution |
| **Photon** | C# | Unity-friendly, commercial |
| **GameSparks** | - | BaaS solution |

---

## 8. Summary

### The Core of Framework Selection

```
Framework selection = team skills + project needs + long-term maintenance
- Prioritize what the team is familiar with
- Consider capacity requirements
- Evaluate the learning cost
- Pay attention to community activity
```

---

## References

- [KBEngine GitHub](https://github.com/kbengine/kbengine)
- [Skynet GitHub](https://github.com/cloudwu/skynet)
- [Pomelo GitHub](https://github.com/NetEase/pomelo)
- [Leaf GitHub](https://github.com/name5566/leaf)
