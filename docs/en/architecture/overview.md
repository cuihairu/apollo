
# Architecture Overview

Apollo targets lightweight room-based multiplayer games. Its target runtime path is `Client → Gateway → RoomServer → Room`: Gateway maintains client connections and routes messages; a RoomServer hosts one or more rooms; a Room is one match and its temporary state. Login authenticates accounts and issues login credentials; persistence stores data that spans matches.

Each player has one authoritative runtime object in a match. Cross-server play transfers authority for that object; after the destination accepts it, Gateway routing is updated and the source object is removed. A state snapshot may briefly exist at both ends during transfer, but both ends must never process requests, advance game state, or perform authoritative writes.

This model does not depend on spatial partitioning, Cell/ghost projections, or cross-process spatial roaming used by seamless-world architectures. MMO describes a possible product scale, not Apollo's current architecture category. Apollo is a lightweight multiplayer game server engine suited to tower defense, co-op, arenas, and dungeons.

Gateway can support availability through multiple instances and rolling deployment, but it does not migrate connections by itself. A RoomServer can roll by stopping new room assignments, waiting for active rooms to finish, and then exiting. This does not preserve active matches across versions or guarantee uninterrupted existing TCP connections.

This is the target model. Executables such as `base-app`, `room-server`, and `baseappmgr` are current prototype names; see [Server Application Status](../apps/index.md). They must not be read as the target topology. BigWorld/KBEngine terms appear only in engine comparison material to explain those external implementations.
