[English](q32-kbengine-cellapp-communication.en.md) | [中文](q32-kbengine-cellapp-communication.md)

# Q32: How do CellApps communicate within the same machine / across machines? What is the evidence?

## Core Conclusion

In this kind of system, same-machine and cross-machine communication usually have no essential difference in logical semantics; the difference is mainly in the transport path and cost:

- Same-machine communication has lower latency, and the system call and network stack path may be shorter
- Cross-machine communication has to bear real network overhead and more complex failure conditions

But no matter same-machine or cross-machine, the core content passed between CellApps is still:

- Entity migration
- Ghost sync
- Boundary collaboration

## 1. Don't understand "same machine" and "cross machine" as two different semantic systems

For the upper-layer logic, the goal is usually:

- Find the target CellApp
- Send internal messages
- Handle them by protocol on the target side

The difference mainly lies in whether the underlying connection goes through the local loopback, IPC, or a real network.

## 2. Why same-machine and cross-machine both need a unified internal protocol

Because as long as the logical semantics differ, the system becomes very hard to maintain:

- Test environment and production environment behave inconsistently
- Semantics change after node migration
- Troubleshooting is difficult

A more robust approach is usually:

- Unified message definitions
- Unified routing semantics
- Only distinguish local and remote optimizations at the underlying transport implementation

## 3. Characteristics of same-machine communication

Usually manifested as:

- Lower latency
- More abundant bandwidth
- More suited to high-frequency internal messages

But it does not mean the communication volume can be arbitrarily amplified. As long as the message volume is too high, same-machine communication is also dragged down by serialization, queues, context switching, and cache invalidation.

## 4. Characteristics of cross-machine communication

Cross-machine links need additional consideration of:

- Network jitter
- Packet loss and retransmission
- Connection recovery
- Failure isolation

This directly affects the design of entity migration and neighbor-cell sync.

## 5. What the really important "evidence" should look at

To judge whether this kind of system supports unified same-machine and cross-machine communication, you should usually focus on several core points in the source code or implementation:

- Whether there is a unified `Channel` or connection abstraction
- Whether there is a unified `EndPoint` / address abstraction
- Whether the network is driven by a `Poller` / event loop
- Whether inter-node communication is all wrapped into a unified message dispatch mechanism

This evidence is more illustrative than simply looking at "whether there is a TCP connection".

## 6. How to evaluate it from an engineering perspective

A more accurate judgment is usually:

- Same-machine and cross-machine share one set of message models at the logic layer
- The underlying layer shields transport differences through the connection abstraction
- Whether to further do local optimization depends on implementation trade-offs

This is more robust than simply saying "same machine uses shared memory, cross machine uses TCP", because many systems actually do not build a whole dedicated set of semantics for same-machine.

## 7. Common misconceptions

### 1. Same-machine communication has no performance problems

No. Message volume, serialization, scheduling, and queue buildup can equally become bottlenecks.

### 2. Cross-machine communication makes no difference as long as the network is fast

No. Cross-machine scenarios have higher jitter, failure, and recovery complexity.

### 3. As long as the underlying layer is all TCP, the design is the same

Incomplete. The more critical thing is whether the connection abstraction, message model, and routing mechanism are unified.

## References

- KBEngine network layer common `Channel` / `EndPoint` / `Poller` design materials
