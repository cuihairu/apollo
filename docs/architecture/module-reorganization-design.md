---
title: 模块目录重组设计
icon: folder-tree
order: 14
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - 模块
  - 目录
  - MonoRepo
---

# 模块目录重组设计

这篇文档解决的是 Apollo 继续往“mono-repo 多模块装配框架”推进时，另一个必须明确的问题：

`当前仓库目录应该怎么重组，才能和新的分层、starter、profile、装配模型对齐。`

这里先明确一点：

- 现在不一定要立刻大规模改目录名

但目录语义必须先校正。

否则后面的模块装配、starter、文档和代码会长期错位。

## 一、先说结论

Apollo 更合理的仓库形态应该是：

```text
repo/
  modules/
    base/
    core/
    transport/
    runtime/
    runtime-ops/
    platform/
    script/
    game-foundation/
    game-session/
    game-world/
    game-battle/
    game-social/
    game-task/
    game-activity/
    distributed-world/
    starter/
  apps/
    gateway-app/
    login-app/
    base-app/
    world-app/
    chat-app/
  tools/
  docs/
```

这不一定要一次性全部重命名到位，但文档和架构语义应该向这个方向收口。

## 二、为什么需要重组

### 1. 当前目录更像“历史原型累积”

Apollo 现在的目录有很多方向是对的，但整体仍然更接近：

- 若干原型模块并列
- 若干 app 直接承载较多结构职责

这和后面要做的：

- 分层清晰
- 模块可装配
- app 只做装配壳

还不完全一致。

### 2. 新分层已经变了

现在 Apollo 已经明确了：

- `Runtime Ops Host`
- `Platform Foundation`
- `Game Foundation`
- `Domain Components`
- `Distributed World Extension`

所以目录也应该逐步往这个语义上靠。

### 3. 不重组目录，starter 很难落地

如果目录层没有清晰模块边界，starter 最终会遇到一个问题：

- 模块到底是什么

所以目录和模块边界至少要基本一致。

## 三、推荐目录分组

建议后续把 `modules/` 下的模块分成 8 组。

### 1. 基础组

建议目录：

- `modules/base`
- `modules/core`

职责：

- 基础设施
- 框架内核

### 2. 传输组

建议目录：

- `modules/transport`

如果短期不重命名，也可以先保留：

- `modules/net`

但语义上建议逐步向：

- transport / messaging / protocol

收口。

### 3. 运行时组

建议目录：

- `modules/runtime`
- `modules/runtime-ops`

当前 `runtime` 可以继续保留，后续逐步把：

- console
- pidfile
- diagnostics
- telemetry

抽到 `runtime-ops`。

### 4. 平台组

建议目录：

- `modules/platform`

负责：

- Redis
- SQL
- repository
- cache
- lock
- leaderboard

当前如果 `modules/data` 已有部分能力，可以采用：

- 保留 `data`
- 再补 `platform`

而不是直接一次性推翻。

### 5. 游戏基础组

建议目录：

- `modules/game-foundation`

短期内如果不重命名，也可以继续以：

- `modules/game/core`
- `modules/script`

这两个模块语义上共同承担。

### 6. 业务域组

建议目录：

- `modules/game-session`
- `modules/game-world`
- `modules/game-battle`
- `modules/game-social`
- `modules/game-task`
- `modules/game-activity`

短期内也可以采用：

- `modules/game/session`
- `modules/game/world`
- `modules/game/battle`

这种更贴近现有仓库的分法。

### 7. 分布式世界组

建议目录：

- `modules/distributed-world`

短期内也可以继续保留：

- `modules/bigworld`

但语义上要明确：

- 它不是纯兼容层
- 最终目标是 distributed world extension

### 8. 装配组

建议目录：

- `modules/starter`

负责：

- starter
- bootstrap
- profile
- module registry

## 四、当前目录的现实调整建议

Apollo 不适合现在立刻做全仓库重命名，因为这样会放大风险。

更合理的是三步走。

### 第一步：先校正语义

也就是先在文档里明确：

- `apps/base-app` 当前实现偏数据服务
- 目标语义应演进为玩家锚点宿主
- `apps/cell-app` 当前更像 world runtime 原型
- `DBMgr / PersistenceService` 应单独理解为持久化执行层

### 第二步：先补缺失模块

优先补：

- `modules/runtime/ops`
- `modules/platform`
- `modules/script`
- `modules/game/session`

### 第三步：再按收益决定是否重命名

例如是否把：

- `modules/net` 改成 `modules/transport`
- `modules/bigworld` 改成 `modules/distributed-world`

可以放到更后面。

## 五、推荐 app 层语义

后续 app 层应该尽量变薄。

### `apps/gateway-app`

语义：

- 接入与会话装配壳

### `apps/login-app`

语义：

- 登录入口装配壳

### `apps/base-app`

语义：

- 玩家锚点宿主装配壳

当前实现还没到位，但方向应该定清楚。

如果未来把偏数据职责独立拆出，更合理的名字应是：

- `db-app`
- `persistence-app`
- `PersistenceService`

### `apps/world-app`

这是语义上最清晰的名字。

短期内当前 `apps/cell-app` 可以继续保留目录名，但文档里按：

- `world-app 原型`

理解更准确。

### `apps/chat-app`

语义：

- 社交通道或聊天装配壳

## 六、推荐 mono-repo 结构草图

```text
apollo/
  modules/
    base/
    core/
    net/
    runtime/
    runtime-ops/
    data/
    platform/
    script/
    game/
      core/
      session/
      world/
      battle/
      social/
      task/
      activity/
    bigworld/
    starter/
  apps/
    gateway-app/
    login-app/
    base-app/
    cell-app/
    chat-app/
  tools/
  docs/
```

### 为什么这版更现实

因为它兼顾了两件事：

- 不推翻当前仓库
- 又给新语义预留了落点

## 七、和 starter 装配的关系

目录重组不是为了“好看”，而是为了让 starter 有稳定模块边界。

例如：

- `starter-mmo` 才能明确依赖哪些模块
- `starter-bigworld` 才能明确是在什么基础上叠加

## 八、结论

Apollo 的目录重组不应该是一次性大手术，而应该是：

- 先校正语义
- 再补关键缺层
- 最后再按收益做目录重命名

最终目标是把 Apollo 收成：

- 一个 mono-repo
- 多个边界清晰的小模块
- 多个薄 app 装配壳
- 一套统一 starter / bootstrap / profile 装配体系

## 相关阅读

- [Apollo 分层设计](./apollo-layering-design.md)
- [Starter 与模块装配设计](./starter-and-module-assembly-design.md)
- [Apollo 渐进式游戏框架理论设计](./apollo-progressive-game-framework.md)
- [进程语义重定义](./process-semantics-redefinition.md)
- [持久化进程与 DBMgr 设计](./persistence-process-and-db-manager-design.md)
