
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q9: What if CellAppMgr goes down? What solutions are there?

## Core Conclusion

A component like CellAppMgr is essentially a "control plane management node", not a data plane node that directly carries most real-time combat logic.

So the typical impact after it goes down is usually not "all players immediately disconnect", but:

- Existing Cells can often keep running for a while
- New scheduling, splitting, migration, and load balancing capabilities stop
- If the failure lasts too long, it gradually evolves into capacity problems, hotspot problems, and recovery problems

Therefore, the most important thing for this kind of problem is not a one-liner "do high availability", but first distinguishing:

- Which capabilities are immediately lost
- Which capabilities can keep running in degraded mode
- Which scenarios must be recovered as soon as possible
- Whether high availability complexity is worth it

---

## 1. First clarify what CellAppMgr is responsible for

In the BigWorld / KBEngine class of architectures, CellAppMgr is generally responsible for space scheduling and management functions, for example:

- Recording which CellApps are online
- Tracking the load of each CellApp
- Allocating new Space / Cells
- Coordinating Cell splitting, merging, and migration
- Managing some topology and boundary adjustment strategies

Its characteristics are:

- It usually does not directly execute most combat logic itself
- But it determines how space logic is organized and migrated
- It belongs to a typical control plane component

So when it goes down, first distinguish that "control plane unavailable" and "data plane unavailable" are not the same thing.

---

## 2. What really happens after the crash

### 2.1 Parts that usually can keep working

If the current space topology is already established, and there are already stable connections between the CellApps, then in many implementations the following capabilities can usually be maintained for a while:

- Existing spaces continue simulating
- Players already in the game keep moving and fighting
- Already established AOI and broadcasting keep running
- Already completed Cell topology does not need immediate recomputation in the short term

In other words, in many cases there will be no "CellAppMgr goes down and the whole server instantly blacks out".

### 2.2 Parts that immediately fail or noticeably degrade

Control plane capabilities usually fail first:

- New Space / new Cell allocation
- New Cell scale-out or scale-in
- Dynamic load balancing
- Cell boundary adjustment
- Scheduling decisions for cross-Cell migration
- Unified scheduling in some exception recovery flows

### 2.3 Chain reactions after sustained failure

If the failure lasts a long time, the impact starts to accumulate:

- Hotspot Cells cannot be split, and load keeps rising
- New maps, new instances, new regions cannot be allocated normally
- Some cross-boundary movement may be blocked or enter degraded logic
- When a certain CellApp fails again, overall recovery is more difficult
- Operations loses a trusted control entry point for the current topology

So a more accurate description should be:

A CellAppMgr crash first makes the system lose "dynamic governance capability", not immediately lose "all real-time running capability".

---

## 3. First do failure level classification

Before discussing solutions, it is more reasonable to classify by recovery goal.

### 3.1 Level 1: fast restart

Goal:
- Restore the original function of the management node as soon as possible after it goes down

Applicable scenarios:
- A scheduling gap of several seconds to tens of seconds is acceptable
- Current project capacity pressure is not high
- Architecture complexity budget is limited

### 3.2 Level 2: state recoverable

Goal:
- After restart, the management view and scheduling state can be recovered as soon as possible

Applicable scenarios:
- Already in long-term operation
- Need to reduce chaos during the recovery period
- Do not want to rely on manual state re-convergence on every restart

### 3.3 Level 3: primary/standby automatic failover

Goal:
- After the management node fails, a standby node automatically takes over

Applicable scenarios:
- Control plane failure already substantively affects the business
- The team has reliable primary/standby failover and state replication capabilities

### 3.4 Level 4: distributed consensus control plane

Goal:
- Make the management plane a truly highly available distributed control plane

Applicable scenarios:
- Very large scale
- Many control plane responsibilities
- The team is capable of long-term maintenance of a distributed consensus system

Not every project needs Level 4.

---

## 4. The most pragmatic solution: process monitoring + automatic restart

This is the most common and easiest-to-land solution.

### 4.1 Approach

- Use `systemd`, `supervisor`, container orchestration, or process guardian tools to monitor `CellAppMgr`
- Automatically restart the process after it exits
- After recovery, re-pull state from each CellApp or request reporting

### 4.2 Advantages

- Simple implementation
- Small changes
- Low cost
- Often already enough for small and mid-sized projects

### 4.3 Disadvantages

- There is a management gap during recovery
- If the machine itself goes down, this solution is ineffective
- If the management state is entirely in memory, it needs to re-converge after restart

### 4.4 What projects it suits

- Single-region scale is not large
- Dynamic scheduling is not a high-frequency hard requirement
- The focus is first on making the system stable, not on pursuing extreme control plane HA

Many projects are already good enough at this stage.

---

## 5. Further improvement: state snapshot + restart recovery

If only automatic restart is relied on, and after recovery the topology still needs manual confirmation, the system is quite fragile. The next step is usually to snapshot the control plane state.

### 5.1 What is suitable to persist

A more reasonable approach is not to forcibly persist all runtime details, but to persist "the key minimum set needed to recover the control plane", for example:

- CellApp registration information
- Space-to-CellApp ownership relations
- Main Cell boundary information
- Current load statistics snapshot
- Version number and timestamp

