
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q7: In-Process Architecture vs Multi-Process Architecture — What Are the Pros and Cons of Each?

## Core Conclusion

This question is essentially about "whether the system boundary is placed inside the process or outside it."

- The advantages of in-process architecture are speed, simplicity, and convenient shared state
- The advantages of multi-process architecture are isolation, scale-out, and better fault tolerance
- A truly reasonable choice is usually not an absolute either/or, but first establishing clear boundaries, and then deciding whether those boundaries need separate processes

The evolution path of many projects is:

`Single process multi-threaded -> single machine multi-process -> multi-machine multi-process`

---

## I. First Clarify the Boundaries

This question is easily confused with two other questions:

- `q6` leans more toward the deployment-level choice of "single machine / distributed"
- `q71` leans more toward the concurrency execution model of "thread vs process"
- `q7` focuses on "whether business modules should be split into independent processes"

Therefore, the focus of the `q7` discussion is neither "how threads are scheduled" nor "how machines are deployed," but:

- Whether the network layer, logic layer, and data layer are in the same process
- Whether between key modules there are function calls, or IPC / RPC
- What benefits and costs the process boundary brings

---

## II. Three Common Forms

### 2.1 Single Process Multi-Threaded

All modules live in the same process, just executing concurrently with multiple threads.

```text
┌────────────────────────────────────────────────────────────────┐
│                        Single Process                        │
│  ┌───────────────┐ ┌───────────────┐ ┌───────────────┐       │
│  │Network Thread │ │Logic Thread   │ │DB Thread      │       │
│  └───────────────┘ └───────────────┘ └───────────────┘       │
│                        Shared Memory                         │
└────────────────────────────────────────────────────────────────┘
```

Characteristics:
- Modules share memory with each other
- Lowest data passing cost
- Most direct state management
- Thread safety problems are the most concentrated

### 2.2 Single Machine Multi-Process

Modules are split into multiple processes, but still run on the same machine.

```text
┌────────────────────────────────────────────┐
│            Single Machine Deployment       │
│  ┌────────┐ ┌────────┐ ┌────────┐         │
│  │Gateway │ │Logic   │ │DBProxy │         │
│  │Process │ │Process │ │Process │         │
│  └────────┘ └────────┘ └────────┘         │
│      IPC / Local TCP / Unix Socket         │
└────────────────────────────────────────────┘
```

Characteristics:
- Process isolation already exists
- Module boundaries are already established
- Can tolerate lower distributed complexity for now
- Still shares the same machine's failure domain

### 2.3 Multi-Machine Multi-Process

After modules are split into independent processes, they are then distributed across multiple machines by responsibility or load.

```text
Client
  │
  ▼
Gateway Process  ──► Session / Base Process  ──► Space / Cell Process
                               │
                               └────────► DB / Cache / MQ
```

Characteristics:
- Both process boundaries and deployment boundaries are separated
- Strongest scale-out capability
- Highest complexity in ops, monitoring, and fault handling

---

## III. Advantages and Costs of In-Process Architecture

### 3.1 Advantages

#### 1. Low Communication Cost

Collaboration between modules in the same process is usually:

- Function calls
- Pointer references
- Data access without serialization

This means:

- Lower latency
- Less CPU overhead
- Data structures are easier to reuse

#### 2. High Development Efficiency

Single-process architecture is friendly to small teams:

- Simple startup
- Short debugging paths
- More concentrated logs and call stacks
- Problem location is usually more direct

#### 3. Simple State Management

Much game logic is itself a tightly coupled state machine.

If everything is in one process:

- State sharing is more natural
- Consistency boundaries are easier to control
- No need to first consider cross-process idempotency, retries, and compensation

### 3.2 Costs

#### 1. Large Blast Radius on Failure

As long as key logic crashes the process, the whole service usually goes down together.

Even without a crash, as long as the following appear:

- Deadlock
- Long stalls
- Memory corruption
- Main thread blocked

The impact is often global too.

