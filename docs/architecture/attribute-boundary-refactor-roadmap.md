---
title: 属性边界重构路线图
icon: road
order: 27
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - Attribute
  - Refactor
  - Roadmap
  - Boundary
---

# 属性边界重构路线图

这篇文档不是原则稿，而是执行稿。

它解决的问题是：

`如果 Apollo 接受“属性变化事实、同步、持久化、RPC、AOI 复制链必须拆开”的方向，代码应该怎么一步步改，而不是停留在概念上。`

这篇文档承接：

- [属性同步、持久化与 RPC 边界设计](./attribute-sync-persistence-rpc-boundary.md)
- [属性同步、持久化与 RPC 现状映射](./attribute-sync-persistence-rpc-current-mapping.md)
- [属性持久化投影层设计](./attribute-persistence-projection-design.md)
- [AOI 到复制链桥接层设计](./aoi-replication-bridge-design.md)

## 一、先说结论

Apollo 这块重构不应该一次性推倒重来，而应该分 4 个阶段：

```text
Phase 1: 立事实层
Phase 2: 拆持久化投影层
Phase 3: 建 AOI 复制桥
Phase 4: 收敛交付层与旧接口
```

每个阶段都要满足一个原则：

- 先补中层
- 再迁调用方
- 最后收旧接口

不要一上来改所有 app。

## 二、当前代码的基本判断

当前 Apollo 里和这条链最相关的代码大致分成 4 组。

### 1. `modules/game/attributes`

当前已有：

- 值容器
- 变化监听
- 同步规则
- 同步批处理

这是当前最接近目标结构的一组模块。

### 2. `modules/data`

当前已有：

- ORM / datasource / cache / redis

但还没有：

- 属性持久化投影层
- SaveCoordinator 的属性级表达

### 3. `modules/game/world`

当前已有：

- AOI
- scene
- world session

但还没有：

- visibility delta
- replication service
- publisher bridge

### 4. `modules/net`

当前已有：

- transport
- tcp/http/websocket
- rpc 原型

这层边界方向基本对，但和属性同步还没形成明确的 publisher 接口。

## 三、总体改造目标

最终希望收成的代码结构应接近：

```text
modules/game/attributes
    attribute_value.*
    attribute_change.*
    attribute_sync.*
    attribute_persistence.*

modules/game/world
    aoi.*
    visibility_resolver.*
    replication_service.*
    replication_publisher.*

modules/data/core
    persistence_writer.*
    save_coordinator.*
    repository interfaces

modules/net
    sync publisher adapters
    rpc delivery
```

注意这里最关键的是：

- `attribute_sync` 和 `attribute_persistence` 平行
- `world` 不直接做属性打包
- `data` 不直接理解 AOI

## 四、Phase 1：立事实层

目标：

- 把“属性变化”从 listener 通知提升成正式事实对象

### 4.1 要做什么

#### 1. 统一 `AttributeChangeEvent`

建议从当前：

- `objectId`
- `attributeId`
- `oldValue`
- `newValue`
- `fromServer`

扩成：

- `version`
- `changedAtMs`
- `source`

#### 2. 补一个小型 change stream 抽象

建议新增：

```cpp
class IAttributeChangeStream {
public:
    virtual ~IAttributeChangeStream() = default;
    virtual void publish(const AttributeChangeEvent& event) = 0;
};
```

#### 3. 让 `AttributeContainer` 只负责产出变化

不要再让后续模块直接把容器当协调中心用。

### 4.2 建议文件落点

新增：

- `include/apollo/game/attributes/attribute_change.hpp`
- `modules/game/attributes/src/attribute_change.cpp`

现有文件保留：

- `include/apollo/game/attributes/attribute_value.h`
- `modules/game/attributes/src/attribute_value.cpp`

### 4.3 这个阶段先不要动什么

- 不先动 DB 代码
- 不先动 Gateway
- 不先动 AOI

先把“变化事实”立住。

## 五、Phase 2：拆持久化投影层

目标：

- 让同步器不再承担任何持久化语义

### 5.1 要做什么

#### 1. 新增持久化规则

建议对象：

- `AttributePersistenceRule`
- `AttributePersistenceSchema`

#### 2. 新增持久化表达对象

建议对象：

- `PersistenceChangeEntry`
- `PersistenceChangeSet`
- `PersistenceSnapshot`

#### 3. 新增 projector

建议接口：

```cpp
class IAttributePersistenceProjector {
public:
    virtual ~IAttributePersistenceProjector() = default;
    virtual std::optional<PersistenceChangeSet> projectChange(
        const AttributeChangeEvent& event) = 0;
};
```

#### 4. 新增 save coordinator

建议对象：

- `SaveCoordinator`

至少先支持：

- immediate
- debounced
- periodic

### 5.2 建议文件落点

短期更推荐先落在：

- `include/apollo/game/attributes/attribute_persistence.hpp`
- `modules/game/attributes/src/attribute_persistence.cpp`

等语义稳定后再考虑迁到 `modules/data/core`。

新增 data 侧接口：

- `modules/data/core/include/apollo/data/persistence/persistence_writer.hpp`
- `modules/data/core/include/apollo/data/persistence/save_coordinator.hpp`

### 5.3 迁移策略

第一步：

- 先只让新 projector 产出对象，不真正接数据库

第二步：

- 再让 `BaseApp` 的保存请求改为提交 `PersistenceChangeSet`

第三步：

- 最后再把旧的属性级写库路径清掉

### 5.4 这个阶段先不要动什么

- 不先把所有 repository 改掉
- 不先追求真实 SQL 落地

先把表达层做出来。

## 六、Phase 3：建 AOI 复制桥

目标：

