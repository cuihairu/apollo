
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q8: How do you design to avoid single points of failure?

## Core Conclusion

Avoiding single points of failure is not simply copying every process; it is answering four questions:

- Which component, once it fails, affects the core path
- Does this component have an alternative path
- Can the alternative path switch over automatically
- Can the state still converge after the switch

So what really matters is not "whether there is a single point", but:

- Where the single point appears
- Whether it affects the core path or the control path
- Whether it must be eliminated completely, or a brief failure with quick recovery is acceptable

---

## One: What Counts as a True Single Point of Failure

A single point of failure is not as simple as "there is only one instance".

More accurately, a single point of failure means:

a component or a class of resource in the system that, once it fails, and the system has no effective alternative path, causes key functionality to become unavailable.

There are three keywords here:

- `Key functionality`
- `No alternative path`
- `Cannot recover quickly`

For example:

- Only one login entry; once it is down, no player can enter
- Only one database primary; once it is down, all core reads and writes stop
- Only one control node; once it is down, the topology cannot be scheduled
- Only one switch or one link; once it is down, the whole cluster cannot see each other

So a single point can be:

- A single process
- A single machine
- A single database instance
- A single network device
- A single configuration center
- A single data center
- A single manual operations procedure

The last one is often ignored. Many "technically high-availability" systems actually still depend on manual operation for switchover, so essentially they still have an operations single point.

---

## Two: Don't Treat All Single Points as the Same Kind of Problem

In an MMO architecture, single points can be divided into at least four categories.

### 2.1 Access-Layer Single Points

For example:

- Login entry
- Gateway entry
- Load balancer entry

When this kind of single point fails, the most direct symptoms are usually:

- Players cannot log in
- New connections cannot be established
- Some shards are completely unreachable

### 2.2 Control-Plane Single Points

For example:

- `BaseAppMgr`
- `CellAppMgr`
- Schedulers
- Registry centers

When this kind of single point fails, the system usually does not stop entirely right away, but:

- Dynamic scheduling fails
- New resource allocation fails
- Failure recovery capability degrades
- Risk accumulates over long-running operation

Their characteristics are:

- They greatly affect "continuous governance capability"
- They are not necessarily immediately fatal to "currently existing business flows"

### 2.3 Data-Layer Single Points

For example:

- MySQL primary
- Redis primary node
- The only message queue node
- A single persistence proxy

This kind of single point is usually more dangerous, because a lot of core business ultimately lands on the data layer:

- Login state
- Character data
- Trading
- Mail
- Inventory
- Payments or orders

If there is no alternative path here, the impact is usually large.

### 2.4 Infrastructure Single Points

For example:

- A single physical machine
- A single switch
- A single power supply
- A single storage volume
- A single data center

This kind of single point is most easily masked by "multiple process instances".

For example, you deploy three service instances, but they are all on the same machine; for machine-level failure they are still the same single point.

---

## Three: Judging Whether Something Is a Single Point Cannot Look Only at Instance Count

A very common misconception is:

"I have already deployed two instances, so there is no single point anymore."

That is not necessarily right.

A more reasonable judgment is to look at the failure domain.

### 3.1 What Is a Failure Domain

A failure domain is "a group of resources that may fail together".

Common failure domains include:

- The same process
- The same machine
- The same rack
- The same switch
- The same data center
- The same storage system
- The same configuration system

### 3.2 Typical Pseudo-Redundancy

The following situations look redundant but still have single points:

- Two instances on the same machine
- Primary and standby both depending on the same configuration center
- Multiple services all depending on the same database primary
- Two systems sharing the same egress link
- Primary/standby switchover depending on the same human operator

So avoiding single points is essentially about splitting failure domains, not just adding up process counts.

---

## Four: Which Single Points in an MMO Architecture Need Priority Handling

Not all single points deserve the same cost investment at the same time.

### 4.1 First Priority: Access and Data

Usually the ones that should be handled first are:

- Login entry
- Gateway entry
- Core database
- Core cache

The reasons are simple:

- They directly affect whether players can enter and whether they can complete core operations
- Once they fail, the blast radius is usually the largest

### 4.2 Second Priority: Session and Space Core Nodes

For example:

- Player session nodes
- Map logic nodes
- Key space scheduling nodes

This kind of component directly affects online player experience, but the handling usually has to distinguish:

- Real-time logic nodes emphasize failure isolation and recovery strategy
- Control nodes emphasize recovery and switchover capability

### 4.3 Third Priority: Backend and Non-Core Functions

For example:

- Leaderboards
- Statistics
- Log archiving
- GM backend

This kind of system should also not be a single point, but even if it is, you can usually prefer "degradation + quick recovery" rather than doing heavy dual-active from the start.

---

## Five: Common Handling Techniques, but Use Them by Scenario

### 5.1 Multi-Instance Redundancy

This is the most basic technique.

Suitable for:

- Login services
- Gateway services
- Stateless or weakly stateful services

Core value:

- When one instance fails, traffic can still switch to other instances

But the premises are:

- The instances are truly interchangeable
- The client or upstream knows how to switch
- State does not depend on unique local memory

### 5.2 Primary/Standby Switchover

Suitable for:

- Control nodes
- Data nodes
- Components with clear primary/standby semantics

The core question is not "is there a standby", but:

- When to judge that the primary is down
- How to avoid dual primaries
- Who is responsible for re-converging state after switchover

### 5.3 Automatic Restart and Quick Recovery

Not all single points must be eliminated completely.

For some control-plane nodes, a more realistic strategy is:

- Accept that it is a single point for a short time
- But make the recovery time short enough

This kind of strategy suits:

- Management nodes
- Scheduling nodes
- Backend nodes on non-core write paths

### 5.4 Data Replication and Failover

The data layer cannot rely only on "the process automatically restarts".

You usually also need:

- Primary/standby replication
- Failure detection
- Read/write switchover
- Data validation

Otherwise, even though the database "recovered", data consistency may already be broken.

### 5.5 Degradation and Circuit Breaking

Some single points cannot be completely eliminated, or the cost is too high; in that case you have to design degradation paths.

For example:

- The leaderboard is down, the main game keeps running
- Mail is sent with a delay, the trading main path is not dragged down
- The chat system fails, combat and map simulation are unaffected

This is also the key way to avoid "a local failure dragging down the whole".

---

## Six: How to View Single Points in Architectures Like KBEngine / BigWorld

The typical characteristics of this kind of architecture are:

- `LoginApp / BaseApp / CellApp` can usually be multi-instance
- Some manager-type components are naturally closer to control-plane single points
- The data layer and infrastructure layer may still have true big single points

So a more accurate statement is not "which component has only one instance, so it is the most dangerous", but:

- When it fails, does it affect access, real-time logic, the control plane, or the data layer
- Can its state recover quickly
- Does it have an alternative path

For example:

- `CellAppMgr` is more like a control-plane single point
- The database is more like a core data single point
- The login entry is more like an access-layer single point

They all matter, but the handling is different.

---

## Seven: A More Pragmatic Design Method

### 7.1 First Draw the Key Paths

First draw out the core business paths:

- Login
- Entering the world
- Map migration
- Trading
- Character saving

Then ask:

- Which components does this path pass through
- Which component failing would interrupt the whole path
- Is there an alternative path

### 7.2 Then Mark Failure Domains

For each key component, mark:

- Whether it is multi-instance
- Whether it spans machines
- Whether it spans racks or availability zones
- Whether it shares the same database, storage, or network equipment

### 7.3 Finally Do Tiered Governance

A more reasonable priority is usually:

1. First eliminate hard single points in the access and data layers
2. Then handle core nodes that affect large-scale online experience
3. Finally handle high-availability optimization for the control plane and backend systems

This is more realistic than "making all components dual-active at the same time".

---

## Eight: When You Should Not Over-Design

The mistake many projects make early on is:

- The business is not stable yet
- Monitoring is not complete yet
- Automated deployment is not solid yet
- And they start designing full-path dual-active, multi-data-center, multi-primary consistency

The result is usually:

- System complexity skyrockets
- Problems become harder to locate
- Real gains are very limited

Therefore, avoiding single points of failure does not equal doing the most complex high-availability architecture from the start.

A more reasonable approach is usually:

- First identify the true hard single points
- First guarantee automatic detection and automatic recovery
- Then gradually upgrade to primary/standby, cross-data-center, or more advanced solutions based on business scale

---

## Nine: A More Practical Checklist

When designing, you can directly ask these questions:

- Is there only one login entry
- Is there only one writable node in the core database with no switchover capability
- Are all gateways in the same failure domain
- Can key managers recover automatically after failure
- When the control plane fails, can the data plane keep running
- When non-core functions fail, do they drag down the core path
- Does primary/standby switchover depend on humans
- Are monitoring, alerting, and drills already established

If several of these questions are answered with "no alternative path", the system probably still has obvious single points.

---

## Ten: Summary

The core of avoiding single points of failure is not "copying everything twice", but:

- Finding the failure points that truly affect key paths
- Splitting failure domains
- Providing alternative paths
- Making switchover and recovery automated

For MMOs, the ones that should be prioritized are:

- Access-layer single points
- Data-layer single points
- Failure isolation for key session and space nodes
- Recovery capability for control-plane nodes

A mature system is usually not "has no single points at all", but:

even if some local part fails, core functionality can continue, or can automatically recover within an acceptable time.

---

## References

- [KBEngine Lab - Disaster Recovery](https://www.kbelab.com/manual/disaster.html)
- [KBEngine GitHub](https://github.com/kbengine/kbengine)
- [MySQL Replication](https://dev.mysql.com/doc/refman/8.0/en/replication.html)
