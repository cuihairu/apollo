---
title: Standard MMO 任务清单
icon: square-check
order: 43
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - MMO
  - Standard MMO
  - Checklist
---

# Standard MMO 任务清单

这篇文档把 `Standard MMO` 的实施计划继续下钻成任务清单。

目标主链：

```text
Client -> LoginApp -> GatewayApp -> BaseApp(PlayerAnchor) -> WorldApp
```

## 一、阶段目标

最终要达成的不是“模块名补齐”，而是下面这条链路稳定可用：

1. 玩家登录
2. `LoginApp` 发放短期票据
3. `GatewayApp` 校验票据
4. `BaseApp` 激活 `PlayerAnchor`
5. `WorldApp` 创建 `WorldSession`
6. 玩家进入地图
7. 断线后可重连恢复

## 二、任务清单

### A. 运行时基础

- [ ] 收口应用启动流程，统一 `ApplicationHost / ProcessHost`
- [ ] 收口配置装载与 profile 切换
- [ ] 收口日志初始化与 console 输出
- [ ] 收口 shutdown hook、pidfile、单实例保护

验收点：

- `login-app / gateway-app / base-app / world-app` 都能稳定启动和关闭

### B. 协议与调用边界

- [ ] 固定客户端登录票据协议
- [ ] 固定心跳、重连、踢线消息
- [ ] 固定内部 service envelope
- [ ] 固定 `Login -> Base -> Gateway -> World` 的调用接口

验收点：

- 不同进程不再各自定义重复消息结构

### C. BaseApp 在线主状态

- [ ] `PlayerAnchor` 生命周期稳定
- [ ] `AnchorManager` 支持查找、替换、销毁
- [ ] `SessionLocator` 可通过 `playerId/sessionId` 找回归属
- [ ] `ReconnectPolicy` 支持踢旧接新、断线恢复窗口
- [ ] `WorldAssignment` 支持 `WorldApp / Instance / Zone`

验收点：

- `BaseApp` 成为唯一在线主状态中心

### D. LoginApp 收口

- [ ] `Authenticator` 稳定支持账号密码校验
- [ ] `LoginRiskController` 支持基础限流和失败计数
- [ ] `EntryAllocator` 支持选择 `GatewayApp`
- [ ] `LoginTicketIssuer` 发放短期票据
- [ ] 删除 `LoginApp` 对长期 session 权威的承担

验收点：

- 登录成功后客户端拿到的是短期登录票据，而不是长期 session 权威

### E. GatewayApp 收口

- [ ] `ClientIngressServer` 负责 accept / read / write
- [ ] `ConnectionRegistry` 只管理连接态
- [ ] `SessionAdmission` 必须向 `BaseApp` 校验
- [ ] `PacketDispatcher` 做分类，不做长期权威裁决
- [ ] `RouteExecutor` 只执行短期路由快照
- [ ] `ClientEgress` 收口下行发送

验收点：

- `GatewayApp` 不再持有玩家长期在线主状态

### F. WorldApp 收口

- [ ] `WorldHost` 负责地图宿主
- [ ] `WorldSession` 负责玩家在世界中的运行态
- [ ] `MapInstance` 支持基础地图/副本
- [ ] `AOI` 支持可见集更新
- [ ] `Movement` 支持移动与广播

验收点：

- 玩家进入地图后可看到其他实体并完成基础移动同步

### G. 持久化收口

- [ ] `Repository` 抽象固定
- [ ] `PersistenceService` 独立为支撑层
- [ ] `BaseApp` 只保留持久化协调
- [ ] 玩家快照支持加载、保存、下线刷盘

验收点：

- `BaseApp != DBMgr`

### H. 重连恢复链

- [ ] `GatewayApp` 检测断线后只上报 `BaseApp`
- [ ] `BaseApp` 决定进入 reconnect window 还是下线
- [ ] `WorldApp` 在窗口期保留最小运行态
- [ ] 重连成功后恢复 `GatewayBinding + WorldSession`

验收点：

- 断线后在窗口期内能恢复在线状态

## 三、建议顺序

推荐按下面顺序推进：

1. 运行时基础
2. 协议与调用边界
3. BaseApp 在线主状态
4. LoginApp 收口
5. GatewayApp 收口
6. WorldApp 收口
7. 持久化收口
8. 重连恢复链

## 四、里程碑

### M1

- 可以稳定启动四个核心进程

### M2

- 登录成功后可激活 `PlayerAnchor`

### M3

- 玩家可通过 `GatewayApp` 进入 `WorldApp`

### M4

- 断线重连链路跑通

### M5

- 持久化从 `BaseApp` 语义中心分离

## 五、结论

`Standard MMO` 是 Apollo 的第一落地点。

只要这条链路没有稳定，就不应进入：

- `Proxy`
- `Witness`
- `Ghost`
- `AuthorityTransfer`

## 相关阅读

- [MMO 模块落地清单](./mmo-module-rollout-plan.md)
- [玩家在线主链设计](./player-online-flow.md)
- [LoginApp 收口设计](./login-app-design.md)
- [Gateway 接入 Facade 设计](./gateway-ingress-facade-design.md)
