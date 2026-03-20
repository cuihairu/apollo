# Q1: 请画出你熟悉的 MMORPG 服务器架构图，说明各组件的职责和通信方式

## 问题分析

本题考察对 MMORPG 服务器整体架构的理解，需要：
1. 画出完整的服务器架构图
2. 说明各组件的职责
3. 说明组件间的通信方式

## 参考架构：BigWorld/KBEngine 风格

> 说明：这里采用 BigWorld/KBEngine 的典型进程模型来回答，不混入通用 `Gateway` 前置网关架构。

### 整体架构图

```mermaid
flowchart TB
    subgraph Clients["客户端层"]
        U["Unity客户端"]
        W["Web客户端"]
        M["移动客户端"]
    end

    subgraph Login["登录服务"]
        LA["LoginApp<br/>登录服务器"]
    end

    subgraph Mgr["管理服务层"]
        BAM["BaseAppMgr<br/>基础应用管理器"]
        CAM["CellAppMgr<br/>空间应用管理器"]
    end

    subgraph Base["基础服务层"]
        BA1["BaseApp1<br/>Proxy/Base Entity"]
        BA2["BaseApp2<br/>Proxy/Base Entity"]
        BA3["BaseApp3<br/>Proxy/Base Entity"]
    end

    subgraph Cell["空间服务层"]
        CA1["CellApp1"]
        CA2["CellApp2"]
        CA3["CellApp3"]
        CA4["CellApp4"]
    end

    subgraph Data["数据服务层"]
        DBM["DBMgr<br/>数据库管理器"]
        RD["Redis<br/>缓存集群"]
        DB[("MySQL<br/>数据库")]
    end

    subgraph Other["其他服务"]
        CHAT["ChatApp<br/>可选聊天服务"]
        MATCH["MatchApp<br/>可选匹配服务"]
    end

    %% 连接关系
    U & W & M --> LA
    U & W & M -.登录后重连.-> BA1
    U & W & M -.登录后重连.-> BA2
    U & W & M -.登录后重连.-> BA3

    LA --> BAM
    LA --> DBM

    BAM <--> BA1
    BAM <--> BA2
    BAM <--> BA3

    BA1 --> CA1
    BA1 --> CA2
    BA2 --> CA2
    BA2 --> CA3
    BA3 --> CA3
    BA3 --> CA4

    CAM <--> CA1
    CAM <--> CA2
    CAM <--> CA3
    CAM <--> CA4

    BA1 & BA2 & BA3 --> DBM

    BA1 & BA2 & BA3 --> RD

    DBM --> DB

    BA1 & BA2 & BA3 --> CHAT
    BA1 & BA2 & BA3 --> MATCH

    style LA fill:#e1f5ff
    style DBM fill:#fff4e1
    style BAM fill:#f0e1ff
    style CAM fill:#f0e1ff
```

### 登录流程图

```mermaid
sequenceDiagram
    participant C as 客户端
    participant LA as LoginApp
    participant DBM as DBMgr
    participant BAM as BaseAppMgr
    participant BA as BaseApp
    participant CA as CellApp

    C->>LA: 登录请求(账号/密码)
    LA->>DBM: 请求账号验证
    DBM-->>LA: 返回验证结果

    alt 登录失败
        LA-->>C: 登录失败
    else 登录成功
        LA->>BAM: 请求分配BaseApp
        BAM-->>LA: 返回BaseApp地址
        LA->>BA: 创建Base/Proxy
        BA->>DBM: 加载玩家数据
        DBM-->>BA: 返回玩家数据
        BA->>CA: 创建Cell Entity
        LA-->>C: 返回BaseApp地址/令牌
        C->>BA: 断开Login后重连BaseApp
    end
```

---

## 各组件职责详解

### 1. 客户端层

| 组件 | 说明 |
|------|------|
| Unity客户端 | PC端主流游戏引擎 |
| Web客户端 | H5小游戏、网页游戏 |
| 移动客户端 | iOS/Android原生或Unity导出 |

### 2. 接入层

#### LoginApp + BaseApp/Proxy（接入主链路）
**职责**：
- `LoginApp` 负责首次接入、账号验证、分配目标 `BaseApp`
- `BaseApp` 中的 `Proxy` 负责玩家的长期会话锚点
- 处理客户端消息编解码、会话管理、断线重连
- 将需要空间计算的消息转发到 `CellApp`

**通信方式**：
- 与客户端：通常是 TCP 长连接 + 自定义二进制协议/Protobuf
- 与内部服务：进程间 TCP + 内部消息/RPC