#### 2. Modules Drag Each Other Down

If networking, logic, storage, AI, and background tasks are all in one process, the following easily appear:

- One hot module slows down the entire service
- One long-tail operation blocks the main loop
- One memory leak drags down the whole

#### 3. Unclear Scale-Out Boundaries

Once the system is written as one giant process, splitting it later is painful, because:

- Modules share too much implicit state
- Dependency chains are too long
- Many calls implicitly depend on synchronous semantics

At that point it is not as simple as "splitting the process apart"; module boundaries must be rebuilt.

---

## IV. Advantages and Costs of Multi-Process Architecture

### 4.1 Advantages

#### 1. Stronger Fault Isolation

The most direct benefit of multi-process is isolation.

For example:

- A chat process anomaly should not drag down combat
- A map process anomaly should not affect login
- A backlog in some background task should not block main logic

This kind of isolation is especially important in MMOs, because services usually run online for long periods.

#### 2. Easier to Scale Out by Responsibility

Only after processes are split can scaling out by module happen more naturally:

- If gateway connection pressure is high, scale out the Gateway
- If scene simulation pressure is high, scale out Space / Cell
- If data access pressure is high, scale out DBProxy or the cache layer

This is also the core reason why architectures like KBEngine split `LoginApp / BaseApp / CellApp / DBMgr` apart.

#### 3. Easier to Do Resource Isolation

Different processes can be individually controlled for:

- CPU affinity
- Memory limits
- Restart strategy
- Deployment location

This makes performance governance and fault governance more controllable.

#### 4. More Natural Evolution to Distributed

As long as the boundaries are already at process level, the later migration from "local process communication" to "cross-machine process communication" is usually far less work than splitting directly from a monolithic big process.

### 4.2 Costs

#### 1. Communication Cost Increases Significantly

Once cross-process, the original function calls become:

- Serialization
- Sending
- Receiving
- Deserialization
- Timeout handling
- Error handling

It not only becomes slower, but also introduces new complexity.

#### 2. Consistency Becomes Harder

Modifying two modules' state in the same process is often just two memory writes.

In multi-process, you must consider:

- Whether the message arrived
- Whether the message is duplicated
- Whether the order was disrupted
- How to compensate after partial success

This upgrades many "originally quite ordinary business logic" into engineering problems.

#### 3. Debugging and Observability Become Harder

After going cross-process, local breakpoints alone are usually not enough; you also need:

- Full-path logs
- Trace ID
- Queue length monitoring
- RPC / message latency monitoring

Otherwise problems become "every process looks fine, but the whole thing is stuck."

#### 4. Deployment and Ops Complexity Rises

Multi-process architecture naturally introduces:

- Process management
- Configuration management
- Startup/shutdown ordering
- Service discovery
- Health checks
- Automatic restarts

This part of the cost cannot be ignored.

---

## V. A More Accurate Comparison Table

| Dimension | In-Process Architecture | Multi-Process Architecture |
|------|------------|------------|
| Module communication | Function calls / shared memory | IPC / RPC / messages |
| State sharing | Simple | Requires explicit sync |
| Development barrier | Low | Medium-high |
| Debugging cost | Low | High |
| Fault isolation | Weak | Strong |
| Resource isolation | Weak | Strong |
| Scale-out capability | Weak | Strong |
| Consistency handling | Relatively simple | More complex |
| Ops complexity | Low | High |
| Suited stage | Prototype, early, small scale | Mid-late stage, complex business, long-term operation |

Note:

- "Multi-process" does not necessarily mean "cross-machine"
- "In-process" does not mean "cannot be multi-threaded"
- The real difference lies in whether module boundaries are raised to process level

---

## VI. Which Modules in MMO Suit Process Splitting

Not every module deserves its own process.

### 6.1 Modules Suitable for Process Splitting

#### 1. Gateway / Access Layer

Reasons:
- High connection count
- Clear boundary with business logic
- Easy to apply rate limiting, authentication, and session forwarding

#### 2. Space Simulation / Map Logic

