
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q22: How do you handle out-of-order network messages?

## Core Conclusion

The essence of the out-of-order problem is not "the packet arrived a bit late", but "there are ordering dependencies between messages, and the network did not deliver them in that dependency order".

The key to handling out-of-order messages is not simply requiring "all messages execute in order", but first classifying messages:

- Messages that must strictly be in order
- Messages that can be re-ordered within a window
- Messages that only care about the latest state
- Messages where dropping the old one is fine

If every message is processed in the strictest order, the system is very easily slowed down by head-of-line blocking.

## One: Why Out-of-Order Happens

Over TCP, the application layer usually receives an ordered byte stream; but in UDP, KCP, QUIC, or in multi-channel, multi-queue, multi-hop forwarding systems, out-of-order is very common.

Even when the lower layer is TCP, the application layer can still exhibit "logical out-of-order":

- Multi-threaded concurrent processing changes callback order
- Different service nodes return at different times
- Asynchronous RPC responses arrive in uncontrollable order
- Client local buffering and rendering delays are not synchronized

So out-of-order is not only a transport-layer problem, it is also a system design problem.

## Two: First Distinguish Three Kinds of Ordering

### 1. Transport Order

The order in which packets arrive over the network.

### 2. Processing Order

The execution order after messages enter business logic.

### 3. Presentation Order

The order in which the client ultimately displays state to the player.

These three often differ. Many engineering problems come from treating them as one and the same.

## Three: Which Messages Must Strictly Be in Order

Usually including:

- Session establishment and teardown
- Login, authentication, map switching
- Inventory changes
- Transaction confirmation
- Quest state progression
- Asset settlement

If these messages are executed out of order, the consequence is usually state corruption rather than merely degraded experience.

Handling is generally:

- Strong-order channel
- Sequence number validation
- Buffer and wait for missing messages
- Trigger retransmission or failure after timeout

## Four: Which Messages Do Not Need Strong Ordering

Typically high-frequency state messages, such as:

- Position
- Facing
- Velocity
- Animation state

For this kind of message, what matters more is "freshness", not "no frame can be lost or out of order".

For example in position updates, if `pos=102` arrives first and `pos=101` arrives later, the latter should usually just be discarded rather than forcibly executed.

Otherwise you get:

- Character snapping backward
- The screen going backwards
- Interpolation anomalies

## Five: Common Handling Techniques

### 1. Sequence Numbers

This is the most basic layer.

Each category of order-sensitive message should carry:

- Session sequence number
- In-stream sequence number
- Or entity-level version number

The server or client uses the sequence number to determine:

- Whether it is a duplicate
- Whether it is out of order
- Whether it has already expired

### 2. Reordering Buffer

For messages that can tolerate a short wait, you can set up a small window cache:

- If the next expected value is `N`, but `N+1` arrives first
- Temporarily store `N+1`
- Wait for `N` to arrive, then process in order

This approach suits:

- Reliable but possibly out-of-order message streams
- Businesses that accept a small amount of waiting

But the window cannot be too large, or latency will climb quickly.

### 3. Only Accept the Latest Version

For state messages, a version-overwrite policy is often more suitable:

- Only apply states with a greater version number
- Discard old states outright

This is especially suitable for:

- Position
- Facing
- Health snapshots
- Entity visibility results

### 4. Separate Events from State

Many systems handle out-of-order poorly because they mix "events" and "state" in the same logic chain.

A safer way is to separate them:

- Events are processed in order, e.g. cast start, damage settlement, death confirmation
- State is overwritten by version, e.g. position, animation facing, current HP

This significantly reduces the complexity of out-of-order handling.

## Six: How the Client Presentation Layer Handles It

The client should not apply every message to the screen immediately.

For remote entities, common practice is:

- Maintain a state buffer sorted by time or sequence
- When rendering, read a slightly earlier point in time
- Interpolate between two adjacent valid states

The benefits of doing this are:

- It absorbs mild out-of-order
- It reduces jitter
- It does not have to jump immediately because one packet arrived late

But the premise is that these messages are naturally suited to stateful representation.

## Seven: The Server Interior Also Needs Protection Against Logical Out-of-Order

Many people only handle out-of-order on the client, but internal server problems are more insidious.

For example:

- Two asynchronous RPC responses return in unstable order
- A persistence completion callback arrives later than the player going offline
- Map switching and combat settlement return across services in reverse order

Transport-layer ordering cannot guarantee against this kind of problem; you usually need:

- State machines
- Version numbers
- Request context IDs
- Compare-and-swap style commits

In other words, the system must have the ability to "automatically invalidate late-arriving results".

## Eight: A More Practical Layered Strategy in Practice

A relatively solid approach is:

- Session control messages: strictly in order
- Asset and quest messages: strictly in order plus idempotency
- High-frequency state messages: version overwrite
- Client presentation layer: short buffer plus interpolation
- Internal asynchronous callbacks: request ID plus state machine validation

This is more realistic than "one sequence number for all messages".

## Nine: Common Misconceptions

### 1. TCP Has No Out-of-Order Problems

Not accurate. TCP guarantees byte-stream ordering, but the application layer can still exhibit logical out-of-order due to asynchronous processing, concurrent tasks, or cross-service returns.

### 2. If Out-of-Order Is Detected, Buffer Everything and Wait

This will slow down the high-frequency real-time path. Many state messages should simply discard old versions rather than backfill history.

### 3. Keep Only the Latest for All Messages

That is also wrong. Trading, quests, inventory, and login flows cannot do this, or you will lose critical business steps.

## References

- Glenn Fiedler, *Snapshot Interpolation*
- RFC 9000, QUIC
- Various real-time game network synchronization and message ordering control practice materials
