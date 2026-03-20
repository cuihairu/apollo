# Q53: 如何设计场景管理？

## 问题分析

本题考察对场景管理系统的理解：
- 场景分层结构
- 场景加载与卸载
- 场景对象管理
- 场景切换与同步

---

## 一、场景管理架构

### 1.1 系统组成

```
┌─────────────────────────────────────────────────────────────┐
│                    场景管理架构                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  场景层 (Scene Layer):                                      │
│  ├── 场景加载/卸载                                          │
│  ├── 场景资源管理                                          │
│  ├── 场景生命周期                                          │
│  └── 场景切换                                              │
│                          │                                  │
│                          ▼                                  │
│  空间层 (Space Layer):                                      │
│  ├── 空间划分管理                                          │
│  ├── 场景分区管理                                          │
│  ├── AOI 管理                                              │
│  └── 跨场景移动                                            │
│                          │                                  │
│                          ▼                                  │
│  对象层 (Entity Layer):                                    │
│  ├── 实体创建/销毁                                          │
│  ├── 实体状态同步                                          │
│  ├── 实体位置更新                                          │
│  └── 实体属性管理                                          │
│                          │                                  │
│                          ▼                                  │
│  渲染层 (Render Layer):                                    │
│  ├── 场景渲染                                              │
│  ├── LOD 管理                                              │
│  ├── 视锥剔除                                              │
│  └── 场景特效                                              │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 场景类型

```
┌─────────────────────────────────────────────────────────────┐
│                    场景类型                                    │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 游戏场景 (Game Scene)                                   │
│  ┌─────────────────────────────────────────────────┐       │
│  │  - 主游戏区域                                       │       │
│  │  - 玩家主要活动区域                                 │       │
│  │  - 需要完整加载                                     │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  2. 副本场景 (Instance Scene)                               │
│  ┌─────────────────────────────────────────────────┐       │
│  │  - 独立的副本空间                                    │       │
│  │  - 每个队伍独立实例                                  │       │
│  │  - 动态创建和销毁                                   │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  3. 房间隔间 (Room Scene)                                   │
│  ┌─────────────────────────────────────────────────┐       │
│  │  - 房间划分的场景                                    │       │
│  │  - 用于室内场景                                     │       │
│  │  - 门作为连接点                                     │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  4. 过渡场景 (Transition Scene)                             │
│  ┌─────────────────────────────────────────────────┐       │
│  │  - 加载界面                                         │       │
│  │  - 场景切换过渡                                     │       │
│  │  - 资源预加载                                       │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、场景管理实现

### 2.1 场景管理器

