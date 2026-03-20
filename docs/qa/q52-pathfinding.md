# Q52: 如何设计寻路系统？

## 问题分析

本题考察对寻路系统的理解：
- 寻路算法 (A*, Dijkstra)
- 导航网格设计
- 动态障碍处理
- 群体寻路优化

---

## 一、寻路算法

### 1.1 常用寻路算法

```
┌─────────────────────────────────────────────────────────────┐
│                    寻路算法对比                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Dijkstra 算法                                              │
│  ┌─────────────────────────────────────────────────┐       │
│  │  特点: 保证找到最短路径                            │       │
│  │  优点: 简单可靠，适用于无权图                       │       │
│  │  缺点: 效率较低，搜索范围大                         │       │
│  │  复杂度: O(V²) 或 O(E+VlogV)                       │       │
│  │  适用: 小地图、无启发式信息                          │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  A* 算法                                                   │
│  ┌─────────────────────────────────────────────────┐       │
│  │  特点: 使用启发式函数指导搜索                       │       │
│  │  f(n) = g(n) + h(n)                              │       │
│  │  g(n): 从起点到当前节点的实际代价                   │       │
│  │  h(n): 从当前节点到终点的估计代价 (启发式)          │       │
│  │                                                   │       │
│  │  优点: 比Dijkstra更快，同样保证最短路径              │       │
│  │  缺点: 需要设计好的启发式函数                       │       │
│  │  复杂度: O(E) (实际比Dijkstra快得多)                │       │
│  │  适用: 大多数游戏场景                               │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  JPS (Jump Point Search)                                   │
│  ┌─────────────────────────────────────────────────┐       │
│  │  特点: A* 的优化版本，跳过不必要的节点               │       │
│  │  优点: 减少搜索节点数量，大幅提升性能                 │       │
│  │  缺点: 只适用于网格地图                             │       │
│  │  适用: 网格地图、RTS游戏                            │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  NavMesh (导航网格)                                         │
│  ┌─────────────────────────────────────────────────┐       │
│  │  特点: 基于凸多边形区域的寻路                       │       │
│  │  优点: 精度高，支持任意形状地图                      │       │
│  │  缺点: 预处理复杂，内存占用大                       │       │
│  │  适用: 3D游戏、复杂场景                             │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 A* 算法实现

```cpp
// A* 寻路算法

struct PathNode {
    int x, y;
    float g;           // 从起点到当前点的代价
    float h;           // 从当前点到终点的估计代价
    float f;           // f = g + h
    PathNode* parent;  // 父节点

    PathNode(int x_, int y_) : x(x_), y(y_), g(0), h(0), f(0), parent(nullptr) {}

    bool operator<(const PathNode& other) const {
        return f > other.f;  // 用于优先队列 (最小堆)
    }
};

