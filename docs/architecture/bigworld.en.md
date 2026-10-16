---
title: BigWorld Architecture Deep Dive
icon: server
order: 30
category:
  - Architecture
  - BigWorld
tag:
  - BigWorld
  - Compatibility Layer
  - BaseApp
  - CellApp
---

[English](bigworld.en.md) | [中文](bigworld.md)

# BigWorld Architecture Deep Dive

This is a wrap-up entry page.

BigWorld in Apollo is an optional distributed-world enhancement layer, not the default starting form. The current mainline is still `Standard MMO` and `Compact GameServer`.

## Semantic Boundaries

- `BaseApp` is closer to `PlayerAnchor + Proxy Host`
- `CellApp` is closer to a spatial real-time authority node
- `LoginApp` handles authentication and entry assignment
- `GatewayApp` is only enabled in Apollo's edge-access assembly

## Compatibility Layer

Apollo already has a BigWorld compatibility layer implementation, see:

- [BigWorld Compatibility Layer](../33-BigWorld_Compatibility.md)

## Recommended Reading

- [BigWorld Process Architecture and Player Lifecycle](./bigworld-lifecycle.en.md)
- [Witness and Ghost Design](./witness-ghost-design.en.md)

## Old Long-Form Version

- [BigWorld Architecture Deep Dive (old version)](../BigWorld架构深度解析.md)
