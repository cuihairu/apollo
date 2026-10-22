
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q64: How to Optimize Network Bandwidth?

## Core Conclusion

The most effective means of bandwidth optimization is usually not "compressing packets smaller," but first reducing content that should never have been sent at all.

The real optimization order should often be:

- Send less
- Send later
- Send merged
- Compress last

If broadcast scope, sync frequency, and message semantics are not controlled, adding compression alone yields very limited benefit.

## I. First Look at Where the Bandwidth Goes

In online games, the bulk is usually not low-frequency system messages, but:

- Position synchronization
- Animation and state updates
- Combat broadcasts
- Entity creation and destruction within AOI

So bandwidth optimization should prioritize high-frequency state streams, rather than starting by squeezing cold messages.

## II. The Most Effective First Step Is Reducing Message Count

Common approaches include:

- AOI culling
- Lowering the frequency of low-value messages
- Overwriting old values with new state
- Removing meaningless fields

These often bring greater benefit than the compression algorithm itself.

## III. First Distinguish Events from State

### 1. State Messages

For example:

- Position
- Facing direction
- Current action

These messages care more about the "latest value," and are well suited for:

- Delta sync
- Overwriting old messages
- Client-side interpolation

### 2. Event Messages

For example:

- Skill cast
- Death
- Loot spawn

These messages care about semantic completeness; you cannot simply keep only the latest one.

If both are mixed under the same send strategy, both bandwidth and experience usually suffer.

## IV. Batching and Merging Are Often Very Valuable

Many high-frequency small messages are individually small, but packet headers, system calls, and protocol overhead add up considerably.

So common practice is:

- Batch sending within one tick
- Merging same-type updates into one packet
- Batch snapshots of multiple entities' state

But the batch window must not be too large, or it increases perceived latency.

## V. Compression Should Be Chosen by Message Type

Compression is not "the more the better."

Considerations:

- Small packets may gain very little from compression
- Compression and decompression also consume CPU
- High-frequency real-time messages may not suit heavy compression

So the more common approach is:

- Large, low-frequency packets suit compression
- High-frequency small packets should first rely on structural optimization and batch sending

## VI. Field Design Directly Affects Bandwidth

Common optimization points include:

- Using enums and integers instead of strings
- Using fixed-point or quantized values instead of high-precision floats
- Syncing only dirty fields
- Removing fields the client can derive locally

These are more fundamental than "turning on gzip."

## VII. A More Robust Bandwidth Optimization Order

Common practice is:

1. First measure the proportion of each message type
2. Prioritize optimizing high-frequency state messages
3. Use AOI and priority to reduce useless sends
4. Then do batching, delta, and structural compression

This usually best matches the order of benefit.

## VIII. Common Misconceptions

### 1. Bandwidth optimization is message compression

Compression is only the last layer; there is more room for gain before it.

### 2. All messages should be synced at high frequency in real time

Many low-value states are simply not worth it.

### 3. Smaller packets are always better

Not necessarily. Over-fragmentation increases packet header and system call costs.

## References

- Real-time state sync, snapshot compression, and AOI culling practice materials