Reasons:
- It is the core hotspot source of MMO
- AOI, movement, combat, and visibility broadcasts naturally cluster here
- Suits splitting by map, cell, or Cell

#### 3. Data Proxy / Persistence Layer

Reasons:
- IO characteristics differ greatly from logic characteristics
- Suits independent rate limiting, connection pools, retries, and asynchronous write-back

#### 4. Background Asynchronous Modules

For example:
- Mail
- Leaderboards
- Statistics
- Log archiving
- GM and operations systems

The reason is that these modules should not drag the main game loop down in turn.

### 6.2 Modules That Should Not Be Split Lightly

Tightly coupled, high-frequency interaction logic, if after splitting it requires cross-process communication every frame, is usually not worth it.

Typical examples:

- Combat resolution within the same combat domain
- AOI updates within the same view domain
- High-frequency shared local state machines

The principle here is:

High-frequency, tightly coupled, low-granularity interaction logic should preferentially stay within the same process boundary.

---

## VII. Why KBEngine Prefers Multi-Process

MMO architectures like KBEngine naturally prefer multi-process, not because "multi-process is more advanced," but because the core problems of MMOs inherently require isolation and splitting:

- Login and access are connection-intensive
- Player sessions and persistent state are session-oriented
- Space logic is real-time simulation
- Data reads and writes are IO-intensive

These load characteristics differ greatly; holding everything in one process usually becomes harder and harder to govern later.

Splitting `BaseApp / CellApp` apart is essentially also drawing state boundaries:

- `BaseApp` leans more toward player persistent state, session state, and background state
- `CellApp` leans more toward space real-time state, AOI, movement, and combat

This is not the only correct approach, but it is a very natural split for MMOs.

---

## VIII. How to Choose

### 8.1 Situations Suited to Prioritizing In-Process Architecture

- The project is still in the prototype stage
- The team is small
- The gameplay is not yet stable
- Online scale and space complexity are both low
- Getting the business path running matters more than scale-out capability

At this point the most important thing is not "how much it can scale in the future," but:

- High development efficiency
- Fast problem location
- Fast enough stress testing and iteration

### 8.2 Situations Suited to Seriously Introducing Multi-Process

- Load characteristics differ obviously between modules
- Failure of certain modules must not drag down the whole
- Certain modules need independent scale-out
- Obvious hotspots and bottlenecks have already appeared
- Long-term stable operation is required

At this point the benefits of multi-process begin to outweigh the complexity cost.

---

## IX. A More Pragmatic Evolution Path

### Phase 1: Single Process Multi-Threaded

Suitable for early validation.

The focus is not "doing everything to the extreme," but:

- Clarify module responsibilities first
- Do not abuse global shared state
- Prepare observability in advance

### Phase 2: Single Machine Multi-Process

This is the most easily overlooked, but very critical phase.

It is recommended to split first:

- Gateway
- Scene logic
- Data proxy
- Background asynchronous systems

The value of this phase lies in:

- Establishing process boundaries
- Validating protocol design
- Practicing exception and timeout handling

### Phase 3: Multi-Machine Multi-Process

Only after boundaries are stable, bottlenecks are clear, and ops capability keeps up, will pushing multi-machine deployment bring fairly certain benefits.

---

## X. Summary

The core difference between in-process architecture and multi-process architecture is not "whether it can be concurrent," but "where the module boundaries are placed."

If the current goal is fast validation, fast iteration, and fast problem location, in-process architecture is usually more suitable.
If the current goal is fault isolation, independent scale-out, and long-term stable operation, multi-process architecture is usually more suitable.

A more robust engineering approach is often not splitting into processes comprehensively from the start, but first designing the boundaries correctly, and then deciding which boundaries are worth upgrading to independent processes based on real bottlenecks.

---

## References

- [KBEngine GitHub](https://github.com/kbengine/kbengine)
- [KBEngine Lab - Engine Overview](https://www.kbelab.com/manual/engine-overview.html)