### 3. 登录服务

#### LoginApp（登录服务器）
**职责**：
- 账号密码验证
- 与 `BaseAppMgr` 协调分配目标 `BaseApp`
- 创建登录票据或连接信息
- 返回后续接入地址

**通信方式**：
- 与DBMgr：RPC
- 与BaseAppMgr：RPC

### 4. 管理服务层

#### BaseAppMgr（基础应用管理器）
**职责**：
- 管理所有BaseApp实例
- 负责新玩家连接分配
- BaseApp负载均衡
- 跟踪BaseApp状态

#### CellAppMgr（空间应用管理器）
**职责**：
- 管理所有CellApp实例
- 空间分区负载均衡
- 动态调整Cell边界
- Entity跨CellApp迁移调度

### 5. 基础服务层

#### BaseApp（基础应用）
**职责**：
- 管理玩家的 `Base` / `Proxy`
- 处理非空间相关逻辑：
  - 背包系统
  - 好友系统
  - 邮件系统
  - 任务系统
  - 商城交易
- 作为客户端的通信锚点
- 将空间相关请求转发给 `CellApp`
- 负责玩家数据加载、保存和生命周期管理

**通信方式**：
- 与客户端：长连接
- 与CellApp：长连接RPC
- 与DBMgr：异步RPC

### 6. 空间服务层

#### CellApp（空间应用）
**职责**：
- 管理空间区域（Cell）
- 处理空间相关逻辑：
  - 玩家移动
  - AOI系统
  - 战斗系统
  - NPC AI
  - 技能释放
  - 场景对象管理
- Entity的位置管理

**通信方式**：
- 与BaseApp：长连接RPC
- 与相邻CellApp：Entity同步

### 7. 数据服务层

#### DBMgr（数据库管理器）
**职责**：
- 数据库连接池管理
- 异步数据存取
- 玩家数据持久化
- 账号数据管理

**数据库划分**：
- **AccountDB**：账号信息、登录记录
- **CharacterDB**：角色数据、属性、背包
- **WorldDB**：世界数据、公会、排行

#### Redis（缓存集群）
**职责**：
- 热点数据缓存（实现上通常由 `BaseApp` 等服务访问）
- 排行榜实时计算
- 会话/状态辅助存储
- 分布式协调辅助能力

### 8. 其他服务

#### ChatApp（聊天服务）
**职责**：
- 频道聊天（世界、公会、队伍）
- 私聊
- 系统广播
- 敏感词过滤

#### MatchApp（匹配服务）
**职责**：
- 玩家匹配（PVP、副本）
- 队伍组建
- 匹配算法（ELO、段位）

---

## 通信方式总结

### 通信协议栈

```
┌─────────────────────────────────────────────────────────────┐
│                      业务协议层                              │
│   Protobuf消息定义 + RPC接口定义                            │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│                      传输协议层                              │
│   客户端↔Login/Base: TCP长连接                               │
│   内部服务: TCP长连接                                        │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│                      网络层                                  │
│   IP + 路由                                                 │
└─────────────────────────────────────────────────────────────┘
```

### 消息格式

```
┌──────────────┬──────────────┬──────────────┬──────────────┐
│   PacketLen  │   MsgType    │   MsgID      │   Body       │
│   (4 bytes)  │   (2 bytes)  │   (4 bytes)  │   (Protobuf) │
└──────────────┴──────────────┴──────────────┴──────────────┘
```

### RPC调用流程

```mermaid
sequenceDiagram
    participant A as 服务A
    participant B as 服务B

    A->>B: RPC请求(RequestID, Method, Params)
    B-->>A: RPC响应(RequestID, Result/Error)

    Note over A,B: 异步处理，支持并发请求
```

---

## 架构特点总结

| 特性 | 说明 |
|------|------|
| **分层设计** | 客户端→LoginApp/BaseApp→CellApp/数据服务，职责清晰 |
| **空间分离** | BaseApp处理非空间逻辑，CellApp处理空间逻辑 |
| **水平扩展** | BaseApp/CellApp可动态增减 |
| **高可用潜力** | Cell/Base可横向扩展，但 LoginApp、Mgr、DBMgr 仍需额外HA方案 |
| **异步通信** | 内部采用异步RPC，提高吞吐量 |

---

## 参考资料

- [KBEngine 技术概览 - 软件组件](https://imgamer.gitbooks.io/kbengine-overview/content/content/5_2DesignIntroduction.html)
- [BigWorld Cell 架构设计介绍](https://blog.csdn.net/monokai/article/details/151142091)