class AStarPathfinding {
public:
    // 寻找路径
    std::vector<Vector2> findPath(const Vector2& start, const Vector2& goal,
                                   const GameMap& map) {
        // 转换为网格坐标
        int startX = static_cast<int>(start.x / CELL_SIZE);
        int startY = static_cast<int>(start.y / CELL_SIZE);
        int goalX = static_cast<int>(goal.x / CELL_SIZE);
        int goalY = static_cast<int>(goal.y / CELL_SIZE);

        // 检查边界
        if (!isValid(startX, startY, map) || !isValid(goalX, goalY, map)) {
            return {};
        }

        // 起点即终点
        if (startX == goalX && startY == goalY) {
            return {start};
        }

        // 开启列表和关闭列表
        std::priority_queue<PathNode*> openList;
        std::map<std::pair<int, int>, PathNode*> openMap;
        std::map<std::pair<int, int>, bool> closedList;

        // 创建起点节点
        PathNode* startNode = new PathNode(startX, startY);
        openList.push(startNode);
        openMap[{startX, startY}] = startNode;

        // 方向数组 (8方向)
        int dx[] = {-1, -1, -1, 0, 0, 1, 1, 1};
        int dy[] = {-1, 0, 1, -1, 1, -1, 0, 1};
        float cost[] = {1.414f, 1.0f, 1.414f, 1.0f, 1.0f, 1.414f, 1.0f, 1.414f};

        while (!openList.empty()) {
            // 获取f值最小的节点
            PathNode* current = openList.top();
            openList.pop();
            openMap.erase({current->x, current->y});

            // 到达终点
            if (current->x == goalX && current->y == goalY) {
                auto path = reconstructPath(current);
                cleanupNodes(openMap, closedList);
                return path;
            }

            // 加入关闭列表
            closedList[{current->x, current->y}] = true;

            // 检查8个方向
            for (int i = 0; i < 8; ++i) {
                int nx = current->x + dx[i];
                int ny = current->y + dy[i];

                // 检查边界
                if (!isValid(nx, ny, map)) {
                    continue;
                }

                // 检查障碍物
                if (isBlocked(nx, ny, map)) {
                    continue;
                }

                // 检查关闭列表
                if (closedList.find({nx, ny}) != closedList.end()) {
                    continue;
                }

                // 计算代价
                float tentativeG = current->g + cost[i];

                // 检查是否已在开启列表
                auto it = openMap.find({nx, ny});
                PathNode* neighbor = nullptr;

                if (it == openMap.end()) {
                    // 创建新节点
                    neighbor = new PathNode(nx, ny);
                    openMap[{nx, ny}] = neighbor;
                } else {
                    neighbor = it->second;

                    // 如果新路径更差，跳过
                    if (tentativeG >= neighbor->g) {
                        continue;
                    }
                }

                // 更新节点
                neighbor->parent = current;
                neighbor->g = tentativeG;
                neighbor->h = heuristic(nx, ny, goalX, goalY);
                neighbor->f = neighbor->g + neighbor->h;

                // 如果是新节点，加入开启列表
                if (it == openMap.end()) {
                    openList.push(neighbor);
                }
            }
        }

        // 没找到路径
        cleanupNodes(openMap, closedList);
        return {};
    }

private:
    // 启发式函数 (曼哈顿距离)
    float heuristic(int x1, int y1, int x2, int y2) {
        return std::abs(x1 - x2) + std::abs(y1 - y2);
    }

    // 欧几里得距离启发式
    float euclideanHeuristic(int x1, int y1, int x2, int y2) {
        float dx = x1 - x2;
        float dy = y1 - y2;
        return std::sqrt(dx * dx + dy * dy);
    }

    // 重建路径
    std::vector<Vector2> reconstructPath(PathNode* node) {
        std::vector<Vector2> path;

        while (node) {
            // 转换为世界坐标
            float wx = node->x * CELL_SIZE + CELL_SIZE / 2;
            float wy = node->y * CELL_SIZE + CELL_SIZE / 2;
            path.push_back({wx, wy});
            node = node->parent;
        }

        std::reverse(path.begin(), path.end());
        return path;
    }

    bool isValid(int x, int y, const GameMap& map) {
        return x >= 0 && x < map.width && y >= 0 && y < map.height;
    }

    bool isBlocked(int x, int y, const GameMap& map) {
        return map.isBlocked(x, y);
    }

    void cleanupNodes(std::map<std::pair<int, int>, PathNode*>& openMap,
                     std::map<std::pair<int, int>, bool>& closedList) {
        for (auto& [key, node] : openMap) {
            delete node;
        }
        // 关闭列表中的节点也在开启列表中处理了
    }

    static constexpr float CELL_SIZE = 1.0f;
};
```

---

## 二、导航网格

### 2.1 NavMesh 设计

```
┌─────────────────────────────────────────────────────────────┐
│                    导航网格 (NavMesh)                         │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  概念: 将地图划分为凸多边形区域                             │
│                                                             │
│  优势:                                                     │
│  ├── 精确的路径 - 沿多边形边界移动                          │
│  ├── 支持任意形状 - 适用于复杂3D场景                         │
│  ├── 内存效率高 - 只需存储多边形                            │
│  └── 支持跳跃、爬墙等动作                                   │
│                                                             │
│  构建流程:                                                  │
│  ┌─────────────────────────────────────────────────┐       │
│  │  1. 输入场景几何数据                                │       │
│  │     ↓                                            │       │
│  │  2. 生成可行走区域                                  │       │
│  │     ↓                                            │       │
│  │  3. 区域分割为凸多边形                              │       │
│  │     ↓                                            │       │
│  │  4. 构建邻接关系                                    │       │
│  │     ↓                                            │       │
│  │  5. 生成导航网格                                    │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  寻路过程:                                                  │
│  ┌─────────────────────────────────────────────────┐       │
│  │  1. 找到起点和终点所在的多边形                      │       │
│  │     ↓                                            │       │
│  │  2. 使用 A* 在多边形间寻路                          │       │
│  │     ↓                                            │       │
│  │  3. 在多边形内部优化路径                            │       │
│  │     ↓                                            │       │
│  │  4. 生成平滑路径                                    │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 NavMesh 数据结构

