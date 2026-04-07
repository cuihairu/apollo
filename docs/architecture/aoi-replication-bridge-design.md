---
title: AOI 到复制链桥接层设计
icon: tower-broadcast
order: 26
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - AOI
  - Replication
  - Visibility
  - WorldHost
---

# AOI 到复制链桥接层设计

这篇文档只解决一个问题：

`Apollo 里 AOI 计算出的“谁可见谁”，应该如何接到属性同步与 world 复制链上，而不是继续停留在一个孤立查询模块。`

这篇文档承接：

- [属性同步、持久化与 RPC 边界设计](./attribute-sync-persistence-rpc-boundary.md)
- [属性同步、持久化与 RPC 现状映射](./attribute-sync-persistence-rpc-current-mapping.md)
- [属性持久化投影层设计](./attribute-persistence-projection-design.md)
- [World 进入与切图设计](./world-entry-transfer-design.md)

这里要补的是最后一块关键中层：

- `AOIService`
  到
- `AttributeSyncProjector / ReplicationPublisher`

之间的桥。

## 一、为什么必须单独有桥接层

Apollo 当前已经有：

- `AOIManager`
- `AttributeSyncController`
- `WorldSession`

但这三者现在还是并列存在的。

如果中间没有独立桥接层，后面通常会滑向两种错误：

### 1. 让 AOI 直接负责属性打包

这样会导致：

- AOI 模块既要算可见集，又要理解属性同步协议
- AOI 被迫知道 full sync / delta / resend / reliability
- 空间索引和复制语义重新耦合

### 2. 让属性同步器自己查 AOI

这样会导致：

- 同步器开始理解 scene / entity / view range
- `game/attributes` 反向依赖 `game/world`
- 属性同步器越来越像 KBE 的重型 `Witness`

Apollo 更合理的路线不是复制 KBE 的“大一统 Witness”，而是明确三层：

- AOI 负责“谁可见谁”
- Replication Bridge 负责“可见性变化如何变成复制任务”
- Sync Projector / Publisher 负责“具体发什么、怎么发”

## 二、先说结论

Apollo 更合理的 world 复制链应该是：

```text
AOIService
  -> VisibilityResolver
  -> ReplicationService
      -> FullSyncPlanner
      -> DeltaSyncPlanner
      -> DespawnPlanner
  -> AttributeSyncProjector
  -> AttributeSyncPublisher
  -> Gateway / Client Route
```

其中：

- `AOIService` 只产出 visibility 变化
- `ReplicationService` 只做复制决策
- `AttributeSyncProjector` 只做状态批次投影
- `Publisher` 只做投递

## 三、AOI 的准确边界

AOI 这层应只回答：

- 当前谁能看到谁
- 谁刚进入视野
- 谁刚离开视野
- 某个实体当前可见集是什么

也就是说，它输出的应该是：

- `VisibilitySet`
- `VisibilityDelta`

而不应该输出：

- 客户端 createEntity 包
- 属性同步包
- 销毁包

