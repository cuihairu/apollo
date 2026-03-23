# Photon 架构分析

## 1. 定位

Photon 不是单一产品，而是一组多人网络产品线。  
对 MMO 架构讨论最 relevant 的有两条线：

- Photon Server 传统服务端模型
- Photon Fusion 现代权威同步模型

因此它更适合被理解成“多人实时网络技术栈”，而不是单一 MMO 引擎。

## 2. 核心架构

### 2.1 Photon Server

Photon Server 官方基础应用文档长期强调：

- `Master Server`
- `Game Server`

这是典型的：

- 接入 / 房间分配
- 房间 / 会话承载

两层模型。

另外官方还保留过 MMO Demo 的 interest management 文档，但官方明确说明该 MMO demo 已停止支持。

### 2.2 Photon Fusion

Fusion 的重点是网络拓扑和状态同步。  
官方文档强调：

- Dedicated Server
- State Authority
- Interest Management

在 Dedicated Server 模式下：

- 服务端拥有完整状态权威
- 客户端发输入或请求
- 服务器决定对象状态

## 3. MMO 核心能力分析

### 3.1 世界模型

Photon 不直接给你完整 MMO 世界模型。  
它擅长的是：

- 会话
- 状态同步
- interest management
- dedicated server topology

要做 MMO，通常需要你自己补：

- 世界分区
- 跨服路由
- AOI 生命周期
- 持久化

### 3.2 Interest Management

Photon 对 MMO 讨论里最值得关注的是 interest management。  
旧版 Photon Server MMO 文档明确强调：

- region-based interest management

Fusion 也继续保留了 interest management 作为高级能力。

### 3.3 部署模型

Fusion Dedicated Server 走的是“每个 session 一个 headless Unity server”思路。  
这对 MMO 副本、战场、局部世界很合适，但对超大持续世界成本较高。

## 4. 特色

- 网络同步和 interest management 做得成熟
- Unity 生态结合好
- Dedicated Server / Host / Shared 多拓扑选择

## 5. 优点

- 实时同步能力强
- 对 Unity 团队接入友好
- 权威服模式清晰
- AOI / interest management 能力成熟

## 6. 缺点

- 不是完整 MMO 世界引擎
- MMO demo 已非官方主推方向
- 持久世界和运营后台能力不是核心优势

## 7. 适用场景

适合：

- Unity 在线游戏
- 房间型多人战斗
- MMO 的局部副本 / 战场 / 战斗服

不太适合：

- 单靠 Photon 直接做完整大世界 MMO 后端

## 8. 对 Apollo 的启发

最值得吸收的是：

- interest management 抽象
- topology 设计
- 将状态权威、对象同步、世界逻辑三者分层

## 9. 参考资料

- Fusion 网络拓扑：https://doc.photonengine.com/fusion/current/manual/network-topologies
- Fusion Interest Management：https://doc.photonengine.com/fusion/current/manual/advanced/interest-management
- Photon Server 基础应用：https://doc.photonengine.com/server/v4/applications/base-applications
- Photon Server MMO Interest Management：https://doc.photonengine.com/server/v4/applications/mmo/mmo-basics
