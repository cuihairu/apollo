---
title: AOI九宫格系统详解
icon: eye
order: 4
category:
  - 架构
  - AOI
tag:
  - AOI
  - 九宫格
  - 视野管理
---

# AOI九宫格系统 - 高效的视野管理

## 目录
- [什么是AOI](#什么是aoi)
- [为什么需要AOI系统](#为什么需要aoi系统)
- [为什么九宫格是高效的](#为什么九宫格是高效的)
- [怎么发现目标](#怎么发现目标)
- [怎么去重](#怎么去重)
- [完整实现示例](#完整实现示例)

---

## 什么是AOI？

**AOI (Area of Interest)** 即"感兴趣区域"，在游戏中特指**视野管理**——每个玩家只能看到自己周围一定范围内的其他实体（怪物、NPC、其他玩家）。

```
例如：玩家只能看到周围100米内的实体
        👤 → 感知范围 100m
           ╔═════════╗
           ║  👻 🐉  ║
           ║ 👤 👾  ║
           ╚═════════╝
```

---

## 为什么需要AOI系统？

### 问题场景
假设游戏世界有 **10,000个玩家**：
- 如果没有AOI：每次移动需要通知 **9,999个玩家** → 网络爆炸
- 有AOI（感知100人）：每次只需通知 **100个玩家** → 可接受

---

## 为什么九宫格是高效的？

### 核心思想：空间换时间

将游戏世界划分为固定的网格，每个格子记录其中的实体。

```
世界地图划分:
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

### 九宫格原理

当玩家在格子(5)时，只需要检查**周围9个格子**：

```
           ┌───┬───┬───┐
           │ 1 │ 2 │ 3 │  ← 只需检查这9个格子
           ├───┼───┼───┤
           │ 4 │ 👤│ 6 │
           ├───┼───┼───┤
           │ 7 │ 8 │ 9 │
           └───┴───┴───┘
```

### 为什么高效？

| 方案 | 查找复杂度 | 移动更新 | 说明 |
|------|-----------|---------|------|
| **全表扫描** | O(N) | O(N) | 遍历所有实体，不实用 |
| **四叉树** | O(log N) | O(log N) | 树结构复杂，实现难 |
| **十字链表** | O(1) | O(1) | 只能处理方形视野 |
| **九宫格** | O(1) | O(1) | 简单、高效、稳定 |

**九宫格的优势：**
1. **O(1)查找**：直接根据坐标算出格子编号
2. **O(1)更新**：跨格子时只需更新2个格子
3. **实现简单**：数组即可存储
4. **内存友好**：不需要复杂的树结构

---

## 怎么发现目标？（核心算法）

### 1. 坐标 → 格子编号

```cpp
// 假设格子大小 100x100，原点在地图左上角
int gridX = (int)(posX / 100);
int gridY = (int)(posY / 100);
int gridIndex = gridY * widthInGrids + gridX;
```

### 2. 获取九宫格索引

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
    return grids;  // 最多9个格子
}
```

### 3. 收集视野内实体

```cpp
std::vector<Entity*> getEntitiesInView(Position pos, float viewRadius) {
    std::vector<Entity*> result;

    // 1. 获取九宫格
    std::vector<int> grids = getNineGrids(pos.x / 100, pos.y / 100);

    // 2. 遍历九宫格内的实体
    for (int gridId : grids) {
        for (Entity* entity : gridMap[gridId]) {
            // 3. 精确距离判断（过滤九宫格边角）
            if (distance(pos, entity->pos) <= viewRadius) {
                result.push_back(entity);
            }
        }
    }
    return result;
}
```

---

## 怎么去重？（关键问题）

### 问题：九宫格边界会重复计算

```
     格子A    格子B
┌────────┬────────┐
│  👾1   │  👾2   │  👾1和👾2都在视野内
│    👤  ├────────┤  但👾2会被计算两次！
│  👾3   │  👾4   │
└────────┴────────┘
```

### 去重方案

#### 方案1: 用集合（最简单）
```cpp
std::unordered_set<Entity*> entitySet;  // 自动去重

for (int gridId : nineGrids) {
    for (Entity* entity : gridMap[gridId]) {
        entitySet.insert(entity);  // insert自动去重
    }
}
```

#### 方案2: 位标记法（高性能）
```cpp
// 每个实体有一个frameId标记
struct Entity {
    uint32_t viewFrameId;  // 每帧+1
};

static uint32_t currentFrameId = 0;

std::vector<Entity*> getEntitiesInView(Position pos) {
    currentFrameId++;  // 每次查询递增
    std::vector<Entity*> result;

    for (int gridId : nineGrids) {
        for (Entity* entity : gridMap[gridId]) {
            if (entity->viewFrameId != currentFrameId) {
                entity->viewFrameId = currentFrameId;  // 标记
                result.push_back(entity);
            }
            // else: 已经添加过，跳过
        }
    }
    return result;
}
```

---

## 完整实现示例

```cpp
class AOIManager {
    struct Grid {
        std::vector<Entity*> entities;
    };

    int mapWidth;       // 地图宽度（格子数）
    int mapHeight;      // 地图高度（格子数）
    float gridSize;     // 每格大小
    std::vector<Grid> grids;

public:
    // 进入游戏世界
    void enter(Entity* entity, Position pos) {
        int gridId = getGridId(pos);
        grids[gridId].entities.push_back(entity);
        entity->gridId = gridId;

        // 通知周围玩家
        notifyEnter(entity);
    }

    // 移动
    void move(Entity* entity, Position newPos) {
        int oldGrid = entity->gridId;
        int newGrid = getGridId(newPos);

        entity->pos = newPos;

        if (oldGrid != newGrid) {
            // 跨格子：从旧格子移除，加入新格子
            removeFromGrid(entity, oldGrid);
            addToGrid(entity, newGrid);

            // 重新计算视野变化
            notifyViewChange(entity, oldGrid, newGrid);
        }
    }

    // 离开
    void leave(Entity* entity) {
        removeFromGrid(entity, entity->gridId);
        notifyLeave(entity);
    }

private:
    // 获取视野内实体（带去重）
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

## 总结

| 要点 | 关键 |
|------|------|
| **AOI本质** | 空间过滤，只处理视野内的实体 |
| **九宫格高效** | O(1)查找，O(1)更新，实现简单 |
| **发现目标** | 坐标→格子→九宫格→遍历+距离判断 |
| **去重方法** | 位标记法 > 集合法（性能） |

这就是为什么大多数MMORPG游戏服务器都采用九宫格AOI系统！
