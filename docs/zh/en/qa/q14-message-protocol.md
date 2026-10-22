
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q14: How to design a message protocol? Protobuf vs JSON vs custom protocol?

## Core Conclusion

The focus of message protocol design is not to pick a side among `JSON / Protobuf / custom binary` first, but to answer four questions first:

- How message boundaries are divided
- How versions evolve
- How debugging is done
- Where the performance bottleneck actually is

In most MMO projects, the more prudent approach is usually:

- The protocol layer first does a unified message header and version boundary
- Business message bodies prefer mature solutions like Protobuf
- JSON is used more for configuration, debugging, and backend interfaces
- Only truly extremely high-frequency, extremely high-performance scenarios consider a small amount of custom binary

---

## 1. First clarify what "message protocol" is actually designing

Many people, as soon as they mention message protocol, start comparing JSON and Protobuf.

In fact, a message protocol has at least three layers of meaning:

### 1.1 Transport encapsulation layer

Responsible for answering:

- Where a message starts and ends
- How the message length is represented
- How the message type is distinguished
- Where the sequence number, session number, and checksum go

### 1.2 Message body structure layer

Responsible for answering:

- How business fields are encoded
- How arrays, nesting, and enums are represented
- How the type system is defined

### 1.3 Evolution and compatibility layer

Responsible for answering:

- Whether old versions can understand new messages
- Whether new versions can be compatible with old clients
- How field deletion, replacement, and retention are done

So protocol design is not finished once you "pick a serialization library."

---

## 2. The most important thing about a good protocol is not speed, but controllability

What protocol design really has to prioritize is usually:

- Clear boundaries
- Evolvable compatibility
- Easy to troubleshoot
- Sufficient performance

Do not reverse the order.

If you only focus on "smallest bytes, fastest speed" at the start, you very easily end up with a protocol that:

- Has low development efficiency
- Is extremely painful to debug
- Has brittle version compatibility
- Nobody on the team dares to touch

Such a protocol is usually not a good protocol in a long-running project.

---

## 3. Which basic rules to define first when designing a protocol

### 3.1 Unified message header

Whatever format the message body uses, it is usually recommended to have a unified header first, containing at least:

- Message ID
- Length
- Sequence number
- Session or connection context
- Version number or protocol marker

The benefits of doing this are:

- Unpacking boundaries are clear
- Debugging is easier
- Switching message body format later is also more convenient

### 3.2 Clarify the three semantics: request, response, notification

This step is often ignored.

Usually you have to at least distinguish:

- Request: the client initiates a request
- Response: the server returns a result
- Notify / Push: the server pushes proactively

If these semantics are unclear, later it is very easy to get:

- Unclear who should carry a sequence number
- Unclear who should wait for a reply packet
- Timeout and retry logic that is hard to do

### 3.3 Design error codes and observability first

A protocol is not only "able to transmit data." It also has to make online problems visible.

At minimum you have to consider:

- Common error codes
- Illegal message handling
- How to print message summaries in logs
- Whether debugging tools can quickly view packet contents

---

## 4. What JSON, Protobuf, and custom protocols are each suited to

### 4.1 JSON

The greatest value of JSON is not performance, but:

- Human-readable
- Convenient to debug
- Naturally compatible with Web and backend systems

It suits:

- Configuration files
- GM / admin backend interfaces
- Debugging interfaces
- Temporary protocols during development

It does not suit well:

- High-frequency binary messages
- Bandwidth-sensitive real-time main paths

The reason is simple:

- Large size
- Slow parsing
- Weak type constraints

### 4.2 Protobuf

The greatest value of Protobuf is:

- It has a type system
- It has mature code generation
- It has explicit field numbering and evolution rules
- Size and performance are both well balanced

It usually suits very well:

- Main business communication between client and server
- Internal server RPC
- Message definitions that need cross-language collaboration

In most projects, if there is no particularly strong reason, Protobuf is often a more stable default choice than "developing a protocol yourself."

### 4.3 Custom binary protocol

The advantages of a custom protocol are:

- Can be completely tailored to the business
- Can be compressed to the extreme
- Can reduce redundant fields and metadata

But the costs are equally obvious:

- High development cost
- Hard to debug
- High compatibility risk
- High team maintenance bar