```cpp
// 导航网格

struct NavMeshPolygon {
    int id;
    std::vector<Vector3> vertices;     // 多边形顶点
    Vector3 center;                    // 中心点
    std::vector<int> neighbors;        // 邻接多边形ID
    float height;                      // 高度 (用于3D)
    int area;                          // 面积
};

struct NavMeshEdge {
    int from;                          // 起始多边形ID
    int to;                            // 目标多边形ID
    Vector3 start;                     // 边起点
    Vector3 end;                       // 边终点
    float cost;                        // 通过代价
};

class NavMesh {
public:
    // 加载导航网格
    bool load(const std::string& filename) {
        // 从文件加载预生成的导航网格
        // ...

        // 构建空间索引 (加速查询)
        buildSpatialIndex();

        return true;
    }

    // 查找点所在的多边形
    int findPolygonAt(const Vector3& point) {
        // 使用空间索引快速查找
        auto candidates = spatialIndex_->query(point);

        for (int polyId : candidates) {
            if (isPointInPolygon(polyId, point)) {
                return polyId;
            }
        }

        return -1;
    }

    // A* 寻路 (多边形级别)
    std::vector<Vector3> findPath(const Vector3& start, const Vector3& goal) {
        int startPoly = findPolygonAt(start);
        int goalPoly = findPolygonAt(goal);

        if (startPoly < 0 || goalPoly < 0) {
            return {};
        }

        // 多边形 A* 寻路
        std::vector<int> polyPath = findPolygonPath(startPoly, goalPoly);

        if (polyPath.empty()) {
            return {};
        }

        // 转换为世界坐标路径
        return buildWorldPath(polyPath, start, goal);
    }

private:
    std::vector<int> findPolygonPath(int startPoly, int goalPoly) {
        // 使用 A* 在多边形图上寻路
        std::priority_queue<PolyNode*> openList;
        std::map<int, PolyNode*> allNodes;

        PolyNode* startNode = new PolyNode{startPoly, 0, heuristic(startPoly, goalPoly), nullptr};
        openList.push(startNode);
        allNodes[startPoly] = startNode;

        std::map<int, bool> closedList;

        while (!openList.empty()) {
            PolyNode* current = openList.top();
            openList.pop();

            if (current->id == goalPoly) {
                auto path = reconstructPolyPath(current);
                cleanupNodes(allNodes);
                return path;
            }

            closedList[current->id] = true;

            // 遍历邻接多边形
            for (int neighborId : polygons_[current->id].neighbors) {
                if (closedList.find(neighborId) != closedList.end()) {
                    continue;
                }

                // 计算代价
                float edgeCost = getEdgeCost(current->id, neighborId);
                float tentativeG = current->g + edgeCost;

                auto it = allNodes.find(neighborId);
                PolyNode* neighbor = nullptr;

                if (it == allNodes.end()) {
                    neighbor = new PolyNode{neighborId, tentativeG,
                                          heuristic(neighborId, goalPoly), current};
                    allNodes[neighborId] = neighbor;
                    openList.push(neighbor);
                } else {
                    neighbor = it->second;
                    if (tentativeG < neighbor->g) {
                        neighbor->g = tentativeG;
                        neighbor->f = neighbor->g + neighbor->h;
                        neighbor->parent = current;
                    }
                }
            }
        }

        cleanupNodes(allNodes);
        return {};
    }

    bool isPointInPolygon(int polyId, const Vector3& point) {
        const auto& poly = polygons_[polyId];

        // 使用射线法判断点是否在多边形内
        int crossings = 0;
        int n = poly.vertices.size();

        for (int i = 0; i < n; ++i) {
            const Vector3& v1 = poly.vertices[i];
            const Vector3& v2 = poly.vertices[(i + 1) % n];

            if ((v1.z > point.z) != (v2.z > point.z)) {
                float atX = (v2.x - v1.x) * (point.z - v1.z) / (v2.z - v1.z) + v1.x;
                if (point.x < atX) {
                    crossings++;
                }
            }
        }

        return crossings % 2 == 1;
    }

    float heuristic(int polyId1, int polyId2) {
        // 使用多边形中心距离作为启发式
        return distance(polygons_[polyId1].center, polygons_[polyId2].center);
    }

    float getEdgeCost(int from, int to) {
        // 查找边代价
        for (const auto& edge : edges_) {
            if (edge.from == from && edge.to == to) {
                return edge.cost;
            }
        }
        return 1.0f;
    }

    std::vector<Vector3> buildWorldPath(const std::vector<int>& polyPath,
                                        const Vector3& start, const Vector3& goal) {
        std::vector<Vector3> path;
        path.push_back(start);

        // 添加多边形连接点 (边的中点)
        for (size_t i = 0; i < polyPath.size() - 1; ++i) {
            Vector3 midPoint = getEdgeMidpoint(polyPath[i], polyPath[i + 1]);
            path.push_back(midPoint);
        }

        path.push_back(goal);

        // 路径平滑
        return smoothPath(path);
    }

    std::vector<Vector3> smoothPath(const std::vector<Vector3>& path) {
        if (path.size() <= 2) {
            return path;
        }

        std::vector<Vector3> smoothed;
        smoothed.push_back(path[0]);

        size_t current = 0;
        while (current < path.size() - 1) {
            size_t next = current + 1;

            // 尝试跳过中间点
            while (next < path.size() - 1) {
                if (hasLineOfSight(path[current], path[next + 1])) {
                    next++;
                } else {
                    break;
                }
            }

            smoothed.push_back(path[next]);
            current = next;
        }

        return smoothed;
    }

    bool hasLineOfSight(const Vector3& from, const Vector3& to) {
        // 射线检测，检查两点间是否有障碍物
        // ...
        return true;
    }

    struct PolyNode {
        int id;
        float g, h, f;
        PolyNode* parent;

        bool operator<(const PolyNode& other) const {
            return f > other.f;
        }
    };

    std::vector<NavMeshPolygon> polygons_;
    std::vector<NavMeshEdge> edges_;
    SpatialIndex* spatialIndex_;  // 空间索引 (四叉树等)
};
```

