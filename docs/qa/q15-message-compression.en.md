[English](q15-message-compression.en.md) | [中文](q15-message-compression.md)

# Q15: How Do You Implement Message Compression?

## Core Conclusion

The key to message compression is not "picking the strongest compression algorithm", but first judging:

- Whether this message is worth compressing
- Whether compressing it actually saves total cost
- Whether what is saved is bandwidth, or whether the pressure is just shifted onto the CPU

For MMOs, the most common and safest principles are usually:

- High-frequency small messages are usually not compressed
- Text, large packets, and batch data are better suited to compression
- Compression must have threshold control and type-based routing
- The compression strategy must be considered together with message frequency, bandwidth pressure, and CPU budget

---

## 1. First Clarify What Compression Is Actually Trading

Compression is not a free optimization.

It is essentially making a trade:

- Trading CPU time for bandwidth
- Trading encoding and decoding complexity for smaller data size

So whether compression is worthwhile depends on what is actually scarce right now.

For example:

- If network bandwidth is tight, compression may well be worth it
- If the CPU is already hot, compression may backfire
- If the message itself is tiny, compression metadata and algorithm overhead may exceed the benefit

So the first principle is not "compress whenever possible", but "do the math first".

---

## 2. Not All Messages Are Suited to Compression

### 2.1 High-Frequency Small Messages Are Usually Unsuitable

For example:

- Position synchronization
- Facing synchronization
- Action state
- Small combat operations

There are usually three reasons:

- The packet itself is too small, leaving limited room to compress
- The algorithm call overhead is relatively more expensive
- High-frequency links fear latency jitter more

The most common optimizations for this kind of message are usually not compression, but:

- Reducing fields
- Fixed-point quantization
- Bit packing
- Lowering frequency
- Sensible broadcast pruning

### 2.2 Medium and Large Messages Are More Worth Compressing

For example:

- Long chat text
- Large batches of inventory or character data
- Leaderboard snapshots
- Configuration delivery
- Batch log reporting

This kind of message usually has:

- Lots of repeated information
- A high proportion of text
- A clearly large per-packet size

So the compression benefit is more stable.

---

## 3. Do "Structural Optimization" Before Compressing

The problem in many systems is not "no compression", but that the message structure itself is too wasteful.

For example:

- Field names repeated over and over
- Sending strings for things that are clearly enums
- Coordinates sent as text floats instead of binary values
- Sending large numbers of fields the client never uses

What should be done first for these problems is:

- Slimming down fields
- Choosing a more compact serialization format
- Trimming data to what the client actually subscribes to
- Avoiding redundant sends

If the structure is still bloated and you turn on compression first, you usually just hide the problem.

---

## 4. When Is Compression Most Cost-Effective

### 4.1 Text-Type Messages

For example:

- JSON
- Chat content
- Configuration text
- Log text

This kind of data often has high redundancy, so compression benefit is usually very good.

### 4.2 Batch Aggregate Messages

For example:

- Batch inventory synchronization
- Batch leaderboard synchronization
- Batch log reporting

A single message is not large, but once aggregated the whole is very large, so compression is often more cost-effective here.

### 4.3 Periodic Large Packets

If a class of messages is not high-frequency real-time, but low-frequency large packets, it is usually more suited to compression.

Because:

- Latency sensitivity is not that high
- Bandwidth benefit is more obvious

---

## 5. When Compression Is Actually Not Cost-Effective

### 5.1 Small Packets

Small messages of a few dozen bytes to a hundred or two hundred bytes often save little after compression, and can even end up larger.

### 5.2 Extremely Time-Sensitive Messages

For example:

- High-frequency real-time operations
- Critical input streams during combat

If compression and decompression introduce extra jitter, the benefit may not be worth it.

### 5.3 Data That Is Already Hard to Compress

For example:

- Already very compact binary structures
- Highly random data
- Content that has already been compressed

Common outcomes for this kind of data are:

- Very poor compression ratio
- Even "the more you compress, the bigger it gets"

---

## 6. Algorithm Choice Must Not Be Detached from the Scenario

### 6.1 Fast Compression Class

For example:

- LZ4
- Snappy

Characteristics:

- Not compressed as aggressively
- But fast

Suitable for:

