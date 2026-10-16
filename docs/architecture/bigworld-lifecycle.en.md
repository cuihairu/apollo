---
title: BigWorld Process Architecture and Player Lifecycle
icon: flow
order: 31
category:
  - Architecture
  - BigWorld
tag:
  - BigWorld
  - Process Architecture
  - Player Lifecycle
  - Login Flow
---

[English](bigworld-lifecycle.en.md) | [中文](bigworld-lifecycle.md)

# BigWorld Process Architecture and Player Lifecycle

This is a wrap-up entry page, used to lay out the process responsibilities under BigWorld semantics.

The mainline Apollo recommends today is not "BaseApp as a database server", but rather:

`Client -> LoginApp -> BaseApp(PlayerAnchor + Proxy) -> CellApp`

Only if edge-access governance is to be layered on top is a separate `GatewayApp` introduced.

## Process Responsibilities

- `LoginApp`: authentication, entry assignment, ticket generation
- `BaseApp`: the player's long-term online main state, session ownership, reconnect recovery
- `GatewayApp`: connection management, routing, and edge protection
- `CellApp`: real-time logic inside a space, AOI, combat, cross-boundary migration

## The Lifecycle Mainline

1. Login
2. Activate `PlayerAnchor`
3. Assign the world/space entry
4. Enter `CellApp`
5. Runtime synchronization
6. Disconnect protection and recovery
7. Persistence closure

## Recommended Reading


## Old Long-Form Version

- [BigWorld Process Architecture and Player Lifecycle (old version)](../BigWorld进程架构与玩家生命周期.md)