---

## 三、动态障碍处理

### 3.1 局部避障

```cpp
// 局部避障 (Steering Behavior)

class SteeringBehavior {
public:
    struct Obstacle {
        Vector3 position;
        float radius;
    };

    // 寻找行为
    Vector3 seek(const Vector3& current, const Vector3& target, float maxSpeed) {
        Vector3 desired = normalize(target - current) * maxSpeed;
        return desired - velocity_;
    }

    // 逃离行为
    Vector3 flee(const Vector3& current, const Vector3& threat, float maxSpeed) {
        Vector3 desired = normalize(current - threat) * maxSpeed;
        return desired - velocity_;
    }

    // 到达行为 (减速接近目标)
    Vector3 arrive(const Vector3& current, const Vector3& target,
                  float maxSpeed, float slowRadius) {
        Vector3 desired = target - current;
        float dist = length(desired);
        desired = normalize(desired);

        if (dist < slowRadius) {
            desired *= maxSpeed * (dist / slowRadius);
        } else {
            desired *= maxSpeed;
        }

        return desired - velocity_;
    }

    // 避障行为
    Vector3 avoidObstacle(const Vector3& current, const Vector3& velocity,
                          float maxSpeed, const std::vector<Obstacle>& obstacles) {
        Vector3 steering(0, 0, 0);

        for (const auto& obstacle : obstacles) {
            Vector3 toObstacle = obstacle.position - current;
            float dist = length(toObstacle);

            // 只考虑前方障碍物
            float dot = dotProduct(normalize(velocity), normalize(toObstacle));
            if (dot < 0.7f) {  // 障碍物不在前方45度范围内
                continue;
            }

            // 计算避障力
            if (dist < obstacle.radius + 2.0f) {
                // 向侧面避让
                Vector3 avoidDir = normalize(current - obstacle.position);
                steering += avoidDir * maxSpeed * (1.0f / dist);
            }
        }

        return steering;
    }

    // 分离行为 (避免群体重叠)
    Vector3 separation(const Vector3& current, const std::vector<Vector3>& neighbors,
                      float separationRadius) {
        Vector3 steering(0, 0, 0);
        int count = 0;

        for (const auto& neighbor : neighbors) {
            Vector3 diff = current - neighbor;
            float dist = length(diff);

            if (dist > 0 && dist < separationRadius) {
                diff = normalize(diff) / dist;  // 距离越近力越大
                steering += diff;
                count++;
            }
        }

        if (count > 0) {
            steering /= count;
            steering = normalize(steering) * maxSpeed_ - velocity_;
        }

        return steering;
    }

    // 组合行为
    Vector3 calculate(const Vector3& current, const Vector3& target,
                     const std::vector<Obstacle>& obstacles,
                     const std::vector<Vector3>& neighbors) {
        Vector3 force(0, 0, 0);

        // 寻找目标
        force += seek(current, target, maxSpeed_) * 1.0f;

        // 避障
        force += avoidObstacle(current, velocity_, maxSpeed_, obstacles) * 3.0f;

        // 群体分离
        if (!neighbors.empty()) {
            force += separation(current, neighbors, 2.0f) * 1.5f;
        }

        // 限制转向力
        if (length(force) > maxForce_) {
            force = normalize(force) * maxForce_;
        }

        // 更新速度
        velocity_ += force * deltaTime_;

        // 限制速度
        if (length(velocity_) > maxSpeed_) {
            velocity_ = normalize(velocity_) * maxSpeed_;
        }

        return velocity_;
    }

private:
    Vector3 velocity_;
    float maxSpeed_ = 5.0f;
    float maxForce_ = 10.0f;
    float deltaTime_ = 0.016f;
};
```

