
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q31: How does KBEngine broadcast efficiently? How does it guarantee messages are not duplicated?

## Core Conclusion

The key to this kind of problem is not only "how to broadcast", but also "who computes the broadcast receiver set" and "how to avoid duplicate delivery when multiple paths converge".

Efficient broadcasting usually depends on:

- AOI culling
- Observer set maintenance
- Incremental state sync

Avoiding duplication depends on:

- A clearly defined single broadcast source
- Receiver deduplication
- Clear routing boundaries

## 1. Why broadcasting is complex

In-scene broadcasting often involves at the same time:

- Entity movement
- AOI enter and leave
- Neighbor-cell mirroring
- Multi-node collaboration

As long as an entity can be observed by multiple logic paths at the same time, the duplicate notification problem appears.

## 2. The premise of efficient broadcasting is that the receiver set is computed correctly first

The most important step is knowing "who should receive this message".

This usually depends on:

- The AOI observer set
- Witness or similar observation relationships
- Neighbor-cell entity mirroring

If the observer set itself is computed coarsely, no matter how the sending is optimized later, it is only a patch.

## 3. Why duplicate messages appear

Common causes include:

- The same target receives updates simultaneously through both the local and neighbor-cell paths
- An entity is included in the broadcast set multiple times in boundary areas
- Create, update, and destroy events are triggered repeatedly across paths

In essence, "the same observer has multiple propagation paths to the same entity".

## 4. How to avoid duplication

### 1. Define a single authoritative source clearly

The authoritative state update of the same entity should have only one real source; other paths can only forward, and should not repeatedly generate the same kind of event.

### 2. Deduplicate the observer set before broadcasting

No matter whether receivers come from multiple adjacent sets or multiple subsystems, they should be merged and deduplicated before final delivery.

### 3. Separate events from state

State sync can be overwritten by version; event sync must guarantee it is produced only once.

If the two are mixed together, the duplication problem is harder to troubleshoot.

## 5. The truly efficient point is usually not in send, but in data organization

Common optimization directions include:

- Maintaining a stable observer set
- Dirty-field sync
- Batch encoding
- Multiple connections reusing the same message body

These usually yield more than simply squeezing the socket layer.

## 6. Why boundary areas are most prone to duplication

Because an entity may be observed simultaneously by:

- Players in the current partition
- Players in neighbor cells
- The Ghost mirroring link

If the system does not clearly separate "who to display to" and "who delivers it", duplicate creation or duplicate updates appear.

## 7. A more robust engineering approach

The common strategy is:

- The authoritative entity generates core updates in only one place
- Neighbor-cell mirroring is only responsible for supplementing observation, and does not repeatedly produce the same kind of event
- Do one more target-set deduplication before final delivery to the client
- The client additionally protects state messages by version

This way the server and the client each carry a layer of deduplication.

## 8. Common misconceptions

### 1. Message duplication is definitely a client problem

In many cases the root cause is actually multi-path broadcasting on the server.

### 2. Client-side deduplication alone is enough

Not enough. Server-side duplicate broadcasting wastes a large amount of bandwidth and CPU.

### 3. Broadcast optimization is just increasing send speed

What truly determines the upper limit is usually observer set maintenance and duplicate path control.

## References

- KBEngine AOI / Witness mechanism related materials
