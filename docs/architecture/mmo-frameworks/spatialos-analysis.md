# SpatialOS 架构分析

## 1. 定位

SpatialOS 在大型多人世界讨论里曾非常有代表性。  
但到当前时间点，它更适合被当作：

- 大规模分布式世界的历史样板
- “server meshing / distributed simulation” 路线的参考

而不是主流新项目的稳妥落地选择。

## 2. 核心架构

SpatialOS 的核心思想长期是：

- 将大型世界拆成分布式 simulation
- 不同 worker 承担不同区域或功能权威
- 用平台层管理状态路由和负载分配

这条路线解决的不是普通房间服问题，而是：

- 超大世界
- 多 worker 协同
- distributed authority

## 3. MMO 核心能力分析

### 3.1 世界模型

SpatialOS 的价值主要在世界层。  
它关注的是：

- 分布式世界
- worker 之间的权威切换或协同
- 大规模 simulation

这比普通房间框架更接近 MMO 理想图景。

### 3.2 同步与权威

它的关键不是单个对象怎么同步，而是：

- 整个世界如何由多个服务共同承载

这也是它和 Photon / Colyseus / Mirror 的根本差别。

### 3.3 现实问题

这类路线的代价也非常高：

- 成本
- 架构复杂度
- 调试复杂度
- 平台依赖

从今天回看，SpatialOS 更像提醒我们：

- 分布式世界很诱人
- 但落地成本极高

## 4. 特色

- 分布式模拟是第一卖点
- 不是简单 room-based multiplayer
- 很适合研究 server meshing 思维

## 5. 优点

- 世界层想象空间大
- 对超大世界问题给出过很有代表性的方案

## 6. 缺点

- 工程和商业成本高
- 平台历史演进复杂
- 今天作为新项目直接采用的确定性不高

## 7. 适用场景

更适合：

- 做架构参考
- 研究 server meshing
- 吸收大世界分布式 simulation 的思路

不太适合：

- 作为大多数团队当前的默认首选底座

## 8. 对 Apollo 的启发

最值得吸收的是：

- 世界和权威不一定只能由单个进程承载
- 分布式世界要把“权威边界”和“状态路由”设计在最前面

## 9. 参考资料

- 历史文档入口：https://docs.spatial-os.org/
- 当前 Spatial Creator Toolkit 多人概览：https://toolkit.spatial.io/docs/multiplayer/overview