- Online links
- Scenarios more concerned with throughput and latency

### 6.2 Balanced Type

For example:

- zlib
- Medium and low compression levels of zstd

Characteristics:

- A fairly balanced compression ratio and speed

Suitable for:

- General message compression
- Most medium-load scenarios

### 6.3 High Compression Ratio Class

For example:

- High levels of zstd
- LZMA

Characteristics:

- High compression ratio
- High CPU cost as well

More suitable for:

- Offline packaging
- Configuration distribution
- Non-real-time large files

Not very suitable for the main real-time link.

---

## 7. The Really Practical Strategy: Threshold Compression

The most common correct posture for message compression is not "compress everything", but "compress only past a threshold".

### 7.1 Why a Threshold Is Needed

Because compression itself has fixed costs:

- The cost of making the decision
- The compression function call
- The decompression function call
- Header flags and protocol overhead

If the message is too small, these fixed costs eat up the benefit.

### 7.2 The Threshold Is Not a Hardcoded Theoretical Value

The threshold should be determined by actual measurement, for example:

- Below 128B, usually do not compress
- 256B to 1KB, decide by message type
- Above 1KB, prioritize compression

But the specific values depend on:

- Your protocol header size
- The compression algorithm
- CPU budget
- Network pressure

---

## 8. The Compression Strategy Must Be Bound to Message Types

A more mature implementation is generally not "one unified compressor handling all messages", but routing by message type.

### 8.1 Typical Strategy

- High-frequency state messages: do not compress
- Medium-frequency text messages: compress by threshold
- Large snapshot messages: prioritize compression
- Offline or background messages: may use a higher compression ratio

### 8.2 Why Route This Way

Because different messages care about different metrics:

- Some care about latency
- Some care about bandwidth
- Some care about CPU
- Some care about total throughput

If all messages share one policy, you usually end up optimizing one class of message well while making another class worse.

---

## 9. Compression Is Not a Single-Point Optimization; Look at the Whole Link

Message compression often works together with these optimizations:

- Packet batching
- Delta synchronization
- Bit packing
- Field trimming
- AOI limits
- Batch sending

In many cases, the truly effective order should be:

1. First reduce the messages that should not be sent
2. Then slim down the messages that must be sent
3. Finally compress the remaining messages

If the order is reversed, it is easy to end up with:

- First compressing the garbage traffic smaller
- But the total volume is still huge

That is not the optimal path.

---

## 10. Which Metrics Must Be Monitored Online

If you do message compression, you cannot judge the effect by feel alone.

At minimum you must look at:

- Bytes before compression
- Bytes after compression
- Average compression ratio
- Compression CPU time
- Decompression CPU time
- Hit rate by message type

Otherwise you are likely to assume "it must be better after compression", when in reality:

- The CPU has been pushed up
- Latency jitter has grown
- Bandwidth savings are not obvious

---

## 11. A More Practical Engineering Suggestion

If the current project is going to do message compression, the safer rollout order is usually:

### Step 1: First Classify by Message

- High-frequency small packets
- Medium text packets
- Low-frequency large packets

### Step 2: First Do Threshold Compression

- Do not force compression on everything
- Start with the obviously large packets

### Step 3: When Choosing an Algorithm, Prioritize "Stable Benefit"

In most online scenarios, compared with pursuing the extreme compression ratio, what matters more is:

- Compressing fast enough
- Decompressing fast enough
- Predictable behavior

### Step 4: After Launch, Look at the Data and Fine-Tune

Truly reasonable thresholds and algorithms are usually not decided on a whim, but jointly determined by stress tests and online data.

---

## 12. Summary

The core of message compression is not "pick the strongest algorithm", but first judging:

- Whether this message is worth compressing
- Whether what compression saves is the current real bottleneck
- Whether compression will hurt latency and CPU in return

For MMOs, the more mature experience is usually:

- Do not compress high-frequency small messages first
- Prioritize compressing text and large packets
- Do threshold control and type-based routing for compression
- Do structural optimization and message pruning before compression

If these judgments are not made first, message compression can easily go from an optimization means to a new burden.

---

## References

- [zlib](https://www.zlib.net/)
- [LZ4](https://lz4.github.io/lz4/)
- [Zstandard](https://github.com/facebook/zstd)