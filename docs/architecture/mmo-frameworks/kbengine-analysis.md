# KBEngine 架构分析

## 1. 定位

KBEngine 是一个典型的 MMO 服务器引擎，强调：

- 分布式进程拆分
- 实体为中心
- Base / Cell 分治
- schema 驱动属性、方法、同步、持久化

它是少数把“大世界实体模型”做成一等公民的开源框架之一。

## 2. 核心架构

官方资料和源码长期呈现出一套稳定分层：

- `loginapp`
- `dbmgr`
- `baseappmgr`
- `baseapp`
- `cellappmgr`
- `cellapp`

职责划分大致是：

- `BaseApp`：账号、在线会话、跨场景逻辑、持久化相关逻辑
- `CellApp`：空间内实体、位置、AOI、战斗、AI、视野相关同步
- `DBMgr`：实体持久化和恢复
- `Mgr` 类进程：调度和分配

这套结构的核心思想是：

- 非空间逻辑和空间逻辑分开
- 空间权威放到 Cell
- 生命周期和持久化更多在 Base

## 3. MMO 核心能力分析

### 3.1 世界与空间

KBEngine 最大的特色是空间模型。世界不是简单按房间切，而是按 `Space` 和 `Cell` 区域切分。

这意味着它天然适合：

- 大地图
- AOI
- 实体迁移
- 区域负载拆分

### 3.2 实体与 Schema

实体定义由：

- `entities.xml`
- `entity_defs/*.def`
- `types.xml`

共同构成。

属性和方法会被加载成运行时描述对象，后续用于：

- 同步
- 持久化
- 默认值
- 类型检查
- 客户端实体定义导出

这是它最大的工程优势。

### 3.3 同步

KBEngine 同步不是“手写协议字段映射”，而是由实体 schema 驱动。

属性上带有 flags，系统自动知道：

- 发给谁
- 何时发
- 是否压缩成 alias
- 如何序列化

### 3.4 持久化

持久化同样依赖 schema。属性是否保存到数据库，不需要在 DB 层重复定义。

### 3.5 脚本扩展

游戏逻辑层通常用 Python。优点是迭代快，缺点是运行时错误和跨层调试复杂度高。

## 4. 特色

- Base / Cell 二元分治非常清晰
- 实体定义统一驱动多条子系统链路
- 适合无缝世界与空间迁移
- 比很多“房间框架”更像完整 MMO 引擎

## 5. 优点

- 世界模型完整，贴近 MMORPG 核心问题
- 属性、方法、同步、持久化自动化程度高
- 进程职责明确，便于扩展到大世界
- 对实体迁移、AOI、Cell 权威等概念支持自然

## 6. 缺点

- runtime 复杂
- Python + C++ + XML 三层调试门槛高
- 错误更多在启动期和运行期暴露
- 团队如果没有 MMO 引擎经验，容易被系统复杂度反噬

## 7. 适用场景

适合：

- 真正的大世界 MMO
- 有 AOI、空间切分、实体迁移需求的项目
- 想做服务端核心引擎的团队

不太适合：

- 小团队快速起盘
- 房间制副本型项目
- 主要需求是社交、匹配、排行榜而非世界引擎

## 8. 对 Apollo 的启发

最值得吸收的不是 XML 形式，而是这几个抽象：

- `EntitySchema`
- `PropertySchema`
- `MethodSchema`
- Base / Cell 职责边界
- schema 驱动同步和持久化

## 9. 参考资料

- 官方主页：https://kbengine.github.io/
- 实体定义文档：https://kbengine.github.io/docs/programming/entitydef.html
- 文档索引：https://kbengine.github.io/cn/docs/
