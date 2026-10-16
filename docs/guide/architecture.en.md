---
title: Architecture Overview
icon: layers
order: 7
prev: /guide/configuration
---

[English](architecture.en.md) | [中文](architecture.md)

# Architecture Overview

> Target-form panorama (landing per `docs/todo.md` batches). The three diagrams on this page are the architecture figures migrated from the README; for the current-state design of the process fleet (eight pieces), authority decomposition, and the baseapp/seamless analysis see the [Architecture Review Report](/analysis/architecture-review) §31, and for the Compact single-process form see [Compact GameServer Design](/30-Compact_GameServer_Design).

## System architecture overview (target form)

```mermaid
graph TB
    subgraph "Client layer"
        C1[Unity client]
        C2[Web client]
        C3[Mobile client]
    end

    subgraph "Access layer"
        LB[Load balancer<br/>Nginx/HAProxy]
        GW[Gateway server<br/>Gateway]
    end

    subgraph "Service layer"
        LS[Login server<br/>Login Server]
        GS1[Game server 1<br/>Game Server]
        GS2[Game server 2<br/>Game Server]
    end

    subgraph "Data layer"
        subgraph "MySQL cluster"
            DB1[(Primary database)]
            DB2[(Replica 1)]
            DB3[(Replica 2)]
        end

        subgraph "Redis cluster"
            R1[(Redis cache)]
            R2[(Redis cache)]
            R3[(Redis cache)]
        end

        subgraph "Message queue"
            MQ[Kafka<br/>observability pipeline · planned]
        end
    end

    subgraph "Monitoring layer"
        MON[Monitoring system<br/>Prometheus · planned]
        LOG[Log pipeline<br/>LogAgent→Kafka→ClickHouse · planned]
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

> 2026-10-01 migration note: the original README diagram's service layer contained two nodes, "chat server / matchmaking server" — a full search of `docs/todo.md` and `docs/design/*` found zero hits (**not registered**; a leftover from the BW topology era, the same dead end as C-93 chat-app), removed with the migration; the third Redis instance's chat connections were cleaned up as well. If chat/matchmaking is ever chartered, add the diagram back through the gap-registration process.

## Core module architecture

```mermaid
graph LR
    subgraph "Network layer"
        T1[Transport<br/>TCP/WebSocket/KCP]
        P1[Protocol<br/>hand-written codec → target protobuf+zstd]
        R1[Routing<br/>Message Router]
    end

    subgraph "Game services (modules/ primitives)"
        A1[Player management]
        S1[Scene management]
        B1[Combat system]
        I1[Item system]
        G1[Guild system]
    end

    subgraph "Base services (modules/ primitives)"
        LOG[Logging]
        CONF[Config]
        CACHE[Cache]
        DB[Database]
    end

    subgraph "App entry (apps/ main.cpp)"
        ASM[ApplicationContextBuilder<br/>type-keyed bean graph · topologically ordered constructor injection]
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

    ASM -.constructor injection.-> A1
    ASM -.constructor injection.-> S1
    HOST ==start/stop/tick==> A1
    HOST ==start/stop/tick==> S1
```

> Inter-module dependencies are **compile-time constructor injection** (solid line = direct dependency, no runtime container intermediary); assembly and lifecycle hosting exist only at the application entry `apps/` (`ApplicationContextBuilder` builds the graph in topological order, `ApplicationHost` hosts the frame driving); modules are unaware of the container internally.

## Distributed deployment architecture (target form)

```mermaid
graph TB
    subgraph "Region 1 - East China"
        subgraph "Access zone"
            ELB1[Load balancer]
            GW1_1[Gateway 1]
            GW1_2[Gateway 2]
        end

        subgraph "Game zone"
            LS1[Login server]
            GS1_1[Game server 1]
            GS1_2[Game server 2]
            GS1_3[Game server 3]
        end

        subgraph "Data zone"
            M1[(MySQL primary)]
            S1[(MySQL replica)]
            R1_1[(Redis cluster)]
        end
    end

    subgraph "Region 2 - North China"
        subgraph "Access zone"
            ELB2[Load balancer]
            GW2_1[Gateway 1]
            GW2_2[Gateway 2]
        end

        subgraph "Game zone"
            LS2[Login server]
            GS2_1[Game server 1]
            GS2_2[Game server 2]
            GS2_3[Game server 3]
        end

        subgraph "Data zone"
            M2[(MySQL primary)]
            S2[(MySQL replica)]
            R2_1[(Redis cluster)]
        end
    end

    subgraph "Central services"
        CC[Control center]
        MON[Monitoring center]
        LOG[Log center]
        MQ[Message center]
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

> Same note as above: the original diagram's game zone contained "chat server" nodes (region 1 CS1 / region 2 CS2) with their connections — not registered, removed with the migration.
