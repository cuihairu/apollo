
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q29: How do KBEngine CellApps communicate with each other? How do they discover their own space position?

## Core Conclusion

Communication between CellApps is not about "whether there is a direct TCP connection", but about how they know:

- Which piece of logical space they are responsible for
- Who their adjacent nodes are
- Which entity is currently managed by whom

The so-called "discovering your own space position" is more accurately discovering your logical area of responsibility and adjacency relationships, not just geometric coordinates.

## One: Why CellApps Must Communicate with Each Other

As soon as the world is split across multiple CellApps, these upstream requirements are unavoidable:

- Entities moving across boundaries
- Neighborhood AOI awareness
- Ghost synchronization
- Migration handover

So CellApps usually need long-term internal communication links rather than being completely isolated from each other.

## Two: What the Communication Content Usually Includes

Commonly includes:

- Node registration and heartbeat
- Space ownership information
- Neighborhood node information
- Entity migration data
- Ghost updates
- Boundary event notifications

Among these messages, the truly high-frequency ones are usually neighborhood collaboration and entity synchronization, not just a one-time registration at startup.

## Three: How They Know Which Piece They Are Responsible For

Common approaches are:

- Register with a management node after startup
- The management node assigns logical regions or space responsibilities
- The node saves its own area of responsibility
- It also learns the adjacent node information

So "space position" is essentially a piece of scheduling and ownership metadata.

## Four: Why This Is Not a Simple Physical Coordinate Problem

Many people easily understand it as:

- `CellApp1` is responsible for `x=0~100`
- `CellApp2` is responsible for `x=100~200`

But real systems are usually more complex, because they also involve:

- Multiple independent Spaces
- Instances and rooms
- Dynamic load balancing
- Hotspot migration

So what a node knows is often not a single rectangle but a set of logical space ownership relationships.

## Five: How CellApps Establish Communication Relationships

The common approach is:

- A central management component first maintains node information
- CellApps then establish internal connections on demand
- Afterwards they use internal protocols to exchange migration and synchronization messages

The key point is not whether it is full-mesh or on-demand, but:

- Whether node discovery is stable
- Whether neighborhood switches are timely
- Whether relationships can be rebuilt after failures

## Six: Why Migration Especially Depends on This Discovery Mechanism

When an entity moves across a boundary, the source node must know:

- Who the new target node is
- How to hand the state over to it
- When the primary/standby switchover completes

If adjacency and ownership information is inaccurate, the migration process easily produces:

- Migration to the wrong node
- Lost Ghost synchronization
- Two Reals coexisting

## Seven: What Matters More in Engineering Is Not "How to Connect" but "How to Maintain the Topology"

The internal communication protocol is only the surface; what is truly difficult is:

- How space partitioning is updated
- How hotspot regions are migrated
- How adjacency relationships are added and removed
- How convergence happens when a node fails

These determine whether the system can run stably over the long term in a dynamic world.

## Eight: Common Misconceptions

### 1. CellApp-to-CellApp Communication Is Just Ordinary RPC

Incomplete. Their communication is strongly coupled with space boundaries, entity ownership, and Ghost synchronization.

### 2. Space Position Is the Map Coordinate

Not accurate. It is more the node's logical area of responsibility and adjacency relationships.

### 3. One Partition Assignment at Startup Is Enough

It may be enough for static small scenes, but usually is not enough for hotspot migration and dynamic scaling.

## References

- KBEngine architecture materials on CellAppMgr, CellApp, and Space management