当前 `AOIManager::GetVisibleEntities(...)` 已经说明它现在更接近这个定位，这是对的。[aoi.cpp](/Users/cui/Workspaces/apollo/modules/game/world/src/aoi.cpp#L209)

## 四、为什么 `Audience::Aoi` 现在还不够

Apollo 当前同步规则里已经有：

- `AttributeSyncAudience::Aoi`

这很好，但它现在只是一个：

- 规则标签

还不是：

- 已解析的视野受众结果

因为“发给 AOI”至少还差三步：

### 1. 知道当前观察者是谁

例如：

- 玩家自己的 world session
- 对应 avatar entity

### 2. 知道当前可见集是谁

也就是：

- 当前视野内实体列表

### 3. 知道这次是 enter / leave / update 中的哪一种

因为：

- enter 需要 full sync
- leave 需要 despawn
- update 才是 delta sync

所以：

`Audience::Aoi` 只能表达“这类属性允许面向 AOI 传播”，不能替代 replication bridge。`

## 五、推荐中层对象

## 六、VisibilitySet

建议先定义世界侧统一的可见集表达：

```cpp
struct VisibilitySet {
    uint64_t observerEntityId = 0;
    uint32_t sceneId = 0;
    std::unordered_set<uint64_t> visibleEntityIds;
};
```

它只描述：

- 某个观察者当前能看见哪些实体

## 七、VisibilityDelta

再定义增量变化对象：

```cpp
struct VisibilityDelta {
    uint64_t observerEntityId = 0;
    uint32_t sceneId = 0;
    std::vector<uint64_t> enteredEntityIds;
    std::vector<uint64_t> leftEntityIds;
    std::vector<uint64_t> stayedEntityIds;
};
```

这是 AOI 和复制桥接之间最关键的交界对象。

原因很简单：

- 复制链需要的不是“某个时刻的几何查询”
- 而是“视野变化后的复制动作计划”

## 八、VisibilityResolver

建议补一层：

```cpp
class IVisibilityResolver {
public:
    virtual ~IVisibilityResolver() = default;
    virtual VisibilitySet current(uint64_t observerEntityId) = 0;
    virtual std::optional<VisibilityDelta> update(uint64_t observerEntityId) = 0;
};
```

### 这层做什么

- 从 AOI 获取当前可见实体
- 比较上一次可见集
- 产出 enter / leave / stay

### 这层不做什么

- 不拼同步包
- 不发网络消息
- 不决定属性哪些该同步

## 九、ReplicationService

这是这篇文档最关键的对象。

建议职责：

- 消费 `VisibilityDelta`
- 为 enter / leave / stay 生成不同复制任务
- 将任务交给 `AttributeSyncProjector` 和 `Publisher`

建议结构：

```text
ReplicationService
    ├── FullSyncPlanner
    ├── DeltaSyncPlanner
    ├── DespawnPlanner
    ├── ReplicationQueue
    └── ReplicationPublisher
```

## 十、三类复制任务

world 复制链至少应显式区分 3 类任务。

### 1. Enter Replication

含义：

- 某实体首次进入某观察者视野

动作：

- 下发 create / spawn
- 下发初始快照
- 必要时附带外观、基础属性、位置、朝向

这类任务不应走普通 delta sync。

### 2. Delta Replication

含义：

- 实体已经在视野内，只同步变化部分

动作：

- 读取属性变化
- 投影 `AttributeSyncBatch`
- 只发允许面向 AOI 的属性

### 3. Leave Replication

含义：

- 实体离开视野

动作：

- 下发 despawn / remove
- 清理客户端可见状态

## 十一、推荐对象模型

```cpp
enum class ReplicationTaskType : uint8_t {
    Enter,
    Delta,
    Leave,
};

struct ReplicationTask {
    ReplicationTaskType type = ReplicationTaskType::Delta;
    uint64_t observerEntityId = 0;
    uint64_t targetEntityId = 0;
    uint64_t routeVersion = 0;
};
```

如果要更完整，可以继续拆成：

- `EnterReplicationTask`
- `DeltaReplicationTask`
- `LeaveReplicationTask`

## 十二、FullSyncPlanner 的边界

它只负责：

- 把“进入视野”转成客户端初始基线

它应考虑：

- 该实体的可见属性白名单
- 初始位置 / 朝向 / 外观
- 是否需要完整属性快照

它不应负责：

- 周期增量发送
- 持久化
- AOI 计算

## 十三、DeltaSyncPlanner 的边界

它只负责：

- 把已经在视野内实体的属性变化转成增量复制任务

它可依赖：

- `AttributeSyncSchema`
- `VisibilitySet`
- `route snapshot`

它不应直接依赖：

- 数据库存储层
- RPC command

### 它和 `AttributeSyncProjector` 的关系

更合理的关系是：

- `DeltaSyncPlanner` 决定“哪些实体之间要复制”
- `AttributeSyncProjector` 决定“这些变化如何形成 batch”

## 十四、DespawnPlanner 的边界

它只负责：

- 生成客户端移除实体的复制动作

它不应负责：

- 实体真实销毁
- DB 删除
- world authority 迁移

因为：

- “离开视野”
- 不等于
- “实体不存在”

## 十五、ReplicationPublisher 的边界

这一层负责：

- 把复制任务转成可下行的消息
- 通过 Gateway / session route / world route 发出

它本质是：

- world replication delivery

它不负责：

- AOI 计算
- 属性变更收集
- DB 写入

## 十六、和 WorldSession 的关系

`WorldSession` 在这条链路里应该承担：

- observer 的 world 上下文
- 当前 route version
- 当前 session binding

当前 `WorldSession` 已经有：

- `session_id`
- `player_id`
- `world_id`
- `map_instance_id`
- `space_id`
- `route_version`

这些字段正好可以作为复制链的上下文基础。[world_session.cpp](/Users/cui/Workspaces/apollo/modules/game/world/src/world_session.cpp#L5)

也就是说：

- `WorldSession` 负责“复制该发给谁”
- `ReplicationService` 负责“该发什么”

## 十七、和 Gateway 的关系

`ReplicationService` 不应直接持有公网连接。

更合理的链路是：

```text
ReplicationService
  -> RouteSnapshot / SessionBinding
  -> Gateway / ClientRoutePublisher
```

也就是说，复制链不应绕过现有会话边界。

## 十八、和 KBE / Witness 的关系

Apollo 这里可以借鉴 KBE 的思想，但不必照搬它的对象形态。

KBE 的做法是：

- `Witness` 一边维护 view，一边做 enter/leave，一边做属性广播

Apollo 更合理的做法是：

- `AOIService`
- `VisibilityResolver`
- `ReplicationService`
- `AttributeSyncProjector`
- `ReplicationPublisher`

分层协作。

### 这样做的优点

- AOI 算法可独立替换
- 同步策略可独立扩展
- Publisher 可独立适配 Gateway / client protocol
- 不会重新回到重型“万能 Witness”

## 十九、一个完整例子

玩家 A 看见玩家 B。

### 第一步：AOI 变化

`VisibilityResolver` 发现：

```text
VisibilityDelta(
  observer=A,
  entered=[B],
  left=[],
  stayed=[]
)
```

### 第二步：ReplicationService 规划

生成：

```text
ReplicationTask(type=Enter, observer=A, target=B)
```

### 第三步：FullSyncPlanner

生成：

- create entity
- initial snapshot
- position / direction / appearance

### 第四步：Publisher 下发

通过 Gateway 对 A 的客户端发送初始可见实体消息。

之后 B 的 `HP` 从 `100` 变成 `75`：

### 第五步：属性变化

```text
AttributeChangeEvent(B, HP, 100 -> 75)
```

### 第六步：DeltaSyncPlanner

检查：

- A 当前仍能看到 B

于是生成：

```text
ReplicationTask(type=Delta, observer=A, target=B)
```

### 第七步：AttributeSyncProjector

生成：

```text
AttributeSyncBatch(audience=Aoi, entries=[HP=75])
```

### 第八步：Publisher 下发

把 delta 包发给 A 对应客户端。

如果 B 离开 A 的视野：

### 第九步：VisibilityDelta

```text
VisibilityDelta(
  observer=A,
  entered=[],
  left=[B],
  stayed=[]
)
```

### 第十步：DespawnPlanner

生成 remove / despawn 消息，下发给 A。

## 二十、和当前代码的落点建议

Apollo 当前最合理的补法，是先加接口和骨架，不急着一次做完整实现。

建议新增：

- `include/apollo/game/world/visibility_resolver.hpp`
- `include/apollo/game/world/replication_service.hpp`
- `modules/game/world/src/visibility_resolver.cpp`
- `modules/game/world/src/replication_service.cpp`

其中：

- `VisibilityResolver` 先基于现有 `AOIManager::GetVisibleEntities(...)`
- `ReplicationService` 先只做：
  - enter
  - leave
  - delta task 分类

真正的下行消息拼装可以先留给：

- `ReplicationPublisher`

后续再接 Gateway。

## 二十一、最重要的边界规则

Apollo 后续在这块必须守住 4 条硬规则。

### 1. AOI 不打包属性

AOI 只产出 visibility。

### 2. 同步器不自己查 AOI

同步器只处理属性变化和同步规则。

### 3. ReplicationService 不写数据库

复制链只做 world 下行状态传播。

### 4. Gateway 不决定 visibility

Gateway 只负责投递，不负责 world 可见性裁决。

## 二十二、最终总结

Apollo 里 `Audience::Aoi` 要想真正成立，必须补一层中间桥：

- `VisibilityResolver`
- `ReplicationService`

这样整个 world 复制链才会真正闭环：

```text
AOI
  -> visibility delta
  -> replication planning
  -> sync projection
  -> delivery
```

这层一旦立住，Apollo 就能同时保持两件事：

- 不重新退回 KBE 式重型 `Witness`
- 又能让 AOI 真正和属性同步产生稳定连接

而这正是 Apollo 从“有 AOI、有属性同步”走向“有完整复制链”的关键一步。
