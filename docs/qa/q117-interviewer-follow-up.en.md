[English](q117-interviewer-follow-up.en.md) | [中文](q117-interviewer-follow-up.md)

# Q117: If you already have the basic MMO questions down, what deeper questions should you keep thinking about next?

## Core Conclusion

Grasping the basic concepts does not mean you truly grasp MMO architecture.

When going deeper, the focus is usually not on "memorizing more terms", but on pushing the basic concepts up to four higher levels:

- Being able to narrate a complete sequence of events
- Being able to explain fault convergence
- Being able to justify engineering trade-offs
- Being able to land on online operations and capacity judgment

If the earlier basic documents mostly answered "what is this", then this article focuses more on "how the system actually runs, breaks, recovers, and why the trade-offs are made in a real environment".

## I. From Conceptual Understanding to System Sequencing

Many architecture discussions stay at the level of component responsibilities, for example:

- Login handles login
- Gateway manages connections
- Base manages sessions
- Cell manages scenes

This kind of understanding only counts as the first layer.

When going deeper, you should be able to narrate a complete chain as a sequence of events:

- When a player logs in, at which hop is the token generated and validated
- When a player enters a scene, which establishes the association first, Base or Cell
- When switching maps, which states migrate and which states are rebuilt
- On reconnect, to which authoritative state the client restores

Truly mature understanding is not knowing "which components exist", but knowing "how messages flow, at which step state changes, and at which step problems will occur".

## II. From the Happy Path to the Failure Path

What really differentiates system design is often not the normal chain, but the abnormal chain.

For example:

- When a CellApp crashes, how are the combat state, trade state, and inventory state handled respectively
- When a trade is half done, the deduction succeeds but the log write fails — how does the system converge
- When a cross-server flow is partially successful, which is authoritative, the old state or the new state
- Data has already been pushed to the client, but the database has not committed yet — what happens if it fails afterwards

The essence of all these questions is:

- Who is the source of authority
- Which operations must be idempotent
- Which side effects can be compensated
- Which states must be rolled back, and which can only be repaired forward

If you can only narrate the happy path, it means your understanding is still at the "feature design" layer and has not yet entered the "online system" layer.

## III. From Listing Solutions to Judging Trade-offs

Many technical discussions easily fall into "why not use a more advanced solution".

When going deeper, what you really need is the ability to make trade-off judgments, for example:

- Why large worlds generally do not use global frame synchronization as the main body
- Why some modules suit the Actor model and some do not
- Why some chains choose eventual consistency instead of strong consistency
- Why some high-frequency chains must validate synchronously, while some chains can persist asynchronously

The most important thing here is not picking sides on answers, but whether you can explain:

- The benefit of the solution
- The cost of the solution
- The boundary of applicability
- Whether it still holds after the scale grows

If your answers are always only "this solution has higher performance" or "that solution is more advanced", it usually means you have not yet truly built an engineering judgment framework.

## IV. From Feature Implementation to Online Observation

Truly mature system design cannot stop at "the features work".

You also need to keep thinking about:

- Which core business metrics should be monitored online
- How to determine where the system bottleneck appears first
- How to locate skill latency, broadcast backlog, lock contention, and database jitter
- How stress testing should construct real load, instead of only pressing empty connections

For example, for an MMO server, the truly valuable metrics are often not only CPU and memory, but also:

- Online player count distribution
- Scene population distribution
- Average AOI neighbor count
- Broadcast fan-out per single message
- Skill processing latency
- Trade failure rate
- Instance creation success rate

These metrics say more about whether the system is healthy than pure machine metrics.

## V. From Single-Module Understanding to Cross-Module Interaction

The basic stage often studies by module:

- AOI
- Synchronization
- Trading
- Auction
- Instance

But when going truly deep, you need to start thinking about how they influence each other.

For example:

- Lag compensation affects the boundary of combat and skill behavior
- Ghost / Shadow affects AOI and migration design
- Trading and inventory share the same asset consistency requirements
- Instance, matchmaking, and scene management are essentially different stages of the same session scheduling chain

If the modules are still isolated from each other, it means your understanding has not yet upgraded from "a collection of knowledge points" to "a system as a whole".

## VI. A Few Directions Most Worth Filling In When Going Deeper

### 1. Sequence Diagram Ability

At minimum, you should be able to draw these flows completely:

- Login to entering the scene
- Map switching
- Disconnect and reconnect
- Trade submission flow
- Instance creation and settlement

### 2. Fault Convergence Ability

At minimum, you should be able to answer:

- What is lost when a node crashes
- Which states are recoverable and which can only be abandoned
- How timeouts, retries, and duplicate execution prevent double writes

### 3. Consistency Boundary Ability

At minimum, you should be able to distinguish:

- Authoritative state
- Cached state
- Display state
- Compensable state

### 4. Capacity and Observation Ability

At minimum, you should be able to answer:

- Why 5000 players on a single server is not a pure networking problem
- Which kind of scene blows out first
- How stress testing can stay close to real behavior
- How metrics should be designed

## VII. Turning Deeper Understanding into a Self-Check Checklist

You can use the following four groups of questions to check whether you truly master the system:

### 1. Can I draw the complete sequence of events

If you can only state component responsibilities but cannot draw state changes and message ordering, your understanding is not deep enough yet.

### 2. Can I explain the failure paths

If you can only describe "what happens on success" but cannot explain "what to do when it fails halfway", you have not yet entered the online system perspective.

### 3. Can I explain why this choice was made

If you can only say "this solution is common" or "this solution is advanced" but cannot explain the cost and the boundary, you still lack engineering trade-off ability.

### 4. Can I explain how to observe and how to troubleshoot

If your design narrative is strong, but you cannot answer how to watch it online or how to troubleshoot a fault, you are still at the design diagram level.

## VIII. Summary

Basic knowledge mostly solves "knowing what this is", while going deeper must solve:

- How the system runs in sequence
- How it converges when things go wrong
- Why the solutions make these trade-offs
- How to observe, verify, and scale it online

If you can explain all of these clearly, you have usually moved from "understanding architecture terms" to "truly being able to command an online MMO system".
