
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q24: How Do You Design a Broadcast Mechanism? How Do You Optimize Large-Scale Broadcasts?

## Core Conclusion

The essence of broadcasting is not "sending one message to many people", but "distributing information to the right scope, at the right time, at a suitable cost".

For real large-scale broadcast optimization, the first step is usually not squeezing the network library, but first reducing the messages that should not be sent:

- Do not send to people who should not see it
- Do not send unimportant state at high frequency
- Do not use the same broadcast strategy for different messages

If scope control is not done well, the later compression, batching, and asynchronous sending can only stop the loss; they cannot solve the problem at the root.

## 1. First Classify Broadcasts by Type

Broadcasts can be divided into at least four categories:

### 1. Local Broadcast Within AOI

For example:

- Movement
- Skill effects
- Entity creation and destruction
- HP, facing, and Buff changes

This is the most common category in MMOs, and the bulk of the traffic.

### 2. Logical-Domain Broadcast

For example:

- Room broadcast
- Team broadcast
- Instance broadcast
- Guild broadcast

Its receiving scope is usually an explicit set, not entirely dependent on spatial position.

### 3. Global or Large-Zone Broadcast

For example:

- System announcements
- Cross-server event notifications
- Leaderboard settlement

This kind of message has low frequency but wide coverage.

### 4. Internal Service Broadcast

For example:

- Configuration hot-update notifications
- Node state changes
- Subscription event distribution

This kind of broadcast is not necessarily aimed at player connections, but it still incurs fan-out cost.

Broadcasts must be classified first; otherwise all problems get misattributed to "network send optimization".

## 2. Where Does the Biggest Cost of Broadcasting Come From

Many people intuitively feel the broadcast bottleneck is the socket `send`, but in engineering the common big items are actually:

- Receiver set computation
- AOI queries
- Serialization and copying
- Repeated encoding on many connections
- Repeated forwarding across nodes

Especially for position and combat state broadcasts, if every time you re-query all receivers, re-pack the packet, and encode per connection, the overhead quickly gets out of control.

## 3. First Optimization Principle: First Shrink the Broadcast Scope

### 1. AOI Pruning

The most basic and most effective.

Only players who can actually see it and need to know the message should receive it.

This requires the AOI system to not only answer "who is nearby", but also maintain at low cost:

- The enter set
- The leave set
- The continuously visible set

### 2. Logical Grouping

Logical domains such as teams, rooms, guilds, and battlefields should not scan the whole server on the fly each time; member sets should be maintained in advance.

### 3. Lower Frequency by Importance

Not all state deserves high-frequency broadcast.

For example:

- Position and facing: high frequency
- HP: medium frequency
- Attribute panel: low frequency
- Non-critical decorative state: even lower frequency

If all fields broadcast at the same frequency as position, bandwidth will soon explode.

## 4. Second Optimization Principle: Distinguish Events from State

Broadcasts often mix two completely different kinds of content:

- Events, such as casting a spell, death, loot drops, level-ups
- State, such as position, HP, facing, current action

These two kinds of messages suit different strategies:

- Events emphasize reliable delivery and ordering
- State emphasizes being latest, overwritable, and interpolatable

If you handle both with the same reliable broadcast strategy, either the state broadcast is too heavy, or the critical events are not stable enough.

## 5. Common Optimization Techniques

### 1. Batch Sending

Within one tick, merging multiple small updates into a batch before sending is usually cheaper than sending immediately on every change:

- Fewer system calls
- Lower header overhead ratio
- Easier to compress

But the batch window must not be too large, or it increases perceived latency.

### 2. Dirty Fields and Delta Synchronization

Many messages only change a few fields; there is no need to resend the whole object.

Common practice is:

- Mark dirty fields
- Broadcast a version number or change mask
- The client merges by field

### 3. Multiple Connections Reuse the Same Encoding Result

If the same broadcast is to be sent to many connections, do not re-serialize for each connection.

The better approach is usually:

- Encode the shared message body once
- Each connection only adds a small header or reference

### 4. Hierarchical Forwarding

Large-range broadcasts should not always be pushed directly by a single node to all connections. Common practice is:

- First distribute by scene, partition, or gateway
- Then have edge nodes fan out to local connections

This makes it easier to:

- Reduce cross-machine traffic
- Control the failure domain
- Shorten single-point hotspot links

## 6. Why AOI Broadcasts Are Usually the Hardest

Because they simultaneously require:

- High frequency
- Low latency
- Dynamically changing scope
- Receiver sets strongly tied to spatial position

Once a movement event fires, the system often must not only broadcast the position change, but also determine:

- Which players newly see the entity
- Which players can no longer see it
- Which players are still within the visible range

Therefore AOI broadcast optimization usually has to be designed together with the AOI data structure, not bolted on afterward.

## 7. How to Avoid Storms During Large-Scale Broadcasts

### 1. Avoid Whole-Server Instant Fan-Out

For things like world announcements or event-start notifications, try not to have one central node push synchronously to all online users.

The safer approach is:

- Layered delivery through gateways
- Sharded parallel sending
- Smooth spreading when necessary

### 2. Give Broadcasts Priority

When the send queue is congested, it should be clear which messages can be delayed, merged, or even dropped.

For example:

- Critical events first
- High-frequency position updates may overwrite old packets
- Non-critical hints can be downgraded

### 3. Control Backpressure

Slow connections must not drag down the send queue without limit. For connections that clearly cannot keep up, the system needs:

- Dropping of stale state messages
- Frequency reduction
- Disconnect protection

Otherwise a few slow connections can drag down the whole broadcast link.

## 8. The Common Safe Combination in Engineering

Many MMOs eventually converge on a strategy like:

- State broadcast within AOI: delta plus batching; position-type messages may overwrite old versions
- Critical combat events: reliable ordered broadcast
- Global notifications: hierarchical forwarding
- Gateway side: per-connection queues and backpressure control
- Server side: aggregate receiver sets by scene or partition

The focus is always to first classify by type, then decide the frequency, reliability, and propagation scope for each class of message.

## 9. Common Misconceptions

### 1. Broadcast Optimization Is Message Compression

Compression is useful, but it is not the first priority. Scope control is usually more important than compression.

### 2. The More Real-Time the Broadcast, the Better

No. Many low-value messages have no impact on experience even if they arrive tens of milliseconds late, yet they significantly increase system cost.

### 3. Broadcasting Is Only Related to the Network Layer

No. AOI, data modeling, dirty field management, gateway layering, and send queue strategy all determine broadcast quality.

## References

- Glenn Fiedler, *Snapshot Compression* / *Snapshot Interpolation*
- Various MMO AOI and broadcast system design practice materials
