# Hathora 架构分析

## 1. 定位

Hathora 更像：

- 低延迟 multiplayer hosting 平台
- 专注实时会话和 dedicated server 承载

它不是 MMO 世界引擎，也不是完整游戏后端平台。

## 2. 核心架构

Hathora 的公开定位长期围绕：

- multiplayer infrastructure
- low-latency session hosting
- server orchestration

从类别上看，它更接近：

- 轻量云托管游戏服平台

## 3. MMO 核心能力分析

### 3.1 世界模型

Hathora 不提供世界模型。

### 3.2 同步

同步由你的游戏服务器决定。  
Hathora 更关心：

- 进程放哪跑
- 玩家如何接入低延迟会话

### 3.3 运维能力

它的价值主要在：

- 承载
- 分区部署
- 房间/实例型实时会话 hosting

## 4. 特色

- 强调低延迟实时会话托管
- 平台较轻
- 更接近“多人游戏 hosting 层”

## 5. 优点

- 适合实时游戏快速托管
- 比自建基础设施更轻量

## 6. 缺点

- 不是 MMO core
- 不是完整后端平台
- 持久化、社交、世界层都要外补

## 7. 适用场景

适合：

- 实时房间服
- 副本服
- 想快速上线 dedicated server 的团队

## 8. 对 Apollo 的启发

最值得吸收的是：

- hosting 层和逻辑层分离
- 低延迟区域部署优先的思路

## 9. 参考资料

- 官网：https://hathora.dev/
- 文档入口：https://hathora.dev/docs
