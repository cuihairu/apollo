[English](q23-rpc-implementation.en.md) | [中文](q23-rpc-implementation.md)

# Q23: How Do You Implement an RPC Call?

## Core Conclusion

The essence of RPC is not "being as elegant as calling a local function", but "wrapping a remote request into a governable calling protocol".

A real RPC design must at least solve:

- How to locate the target
- How to serialize parameters
- How to correlate requests with responses
- How to handle timeout, failure, retry, and idempotency
- How to observe call quality

If you stay at the surface of "remote function calling", problems will surface in engineering very quickly.

## I. What an RPC Actually Does

A single RPC call usually contains these steps:

1. The caller constructs the request
2. The method name, target object, and parameters are serialized
3. It is sent over the network to the target node
4. The target node deserializes and dispatches to the corresponding handler
5. After execution, a result or error is returned
6. The caller looks up the waiting context by request ID

So RPC is not just a function-call interface, but a whole set of communication conventions.

## II. The Core Parts of an RPC

### 1. Service Discovery or Target Addressing

The caller must know where to send the request.

Common approaches include:

- Fixed node address
- Routing table
- Gateway forwarding
- Registry center
- Routing based on entity ID or shard key

In game services, many RPCs are not "find a service name" but "find the node responsible for this player, scene, room, or Cell".

### 2. Protocol and Serialization

An RPC needs to define the request packet structure, usually containing at least:

- `request_id`
- `method`
- `target`
- `timeout`
- `payload`

Serialization can use:

- Protobuf
- FlatBuffers
- MessagePack
- A custom binary protocol

The point here is not "which one is most advanced", but whether the team can evolve and debug it stably.

### 3. Request Context Management

After the caller sends the request, it must correlate the response back with the original request.

So you need to maintain:

- A table of outstanding requests
- Timeout timers
- Cancellation or invalidation logic

If this layer is not done well, things get messy during timeouts, retries, and node switching.

### 4. Error Model

RPC failure is not just one kind of failure.

At minimum you should distinguish:

- Network unreachable
- Target does not exist
- Business rejection
- Call timeout
- Node busy
- Result unknown

"Result unknown" is especially important, because a timeout does not necessarily mean the operation did not execute; it may only mean the result did not come back.

## III. How to Choose Between Synchronous RPC and Asynchronous RPC

### 1. Synchronous RPC

The benefit is that the calling code is straightforward, suitable for:

- Initialization flows
- Admin backends
- Low-frequency tooling

The downsides are equally clear:

- Easy to block threads
- Easy to lengthen call chains
- A slightly slower upstream causes cascading buildup layer by layer

### 2. Asynchronous RPC

Asynchronous is better suited to the main chain of a game service, because it makes it easier to:

- Control thread occupancy
- Do timeouts and cancellation
- Fit an event-driven model

The cost is:

- More complex code structure
- Error handling is easier to miss

In online games, most core chains eventually lean toward asynchronous or message-driven, rather than widespread synchronous waiting.

## IV. How RPC in Game Services Differs from Generic Microservice RPC

RPC in game services often emphasizes more:

- Entity- or partition-based routing
- High-frequency short messages
- Tight coupling with state machines, AOI, rooms, and sessions
- Very high sensitivity to tail latency

It is not exactly the same as backend business services. Many times "method calling" is only the surface form; underneath it looks more like a targeted messaging mechanism.

For example, when a player uses a skill, the real chain may be:

- The gateway receives the packet
- It is routed to the logical node the player is on
- The logical node then calls the scene node
- The scene node triggers nearby broadcast or settlement

What matters most in this chain is not whether the function interface looks local, but whether each hop clearly knows its responsibilities and timeout semantics.

## V. What Is Most Often Overlooked When Implementing RPC

### 1. A Timeout Does Not Equal a Failed Rollback

If a request times out, the caller can only confirm "the result did not return within the time limit", and cannot directly infer "the target definitely did not execute".

This directly affects:

- Whether retry is allowed
- Whether idempotency is needed
- Whether the final state needs to be queried

### 2. Retries Must Look at Operation Semantics

Not all RPCs can be retried automatically.

Usually suitable for retry:

- Read-only queries
- Idempotent writes
- Operations that explicitly support de-duplication

Not suitable for blind retry:

- Charging
- Granting rewards
- Migration and leader switching
- Multi-stage state advancement

### 3. Observability Must Be Built In

Once an RPC crosses process boundaries, troubleshooting depends heavily on:

- Request ID
- Call chain logs
- Timeout distribution
- Error codes
- Per-node statistics

Without these, online problems are almost untraceable.

## VI. A Practical RPC Packet Structure

Most custom RPCs eventually converge to a similar structure:

- Packet header: magic number, version, length, flags
- Routing fields: service name, node ID, entity ID, partition key
- Request fields: `request_id`, `method_id`
- Metadata: timeout, trace_id, retry flag
- Payload: serialized parameters

The response packet then contains:

- `request_id`
- `status`
- `error_code`
- `payload`

The structure is not complex, but you must always reserve room for version evolution.

## VII. A Safer Practice in Game Services

Many times, a better practice than "writing RPC everywhere" is to layer:

- Query-type requests can use RPC
- High-value state advancement uses message-driven or single-threaded entity mailboxes
- Broadcast or event propagation uses an event bus
- Strong consistency chains should minimize cross-node synchronous calls

The more cross-node synchronous waits there are, the easier it is to form long call chains and cascading timeouts.

## VIII. Common Misconceptions

### 1. RPC Is Just Syntactic Sugar for Remote Function Calls

Wrong. The truly hard parts are timeout, routing, error models, retry semantics, and observability.

### 2. All Service Interaction Is Suitable for RPC

Wrong. Large fan-out, asynchronous broadcast, and event propagation scenarios often suit message queues or event mechanisms better.

### 3. Automatic Retry After Timeout Is Safest

Wrong. Many write operations end up in a "result unknown" state after timeout, and retry may create double writes.

## References

- Birrell and Nelson, *Implementing Remote Procedure Calls*
- gRPC design documents
- Various materials on entity routing and asynchronous calling practices in online game services
