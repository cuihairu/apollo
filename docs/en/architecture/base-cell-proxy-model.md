---
title: Base Cell Proxy Object Model
icon: project-diagram
order: 19
category:
  - Architecture
  - Apollo
tag:
  - Apollo
  - BaseApp
  - CellApp
  - Proxy
---

# Base Cell Proxy Object Model

This page explains KBEngine's external object responsibilities, not another Apollo operating mode. Apollo uses gateway, loginServer, baseServer, worldServer, cell, database, and machined. See [player objects](../design/player-object-model.md).

## KBEngine responsibilities

Proxy exposes client-associated capabilities, Base hosts non-spatial entities/long-lived logic, and Cell hosts spatial simulation. Database access is a distinct responsibility. These are genuine third-party names rather than direct Apollo process/class mappings.

Read upstream [proxy.h](https://github.com/kbengine/kbengine/blob/master/kbe/src/server/baseapp/proxy.h), [base.h](https://github.com/kbengine/kbengine/blob/master/kbe/src/server/baseapp/base.h), and [cellapp/entity.h](https://github.com/kbengine/kbengine/blob/master/kbe/src/server/cellapp/entity.h) for their interactions. Exact fields depend on the source version.

## Apollo boundaries

| Boundary | Owner |
|---|---|
| Client connections/delivery | gateway |
| Authentication/directories | loginServer |
| Persistent-player objects/base gameplay | baseServer; reuse PlayerAnchor |
| Space/Cell management | worldServer; CellAppMgr-like management |
| Scene entities | authoritative Cell in cell; reuse Avatar/Entity |
| Profile loading/durable saving | database |
| Local process supervision | machined |

Cross-server play moves simulation ownership while preserving Base/storage affiliation. Instance destruction does not destroy persistent objects. Base replacement separately transfers persistent objects and sessions. Sessions, profiles, and simulation state must not become one complete profile that moves with an Instance.

## Lifecycle and calls

Base loads profiles through database and supplies gameplay snapshots/admission to cell. worldServer manages placement; cell simulates scene entities and submits stable settlement IDs to the owning Base. Base validates and obtains durable confirmation through database. cell does not receive home database credentials. Directories maintain independent Base/simulation associations.

Existing types/routing primitives still need integration. External references do not establish completed migration. See [applications](../apps/index.md), [cross-server scenes](../design/cross-server-scenes.md), and [update boundaries](../design/gateway-topology-survey.md).