- 让 `Audience::Aoi` 从规则标签变成真正复制链

### 6.1 要做什么

#### 1. 新增 `VisibilitySet / VisibilityDelta`

建议文件：

- `include/apollo/game/world/visibility.hpp`

#### 2. 新增 `VisibilityResolver`

基于现有：

- `AOIManager::GetVisibleEntities(...)`

先做最小版本。

#### 3. 新增 `ReplicationService`

最少支持三类任务：

- `Enter`
- `Delta`
- `Leave`

#### 4. 新增 `ReplicationPublisher`

先定义接口，不必一开始就接完整 Gateway：

```cpp
class IReplicationPublisher {
public:
    virtual ~IReplicationPublisher() = default;
    virtual void publishEnter(const ReplicationEnterTask& task) = 0;
    virtual void publishDelta(const AttributeSyncBatch& batch) = 0;
    virtual void publishLeave(const ReplicationLeaveTask& task) = 0;
};
```

### 6.2 建议文件落点

新增：

- `include/apollo/game/world/visibility_resolver.hpp`
- `include/apollo/game/world/replication_service.hpp`
- `modules/game/world/src/visibility_resolver.cpp`
- `modules/game/world/src/replication_service.cpp`

### 6.3 迁移策略

第一步：

- 先只做 visibility delta 计算

第二步：

- 让 world 侧 enter / leave 走统一 replication task

第三步：

- 再把 AOI 相关属性 delta 接到 `AttributeSyncProjector`

### 6.4 这个阶段先不要动什么

- 不要让 AOI 自己拼客户端协议
- 不要让 SyncProjector 反向依赖 AOIManager

## 七、Phase 4：收敛交付层与旧接口

目标：

- 让 sync delivery / persistence delivery / rpc delivery 三条交付层最终明确分流

### 7.1 要做什么

#### 1. 定义统一 publisher / writer 接口

建议接口：

- `IAttributeSyncPublisher`
- `IPersistenceWriter`
- `IRpcBus`

#### 2. Gateway 只接 sync publisher

不要让 Gateway 理解：

- persistence rule
- attribute dirty container

#### 3. DBMgr / PersistenceService 只接 writer

不要让 DBMgr 直接监听：

- `AttributeContainer`
- `AttributeSyncBatch`

#### 4. 清理旧路径

逐步去掉：

- 容器直接被多方监听的隐式路径
- app 内手写属性打包路径
- 同步与保存揉在一起的调用链

## 八、建议的模块归位

可以按下面这张表理解。

### 保留原位

- `modules/game/attributes/src/attribute_sync.cpp`
  - 但语义改为 projector / batcher
- `modules/game/world/src/aoi.cpp`
  - 保持 AOI 纯度
- `modules/net/*`
  - 继续作为 delivery 层

### 新增中层

- `attribute_change.*`
- `attribute_persistence.*`
- `visibility_resolver.*`
- `replication_service.*`
- `save_coordinator.*`

### 后续再考虑迁移

- 属性持久化 projector
  - 短期可放 `game/attributes`
  - 长期再迁 `data/core`

## 九、建议的最小实施顺序

如果只按最小闭环来做，最推荐的顺序是：

### Step 1

补：

- `attribute_change.hpp`

让 `AttributeChangeEvent` 成型。

### Step 2

补：

- `attribute_persistence.hpp`

让 `PersistenceChangeSet` 成型。

### Step 3

补：

- `save_coordinator.hpp`

先只做内存版合并调度。

### Step 4

补：

- `visibility_resolver.hpp`

让 AOI 产出 delta。

### Step 5

补：

- `replication_service.hpp`

让 AOI enter/leave/delta 形成复制任务。

### Step 6

最后才去接：

- Gateway 下行
- DBMgr writer

## 十、哪些地方现在不要碰

为了避免重构发散，下面这些地方短期不建议先动：

### 1. 不先全面重写 `apps/*`

先在模块层把中层补出来，再迁 app。

### 2. 不先改完所有 `modules/data/*`

当前 `data` 还偏基础设施层，先补接口和表达对象更划算。

### 3. 不先做完整 KBE 式 detailLevel / volatile 系统

Apollo 目前最缺的是边界，不是先补最复杂的复制优化。

### 4. 不先统一全部旧属性 API

仓库里现在同时存在：

- 旧版 `attribute.hpp`
- 新版 `attribute_value.h`

先别同时重写两套，把新链路立起来之后再清理旧口子。

## 十一、一个建议的里程碑切法

### Milestone A：事实层完成

完成标志：

- `AttributeChangeEvent` 稳定
- `AttributeContainer` 统一发事实事件

### Milestone B：持久化投影完成

完成标志：

- `PersistenceChangeSet` 成型
- `SaveCoordinator` 可收 patch

### Milestone C：AOI 复制桥完成

完成标志：

- 有 `VisibilityDelta`
- 有 `ReplicationService`
- `Audience::Aoi` 不再只是标签

### Milestone D：交付层收敛完成

完成标志：

- sync publisher / persistence writer / rpc bus 分流
- app 层不再混写三种语义

## 十二、最终判断

Apollo 这块改造最容易失败的地方，不是技术实现，而是顺序错误。

最常见的失败方式是：

- 还没立事实层就开始改 DB
- 还没补 persistence projector 就让同步器继续扩职责
- 还没做 visibility delta 就让 AOI 直接拼客户端包

正确顺序应当一直保持：

- 先定义事实
- 再定义投影
- 再定义桥接
- 最后收交付层

只要按这个顺序推进，Apollo 就能把这块长期收成真正稳定的边界：

- 属性变化事实层
- 同步投影层
- 持久化投影层
- AOI 复制桥接层
- 独立交付层