```cpp
// 场景管理器

class SceneManager {
public:
    // 初始化场景管理器
    void initialize() {
        // 创建默认场景
        createScene("default");
    }

    // 创建场景
    Scene* createScene(const std::string& name) {
        auto scene = std::make_unique<Scene>();
        scene->name = name;
        scene->sceneId = generateSceneId();
        scene->state = SceneState::INITIALIZING;

        scenes_[scene->sceneId] = std::move(scene);

        INFO("Scene created: {} (id: {})", name, scene->sceneId);

        return scenes_[scene->sceneId].get();
    }

    // 加载场景
    bool loadScene(uint64_t sceneId) {
        auto* scene = getScene(sceneId);
        if (!scene) {
            ERROR("Scene not found: {}", sceneId);
            return false;
        }

        // 如果场景已加载，直接返回
        if (scene->state == SceneState::LOADED) {
            return true;
        }

        INFO("Loading scene: {}", scene->name);

        // 设置为加载中
        scene->state = SceneState::LOADING;

        // 加载场景资源
        if (!loadSceneResources(scene)) {
            scene->state = SceneState::UNLOADED;
            return false;
        }

        // 初始化场景
        if (!initializeScene(scene)) {
            scene->state = SceneState::UNLOADED;
            return false;
        }

        scene->state = SceneState::LOADED;
        INFO("Scene loaded: {}", scene->name);

        return true;
    }

    // 卸载场景
    bool unloadScene(uint64_t sceneId) {
        auto* scene = getScene(sceneId);
        if (!scene) {
            return false;
        }

        INFO("Unloading scene: {}", scene->name);

        // 通知场景中的实体
        for (auto* entity : scene->entities) {
            entity->onSceneUnload(sceneId);
        }

        // 清理场景
        cleanupScene(scene);

        scene->state = SceneState::UNLOADED;

        return true;
    }

    // 销毁场景
    bool destroyScene(uint64_t sceneId) {
        auto it = scenes_.find(sceneId);
        if (it == scenes_.end()) {
            return false;
        }

        INFO("Destroying scene: {}", it->second->name);

        // 先卸载
        unloadScene(sceneId);

        // 移除场景
        scenes_.erase(it);

        return true;
    }

    // 获取场景
    Scene* getScene(uint64_t sceneId) {
        auto it = scenes_.find(sceneId);
        return it != scenes_.end() ? it->second.get() : nullptr;
    }

    // 添加实体到场景
    bool addEntityToScene(uint64_t entityId, uint64_t sceneId) {
        auto* scene = getScene(sceneId);
        if (!scene) {
            return false;
        }

        // 检查实体是否已在场景中
        if (getEntityScene(entityId) != nullptr) {
            return false;
        }

        // 添加到场景
        scene->entities.push_back(entityId);
        entityToScene_[entityId] = sceneId;

        // 通知实体
        auto* entity = getEntity(entityId);
        if (entity) {
            entity->onSceneEnter(sceneId);
        }

        return true;
    }

    // 从场景移除实体
    bool removeEntityFromScene(uint64_t entityId) {
        auto it = entityToScene_.find(entityId);
        if (it == entityToScene_.end()) {
            return false;
        }

        uint64_t sceneId = it->second;
        auto* scene = getScene(sceneId);
        if (!scene) {
            return false;
        }

        // 从场景实体列表移除
        auto entityIt = std::find(scene->entities.begin(), scene->entities.end(), entityId);
        if (entityIt != scene->entities.end()) {
            scene->entities.erase(entityIt);
        }

        // 通知实体
        auto* entity = getEntity(entityId);
        if (entity) {
            entity->onSceneLeave(sceneId);
        }

        entityToScene_.erase(it);

        return true;
    }

    // 更新场景 (每帧调用)
    void update(uint32 deltaTime) {
        for (auto& [sceneId, scene] : scenes_) {
            if (scene->state == SceneState::LOADED) {
                updateScene(scene.get(), deltaTime);
            }
        }
    }

    // 获取实体所在场景
    Scene* getEntityScene(uint64_t entityId) {
        auto it = entityToScene_.find(entityId);
        if (it == entityToScene_.end()) {
            return nullptr;
        }
        return getScene(it->second);
    }

private:
    bool loadSceneResources(Scene* scene) {
        // 加载地形
        if (!loadTerrain(scene)) {
            return false;
        }

        // 加载静态对象
        if (!loadStaticObjects(scene)) {
            return false;
        }

        // 加载光源
        if (!loadLights(scene)) {
            return false;
        }

        // 加载特效
        if (!loadEffects(scene)) {
            return false;
        }

        return true;
    }

    bool initializeScene(Scene* scene) {
        // 初始化物理
        if (!initPhysics(scene)) {
            return false;
        }

        // 初始化导航网格
        if (!initNavMesh(scene)) {
            return false;
        }

        // 初始化AOI
        if (!initAOI(scene)) {
            return false;
        }

        return true;
    }

    void updateScene(Scene* scene, uint32 deltaTime) {
        // 更新场景中的实体
        for (uint64_t entityId : scene->entities) {
            auto* entity = getEntity(entityId);
            if (entity) {
                entity->update(deltaTime);
            }
        }

        // 更新场景特效
        updateEffects(scene, deltaTime);

        // 更新物理
        updatePhysics(scene, deltaTime);

        // 更新AOI
        updateAOI(scene);
    }

    void cleanupScene(Scene* scene) {
        // 清理实体
        scene->entities.clear();

        // 清理资源
        scene->resources.clear();

        // 清理物理
        cleanupPhysics(scene);

        // 清理导航网格
        cleanupNavMesh(scene);
    }

    std::unordered_map<uint64_t, std::unique_ptr<Scene>> scenes_;
    std::unordered_map<uint64_t, uint64_t> entityToScene_;  // entityId -> sceneId
};

// 场景数据结构
enum class SceneState {
    UNLOADED = 0,       // 未加载
    INITIALIZING = 1,   // 初始化中
    LOADING = 2,        // 加载中
    LOADED = 3,         // 已加载
    UNLOADING = 4,      // 卸载中
};

struct Scene {
    uint64_t sceneId;
    std::string name;
    SceneState state;

    // 场景内容
    std::vector<uint64_t> entities;
    std::vector<std::unique_ptr<SceneObject>> staticObjects;
    std::vector<std::unique_ptr<Light>> lights;
    std::vector<std::unique_ptr<Effect>> effects;

    // 场景数据
    Terrain* terrain;
    NavMesh* navMesh;
    PhysicsWorld* physicsWorld;
    AOIManager* aoiManager;

    // 场景配置
    SceneConfig config;
};
```

