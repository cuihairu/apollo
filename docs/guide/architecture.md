---
title: 架构总览
icon: layers
order: 7
prev: /guide/configuration
---

# 架构总览

> 目标形态全景（按 `docs/todo.md` 批次落地）。本页三张图为 README 迁入的架构图；进程编队（八件）、权威分解与 Zone/无缝辨析的现状设计见[架构审计报告](/analysis/architecture-review) §31，Compact 单进程形态见 [Compact GameServer 设计](/30-Compact_GameServer_Design)。

## 系统架构概览（目标形态）

```mermaid
graph TB
    subgraph "客户端层"
        C1[Unity客户端]
        C2[Web客户端]
        C3[移动客户端]
    end

    subgraph "接入层"
        LB[负载均衡器<br/>Nginx/HAProxy]
        GW[网关服务器<br/>Gateway]
    end

    subgraph "服务层"
        LS[登录服务器<br/>Login Server]
        GS1[游戏服务器1<br/>Game Server]
        GS2[游戏服务器2<br/>Game Server]
    end

    subgraph "数据层"
        subgraph "MySQL集群"
            DB1[(主数据库)]
            DB2[(从数据库1)]
            DB3[(从数据库2)]
        end

        subgraph "Redis集群"
            R1[(Redis缓存)]
            R2[(Redis缓存)]
            R3[(Redis缓存)]
        end

        subgraph "消息队列"
            MQ[Kafka<br/>可观测管道·规划]
        end
    end

    subgraph "监控层"
        MON[监控系统<br/>Prometheus·规划]
        LOG[日志链路<br/>LogAgent→Kafka→ClickHouse·规划]
    end

    C1 --> LB
    C2 --> LB
    C3 --> LB
    LB --> GW
    GW --> LS
    GW --> GS1
    GW --> GS2

    LS --> DB1
    GS1 --> DB1
    GS2 --> DB1
    GS1 --> R1
    GS2 --> R2

    GS1 --> MQ
    GS2 --> MQ

    DB1 -.-> DB2
    DB1 -.-> DB3

    LS --> LOG
    GS1 --> LOG
    GS2 --> LOG

    LS --> MON
    GW --> MON
    GS1 --> MON
    GS2 --> MON
```

> 2026-10-01 迁入注记：原 README 图服务层含「聊天服务器 / 匹配服务器」两节点——对 `docs/todo.md` 与 `docs/design/*` 全量检索零命中（**不在册**，BW 拓扑时代遗留，同 C-93 chat-app 死路一源），随迁删除；Redis 第三实例与聊天的连线一并清理。聊天/匹配若未来立项，按 gap 登记流程补图。

## 核心模块架构

```mermaid
graph LR
    subgraph "网络层"
        T1[传输层<br/>TCP/WebSocket/KCP]
        P1[协议层<br/>手写编码→目标 protobuf+zstd]
        R1[路由层<br/>Message Router]
    end

    subgraph "业务服务（modules/ 模块原语）"
        A1[玩家管理]
        S1[场景管理]
        B1[战斗系统]
        I1[物品系统]
        G1[公会系统]
    end

    subgraph "基础服务（modules/ 模块原语）"
        LOG[日志]
        CONF[配置]
        CACHE[缓存]
        DB[数据库]
    end

    subgraph "应用入口（apps/ main.cpp）"
        ASM[ApplicationContextBuilder<br/>类型键 bean 图·拓扑序构造注入]
        HOST[ApplicationHost<br/>IHostedService start/stop/tick]
    end

    T1 --> P1
    P1 --> R1
    R1 --> A1
    R1 --> S1
    R1 --> B1
    R1 --> I1
    R1 --> G1

    A1 --> LOG
    S1 --> CONF
    B1 --> CACHE
    I1 --> DB
    G1 --> LOG

    ASM -.构造注入.-> A1
    ASM -.构造注入.-> S1
    HOST ==start/stop/tick==> A1
    HOST ==start/stop/tick==> S1
```

> 模块间依赖为**编译期构造注入**（实线 = 直接依赖，无运行时容器中介）；装配与生命周期托管只存在于应用入口 `apps/`（`ApplicationContextBuilder` 拓扑序建图、`ApplicationHost` 帧驱动托管），模块内部互不感知容器。

## 分布式部署架构（目标形态）

```mermaid
graph TB
    subgraph "区域1 - 华东"
        subgraph "接入区"
            ELB1[负载均衡]
            GW1_1[网关1]
            GW1_2[网关2]
        end

        subgraph "游戏区"
            LS1[登录服务器]
            GS1_1[游戏服1]
            GS1_2[游戏服2]
            GS1_3[游戏服3]
        end

        subgraph "数据区"
            M1[(MySQL主)]
            S1[(MySQL从)]
            R1_1[(Redis集群)]
        end
    end

    subgraph "区域2 - 华北"
        subgraph "接入区"
            ELB2[负载均衡]
            GW2_1[网关1]
            GW2_2[网关2]
        end

        subgraph "游戏区"
            LS2[登录服务器]
            GS2_1[游戏服1]
            GS2_2[游戏服2]
            GS2_3[游戏服3]
        end

        subgraph "数据区"
            M2[(MySQL主)]
            S2[(MySQL从)]
            R2_1[(Redis集群)]
        end
    end

    subgraph "中心服务"
        CC[控制中心]
        MON[监控中心]
        LOG[日志中心]
        MQ[消息中心]
    end

    ELB1 --> GW1_1
    ELB1 --> GW1_2
    ELB2 --> GW2_1
    ELB2 --> GW2_2

    GW1_1 --> LS1
    GW1_1 --> GS1_1
    GW1_1 --> GS1_2
    GW1_1 --> GS1_3

    GW2_1 --> LS2
    GW2_1 --> GS2_1
    GW2_1 --> GS2_2
    GW2_1 --> GS2_3

    LS1 --> M1
    GS1_1 --> R1_1
    GS1_2 --> R1_1
    GS1_3 --> R1_1

    LS2 --> M2
    GS2_1 --> R2_1
    GS2_2 --> R2_1
    GS2_3 --> R2_1

    M1 -.-> S1
    M2 -.-> S2

    M1 <==> M2
    R1_1 <==> R2_1

    LS1 --> MQ
    LS2 --> MQ
    GS1_1 --> MQ
    GS2_1 --> MQ
```

> 同上注记：原图游戏区含「聊天服务器」节点（区域1 CS1 / 区域2 CS2）与对应连线，不在册随迁删除。