So it is more suited to:

- Extremely high-frequency messages
- Extreme performance hot spots
- Scenarios where the team has long-term maintenance capability

Rather than "customize everything from the start of the project."

---

## 5. Why most projects prefer Protobuf first

This is a very pragmatic choice.

The reason is usually not that it is absolutely the fastest, but that it is well balanced across many dimensions:

- Performance is high enough
- Size is small enough
- The compatibility mechanism is clear
- The toolchain is mature
- Team collaboration cost is low

For MMO, this value of "most stable overall" is often higher than any single performance metric.

If you have not yet proven that protocol serialization is the bottleneck, customizing a protocol early usually does not pay off much.

---

## 6. The scenarios where a custom protocol is truly worth investing in

### 6.1 High-frequency state synchronization

For example:

- Position
- Facing
- Input state
- High-frequency AOI updates

The characteristics of these messages are:

- High frequency
- Fixed fields
- Very sensitive to size

At this point you can consider doing a dedicated, lighter binary layout.

### 6.2 Fixed structure, extremely small messages

If a message always has only a few fixed fields, using a complete general-purpose serialization framework may actually have extra cost.

### 6.3 But you still have to watch the boundary

Even if you do custom, it is not recommended to drag the whole system in.

A more reasonable approach is usually:

- Main business messages still use a mature protocol
- A small number of hot-spot messages are customized separately

That balances benefit and maintenance cost better.

---

## 7. Protocol evolution matters more than format selection

In long-running projects, what really kills a protocol is usually not the serialization method, but an evolution strategy that was not designed well.

### 7.1 Common evolution rules

- New fields should be backward-compatible as much as possible
- Do not reuse old field numbers casually
- Deprecate a field before deleting it
- The server should tolerate old clients missing new fields

### 7.2 What is most feared is not "too many fields," but "semantic drift"

For example:

- A field initially means "level"
- Later it is quietly changed to "rank"

This kind of problem is far more dangerous than the format choice itself.

### 7.3 So the protocol documentation must be stable

A message protocol had better:

- Have a single source of definition
- Have version management
- Have field comments
- Have a generation process

Otherwise you very quickly get a situation where the client and the server each understand it differently.

---

## 8. Debugging capability must be treated as part of protocol design

If a protocol is fast but cannot be quickly understood online, the long-term cost will be very high.

At minimum you have to consider:

- Whether message summaries can be printed
- Whether binary messages can be converted to readable text
- Whether there are packet capture and replay tools
- Whether field misalignment and version mismatch can be quickly located

This is also why many projects, even when the main protocol uses Protobuf, still keep:

- JSON debug output
- Text log mapping
- Protocol visualization tools

---

## 9. A more practical way to choose

### 9.1 Default strategy

In most cases you can do this first:

- Unified message header: custom
- Business message body: Protobuf
- Configuration and backend interfaces: JSON

This is a very common and very stable combination.

### 9.2 When to introduce custom binary

Consider it when these conditions are met:

- Certain messages are stable hot spots
- Serialization and bandwidth have been confirmed as the main bottleneck
- The team has toolchain and debugging capability

### 9.3 Do not treat "pursuing the extreme" as the default route

The mistake many teams most easily make is:

- Wanting to do the most extreme custom protocol right from the start
- Ending up, before even launching, held back by the protocol toolchain and compatibility problems

A better approach is usually:

- First get it working with a mature protocol
- Then optimize locally against real hot spots

---

## 10. Summary

The core of message protocol design is not to choose JSON, Protobuf, or custom first, but to design the boundaries, versions, debugging, and evolution method clearly first.

For most MMO projects, the more prudent experience is usually:

- Prioritize Protobuf for main business communication
- Prioritize JSON for configuration and debugging
- Consider local customization for high-frequency hot-spot messages

If protocol evolution and debugging tools are not done well first, even the fastest protocol can easily become a burden in long-term maintenance.

---

## References

- [Protobuf](https://protobuf.dev/)
- [FlatBuffers](https://google.github.io/flatbuffers/)
- [MessagePack](https://msgpack.org/)
- [KBEngine GitHub - Messages](https://github.com/kbengine/kbengine/tree/master/kbe/src/server/messages)
