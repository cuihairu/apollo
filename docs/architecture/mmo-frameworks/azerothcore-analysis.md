# AzerothCore 架构分析

## 1. 定位

AzerothCore 与 TrinityCore 同源，但它更强调：

- 模块化
- 可维护二开
- 通过 hooks 和 modules 扩展核心

所以它的价值不只在 MMORPG 世界服本身，还在“如何治理一个大型 MMORPG Core 项目”。

## 2. 核心架构

整体运行模型仍是经典世界服路线：

- 认证服务
- 世界服务
- 多数据库

但和 TrinityCore 相比，它把“定制扩展不要直接 patch core”当成非常重要的工程原则。

## 3. MMO 核心能力分析

### 3.1 世界与业务

在世界层面，它仍然属于传统 MMORPG Core：

- 地图
- 生物
- 副本
- 任务
- AI
- 战斗

它不是面向动态空间切片和分布式 Cell 的方案。

### 3.2 模块化

这是它最有特色的地方。

官方资料明确强调：

- 基于模块目录扩展功能
- 自定义特性通过 module 注入
- 保持 core 干净，便于升级同步

它实际上是在回答另一个现实问题：

- 长生命周期 MMORPG 项目如何避免 core 被改烂

### 3.3 Hooks 体系

模块扩展依赖 hooks / script hooks。  
这让它更像：

- 一个可扩展的 MMORPG 产品核心
- 而不是“抽象优先的 MMO 通用引擎”

## 4. 特色

- 模块化治理明显强于同类老牌私服框架
- 很适合长期定制和多人协作维护
- 更强调工程可维护性

## 5. 优点

- 二开体验更好
- 升级主线时冲突更少
- 适合积累模块生态
- 对内容型 MMORPG 项目很友好

## 6. 缺点

- 本质上仍不是分布式 MMO 世界引擎
- 世界模型仍偏传统 worldserver
- 对 Apollo 这种通用框架的直接启发主要在模块治理，而非空间内核

## 7. 适用场景

适合：

- 长期维护的 MMORPG 私服/定制服
- 多人协作的模块化业务服
- 想研究“老 MMORPG Core 如何工程化改良”

## 8. 对 Apollo 的启发

最值得吸收的是：

- 模块边界
- 扩展点设计
- hook 体系
- 保持 core 干净的工程纪律

## 9. 参考资料

- 官方站点：https://www.azerothcore.org/
- 模块化结构：https://www.azerothcore.org/wiki/es/the-modular-structure
- Hook / ScriptAI：https://www.azerothcore.org/wiki/es/hooks-script
