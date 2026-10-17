# Cross-server scene design reservation

Cross-server gameplay is an explicit World extension requirement. The design is reserved now; cross-server deployment is not enabled. Local and cross-server gameplay reuse `World`, `Scene`, and `Instance`, without another process role or seamless maps.

## Server affiliation and runtime location

A player's home game server is a business affiliation. World identifies a simulation process. These require distinct identities. Players from several game servers may enter the same Scene hosted by one selected World. Worlds can serve both local and cross-server activities; a dedicated World pool may later provide capacity or fault isolation using the same `world-app` executable.

Cross-server Scenes may be persistent. Temporary group activities associate the existing Instance lifecycle with their Scene; cross-server gameplay is not restricted to temporary instances.

The following are reserved interface requirements, not claims about fields already present on the wire.

| Data | Purpose |
|---|---|
| Unique player ID and home server ID | Preserve identity and affiliation without creating a new account on the destination |
| World ID, Scene ID, optional Instance ID | Runtime location; process-local Scene/Instance IDs require World context |
| World process incarnation and player authority epoch | Reject messages from earlier processes and owners |
| Session ID, Gateway ID, route version | Preserve authentication while forwarding to the new owner |
| Transfer ID, destination admission credential and expiry | Deduplication and server-authorized destination admission |
| Settlement ID and return location | Deduplicate rewards and return to the home server, with a safe fallback |

Extend the existing identity and contract mechanisms when wiring the feature. The legacy `home_zone_id` field does not establish an implemented business server identity.

## Responsibilities and placement

Login hosts the directory, placement, and authority-transfer coordination. Persistent routing and recovery are prerequisites for enabling cross-server play. Gateway retains the client connection and refreshes forwarding routes without requiring another login. World validates admission and holds the player's single active simulation object and exclusive profile write authority.

Local activities accept their own server by default. Cross-server activities explicitly configure participating servers, grouping, capacity, and compatible versions. Clients request an activity; they cannot nominate arbitrary destinations or assert membership. Placement deduplicates group requests and cancels or replaces reservations if a World drains, overloads, or becomes unavailable.

## Authority transfer

1. Validate membership and capacity; record transfer ID, source/destination, expected authority epoch, and return location.
2. Freeze the source at a simulation boundary, complete confirmed persistent writes, and export state. Reject further source simulation input and writes.
3. Restore the destination as prepared but inactive, validating identity, version, credential, expiry, and transfer ID.
4. Conditionally commit the new owner and epoch in the directory against the expected source and epoch. Activate the destination only after obtaining commit evidence. Refresh Gateway routing and clear source objects and write authority.
5. Record completion and release snapshots and reservations. Retries return the same transfer outcome.

Prepared does not mean active. An ambiguous timeout requires checking durable directory and transfer records before unfreezing the source. Roll back to the source only when commit is proven absent. After commit, recover the destination authority. Reject stale session/authority epochs, duplicates, and expired input.

Test in-process SceneTransfer, same-server cross-World transfer, and cross-server admission separately. Cross-server play additionally requires affiliation, admission, and settlement policies.

## Settlement, return, and failure

The player retains home-server affiliation while the current World owns the only writable profile. Home-server asynchronous rewards and administrative writes must reach the current owner or wait for transfer completion. Never update two independent profile copies. Reuse existing reward/persistence interfaces, deduplicate by settlement ID, and confirm durability before announcing completed settlement.

Completion, leave, disband, and expiry settle first, then return through the same authority-transfer protocol. If the original World is unavailable, allocate a safe Scene on the home server. Retain recoverable state, confirmed rewards, and return work when return fails.

Reconnect resolves the current cross-server location through the directory. Destination crashes, directory restarts, settlement retries, and forced drain must reconcile durable authority records. Where active gameplay cannot recover, fail the activity explicitly while preserving confirmed rewards and return information. Supervised restart alone does not recover a match.

## Acceptance before activation

- Players from at least two business servers share one Scene/Instance without identity collisions or affiliation changes.
- Local activities reject unauthorized cross-server joins; cross-server admission rejects forged membership, expired credentials, excess capacity, and incompatible versions.
- Transfers retain one authority under retries, per-stage timeouts, delayed routing, and source/destination restarts.
- Settlement retries issue rewards once; reconnect avoids another login; return supports unavailable original Worlds.
- Multi-Scene isolation, multi-World placement, and draining work without seamless maps or TCP socket handoff.

Implementation and verification remain tracked in the root [`todo.md`](https://github.com/cuihairu/apollo/blob/main/todo.md).
