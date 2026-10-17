[English](gateway-topology-survey.en.md) | [中文](gateway-topology-survey.md)

# Gateway Responsibilities and Update Boundaries

This document defines Gateway in Apollo's room-based architecture. See the [Architecture Overview](../guide/architecture.en.md) for the target topology and [Common Names in Multiplayer Game Servers](../36-MMO_Frameworks_Comparative_Analysis.en.md) for name comparisons.

## Gateway responsibilities

1. Accept client game connections and manage their lifecycle.
2. Validate login credentials, associate login sessions, and apply connection-level rate limits.
3. Route client messages using online routing and return RoomServer responses to the correct client.
4. Report session state on disconnect and support session recovery over a new client connection.

Gateway does not own player match objects, simulate rooms, or replace account authentication or room admission. One RoomServer may host multiple rooms; Gateway forwards messages according to each player's current route.

## Multiple Gateways

Multiple Gateways can share new connections and network load. Each existing TCP connection remains owned by the Gateway process that accepted it; changing a RoomServer route changes the destination of later messages but does not move the socket. Each Gateway instance needs a unique identity so routing and inter-service links do not mistake multiple replicas for the same endpoint.

## Update behavior and guarantees

| Goal | Required mechanism | Conclusion |
|---|---|---|
| Continue accepting new connections | Multiple Gateway instances; old instance stops accepting new connections | Server ingress can roll. |
| Keep existing connections alive | Old Gateway retains connections while draining, or implement cross-process connection handoff | Replicas and a load balancer alone are insufficient. |
| Avoid a new login | Client reconnect plus a verifiable session-recovery flow | A connection may be rebuilt; this is not seamless TCP migration. |
| Keep active rooms running | Room-state transfer, target-version compatibility, authority handoff, route switch, and rollback | Gateway does not provide this; RoomServer needs its own migration design. |

A drain rollout generally works as follows: the new Gateway starts accepting connections; the old Gateway stops accepting new connections but continues serving existing ones; after connections leave naturally or a drain deadline expires, the old process exits. Clients still connected at the deadline need a reconnect or connection-migration strategy.

## Current implementation status

The repository contains a `gateway-app` prototype, but it does not yet provide a validated multi-instance production topology or cross-process TCP connection handoff. Route switching and RoomServer authority transfer also lack end-to-end acceptance. Replica count, successful process startup, and reliable transport alone do not prove client-transparent hot updates.