---

## 四、性能优化

### 4.1 优化策略

```
┌─────────────────────────────────────────────────────────────┐
│                    寻路性能优化                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 路径缓存                                                 │
│  ┌─────────────────────────────────────────────────┐       │
│  │  - 缓存常用路径                                   │       │
│  │  - LRU 缓存淘汰                                  │       │
│  │  - 分层级缓存 (宏观/微观)                        │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  2. 分层寻路 (HPA*)                                         │
│  ┌─────────────────────────────────────────────────┐       │
│  │  - 高层: 大格子快速寻路                            │       │
│  │  - 低层: 小格子精细寻路                            │       │
│  │  - 大幅减少搜索节点数                              │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  3. 路径复用                                                │
│  ┌─────────────────────────────────────────────────┐       │
│  │  - 相同起点终点共享路径                            │       │
│  │  - 群体单位部分跟随                                │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  4. 帧分割                                                  │
│  ┌─────────────────────────────────────────────────┐       │
│  │  - 每帧只处理部分寻路请求                          │       │
│  │  - 避免单帧卡顿                                    │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  5. LOD (Level of Detail)                                  │
│  ┌─────────────────────────────────────────────────┐       │
│  │  - 远距离单位使用简化寻路                          │       │
│  │  - 近距离单位使用精确寻路                          │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 五、最佳实践

### 5.1 寻路系统设计建议

| 实践 | 说明 |
|------|------|
| **预计算** | 离线生成导航网格 |
| **分层寻路** | 宏观路径 + 微观调整 |
| **路径缓存** | 缓存常用路径 |
| **局部避障** | 动态障碍实时避让 |
| **异步处理** | 后台线程寻路 |

---

## 六、总结

### 寻路系统核心

```
寻路系统 = 全局寻路 + 局部避障 + 路径平滑
- A*/NavMesh 全局寻路
- Steering Behavior 局部避障
- 路径缓存提升性能
- 分层处理降低复杂度
```

---

## 参考资料

- [A* 算法详解](https://www.redblobgames.com/pathfinding/a-star/introduction.html)
- [Recast Navigation](https://github.com/recastnavigation/recastnavigation)
- [游戏编程精粹系列](https://www.gameprogramminggems.com/)
