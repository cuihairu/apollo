---
title: AOI 3×3 Grid System In Depth
icon: eye
order: 4
category:
  - Architecture
  - AOI
tag:
  - AOI
  - 3×3 Grid
  - View Management
---

# AOI 3×3 Grid System - Efficient View Management

> **Implementation cross-check (2026-10-04 reconciliation)**: this page explains the 3×3-grid algorithm principle; the code examples are teaching pseudocode.
> The repo's current implementation = `apollo::game::world::SceneAoi` (after the P1-3 convergence, the repo's sole AOI implementation,
> `modules/game/world/scene_aoi.hpp`): owned exclusively by Scene, isolated per scene, Enter/Sync/Leave
> events dispatched through a sink, with the diff baseline being `ViewerState` (a per-observer watermark). The `AOIManager`
> example class name and interfaces below (`getViewers` etc.) diverge in shape from the implementation; the header file is authoritative.

## Table of Contents
- [What is AOI](#what-is-aoi)
- [Why an AOI system is needed](#why-an-aoi-system-is-needed)
- [Why the 3×3 grid is efficient](#why-the-3×3-grid-is-efficient)
- [How are targets discovered](#how-are-targets-discovered-the-core-algorithm)
- [How is deduplication done](#how-is-deduplication-done-the-key-problem)
- [Complete implementation example](#complete-implementation-example)

---

## What is AOI?

**AOI (Area of Interest)** means "area of interest"; in games it specifically refers to **view management** — every player can only see the other entities (monsters, NPCs, other players) within a certain range around them.

```
For example: a player sees only entities within 100 meters (P=player M=monster G=Ghost B=Boss)
        P → perception range 100m
           ╔═════════╗
           ║  G  B   ║
           ║ P  M    ║
           ╚═════════╝
```

---

## Why an AOI System Is Needed?

### The problem scenario
Suppose the game world has **10,000 players**:
- Without AOI: every movement must notify **9,999 players** → network explosion
- With AOI (100 perceived): each movement notifies only **100 players** → acceptable

---

## Why the 3×3 Grid Is Efficient?

### The core idea: trade space for time

Divide the game world into fixed grids, with each grid recording the entities inside it.

```
World map division:
┌──────┬──────┬──────┬──────┐
│  0   │  1   │  2   │  3   │
├──────┼──────┼──────┼──────┤
│  4   │  5   │  6   │  7   │
├──────┼──────┼──────┼──────┤
│  8   │  9   │ 10   │ 11   │
├──────┼──────┼──────┼──────┤
│ 12   │ 13   │ 14   │ 15   │
└──────┴──────┴──────┴──────┘
```

### The 3×3 grid principle

When a player is in grid (5), only the **9 surrounding grids** need to be checked:

```
           ┌───┬───┬───┐
           │ 1 │ 2 │ 3 │  ← only these 9 grids need to be checked
           ├───┼───┼───┤
           │ 4 │ P │ 6 │
           ├───┼───┼───┤
           │ 7 │ 8 │ 9 │
           └───┴───┴───┘
```

### Why is it efficient?

| Scheme | Lookup complexity | Movement update | Notes |
|------|-----------|---------|------|
| **Full scan** | O(N) | O(N) | Iterates all entities; impractical |
| **Quadtree** | O(log N) | O(log N) | Complex tree structure; hard to implement |
| **Cross linked list** | O(1) | O(1) | Only handles square view fields |
| **3×3 grid** | O(1) | O(1) | Simple, efficient, stable |

**Advantages of the 3×3 grid:**
1. **O(1) lookup**: the grid number is computed directly from the coordinates
2. **O(1) update**: crossing a grid boundary touches only 2 grids
3. **Simple to implement**: an array suffices for storage
4. **Memory friendly**: no complex tree structure needed

---

## How Are Targets Discovered? (the Core Algorithm)

### 1. Coordinates → grid number

```cpp
// 假设格子大小 100x100，原点在地图左上角（assume grid size 100x100, origin at the map's top-left corner）
int gridX = (int)(posX / 100);
int gridY = (int)(posY / 100);
int gridIndex = gridY * widthInGrids + gridX;
```

### 2. Get the 3×3 grid indices

```cpp
std::vector<int> getNineGrids(int gridX, int gridY) {
    std::vector<int> grids;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int nx = gridX + dx;
            int ny = gridY + dy;
            if (nx >= 0 && nx < mapWidth && ny >= 0 && ny < mapHeight) {
                grids.push_back(ny * widthInGrids + nx);
            }
        }
    }
    return grids;  // 最多9个格子（at most 9 grids）
}
```

### 3. Collect entities in view

```cpp
std::vector<Entity*> getEntitiesInView(Position pos, float viewRadius) {
    std::vector<Entity*> result;

    // 1. 获取九宫格（get the 3×3 grid）
    std::vector<int> grids = getNineGrids(pos.x / 100, pos.y / 100);

    // 2. 遍历九宫格内的实体（iterate the entities within the 3×3 grid）
    for (int gridId : grids) {
        for (Entity* entity : gridMap[gridId]) {
            // 3. 精确距离判断（过滤九宫格边角）（exact distance check, filtering 3×3-grid corners）
            if (distance(pos, entity->pos) <= viewRadius) {
                result.push_back(entity);
            }
        }
    }
    return result;
}
```

---

## How Is Deduplication Done? (the Key Problem)

### Problem: 3×3-grid boundaries cause double counting

```
     Grid A    Grid B
┌────────┬────────┐
│  M1    │  M2    │  M1 and M2 are both in view
│    P   ├────────┤  but M2 gets counted twice!
│  M3    │  M4    │
└────────┴────────┘
```

### Deduplication schemes

#### Scheme 1: Use a set (simplest)
```cpp
std::unordered_set<Entity*> entitySet;  // 自动去重（automatic deduplication）

for (int gridId : nineGrids) {
    for (Entity* entity : gridMap[gridId]) {
        entitySet.insert(entity);  // insert自动去重（insert deduplicates automatically）
    }
}
```

#### Scheme 2: Flag marking (high performance)
```cpp
// 每个实体有一个frameId标记（each entity carries a frameId flag）
struct Entity {
    uint32_t viewFrameId;  // 每帧+1（+1 per frame）
};

static uint32_t currentFrameId = 0;

std::vector<Entity*> getEntitiesInView(Position pos) {
    currentFrameId++;  // 每次查询递增（increment on every query）
    std::vector<Entity*> result;

    for (int gridId : nineGrids) {
        for (Entity* entity : gridMap[gridId]) {
            if (entity->viewFrameId != currentFrameId) {
                entity->viewFrameId = currentFrameId;  // 标记（mark）
                result.push_back(entity);
            }
            // else: 已经添加过，跳过（already added, skip）
        }
    }
    return result;
}
```

---

## Complete Implementation Example

```cpp
class AOIManager {
    struct Grid {
        std::vector<Entity*> entities;
    };

    int mapWidth;       // 地图宽度（格子数）（map width in grids）
    int mapHeight;      // 地图高度（格子数）（map height in grids）
    float gridSize;     // 每格大小（per-grid size）
    std::vector<Grid> grids;

public:
    // 进入游戏世界（enter the game world）
    void enter(Entity* entity, Position pos) {
        int gridId = getGridId(pos);
        grids[gridId].entities.push_back(entity);
        entity->gridId = gridId;

        // 通知周围玩家（notify nearby players）
        notifyEnter(entity);
    }

    // 移动（move）
    void move(Entity* entity, Position newPos) {
        int oldGrid = entity->gridId;
        int newGrid = getGridId(newPos);

        entity->pos = newPos;

        if (oldGrid != newGrid) {
            // 跨格子：从旧格子移除，加入新格子（crossed grids: remove from the old grid, add to the new one）
            removeFromGrid(entity, oldGrid);
            addToGrid(entity, newGrid);

            // 重新计算视野变化（recompute the view change）
            notifyViewChange(entity, oldGrid, newGrid);
        }
    }

    // 离开（leave）
    void leave(Entity* entity) {
        removeFromGrid(entity, entity->gridId);
        notifyLeave(entity);
    }

private:
    // 获取视野内实体（带去重）（get entities in view, with deduplication）
    std::vector<Entity*> getViewers(Entity* entity) {
        static uint32_t frameId = 0;
        frameId++;

        std::vector<Entity*> result;
        int gx = entity->gridId % mapWidth;
        int gy = entity->gridId / mapWidth;

        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                int nx = gx + dx;
                int ny = gy + dy;
                if (nx < 0 || nx >= mapWidth || ny < 0 || ny >= mapHeight) continue;

                int neighborGrid = ny * mapWidth + nx;
                for (Entity* other : grids[neighborGrid].entities) {
                    if (other->viewFrameId != frameId) {
                        other->viewFrameId = frameId;
                        if (inRange(entity, other)) {
                            result.push_back(other);
                        }
                    }
                }
            }
        }
        return result;
    }
};
```

---

## Summary

| Point | Key |
|------|------|
| **AOI essence** | Spatial filtering: process only entities in view |
| **3×3-grid efficiency** | O(1) lookup, O(1) update, simple implementation |
| **Finding targets** | coordinates → grid → 3×3 grid → iterate + distance check |
| **Deduplication** | Flag marking > sets (performance) |

That is why most MMORPG game servers use a 3×3-grid AOI system!