---

## 三、场景切换

### 3.1 切换流程

```
┌─────────────────────────────────────────────────────────────┐
│                    场景切换流程                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 准备切换                                                │
│     ├── 保存当前场景状态                                    │
│     ├── 通知实体即将离开场景                                │
│     └── 显示加载界面                                        │
│                                                             │
│  2. 卸载当前场景                                            │
│     ├── 停止场景更新                                        │
│     ├── 卸载场景资源                                        │
│     └── 清理场景数据                                        │
│                                                             │
│  3. 加载目标场景                                            │
│     ├── 加载场景资源                                        │
│     ├── 初始化场景数据                                        │
│     └── 预加载周边区域                                      │
│                                                             │
│  4. 切换完成                                                │
│     ├── 传送玩家到新场景                                    │
│     ├── 恢复玩家状态                                        │
│     ├── 隐藏加载界面                                        │
│     └── 开始场景更新                                        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 场景切换实现

```cpp
// 场景切换

class SceneTransition {
public:
    // 切换场景
    bool transition(uint64_t playerId, uint64_t fromSceneId,
                   uint64_t toSceneId, const Vector3& position) {
        // 1. 保存玩家状态
        PlayerState state = savePlayerState(playerId);

        // 2. 显示加载界面
        showLoadingScreen(playerId);

        // 3. 从旧场景移除
        auto* fromScene = sceneManager_->getScene(fromSceneId);
        if (fromScene) {
            sceneManager_->removeEntityFromScene(playerId);
        }

        // 4. 加载目标场景 (如果未加载)
        auto* toScene = sceneManager_->getScene(toSceneId);
        if (!toScene || toScene->state != SceneState::LOADED) {
            if (!sceneManager_->loadScene(toSceneId)) {
                // 加载失败，返回原场景
                hideLoadingScreen(playerId);
                return false;
            }
            toScene = sceneManager_->getScene(toSceneId);
        }

        // 5. 添加到新场景
        if (!sceneManager_->addEntityToScene(playerId, toSceneId)) {
            hideLoadingScreen(playerId);
            return false;
        }

        // 6. 设置玩家位置
        auto* entity = getEntity(playerId);
        if (entity) {
            entity->setPosition(position);
            entity->onSceneEnter(toSceneId);
        }

        // 7. 恢复玩家状态
        restorePlayerState(playerId, state);

        // 8. 隐藏加载界面
        hideLoadingScreen(playerId);

        // 9. 通知客户端
        sendToClient(playerId, "onSceneChanged", toSceneId, position.x, position.y, position.z);

        INFO("Player {} transitioned from scene {} to scene {}",
             playerId, fromSceneId, toSceneId);

        return true;
    }

    // 异步切换场景 (用于跨服务器场景)
    bool asyncTransition(uint64_t playerId, const std::string& targetServer,
                        uint64_t toSceneId, const Vector3& position) {
        // 1. 保存玩家数据到数据库
        savePlayerData(playerId);

        // 2. 通知目标服务器
        if (!notifyTargetServer(targetServer, playerId, toSceneId, position)) {
            return false;
        }

        // 3. 断开当前连接
        disconnectPlayer(playerId);

        return true;
    }

private:
    PlayerState savePlayerState(uint64_t playerId) {
        PlayerState state;
        auto* entity = getEntity(playerId);
        if (entity) {
            state.position = entity->getPosition();
            state.rotation = entity->getRotation();
            state.hp = entity->getHP();
            state.mp = entity->getMP();
        }
        return state;
    }

