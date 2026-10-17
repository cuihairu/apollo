
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q4: Why Doesn't Apollo Implement a Seamless World? How Do Rooms Transition?

Apollo targets lightweight multiplayer games organized around matches. It does not implement a persistent seamless world where players roam continuously across spatial simulation processes, so Cell spatial partitioning, Ghost replicas, and cross-Cell handoff are not foundational architecture requirements.

Moving between Rooms is an explicit gameplay flow: the client leaves or completes the current Room, then enters a destination Room through room allocation or matchmaking. If the player must move between RoomServers, transfer the one authoritative runtime object: freeze and export it at the source, restore and confirm ownership at the destination, update online routing, then remove the source copy. At every point, only one side may process input, advance state, or make authoritative writes.

Cross-RoomServer authority transfer is a target requirement. The current prototypes do not provide complete Room management, matchmaking integration, or end-to-end route migration. See the root [`todo.md`](https://github.com/cuihairu/apollo/blob/main/todo.md) for current scope and acceptance criteria.