### 5.2 Recovery flow

1. The management node restarts
2. Load the most recent snapshot
3. Pull from all online CellApps or request a full report
4. Use the online report results to correct the snapshot
5. Restore scheduling capability

### 5.3 Key points

The snapshot cannot be treated as the absolute source of truth, because the data in the last few seconds before the crash may have already changed.

A more robust approach is:

- The snapshot is only used to quickly restore the framework
- Real convergence is based on the current reports of online CellApps

### 5.4 Pros and cons

Pros:
- Faster recovery
- Control plane state is easier to converge
- Lower manual intervention cost

Cons:
- Need to design state versions and correction logic
- Need to consider deviation between snapshot and real-time state

---

## 6. Further up: primary/standby failover

If even a short control plane failure is unacceptable, the primary/standby mode can be considered.

### 6.1 Basic approach

- One primary CellAppMgr works externally
- One or more standby nodes stay on standby
- The primary node periodically replicates key state
- After the primary node is lost, a standby node campaigns or takes over

### 6.2 What is really hard about this solution

The difficulty is not "starting a standby process", but:

- Who judges that the primary is really dead, rather than a network jitter
- How to avoid double primaries during primary/standby failover
- Whether the state obtained by the standby node is new enough
- How each CellApp re-binds to the new primary during failover

### 6.3 What situations it suits

- Control plane failure has repeatedly caused online problems
- Dynamic migration and load balancing are high-frequency capabilities
- The team can already handle distributed locks, leases, and failover

### 6.4 What situations it does not suit

- Even automatic restart and state recovery are not yet stable
- Control plane responsibilities are not yet complex
- The team has no distributed failure handling experience

If the basic capabilities are not in place, going straight to primary/standby usually only detonates the complexity ahead of time.

---

## 7. Highest complexity solution: consensus control plane

It is doable to further make a node like CellAppMgr into a control plane based on `etcd`, `ZooKeeper`, or similar consensus systems, but it should not be written as the default answer.

### 7.1 What problems it solves

- Leader election
- Lease and liveness detection
- Consistent metadata storage
- Watch mechanism
- Control plane convergence after failover

### 7.2 The costs it brings

- Need to maintain additional infrastructure
- Need to handle the operations and alerting of the consensus system itself
- Higher requirements for team engineering capability
- Easy to upgrade an originally simple management node problem into a whole control plane engineering effort

### 7.3 When it is worth it

- Your control plane is no longer just a simple manager
- There are many dynamic topology decisions and automated governance
- The failover window is really sensitive
- The team has the capability to maintain such a system long-term

Many small and mid-sized MMO projects actually never reach this step.

---

## 8. A more reasonable solution comparison

| Solution | Complexity | Recovery speed | Machine failure handling | Suitable stage |
|------|--------|----------|--------------|----------|
| Process guardian + automatic restart | Low | Medium | Weak | Early to mid stage |
| Automatic restart + state snapshot recovery | Medium | Medium-high | Weak | Mid-stage stable operation |
| Primary/standby failover | Medium-high | High | Medium | When the control plane is relatively critical |
| Consensus control plane | High | High | Strong | Large-scale long-term evolution |

There is no absolute recommendation here, only choices matched to the project stage.

---

## 9. A more practical engineering suggestion

For a typical MMO project, it is more recommended to do it in the following order, rather than landing a complex HA system in one step.

### Step 1: first do automatic restart well

- Process guardian
- Exit alerting
- Startup logs
- Health checks

### Step 2: then do state recovery well

- Key metadata snapshots
- Full CellApp reporting
- Automatic correction after startup

### Step 3: finally decide whether to do primary/standby

It is more reasonable to go to primary/standby only when the following conditions hold:

- Short control plane unavailability is already a substantive problem
- Hotspot scheduling and migration must be continuously available
- The team can guarantee no double primaries and no wrong failover

---

## 10. If it is Apollo, how to trade off

If Apollo is still in the architecture construction or mid-early verification stage, the more reasonable priority is usually:

1. First define CellAppMgr as a clear control plane component
2. Clarify which states must be recovered and which states can re-converge
3. First do automatic restart and state recovery
4. Wait for real online scale to prove that the control plane window is unacceptable, then consider primary/standby or a consensus control plane

In other words, do not directly make the control plane into a complex distributed system just because it is "theoretically more advanced".

Using the lowest complexity to solve real problems is usually more stable.

---

## 11. Summary

After a CellAppMgr crash, what is lost first is usually control plane capability, not all real-time logic stopping immediately.

The really important question is not a one-liner "how to do high availability", but:

- Whether existing Cells can keep running
- Whether new scheduling and migration are allowed to freeze briefly
- What recovery relies on to re-converge state
- Whether the control plane failure really deserves introducing heavier distributed complexity

For most projects, a more reasonable route is:

`automatic restart -> state recovery -> primary/standby failover -> consensus control plane`

Rather than upgrading the management node problem into a complete distributed control plane from the start.

---

## References

- [KBEngine Lab - Engine Overview](https://www.kbelab.com/manual/engine-overview.html)
- [etcd Documentation](https://etcd.io/docs/)
- [Raft Consensus Algorithm](https://raft.github.io/)
