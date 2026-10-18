---
title: Witness and Ghost Design
icon: eye
order: 16
category:
  - Architecture
  - Apollo
tag:
  - Apollo
  - Witness
  - Ghost
  - Replication
---

# Witness and Ghost Design

Apply BigWorld terms to the [Apollo mapping](../design/bigworld-apollo-mapping.md). Current local AOI is not a complete Witness implementation, and Ghost replication is absent.

## Meaning and current correspondence

Witness concerns an observer's visible set, update context, and progress. SceneAoi, ViewerState, and attribute synchronization cover part of these duties. Ghost is a nonauthoritative entity projection in another Cell for neighboring queries and visibility. It must not independently apply damage, grant rewards, execute player input, or save profiles.

BigWorld [Witness](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/cellapp/witness.hpp) and [Entity](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/cellapp/entity.hpp) establish the terminology. This adds neither a duplicate profile object nor another business process.

## Default path

One Cell per Space executes local AOI in cell; worldServer manages placement. Explicit cross-server admission/handoff does not require Ghost. Persistent objects remain in Base and use database.

A transfer snapshot prepares an inactive destination; it is not a Ghost. Targets cannot simulate before commit and sources are removed afterward. Two snapshots do not permit two authorities.

## Seamless-map reservation

Future multiple Cells in one Space require neighboring projections, versions/invalidation, merged and deduplicated visibility, continuous observation, and Real authority handoff. Neither topology changes nor projection updates grant Ghost write authority. Reserve these boundaries now; defer splitting and replication algorithms.

Complete Instance continuity still needs shared gameplay state handoff; Ghost is not settlement or recovery evidence. Track work in root todo.md.