    void restorePlayerState(uint64_t playerId, const PlayerState& state) {
        auto* entity = getEntity(playerId);
        if (entity) {
            entity->setPosition(state.position);
            entity->setRotation(state.rotation);
            entity->setHP(state.hp);
            entity->setMP(state.mp);
        }
    }

    void showLoadingScreen(uint64_t playerId) {
        sendToClient(playerId, "onShowLoadingScreen");
    }

    void hideLoadingScreen(uint64_t playerId) {
        sendToClient(playerId, "onHideLoadingScreen");
    }
};
```

---

## 四、KBEngine 场景管理

### 4.1 KBEngine Space 管理

```python
# KBEngine Space 管理

# scripts/spaces/space_base.py
import KBEngine
from KBEDedef import *

class SpaceBase(KBEngine.Space):
    def __init__(self):
        KBEngine.Space.__init__(self)

        # Space 数据
        self.spaceID = id
        self.spaceName = ""
        self.entityCount = 0
        self.maxEntities = 100

        # Space 配置
        self.isPVP = False
        self.isInstance = False
        self.cell = None

    def onEnter(self, entity):
        """实体进入 Space"""
        self.entityCount += 1
        INFO(f"Entity {entity.id} entered space {self.spaceID}")
        entity.onEnterSpace(self.spaceID)

    def onLeave(self, entity):
        """实体离开 Space"""
        self.entityCount -= 1
        INFO(f"Entity {entity.id} left space {self.spaceID}")
        entity.onLeaveSpace(self.spaceID)

    def isFull(self):
        """检查 Space 是否已满"""
        return self.entityCount >= self.maxEntities

    def getEntitiesInRange(self, position, range):
        """获取范围内的实体"""
        entities = []
        for entityID, entity in KBEngine.entities.items():
            if hasattr(entity, 'spaceID') and entity.spaceID == self.spaceID:
                if entity.position.distanceTo(position) <= range:
                    entities.append(entity)
        return entities

# scripts/spaces/space_instance.py
class SpaceInstance(SpaceBase):
    def __init__(self):
        SpaceBase.__init__(self)

        self.isInstance = True
        self.teamID = 0
        self.ownerID = 0
        self.createTime = 0

    def initialize(self, teamID, mapID):
        """初始化副本"""
        self.teamID = teamID
        self.createTime = time.time()

        # 创建 CellApp
        self.createCell(mapID)

    def createCell(self, mapID):
        """创建 Cell"""
        self.cell = KBEngine.createEntityAnywhere(Cell, {})
        self.cell.spaceID = self.spaceID
        self.cell.mapID = mapID

    def onAllPlayersLeft(self):
        """所有玩家离开副本"""
        # 延迟销毁副本
        KBEngine.addTimer(30, 0, self.destroy)

    def destroy(self):
        """销毁副本"""
        INFO(f"Destroying instance space {self.spaceID}")
        # 销毁所有实体
        for entityID in self.getEntities():
            KBEngine.destroyEntity(entityID)
        # 销毁自己
        KBEngine.destroyEntity(self.spaceID)
```

---

## 五、最佳实践

### 5.1 场景管理设计建议

| 实践 | 说明 |
|------|------|
| **异步加载** | 后台加载场景资源 |
| **资源池化** | 复用场景资源 |
| **分层卸载** | 优先卸载远处场景 |
| **状态保存** | 场景切换保存状态 |
| **预加载** | 预加载可能进入的场景 |

### 5.2 性能优化

```
优化策略:
1. 场景资源池化共享
2. LOD 分级加载
3. 异步场景切换
4. 增量场景更新
5. 对象池管理实体
```

---

## 六、总结

### 场景管理核心

```
场景管理 = 场景加载 + 实体管理 + 场景切换
- 按需加载场景资源
- 管理场景中的实体
- 平滑的场景切换
- AOI 优化同步范围
```

---

## 参考资料

- [Unity 场景管理](https://docs.unity3d.com/)
- [Unreal World Partition](https://docs.unrealengine.com/)
- [KBEngine Space 文档](https://kbengine.github.io/docs/)
